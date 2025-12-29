/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
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

#include "lib/ubsim/mem/if.h"

namespace ubsim {

BaseIfParams MemIf::DefaultParams() {
  BaseIfParams params;
  params.link_latency = 500 * 1000;
  params.sync_interval = params.link_latency;
  params.sync_mode = SyncMode::kOptional;
  params.in_num_entries = 8192;
  params.out_num_entries = 8192;
  params.in_entries_size = 8192 + sizeof(UbsimProtoMemH2M);
  params.out_entries_size = params.in_entries_size;
  params.blocking_conn = false;
  params.upper_layer_proto = UBSIM_PROTO_ID_MEM;
  return params;
}

volatile UbsimProtoMemH2M *MemIf::H2MOutAlloc(uint64_t ts) {
  return reinterpret_cast<volatile UbsimProtoMemH2M *>(base_.OutAlloc(ts));
}

void MemIf::H2MOutSend(volatile UbsimProtoMemH2M *msg, uint8_t type) {
  base_.OutSend(&msg->base, type);
}

int MemIf::H2MOutSync(uint64_t ts) {
  return base_.OutSync(ts);
}

size_t MemIf::H2MOutMsgLen() const {
  return base_.OutMsgLen();
}

volatile UbsimProtoMemH2M *MemIf::H2MInPoll(uint64_t ts) {
  return reinterpret_cast<volatile UbsimProtoMemH2M *>(base_.InPoll(ts));
}

uint8_t MemIf::H2MInType(volatile UbsimProtoMemH2M *msg) const {
  return base_.InType(&msg->base);
}

void MemIf::H2MInDone(volatile UbsimProtoMemH2M *msg) {
  base_.InDone(&msg->base);
}

uint64_t MemIf::H2MInTimestamp() const {
  return base_.InTimestamp();
}

volatile UbsimProtoMemM2H *MemIf::M2HOutAlloc(uint64_t ts) {
  return reinterpret_cast<volatile UbsimProtoMemM2H *>(base_.OutAlloc(ts));
}

void MemIf::M2HOutSend(volatile UbsimProtoMemM2H *msg, uint8_t type) {
  base_.OutSend(&msg->base, type);
}

volatile UbsimProtoMemM2H *MemIf::M2HInPoll(uint64_t ts) {
  return reinterpret_cast<volatile UbsimProtoMemM2H *>(base_.InPoll(ts));
}

uint8_t MemIf::M2HInType(volatile UbsimProtoMemM2H *msg) const {
  return base_.InType(&msg->base);
}

void MemIf::M2HInDone(volatile UbsimProtoMemM2H *msg) {
  base_.InDone(&msg->base);
}

uint64_t MemIf::M2HInTimestamp() const {
  return base_.InTimestamp();
}

}  // namespace ubsim
