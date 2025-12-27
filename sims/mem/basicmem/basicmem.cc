#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

volatile ubsim::MemM2H *AllocM2H(ubsim::MemIf *memif, uint64_t ts) {
  volatile ubsim::MemM2H *msg = nullptr;
  bool warned = false;
  while ((msg = ubsim::M2HOutAlloc(memif, ts)) == nullptr) {
    if (!warned) {
      ubsim::LogWarn("basicmem", "waiting for free M2H entry");
      warned = true;
    }
  }
  if (warned) {
    ubsim::LogInfo("basicmem", "M2H entry allocated");
  }
  return msg;
}

void PollH2M(ubsim::MemIf *memif, uint64_t ts,
             std::vector<uint8_t> *memory) {
  volatile ubsim::MemH2M *msg = ubsim::H2MInPoll(memif, ts);
  if (msg == nullptr) {
    return;
  }

  uint8_t type = ubsim::H2MInType(memif, msg);
  switch (type) {
    case ubsim::kMemMsgRead: {
      uint64_t addr = msg->read.addr;
      uint64_t len = msg->read.len;
      volatile ubsim::MemM2H *resp = AllocM2H(memif, ts);
      resp->readcomp.req_id = msg->read.req_id;
      std::memcpy((void *)resp->readcomp.data, &(*memory)[addr], len);
      ubsim::M2HOutSend(memif, resp, ubsim::kMemMsgReadComp);
      break;
    }
    case ubsim::kMemMsgWrite: {
      uint64_t addr = msg->write.addr;
      uint64_t len = msg->write.len;
      std::memcpy(&(*memory)[addr], (const void *)msg->write.data, len);
      volatile ubsim::MemM2H *resp = AllocM2H(memif, ts);
      resp->writecomp.req_id = msg->write.req_id;
      ubsim::M2HOutSend(memif, resp, ubsim::kMemMsgWriteComp);
      break;
    }
    case ubsim::kMemMsgWritePosted: {
      uint64_t addr = msg->write.addr;
      uint64_t len = msg->write.len;
      std::memcpy(&(*memory)[addr], (const void *)msg->write.data, len);
      break;
    }
    case ubsim::kMsgSync:
      break;
    default:
      ubsim::LogWarn("basicmem",
                     "unsupported H2M message type: " +
                         std::to_string(type));
  }

  ubsim::H2MInDone(memif, msg);
}

}  // namespace

int main(int argc, char *argv[]) {
  std::signal(SIGINT, HandleSigint);
  std::signal(SIGUSR1, HandleSigusr1);
  ubsim::Logger::Instance().SetLevel(ubsim::LogLevel::kDebug);

  if (argc < 4 || argc > 8) {
    std::fprintf(stderr,
                 "Usage: basicmem [SIZE] [BASE-ADDR] [ASID] [SYNC-MODE] "
                 "[START-TICK] [SYNC-PERIOD] [MEM-LATENCY]\n");
    return EXIT_FAILURE;
  }

  auto size = static_cast<uint64_t>(std::strtoull(argv[1], nullptr, 0));
  uint64_t base_addr = std::strtoull(argv[2], nullptr, 0);
  ubsim::LogDebug("basicmem",
                  "starting with size=" + std::to_string(size) +
                      " base_addr=" + std::to_string(base_addr));

  SimbricksBaseIfParams params;
  ubsim::DefaultMemParams(&params);

  if (argc >= 5) {
    params.sync_mode =
        static_cast<SimbricksBaseIfSyncMode>(std::strtoul(argv[4], nullptr, 0));
  }
  if (argc >= 6) {
    g_current_ts = std::strtoull(argv[5], nullptr, 0);
  }
  if (argc >= 7) {
    params.sync_interval = std::strtoull(argv[6], nullptr, 0) * 1000ULL;
  }
  if (argc >= 8) {
    params.link_latency = std::strtoull(argv[7], nullptr, 0) * 1000ULL;
  }

  ubsim::ManagerPort port;
  ubsim::ManagerPortLoader loader;
  if (!loader.LoadFromEnv("mem", &port)) {
    ubsim::LogError("basicmem", "failed to load manager port");
    return EXIT_FAILURE;
  }
  ubsim::LogDebug("basicmem",
                  "port shm=" + port.shm_path +
                      " in_offset=" + std::to_string(port.in_offset) +
                      " out_offset=" + std::to_string(port.out_offset));

  params.blocking_conn = true;

  std::vector<uint8_t> memory(size, 0);
  ubsim::MemIf memif{};
  ubsim::ChannelAttachment attachment;
  if (!attachment.Attach(&memif.raw.base, &params, port)) {
    ubsim::LogError("basicmem", "failed to attach to manager channel");
    return EXIT_FAILURE;
  }

  ubsim::LogInfo("basicmem", "attached to manager channel");

  uint64_t next_ts = 0;
  while (!g_exiting) {
    while (ubsim::H2MOutSync(&memif, g_current_ts)) {
      ubsim::LogWarn("basicmem", "sync failed at timestamp " +
                                     std::to_string(g_current_ts));
    }

    do {
      PollH2M(&memif, g_current_ts, &memory);
      next_ts = ubsim::H2MInTimestamp(&memif);
    } while (!g_exiting && next_ts <= g_current_ts);

    g_current_ts = next_ts;
  }

  (void)base_addr;
  return 0;
}
