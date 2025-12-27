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

#ifndef UBSIM_MEM_ZMQ_H_
#define UBSIM_MEM_ZMQ_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <zmq.h>

#include <ubsim/base/if.h>
#include <ubsim/mem/proto.h>

namespace ubsim {

class ZmqMemIf {
 public:
  ZmqMemIf();
  ~ZmqMemIf();

  bool Open(const std::string &in_endpoint, const std::string &out_endpoint,
            bool in_bind, bool out_bind, size_t entry_size, size_t depth,
            const BaseIfParams &params);
  void Close();

  volatile UbsimProtoMemH2M *H2MOutAlloc(uint64_t ts);
  void H2MOutSend(volatile UbsimProtoMemH2M *msg, uint8_t type);
  int H2MOutSync(uint64_t ts);
  size_t H2MOutMsgLen() const;

  volatile UbsimProtoMemH2M *H2MInPoll(uint64_t ts);
  uint8_t H2MInType(volatile UbsimProtoMemH2M *msg) const;
  void H2MInDone(volatile UbsimProtoMemH2M *msg);
  uint64_t H2MInTimestamp() const;

  volatile UbsimProtoMemM2H *M2HOutAlloc(uint64_t ts);
  void M2HOutSend(volatile UbsimProtoMemM2H *msg, uint8_t type);

  volatile UbsimProtoMemM2H *M2HInPoll(uint64_t ts);
  uint8_t M2HInType(volatile UbsimProtoMemM2H *msg) const;
  void M2HInDone(volatile UbsimProtoMemM2H *msg);
  uint64_t M2HInTimestamp() const;

 private:
  bool Send(volatile UbsimProtoBaseMsg *msg, uint8_t type);
  volatile UbsimProtoBaseMsg *Receive();
  bool InitSocket(void **socket, const std::string &endpoint, bool bind);

  void *context_ = nullptr;
  void *in_socket_ = nullptr;
  void *out_socket_ = nullptr;
  size_t entry_size_ = 0;
  size_t depth_ = 0;
  BaseIfParams params_{};
  uint64_t last_in_timestamp_ = 0;
  std::vector<uint8_t> in_buffer_;
  std::vector<uint8_t> out_buffer_;
};

}  // namespace ubsim

#endif  // UBSIM_MEM_ZMQ_H_
