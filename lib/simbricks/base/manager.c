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

#include "lib/simbricks/base/manager.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ParseChannelType(const char *token,
                            enum SimbricksManagerChannelType *out) {
  if (strcmp(token, "shm_ring") == 0) {
    *out = kSimbricksManagerChannelShmRing;
    return 0;
  }
  if (strcmp(token, "socket") == 0) {
    *out = kSimbricksManagerChannelSocket;
    return 0;
  }
  return -1;
}

static char *TrimLeading(char *line) {
  while (*line && isspace((unsigned char)*line)) {
    line++;
  }
  return line;
}

int SimbricksManagerGetPort(const char *path, const char *port_name,
                            struct SimbricksManagerPort *out) {
  FILE *handle = fopen(path, "r");
  if (handle == NULL) {
    perror("SimbricksManagerGetPort: fopen failed");
    return -1;
  }

  char line[1024];
  while (fgets(line, sizeof(line), handle) != NULL) {
    char *trimmed = TrimLeading(line);
    if (*trimmed == '\0' || *trimmed == '#') {
      continue;
    }

    char *token = strtok(trimmed, " \t\n");
    if (token == NULL) {
      continue;
    }
    if (strcmp(token, port_name) != 0) {
      continue;
    }

    char *type_tok = strtok(NULL, " \t\n");
    char *sock_tok = strtok(NULL, " \t\n");
    char *shm_tok = strtok(NULL, " \t\n");
    char *in_off_tok = strtok(NULL, " \t\n");
    char *in_entries_tok = strtok(NULL, " \t\n");
    char *in_esz_tok = strtok(NULL, " \t\n");
    char *out_off_tok = strtok(NULL, " \t\n");
    char *out_entries_tok = strtok(NULL, " \t\n");
    char *out_esz_tok = strtok(NULL, " \t\n");

    if (type_tok == NULL || sock_tok == NULL || shm_tok == NULL ||
        in_off_tok == NULL || in_entries_tok == NULL || in_esz_tok == NULL ||
        out_off_tok == NULL || out_entries_tok == NULL || out_esz_tok == NULL) {
      fprintf(stderr, "SimbricksManagerGetPort: malformed entry\n");
      fclose(handle);
      return -1;
    }

    if (ParseChannelType(type_tok, &out->channel_type) != 0) {
      fprintf(stderr, "SimbricksManagerGetPort: unknown channel type\n");
      fclose(handle);
      return -1;
    }

    snprintf(out->name, sizeof(out->name), "%s", port_name);
    snprintf(out->socket_path, sizeof(out->socket_path), "%s", sock_tok);
    snprintf(out->shm_path, sizeof(out->shm_path), "%s", shm_tok);
    out->in_offset = (size_t)strtoull(in_off_tok, NULL, 0);
    out->in_entries = (size_t)strtoull(in_entries_tok, NULL, 0);
    out->in_entry_size = (size_t)strtoull(in_esz_tok, NULL, 0);
    out->out_offset = (size_t)strtoull(out_off_tok, NULL, 0);
    out->out_entries = (size_t)strtoull(out_entries_tok, NULL, 0);
    out->out_entry_size = (size_t)strtoull(out_esz_tok, NULL, 0);

    fclose(handle);
    return 0;
  }

  fclose(handle);
  errno = ENOENT;
  return -1;
}

int SimbricksManagerGetPortFromEnv(const char *port_name,
                                   struct SimbricksManagerPort *out) {
  const char *path = getenv("SIMBRICKS_MANAGER_PORTS");
  if (path == NULL) {
    fprintf(stderr, "SIMBRICKS_MANAGER_PORTS not set\n");
    errno = ENOENT;
    return -1;
  }
  return SimbricksManagerGetPort(path, port_name, out);
}

int SimbricksManagerAttachBaseIf(struct SimbricksBaseIf *base_if,
                                 struct SimbricksBaseIfParams *params,
                                 struct SimbricksBaseIfSHMPool *pool,
                                 const struct SimbricksManagerPort *port) {
  if (SimbricksBaseIfInit(base_if, params)) {
    perror("SimbricksManagerAttachBaseIf: SimbricksBaseIfInit failed");
    return -1;
  }

  if (port->channel_type != kSimbricksManagerChannelShmRing) {
    fprintf(stderr,
            "SimbricksManagerAttachBaseIf: unsupported channel type\n");
    return -1;
  }

  if (SimbricksBaseIfSHMPoolMap(pool, port->shm_path) != 0) {
    perror("SimbricksManagerAttachBaseIf: shm map failed");
    return -1;
  }

  params->in_num_entries = port->in_entries;
  params->out_num_entries = port->out_entries;
  params->in_entries_size = port->in_entry_size;
  params->out_entries_size = port->out_entry_size;
  base_if->params = *params;

  return SimbricksBaseIfManagerSetup(base_if, pool, port->in_offset,
                                     port->out_offset, port->in_entries,
                                     port->out_entries, port->in_entry_size,
                                     port->out_entry_size);
}
