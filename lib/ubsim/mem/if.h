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

#include <stddef.h>
#include <stdint.h>

#include <ubsim/base/generic.h>
#include <ubsim/base/if.h>
#include <ubsim/mem/proto.h>

#ifdef __cplusplus
extern "C" {
#endif

void UbsimMemIfDefaultParams(struct UbsimBaseIfParams *params);

struct UbsimMemIf {
  struct UbsimBaseIf base;
};

/** Generate queue access functions for both directions */
UBSIM_BASEIF_GENERIC(UbsimMemIfH2M, UbsimProtoMemH2M,
                         UbsimMemIf);
UBSIM_BASEIF_GENERIC(UbsimMemIfM2H, UbsimProtoMemM2H,
                         UbsimMemIf);

#ifdef __cplusplus
}
#endif

#endif  // UBSIM_MEM_IF_H_
