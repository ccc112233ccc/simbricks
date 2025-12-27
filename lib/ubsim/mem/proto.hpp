#ifndef UBSIM_MEM_PROTO_HPP_
#define UBSIM_MEM_PROTO_HPP_

#include <cstdint>

#include <simbricks/mem/proto.h>

namespace ubsim {

using MemH2M = SimbricksProtoMemH2M;
using MemM2H = SimbricksProtoMemM2H;
using MemH2MRead = SimbricksProtoMemH2MRead;
using MemH2MWrite = SimbricksProtoMemH2MWrite;
using MemM2HReadComp = SimbricksProtoMemM2HReadcomp;
using MemM2HWriteComp = SimbricksProtoMemM2HWritecomp;

constexpr uint8_t kMemMsgRead = SIMBRICKS_PROTO_MEM_H2M_MSG_READ;
constexpr uint8_t kMemMsgWrite = SIMBRICKS_PROTO_MEM_H2M_MSG_WRITE;
constexpr uint8_t kMemMsgWritePosted = SIMBRICKS_PROTO_MEM_H2M_MSG_WRITE_POSTED;
constexpr uint8_t kMemMsgReadComp = SIMBRICKS_PROTO_MEM_M2H_MSG_READCOMP;
constexpr uint8_t kMemMsgWriteComp = SIMBRICKS_PROTO_MEM_M2H_MSG_WRITECOMP;
constexpr uint8_t kMsgSync = SIMBRICKS_PROTO_MSG_TYPE_SYNC;

}  // namespace ubsim

#endif  // UBSIM_MEM_PROTO_HPP_
