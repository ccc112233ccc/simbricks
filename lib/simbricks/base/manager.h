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

#ifndef SIMBRICKS_BASE_MANAGER_H_
#define SIMBRICKS_BASE_MANAGER_H_

#include <limits.h>
#include <stddef.h>

#include <simbricks/base/if.h>

#ifdef __cplusplus
extern "C" {
#endif

enum SimbricksManagerChannelType {
  kSimbricksManagerChannelShmRing,
  kSimbricksManagerChannelSocket,
};

struct SimbricksManagerPort {
  char name[64];
  enum SimbricksManagerChannelType channel_type;
  char socket_path[108];
  char shm_path[PATH_MAX];
  size_t in_offset;
  size_t out_offset;
  size_t in_entries;
  size_t out_entries;
  size_t in_entry_size;
  size_t out_entry_size;
};

int SimbricksManagerGetPort(const char *path, const char *port_name,
                            struct SimbricksManagerPort *out);
int SimbricksManagerGetPortFromEnv(const char *port_name,
                                   struct SimbricksManagerPort *out);
int SimbricksManagerAttachBaseIf(struct SimbricksBaseIf *base_if,
                                 struct SimbricksBaseIfParams *params,
                                 struct SimbricksBaseIfSHMPool *pool,
                                 const struct SimbricksManagerPort *port);

#ifdef __cplusplus
}
#endif

#endif  // SIMBRICKS_BASE_MANAGER_H_
