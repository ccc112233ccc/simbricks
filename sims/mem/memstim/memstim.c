/*
 * Copyright 2024 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <simbricks/base/manager.h>
#include <simbricks/mem/if.h>
#include <simbricks/mem/proto.h>

static int exiting = 0;
static uint64_t cur_ts = 0;

static void sigint_handler(int dummy) {
  (void)dummy;
  exiting = 1;
}

static void sigusr1_handler(int dummy) {
  (void)dummy;
  fprintf(stderr, "main_time = %lu\n", cur_ts);
}

static volatile union SimbricksProtoMemH2M *H2MAlloc(
    struct SimbricksMemIf *memif, uint64_t ts) {
  volatile union SimbricksProtoMemH2M *msg_to;
  bool first = true;

  while ((msg_to = SimbricksMemIfH2MOutAlloc(memif, ts)) == NULL) {
    if (first) {
      fprintf(stderr, "H2MAlloc: warning waiting for entry (%zu)\n",
              memif->base.out_pos);
      first = false;
    }
  }

  if (!first) {
    fprintf(stderr, "H2MAlloc: entry successfully allocated\n");
  }

  return msg_to;
}

static int WaitForCompletion(struct SimbricksMemIf *memif, uint64_t req_id,
                             uint8_t expected_type,
                             const uint8_t *expected_data, uint16_t len) {
  while (!exiting) {
    while (SimbricksMemIfH2MOutSync(memif, cur_ts)) {
      fprintf(stderr, "warn: SimbricksMemIfSync failed (t=%lu)\n", cur_ts);
    }

    volatile union SimbricksProtoMemM2H *msg =
        SimbricksMemIfM2HInPoll(memif, cur_ts);

    if (msg == NULL) {
      if (memif->base.sync) {
        uint64_t next_ts = SimbricksMemIfM2HInTimestamp(memif);
        if (next_ts > cur_ts) {
          cur_ts = next_ts;
        }
      }
      continue;
    }

    uint8_t type = SimbricksMemIfM2HInType(memif, msg);
    switch (type) {
      case SIMBRICKS_PROTO_MEM_M2H_MSG_READCOMP:
        if (msg->readcomp.req_id != req_id) {
          fprintf(stderr,
                  "unexpected read completion req_id=%lu expected=%lu\n",
                  msg->readcomp.req_id, req_id);
          SimbricksMemIfM2HInDone(memif, msg);
          continue;
        }
        if (expected_type != SIMBRICKS_PROTO_MEM_M2H_MSG_READCOMP) {
          fprintf(stderr, "unexpected read completion while waiting\n");
          SimbricksMemIfM2HInDone(memif, msg);
          return -1;
        }
        if (expected_data != NULL &&
            memcmp((const void *)msg->readcomp.data, expected_data, len) != 0) {
          fprintf(stderr, "read data mismatch for req_id=%lu\n", req_id);
          SimbricksMemIfM2HInDone(memif, msg);
          return -1;
        }
        SimbricksMemIfM2HInDone(memif, msg);
        return 0;
      case SIMBRICKS_PROTO_MEM_M2H_MSG_WRITECOMP:
        if (msg->writecomp.req_id != req_id) {
          fprintf(stderr,
                  "unexpected write completion req_id=%lu expected=%lu\n",
                  msg->writecomp.req_id, req_id);
          SimbricksMemIfM2HInDone(memif, msg);
          continue;
        }
        if (expected_type != SIMBRICKS_PROTO_MEM_M2H_MSG_WRITECOMP) {
          fprintf(stderr, "unexpected write completion while waiting\n");
          SimbricksMemIfM2HInDone(memif, msg);
          return -1;
        }
        SimbricksMemIfM2HInDone(memif, msg);
        return 0;
      case SIMBRICKS_PROTO_MSG_TYPE_SYNC:
        SimbricksMemIfM2HInDone(memif, msg);
        break;
      default:
        fprintf(stderr, "WaitForCompletion: unsupported type=%u\n", type);
        SimbricksMemIfM2HInDone(memif, msg);
    }
  }

  return -1;
}

static void FillPattern(uint8_t *buf, uint16_t len, uint64_t seed) {
  for (uint16_t i = 0; i < len; i++) {
    buf[i] = (uint8_t)((seed + i) & 0xff);
  }
}

static size_t MaxWritePayload(struct SimbricksMemIf *memif) {
  size_t msg_len = SimbricksMemIfH2MOutMsgLen(memif);
  size_t header_len = offsetof(struct SimbricksProtoMemH2MWrite, data);
  if (msg_len <= header_len) {
    return 0;
  }
  return msg_len - header_len;
}

bool MemifInit(struct SimbricksMemIf *memif,
               struct SimbricksBaseIfParams *memParams,
               const struct SimbricksManagerPort *port) {
  struct SimbricksBaseIf *membase = &memif->base;
  struct SimbricksBaseIfSHMPool pool_;
  memset(&pool_, 0, sizeof(pool_));

  if (SimbricksManagerAttachBaseIf(membase, memParams, &pool_, port) != 0) {
    fprintf(stderr, "MemifInit: attach to manager port failed\n");
    return false;
  }

  printf("done connecting\n");
  return true;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigint_handler);
  signal(SIGUSR1, sigusr1_handler);

  struct SimbricksBaseIfParams memParams;
  struct SimbricksMemIf memif;
  uint64_t base_addr;
  uint64_t as_id;
  uint64_t num_ops;
  uint16_t len;
  uint64_t req_id = 1;
  struct SimbricksManagerPort port;

  SimbricksMemIfDefaultParams(&memParams);

  if (argc < 5 || argc > 9) {
    fprintf(stderr,
            "Usage: memstim [BASE-ADDR] [ASID] [OPS] [LEN] [SYNC-MODE] "
            "[START-TICK] [SYNC-PERIOD] [MEM-LATENCY]\n");
    return -1;
  }

  base_addr = strtoull(argv[1], NULL, 0);
  as_id = strtoull(argv[2], NULL, 0);
  num_ops = strtoull(argv[3], NULL, 0);
  len = (uint16_t)strtoul(argv[4], NULL, 0);

  if (argc >= 6) {
    memParams.sync_mode =
        (enum SimbricksBaseIfSyncMode)strtoul(argv[5], NULL, 0);
  } else {
    memParams.sync_mode = kSimbricksBaseIfSyncOptional;
  }

  if (argc >= 7) {
    cur_ts = strtoull(argv[6], NULL, 0);
  }
  if (argc >= 8) {
    memParams.sync_interval = strtoull(argv[7], NULL, 0) * 1000ULL;
  }
  if (argc >= 9) {
    memParams.link_latency = strtoull(argv[8], NULL, 0) * 1000ULL;
  }

  if (SimbricksManagerGetPortFromEnv("mem", &port) != 0) {
    fprintf(stderr, "failed to load manager port info\n");
    return EXIT_FAILURE;
  }
  if (port.channel_type != kSimbricksManagerChannelShmRing) {
    fprintf(stderr, "unsupported channel type for mem port\n");
    return EXIT_FAILURE;
  }
  memParams.blocking_conn = true;

  if (!MemifInit(&memif, &memParams, &port)) {
    return EXIT_FAILURE;
  }

  size_t max_payload = MaxWritePayload(&memif);
  if (len == 0 || len > max_payload) {
    fprintf(stderr, "len=%u exceeds max payload %zu\n", len, max_payload);
    return EXIT_FAILURE;
  }

  uint8_t *payload = calloc(len, sizeof(uint8_t));
  if (payload == NULL) {
    perror("payload allocation failed");
    return EXIT_FAILURE;
  }

  for (uint64_t i = 0; i < num_ops && !exiting; i++) {
    uint64_t addr = base_addr + i * len;

    FillPattern(payload, len, i);
    volatile union SimbricksProtoMemH2M *write_msg =
        H2MAlloc(&memif, cur_ts);
    write_msg->write.req_id = req_id++;
    write_msg->write.as_id = as_id;
    write_msg->write.addr = addr;
    write_msg->write.len = len;
    memcpy((void *)write_msg->write.data, payload, len);

    SimbricksMemIfH2MOutSend(&memif, write_msg,
                            SIMBRICKS_PROTO_MEM_H2M_MSG_WRITE);

    if (WaitForCompletion(&memif, write_msg->write.req_id,
                          SIMBRICKS_PROTO_MEM_M2H_MSG_WRITECOMP, NULL, 0) !=
        0) {
      fprintf(stderr, "write completion failed for req_id=%lu\n",
              write_msg->write.req_id);
      break;
    }

    volatile union SimbricksProtoMemH2M *read_msg =
        H2MAlloc(&memif, cur_ts);
    read_msg->read.req_id = req_id++;
    read_msg->read.as_id = as_id;
    read_msg->read.addr = addr;
    read_msg->read.len = len;

    SimbricksMemIfH2MOutSend(&memif, read_msg,
                            SIMBRICKS_PROTO_MEM_H2M_MSG_READ);

    if (WaitForCompletion(&memif, read_msg->read.req_id,
                          SIMBRICKS_PROTO_MEM_M2H_MSG_READCOMP, payload,
                          len) != 0) {
      fprintf(stderr, "read completion failed for req_id=%lu\n",
              read_msg->read.req_id);
      break;
    }
  }

  free(payload);
  return 0;
}
