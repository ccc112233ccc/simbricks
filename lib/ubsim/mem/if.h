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

#ifndef UBSIM_MEM_IF_H_
#define UBSIM_MEM_IF_H_

#include <cstddef>
#include <cstdint>

#include <ubsim/base/if.h>
#include <ubsim/mem/proto.h>

namespace ubsim {

class MemIf {
 public:
  BaseIf &base() { return base_; }
  const BaseIf &base() const { return base_; }

  static BaseIfParams DefaultParams();

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
  BaseIf base_;
};

}  // namespace ubsim

#endif  // UBSIM_MEM_IF_H_
