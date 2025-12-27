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

#include "lib/ubsim/mem/mq.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>

namespace ubsim {
namespace {

bool IsSyncEnabled(const BaseIfParams &params) {
  return params.sync_mode != SyncMode::kDisabled;
}

}  // namespace

MqMemIf::MqMemIf() = default;

MqMemIf::~MqMemIf() {
  Close();
}

bool MqMemIf::Open(const std::string &in_name, const std::string &out_name,
                   size_t entry_size, size_t depth,
                   const BaseIfParams &params) {
  Close();

  entry_size_ = entry_size;
  depth_ = depth;
  params_ = params;
  in_buffer_.assign(entry_size_, 0);
  out_buffer_.assign(entry_size_, 0);

  struct mq_attr attr {};
  attr.mq_flags = 0;
  attr.mq_maxmsg = static_cast<long>(depth_);
  attr.mq_msgsize = static_cast<long>(entry_size_);
  attr.mq_curmsgs = 0;

  in_queue_ = mq_open(in_name.c_str(), O_CREAT | O_RDWR | O_NONBLOCK, 0660,
                      &attr);
  if (in_queue_ == static_cast<mqd_t>(-1)) {
    perror("MqMemIf::Open: mq_open in failed");
    return false;
  }

  out_queue_ = mq_open(out_name.c_str(), O_CREAT | O_RDWR | O_NONBLOCK, 0660,
                       &attr);
  if (out_queue_ == static_cast<mqd_t>(-1)) {
    perror("MqMemIf::Open: mq_open out failed");
    mq_close(in_queue_);
    in_queue_ = static_cast<mqd_t>(-1);
    return false;
  }

  return true;
}

void MqMemIf::Close() {
  if (in_queue_ != static_cast<mqd_t>(-1)) {
    mq_close(in_queue_);
    in_queue_ = static_cast<mqd_t>(-1);
  }
  if (out_queue_ != static_cast<mqd_t>(-1)) {
    mq_close(out_queue_);
    out_queue_ = static_cast<mqd_t>(-1);
  }
}

volatile UbsimProtoMemH2M *MqMemIf::H2MOutAlloc(uint64_t ts) {
  auto *msg = reinterpret_cast<volatile UbsimProtoMemH2M *>(out_buffer_.data());
  msg->base.header.timestamp = ts + params_.link_latency;
  return msg;
}

void MqMemIf::H2MOutSend(volatile UbsimProtoMemH2M *msg, uint8_t type) {
  Send(&msg->base, type);
}

int MqMemIf::H2MOutSync(uint64_t ts) {
  if (!IsSyncEnabled(params_)) {
    return 0;
  }
  auto *msg = H2MOutAlloc(ts);
  H2MOutSend(msg, UBSIM_PROTO_MSG_TYPE_SYNC);
  return 0;
}

size_t MqMemIf::H2MOutMsgLen() const {
  return entry_size_;
}

volatile UbsimProtoMemH2M *MqMemIf::H2MInPoll(uint64_t) {
  return reinterpret_cast<volatile UbsimProtoMemH2M *>(Receive());
}

uint8_t MqMemIf::H2MInType(volatile UbsimProtoMemH2M *msg) const {
  return msg->base.header.own_type & ~UBSIM_PROTO_MSG_OWN_MASK;
}

void MqMemIf::H2MInDone(volatile UbsimProtoMemH2M *) {}

uint64_t MqMemIf::H2MInTimestamp() const {
  return last_in_timestamp_;
}

volatile UbsimProtoMemM2H *MqMemIf::M2HOutAlloc(uint64_t ts) {
  auto *msg = reinterpret_cast<volatile UbsimProtoMemM2H *>(out_buffer_.data());
  msg->base.header.timestamp = ts + params_.link_latency;
  return msg;
}

void MqMemIf::M2HOutSend(volatile UbsimProtoMemM2H *msg, uint8_t type) {
  Send(&msg->base, type);
}

volatile UbsimProtoMemM2H *MqMemIf::M2HInPoll(uint64_t) {
  return reinterpret_cast<volatile UbsimProtoMemM2H *>(Receive());
}

uint8_t MqMemIf::M2HInType(volatile UbsimProtoMemM2H *msg) const {
  return msg->base.header.own_type & ~UBSIM_PROTO_MSG_OWN_MASK;
}

void MqMemIf::M2HInDone(volatile UbsimProtoMemM2H *) {}

uint64_t MqMemIf::M2HInTimestamp() const {
  return last_in_timestamp_;
}

bool MqMemIf::Send(volatile UbsimProtoBaseMsg *msg, uint8_t type) {
  msg->header.own_type =
      UBSIM_PROTO_MSG_OWN_CON | (type & UBSIM_PROTO_MSG_TYPE_MASK);

  if (out_queue_ == static_cast<mqd_t>(-1)) {
    std::fprintf(stderr, "MqMemIf::Send: output queue not open\n");
    return false;
  }

  if (mq_send(out_queue_, reinterpret_cast<const char *>(out_buffer_.data()),
              entry_size_, 0) != 0) {
    perror("MqMemIf::Send: mq_send failed");
    return false;
  }
  return true;
}

volatile UbsimProtoBaseMsg *MqMemIf::Receive() {
  if (in_queue_ == static_cast<mqd_t>(-1)) {
    std::fprintf(stderr, "MqMemIf::Receive: input queue not open\n");
    return nullptr;
  }

  ssize_t received = mq_receive(in_queue_,
                                reinterpret_cast<char *>(in_buffer_.data()),
                                entry_size_, nullptr);
  if (received < 0) {
    if (errno == EAGAIN) {
      return nullptr;
    }
    perror("MqMemIf::Receive: mq_receive failed");
    return nullptr;
  }

  auto *msg = reinterpret_cast<volatile UbsimProtoBaseMsg *>(in_buffer_.data());
  last_in_timestamp_ = msg->header.timestamp;
  return msg;
}

}  // namespace ubsim
