#ifndef UBSIM_MEM_PROTO_HPP_
#define UBSIM_MEM_PROTO_HPP_

#include <cstdint>

#include <ubsim/mem/proto.h>

namespace ubsim {

using MemH2M = UbsimProtoMemH2M;
using MemM2H = UbsimProtoMemM2H;
using MemH2MRead = UbsimProtoMemH2MRead;
using MemH2MWrite = UbsimProtoMemH2MWrite;
using MemM2HReadComp = UbsimProtoMemM2HReadcomp;
using MemM2HWriteComp = UbsimProtoMemM2HWritecomp;

constexpr uint8_t kMemMsgRead = UBSIM_PROTO_MEM_H2M_MSG_READ;
constexpr uint8_t kMemMsgWrite = UBSIM_PROTO_MEM_H2M_MSG_WRITE;
constexpr uint8_t kMemMsgWritePosted = UBSIM_PROTO_MEM_H2M_MSG_WRITE_POSTED;
constexpr uint8_t kMemMsgReadComp = UBSIM_PROTO_MEM_M2H_MSG_READCOMP;
constexpr uint8_t kMemMsgWriteComp = UBSIM_PROTO_MEM_M2H_MSG_WRITECOMP;
constexpr uint8_t kMsgSync = UBSIM_PROTO_MSG_TYPE_SYNC;

}  // namespace ubsim

#endif  // UBSIM_MEM_PROTO_HPP_
