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

#include "lib/ubsim/mem/zmq.h"

#include <cstdio>
#include <cstring>

namespace ubsim {
namespace {

bool IsSyncEnabled(const BaseIfParams &params) {
  return params.sync_mode != SyncMode::kDisabled;
}

}  // namespace

ZmqMemIf::ZmqMemIf() = default;

ZmqMemIf::~ZmqMemIf() {
  Close();
}

bool ZmqMemIf::Open(const std::string &in_endpoint,
                    const std::string &out_endpoint, bool in_bind,
                    bool out_bind, size_t entry_size, size_t depth,
                    const BaseIfParams &params) {
  Close();

  entry_size_ = entry_size;
  depth_ = depth;
  params_ = params;
  in_buffer_.assign(entry_size_, 0);
  out_buffer_.assign(entry_size_, 0);

  context_ = zmq_ctx_new();
  if (context_ == nullptr) {
    std::fprintf(stderr, "ZmqMemIf::Open: zmq_ctx_new failed\n");
    return false;
  }

  if (!InitSocket(&in_socket_, in_endpoint, in_bind)) {
    Close();
    return false;
  }
  if (!InitSocket(&out_socket_, out_endpoint, out_bind)) {
    Close();
    return false;
  }

  int linger = 0;
  zmq_setsockopt(in_socket_, ZMQ_LINGER, &linger, sizeof(linger));
  zmq_setsockopt(out_socket_, ZMQ_LINGER, &linger, sizeof(linger));

  return true;
}

void ZmqMemIf::Close() {
  if (in_socket_ != nullptr) {
    zmq_close(in_socket_);
    in_socket_ = nullptr;
  }
  if (out_socket_ != nullptr) {
    zmq_close(out_socket_);
    out_socket_ = nullptr;
  }
  if (context_ != nullptr) {
    zmq_ctx_term(context_);
    context_ = nullptr;
  }
}

bool ZmqMemIf::InitSocket(void **socket, const std::string &endpoint,
                          bool bind) {
  *socket = zmq_socket(context_, ZMQ_PAIR);
  if (*socket == nullptr) {
    std::fprintf(stderr, "ZmqMemIf::Open: zmq_socket failed\n");
    return false;
  }

  int rc = bind ? zmq_bind(*socket, endpoint.c_str())
                : zmq_connect(*socket, endpoint.c_str());
  if (rc != 0) {
    std::fprintf(stderr, "ZmqMemIf::Open: zmq_%s failed for %s\n",
                 bind ? "bind" : "connect", endpoint.c_str());
    return false;
  }

  int snd_hwm = static_cast<int>(depth_);
  int rcv_hwm = static_cast<int>(depth_);
  zmq_setsockopt(*socket, ZMQ_SNDHWM, &snd_hwm, sizeof(snd_hwm));
  zmq_setsockopt(*socket, ZMQ_RCVHWM, &rcv_hwm, sizeof(rcv_hwm));
  return true;
}

volatile UbsimProtoMemH2M *ZmqMemIf::H2MOutAlloc(uint64_t ts) {
  auto *msg = reinterpret_cast<volatile UbsimProtoMemH2M *>(out_buffer_.data());
  msg->base.header.timestamp = ts + params_.link_latency;
  return msg;
}

void ZmqMemIf::H2MOutSend(volatile UbsimProtoMemH2M *msg, uint8_t type) {
  Send(&msg->base, type);
}

int ZmqMemIf::H2MOutSync(uint64_t ts) {
  if (!IsSyncEnabled(params_)) {
    return 0;
  }
  auto *msg = H2MOutAlloc(ts);
  H2MOutSend(msg, UBSIM_PROTO_MSG_TYPE_SYNC);
  return 0;
}

size_t ZmqMemIf::H2MOutMsgLen() const {
  return entry_size_;
}

volatile UbsimProtoMemH2M *ZmqMemIf::H2MInPoll(uint64_t) {
  return reinterpret_cast<volatile UbsimProtoMemH2M *>(Receive());
}

uint8_t ZmqMemIf::H2MInType(volatile UbsimProtoMemH2M *msg) const {
  return msg->base.header.own_type & ~UBSIM_PROTO_MSG_OWN_MASK;
}

void ZmqMemIf::H2MInDone(volatile UbsimProtoMemH2M *) {}

uint64_t ZmqMemIf::H2MInTimestamp() const {
  return last_in_timestamp_;
}

volatile UbsimProtoMemM2H *ZmqMemIf::M2HOutAlloc(uint64_t ts) {
  auto *msg = reinterpret_cast<volatile UbsimProtoMemM2H *>(out_buffer_.data());
  msg->base.header.timestamp = ts + params_.link_latency;
  return msg;
}

void ZmqMemIf::M2HOutSend(volatile UbsimProtoMemM2H *msg, uint8_t type) {
  Send(&msg->base, type);
}

volatile UbsimProtoMemM2H *ZmqMemIf::M2HInPoll(uint64_t) {
  return reinterpret_cast<volatile UbsimProtoMemM2H *>(Receive());
}

uint8_t ZmqMemIf::M2HInType(volatile UbsimProtoMemM2H *msg) const {
  return msg->base.header.own_type & ~UBSIM_PROTO_MSG_OWN_MASK;
}

void ZmqMemIf::M2HInDone(volatile UbsimProtoMemM2H *) {}

uint64_t ZmqMemIf::M2HInTimestamp() const {
  return last_in_timestamp_;
}

bool ZmqMemIf::Send(volatile UbsimProtoBaseMsg *msg, uint8_t type) {
  if (out_socket_ == nullptr) {
    std::fprintf(stderr, "ZmqMemIf::Send: output socket not open\n");
    return false;
  }

  msg->header.own_type =
      UBSIM_PROTO_MSG_OWN_CON | (type & UBSIM_PROTO_MSG_TYPE_MASK);

  int rc = zmq_send(out_socket_, out_buffer_.data(), entry_size_, ZMQ_DONTWAIT);
  if (rc < 0) {
    std::fprintf(stderr, "ZmqMemIf::Send: zmq_send failed\n");
    return false;
  }
  return true;
}

volatile UbsimProtoBaseMsg *ZmqMemIf::Receive() {
  if (in_socket_ == nullptr) {
    std::fprintf(stderr, "ZmqMemIf::Receive: input socket not open\n");
    return nullptr;
  }

  int rc = zmq_recv(in_socket_, in_buffer_.data(), entry_size_, ZMQ_DONTWAIT);
  if (rc < 0) {
    if (zmq_errno() == EAGAIN) {
      return nullptr;
    }
    std::fprintf(stderr, "ZmqMemIf::Receive: zmq_recv failed\n");
    return nullptr;
  }

  auto *msg = reinterpret_cast<volatile UbsimProtoBaseMsg *>(in_buffer_.data());
  last_in_timestamp_ = msg->header.timestamp;
  return msg;
}

}  // namespace ubsim
