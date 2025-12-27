#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <string>
#include <vector>

#include "lib/ubsim/base/log.hpp"
#include "lib/ubsim/base/manager.hpp"
#include "lib/ubsim/mem/interface.hpp"
#include "lib/ubsim/mem/proto.hpp"

namespace {

volatile sig_atomic_t g_exiting = 0;
uint64_t g_current_ts = 0;

void HandleSigint(int) { g_exiting = 1; }

void HandleSigusr1(int) {
  std::fprintf(stderr, "main_time = %lu\n", g_current_ts);
}

volatile ubsim::MemH2M *AllocH2M(ubsim::MemIf *memif, uint64_t ts) {
  volatile ubsim::MemH2M *msg = nullptr;
  bool warned = false;
  while ((msg = ubsim::H2MOutAlloc(memif, ts)) == nullptr) {
    if (!warned) {
      ubsim::LogWarn("memstim", "waiting for free H2M entry");
      warned = true;
    }
  }
  if (warned) {
    ubsim::LogInfo("memstim", "H2M entry allocated");
  }
  return msg;
}

int WaitForCompletion(ubsim::MemIf *memif, uint64_t req_id,
                      uint8_t expected_type, const uint8_t *expected_data,
                      uint16_t len) {
  while (!g_exiting) {
    while (ubsim::H2MOutSync(memif, g_current_ts)) {
      ubsim::LogWarn("memstim", "sync failed at timestamp " +
                                    std::to_string(g_current_ts));
    }

    volatile ubsim::MemM2H *msg = ubsim::M2HInPoll(memif, g_current_ts);

    if (msg == nullptr) {
      uint64_t next_ts = ubsim::M2HInTimestamp(memif);
      if (next_ts > g_current_ts) {
        g_current_ts = next_ts;
      }
      continue;
    }

    uint8_t type = ubsim::M2HInType(memif, msg);
    switch (type) {
      case ubsim::kMemMsgReadComp:
        if (msg->readcomp.req_id != req_id) {
          ubsim::LogWarn("memstim", "unexpected read completion");
          ubsim::M2HInDone(memif, msg);
          continue;
        }
        if (expected_type != ubsim::kMemMsgReadComp) {
          ubsim::LogError("memstim",
                          "read completion received unexpectedly");
          ubsim::M2HInDone(memif, msg);
          return -1;
        }
        if (expected_data != nullptr &&
            std::memcmp((const void *)msg->readcomp.data, expected_data,
                        len) != 0) {
          ubsim::LogError("memstim", "read data mismatch");
          ubsim::M2HInDone(memif, msg);
          return -1;
        }
        ubsim::M2HInDone(memif, msg);
        return 0;
      case ubsim::kMemMsgWriteComp:
        if (msg->writecomp.req_id != req_id) {
          ubsim::LogWarn("memstim", "unexpected write completion");
          ubsim::M2HInDone(memif, msg);
          continue;
        }
        if (expected_type != ubsim::kMemMsgWriteComp) {
          ubsim::LogError("memstim",
                          "write completion received unexpectedly");
          ubsim::M2HInDone(memif, msg);
          return -1;
        }
        ubsim::M2HInDone(memif, msg);
        return 0;
      case ubsim::kMsgSync:
        ubsim::M2HInDone(memif, msg);
        break;
      default:
        ubsim::LogWarn("memstim",
                       "unsupported M2H message type: " +
                           std::to_string(type));
        ubsim::M2HInDone(memif, msg);
    }
  }

  return -1;
}

void FillPattern(uint8_t *buf, uint16_t len, uint64_t seed) {
  for (uint16_t i = 0; i < len; i++) {
    buf[i] = static_cast<uint8_t>((seed + i) & 0xff);
  }
}

size_t MaxWritePayload(ubsim::MemIf *memif) {
  size_t msg_len = ubsim::H2MOutMsgLen(memif);
  size_t header_len = offsetof(ubsim::MemH2MWrite, data);
  if (msg_len <= header_len) {
    return 0;
  }
  return msg_len - header_len;
}

}  // namespace

int main(int argc, char *argv[]) {
  std::signal(SIGINT, HandleSigint);
  std::signal(SIGUSR1, HandleSigusr1);
  ubsim::Logger::Instance().SetLevel(ubsim::LogLevel::kDebug);

  if (argc < 5 || argc > 9) {
    std::fprintf(stderr,
                 "Usage: memstim [BASE-ADDR] [ASID] [OPS] [LEN] "
                 "[SYNC-MODE] [START-TICK] [SYNC-PERIOD] [MEM-LATENCY]\n");
    return EXIT_FAILURE;
  }

  uint64_t base_addr = std::strtoull(argv[1], nullptr, 0);
  uint64_t as_id = std::strtoull(argv[2], nullptr, 0);
  uint64_t num_ops = std::strtoull(argv[3], nullptr, 0);
  uint16_t len = static_cast<uint16_t>(std::strtoul(argv[4], nullptr, 0));
  ubsim::LogDebug("memstim",
                  "starting base_addr=" + std::to_string(base_addr) +
                      " ops=" + std::to_string(num_ops) +
                      " len=" + std::to_string(len));

  SimbricksBaseIfParams params;
  ubsim::DefaultMemParams(&params);

  if (argc >= 6) {
    params.sync_mode =
        static_cast<SimbricksBaseIfSyncMode>(std::strtoul(argv[5], nullptr, 0));
  }
  if (argc >= 7) {
    g_current_ts = std::strtoull(argv[6], nullptr, 0);
  }
  if (argc >= 8) {
    params.sync_interval = std::strtoull(argv[7], nullptr, 0) * 1000ULL;
  }
  if (argc >= 9) {
    params.link_latency = std::strtoull(argv[8], nullptr, 0) * 1000ULL;
  }

  ubsim::ManagerPort port;
  ubsim::ManagerPortLoader loader;
  if (!loader.LoadFromEnv("mem", &port)) {
    ubsim::LogError("memstim", "failed to load manager port");
    return EXIT_FAILURE;
  }
  ubsim::LogDebug("memstim",
                  "port shm=" + port.shm_path +
                      " in_offset=" + std::to_string(port.in_offset) +
                      " out_offset=" + std::to_string(port.out_offset));

  params.blocking_conn = true;

  ubsim::MemIf memif{};
  ubsim::ChannelAttachment attachment;
  if (!attachment.Attach(&memif.raw.base, &params, port)) {
    ubsim::LogError("memstim", "failed to attach to manager channel");
    return EXIT_FAILURE;
  }

  size_t max_payload = MaxWritePayload(&memif);
  if (len == 0 || len > max_payload) {
    ubsim::LogError("memstim", "len exceeds max payload");
    return EXIT_FAILURE;
  }

  std::vector<uint8_t> payload(len, 0);
  uint64_t req_id = 1;

  for (uint64_t i = 0; i < num_ops && !g_exiting; i++) {
    uint64_t addr = base_addr + i * len;

    FillPattern(payload.data(), len, i);
    volatile ubsim::MemH2M *write_msg =
        AllocH2M(&memif, g_current_ts);
    write_msg->write.req_id = req_id++;
    write_msg->write.as_id = as_id;
    write_msg->write.addr = addr;
    write_msg->write.len = len;
    std::memcpy((void *)write_msg->write.data, payload.data(), len);

    ubsim::H2MOutSend(&memif, write_msg, ubsim::kMemMsgWrite);

    if (WaitForCompletion(&memif, write_msg->write.req_id,
                          ubsim::kMemMsgWriteComp, nullptr,
                          0) != 0) {
      ubsim::LogError("memstim", "write completion failed");
      break;
    }

    volatile ubsim::MemH2M *read_msg =
        AllocH2M(&memif, g_current_ts);
    read_msg->read.req_id = req_id++;
    read_msg->read.as_id = as_id;
    read_msg->read.addr = addr;
    read_msg->read.len = len;

    ubsim::H2MOutSend(&memif, read_msg, ubsim::kMemMsgRead);

    if (WaitForCompletion(&memif, read_msg->read.req_id,
                          ubsim::kMemMsgReadComp,
                          payload.data(), len) != 0) {
      ubsim::LogError("memstim", "read completion failed");
      break;
    }
  }

  return 0;
}
