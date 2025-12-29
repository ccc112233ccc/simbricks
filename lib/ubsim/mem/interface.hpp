#ifndef UBSIM_MEM_INTERFACE_HPP_
#define UBSIM_MEM_INTERFACE_HPP_

#include <cstdint>

#include <ubsim/mem/if.h>

namespace ubsim {

using MemIf = ubsim::MemIf;

inline BaseIfParams DefaultMemParams() { return MemIf::DefaultParams(); }

inline volatile UbsimProtoMemH2M *H2MOutAlloc(MemIf *mem, uint64_t ts) {
  return mem->H2MOutAlloc(ts);
}

inline void H2MOutSend(MemIf *mem, volatile UbsimProtoMemH2M *msg,
                       uint8_t type) {
  mem->H2MOutSend(msg, type);
}

inline int H2MOutSync(MemIf *mem, uint64_t ts) {
  return mem->H2MOutSync(ts);
}

inline size_t H2MOutMsgLen(MemIf *mem) {
  return mem->H2MOutMsgLen();
}

inline volatile UbsimProtoMemH2M *H2MInPoll(MemIf *mem, uint64_t ts) {
  return mem->H2MInPoll(ts);
}

inline uint8_t H2MInType(MemIf *mem, volatile UbsimProtoMemH2M *msg) {
  return mem->H2MInType(msg);
}

inline void H2MInDone(MemIf *mem, volatile UbsimProtoMemH2M *msg) {
  mem->H2MInDone(msg);
}

inline uint64_t H2MInTimestamp(MemIf *mem) {
  return mem->H2MInTimestamp();
}

inline volatile UbsimProtoMemM2H *M2HOutAlloc(MemIf *mem, uint64_t ts) {
  return mem->M2HOutAlloc(ts);
}

inline void M2HOutSend(MemIf *mem, volatile UbsimProtoMemM2H *msg,
                       uint8_t type) {
  mem->M2HOutSend(msg, type);
}

inline volatile UbsimProtoMemM2H *M2HInPoll(MemIf *mem, uint64_t ts) {
  return mem->M2HInPoll(ts);
}

inline uint8_t M2HInType(MemIf *mem, volatile UbsimProtoMemM2H *msg) {
  return mem->M2HInType(msg);
}

inline void M2HInDone(MemIf *mem, volatile UbsimProtoMemM2H *msg) {
  mem->M2HInDone(msg);
}

inline uint64_t M2HInTimestamp(MemIf *mem) {
  return mem->M2HInTimestamp();
}

}  // namespace ubsim

#endif  // UBSIM_MEM_INTERFACE_HPP_
