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

#ifndef UBSIM_BASE_IF_H_
#define UBSIM_BASE_IF_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <ubsim/base/proto.h>

namespace ubsim {

class ShmPool {
 public:
  ShmPool();

  int Create(const std::string &path, size_t pool_size);
  int MapFd(int fd);
  int Map(const std::string &path);
  int Unmap();
  int Unlink();

  const std::string &path() const { return path_; }
  int fd() const { return fd_; }
  void *base() const { return base_; }
  size_t size() const { return size_; }
  size_t pos() const { return pos_; }
  void set_pos(size_t pos) { pos_ = pos; }

 private:
  std::string path_;
  int fd_ = -1;
  void *base_ = nullptr;
  size_t size_ = 0;
  size_t pos_ = 0;
};

enum class SyncMode {
  kDisabled,
  kOptional,
  kRequired,
};

struct BaseIfParams {
  uint64_t link_latency = 0;
  uint64_t sync_interval = 0;
  std::string sock_path;
  SyncMode sync_mode = SyncMode::kOptional;
  bool blocking_conn = false;
  size_t in_num_entries = 0;
  size_t in_entries_size = 0;
  size_t out_num_entries = 0;
  size_t out_entries_size = 0;
  uint64_t upper_layer_proto = 0;
};

class BaseIf {
 public:
  BaseIf();

  int Init(const BaseIfParams &params);
  int ManagerSetup(ShmPool *pool, size_t in_offset, size_t out_offset,
                   size_t in_entries, size_t out_entries, size_t in_entry_size,
                   size_t out_entry_size);
  int Listen(ShmPool *pool);
  int Connect();
  int Connected();
  int ConnFd() const;

  int IntroSend(const void *payload, size_t payload_len);
  int IntroRecv(void *payload, size_t *payload_len);
  int IntroFd() const;

  void Close();
  void Unlink();

  uint8_t InType(volatile UbsimProtoBaseMsg *msg) const;
  volatile UbsimProtoBaseMsg *InPeek(uint64_t timestamp);
  volatile UbsimProtoBaseMsg *InPoll(uint64_t timestamp);
  void InDone(volatile UbsimProtoBaseMsg *msg);
  uint64_t InTimestamp() const;
  bool InTerminated() const;

  volatile UbsimProtoBaseMsg *OutAlloc(uint64_t timestamp);
  void OutSend(volatile UbsimProtoBaseMsg *msg, uint8_t msg_type);
  int OutSync(uint64_t timestamp);
  uint64_t OutNextSync() const;
  size_t OutMsgLen() const;
  bool SyncEnabled() const;

  BaseIfParams &params() { return params_; }
  const BaseIfParams &params() const { return params_; }

  static size_t SHMSize(const BaseIfParams &params);

 private:
  enum class ConnState {
    kClosed = 0,
    kListening,
    kConnecting,
    kAwaitHandshakeRxTx,
    kAwaitHandshakeRx,
    kAwaitHandshakeTx,
    kOpen,
  };

  int Accept();

  void *in_queue_ = nullptr;
  size_t in_pos_ = 0;
  size_t in_elen_ = 0;
  size_t in_enum_ = 0;
  uint64_t in_timestamp_ = 0;

  void *out_queue_ = nullptr;
  size_t out_pos_ = 0;
  size_t out_elen_ = 0;
  size_t out_enum_ = 0;
  uint64_t out_timestamp_ = 0;

  bool in_terminated_ = false;

  ConnState conn_state_ = ConnState::kClosed;
  bool sync_ = false;
  BaseIfParams params_{};
  ShmPool *shm_ = nullptr;
  int listen_fd_ = -1;
  int conn_fd_ = -1;
  bool listener_ = false;
};

struct EstablishData {
  BaseIf *base_if = nullptr;
  const void *tx_intro = nullptr;
  size_t tx_intro_len = 0;
  void *rx_intro = nullptr;
  size_t rx_intro_len = 0;
};

int ConnsWait(const std::vector<BaseIf *> &base_ifs);
int Establish(std::vector<EstablishData> *ifs);

}  // namespace ubsim

#endif  // UBSIM_BASE_IF_H_
