#ifndef UBSIM_MEM_INTERFACE_HPP_
#define UBSIM_MEM_INTERFACE_HPP_

#include <cstdint>

#include <simbricks/mem/if.h>

namespace ubsim {

struct MemIf {
  SimbricksMemIf raw;
};

inline void DefaultMemParams(SimbricksBaseIfParams *params) {
  SimbricksMemIfDefaultParams(params);
}

inline volatile union SimbricksProtoMemH2M *H2MOutAlloc(MemIf *mem,
                                                        uint64_t ts) {
  return SimbricksMemIfH2MOutAlloc(&mem->raw, ts);
}

inline void H2MOutSend(MemIf *mem, volatile union SimbricksProtoMemH2M *msg,
                       uint8_t type) {
  SimbricksMemIfH2MOutSend(&mem->raw, msg, type);
}

inline int H2MOutSync(MemIf *mem, uint64_t ts) {
  return SimbricksMemIfH2MOutSync(&mem->raw, ts);
}

inline size_t H2MOutMsgLen(MemIf *mem) {
  return SimbricksMemIfH2MOutMsgLen(&mem->raw);
}

inline volatile union SimbricksProtoMemH2M *H2MInPoll(MemIf *mem,
                                                      uint64_t ts) {
  return SimbricksMemIfH2MInPoll(&mem->raw, ts);
}

inline uint8_t H2MInType(MemIf *mem, volatile union SimbricksProtoMemH2M *msg) {
  return SimbricksMemIfH2MInType(&mem->raw, msg);
}

inline void H2MInDone(MemIf *mem, volatile union SimbricksProtoMemH2M *msg) {
  SimbricksMemIfH2MInDone(&mem->raw, msg);
}

inline uint64_t H2MInTimestamp(MemIf *mem) {
  return SimbricksMemIfH2MInTimestamp(&mem->raw);
}

inline volatile union SimbricksProtoMemM2H *M2HOutAlloc(MemIf *mem,
                                                        uint64_t ts) {
  return SimbricksMemIfM2HOutAlloc(&mem->raw, ts);
}

inline void M2HOutSend(MemIf *mem, volatile union SimbricksProtoMemM2H *msg,
                       uint8_t type) {
  SimbricksMemIfM2HOutSend(&mem->raw, msg, type);
}

inline volatile union SimbricksProtoMemM2H *M2HInPoll(MemIf *mem,
                                                      uint64_t ts) {
  return SimbricksMemIfM2HInPoll(&mem->raw, ts);
}

inline uint8_t M2HInType(MemIf *mem, volatile union SimbricksProtoMemM2H *msg) {
  return SimbricksMemIfM2HInType(&mem->raw, msg);
}

inline void M2HInDone(MemIf *mem, volatile union SimbricksProtoMemM2H *msg) {
  SimbricksMemIfM2HInDone(&mem->raw, msg);
}

inline uint64_t M2HInTimestamp(MemIf *mem) {
  return SimbricksMemIfM2HInTimestamp(&mem->raw);
}

}  // namespace ubsim

#endif  // UBSIM_MEM_INTERFACE_HPP_
