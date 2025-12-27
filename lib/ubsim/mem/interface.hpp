#ifndef UBSIM_MEM_INTERFACE_HPP_
#define UBSIM_MEM_INTERFACE_HPP_

#include <cstdint>

#include <ubsim/mem/if.h>

namespace ubsim {

struct MemIf {
  UbsimMemIf raw;
};

inline void DefaultMemParams(UbsimBaseIfParams *params) {
  UbsimMemIfDefaultParams(params);
}

inline volatile union UbsimProtoMemH2M *H2MOutAlloc(MemIf *mem,
                                                        uint64_t ts) {
  return UbsimMemIfH2MOutAlloc(&mem->raw, ts);
}

inline void H2MOutSend(MemIf *mem, volatile union UbsimProtoMemH2M *msg,
                       uint8_t type) {
  UbsimMemIfH2MOutSend(&mem->raw, msg, type);
}

inline int H2MOutSync(MemIf *mem, uint64_t ts) {
  return UbsimMemIfH2MOutSync(&mem->raw, ts);
}

inline size_t H2MOutMsgLen(MemIf *mem) {
  return UbsimMemIfH2MOutMsgLen(&mem->raw);
}

inline volatile union UbsimProtoMemH2M *H2MInPoll(MemIf *mem,
                                                      uint64_t ts) {
  return UbsimMemIfH2MInPoll(&mem->raw, ts);
}

inline uint8_t H2MInType(MemIf *mem, volatile union UbsimProtoMemH2M *msg) {
  return UbsimMemIfH2MInType(&mem->raw, msg);
}

inline void H2MInDone(MemIf *mem, volatile union UbsimProtoMemH2M *msg) {
  UbsimMemIfH2MInDone(&mem->raw, msg);
}

inline uint64_t H2MInTimestamp(MemIf *mem) {
  return UbsimMemIfH2MInTimestamp(&mem->raw);
}

inline volatile union UbsimProtoMemM2H *M2HOutAlloc(MemIf *mem,
                                                        uint64_t ts) {
  return UbsimMemIfM2HOutAlloc(&mem->raw, ts);
}

inline void M2HOutSend(MemIf *mem, volatile union UbsimProtoMemM2H *msg,
                       uint8_t type) {
  UbsimMemIfM2HOutSend(&mem->raw, msg, type);
}

inline volatile union UbsimProtoMemM2H *M2HInPoll(MemIf *mem,
                                                      uint64_t ts) {
  return UbsimMemIfM2HInPoll(&mem->raw, ts);
}

inline uint8_t M2HInType(MemIf *mem, volatile union UbsimProtoMemM2H *msg) {
  return UbsimMemIfM2HInType(&mem->raw, msg);
}

inline void M2HInDone(MemIf *mem, volatile union UbsimProtoMemM2H *msg) {
  UbsimMemIfM2HInDone(&mem->raw, msg);
}

inline uint64_t M2HInTimestamp(MemIf *mem) {
  return UbsimMemIfM2HInTimestamp(&mem->raw);
}

}  // namespace ubsim

#endif  // UBSIM_MEM_INTERFACE_HPP_
