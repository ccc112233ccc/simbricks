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

#ifndef UBSIM_MEM_PROTO_H_
#define UBSIM_MEM_PROTO_H_

#include <assert.h>
#include <stdint.h>

#include <ubsim/base/proto.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/* Initialization messages on Unix socket */

/** welcome message sent by memory to host. */
struct UbsimProtoMemMemIntro {
  uint32_t dummy; /* not used, but need to avoid empty struct for standard C */
} __attribute__((packed));

/** welcome message sent by host to memory */
struct UbsimProtoMemHostIntro {
  uint32_t dummy; /* not used, but need to avoid empty struct for standard C */
} __attribute__((packed));

/******************************************************************************/
/* Messages on memory to host channel */

/** Mask for type value in own_type field */
#define UBSIM_PROTO_MEM_M2H_MSG_READCOMP 0x40
#define UBSIM_PROTO_MEM_M2H_MSG_WRITECOMP 0x41

struct UbsimProtoMemM2HReadcomp {
  uint64_t req_id;
  uint8_t pad[40];
  uint64_t timestamp;
  uint8_t pad_[7];
  uint8_t own_type;
  uint8_t data[];
} __attribute__((packed));
UBSIM_PROTO_MSG_SZCHECK(struct UbsimProtoMemM2HReadcomp);

struct UbsimProtoMemM2HWritecomp {
  uint64_t req_id;
  uint8_t pad[40];
  uint64_t timestamp;
  uint8_t pad_[7];
  uint8_t own_type;
} __attribute__((packed));
UBSIM_PROTO_MSG_SZCHECK(struct UbsimProtoMemM2HWritecomp);

union UbsimProtoMemM2H {
  union UbsimProtoBaseMsg base;
  struct UbsimProtoMemM2HReadcomp readcomp;
  struct UbsimProtoMemM2HWritecomp writecomp;
} __attribute__((packed));
UBSIM_PROTO_MSG_SZCHECK(union UbsimProtoMemM2H);

/******************************************************************************/
/* Messages on host to memory channel */

#define UBSIM_PROTO_MEM_H2M_MSG_READ 0x60
#define UBSIM_PROTO_MEM_H2M_MSG_WRITE 0x61
// treat write as posted so don't send a completion message
#define UBSIM_PROTO_MEM_H2M_MSG_WRITE_POSTED 0x62

struct UbsimProtoMemH2MRead {
  uint64_t req_id;
  uint64_t as_id;
  uint64_t addr;
  uint16_t len;
  uint8_t pad[22];
  uint64_t timestamp;
  uint8_t pad_[7];
  uint8_t own_type;
} __attribute__((packed));
UBSIM_PROTO_MSG_SZCHECK(struct UbsimProtoMemH2MRead);

struct UbsimProtoMemH2MWrite {
  uint64_t req_id;
  uint64_t as_id;
  uint64_t addr;
  uint16_t len;
  uint8_t pad[22];
  uint64_t timestamp;
  uint8_t pad_[7];
  uint8_t own_type;
  uint8_t data[];
} __attribute__((packed));
UBSIM_PROTO_MSG_SZCHECK(struct UbsimProtoMemH2MWrite);

union UbsimProtoMemH2M {
  union UbsimProtoBaseMsg base;
  struct UbsimProtoMemH2MRead read;
  struct UbsimProtoMemH2MWrite write;
} __attribute__((packed));
UBSIM_PROTO_MSG_SZCHECK(union UbsimProtoMemH2M);

#ifdef __cplusplus
}
#endif

#endif  // UBSIM_MEM_PROTO_H_
