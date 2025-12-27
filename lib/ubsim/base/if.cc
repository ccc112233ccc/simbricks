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

#define _GNU_SOURCE

#include "lib/ubsim/base/if.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace ubsim {
namespace {

#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

int Accept4Compat(int sockfd, struct sockaddr *addr, socklen_t *addrlen,
                  int flags) {
#ifdef __linux__
  return accept4(sockfd, addr, addrlen, flags);
#else
  int fd = accept(sockfd, addr, addrlen);
  if (fd == -1) {
    return -1;
  }
  if (flags & SOCK_NONBLOCK) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl == -1) {
      close(fd);
      return -1;
    }
    if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) == -1) {
      close(fd);
      return -1;
    }
  }
  return fd;
#endif
}

bool MustCheckSync(SyncMode mode) {
  return mode == SyncMode::kOptional || mode == SyncMode::kRequired;
}

}  // namespace

ShmPool::ShmPool() = default;

int ShmPool::Create(const std::string &path, size_t pool_size) {
  path_ = path;
  size_ = pool_size;
  pos_ = 0;

  fd_ = open(path.c_str(), O_CREAT | O_RDWR, 0666);
  if (fd_ == -1) {
    perror("ShmPool::Create: open failed");
    return -1;
  }

  if (ftruncate(fd_, static_cast<off_t>(pool_size)) != 0) {
    perror("ShmPool::Create: ftruncate failed");
    close(fd_);
    return -1;
  }

  base_ = mmap(nullptr, pool_size, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_POPULATE, fd_, 0);
  if (base_ == MAP_FAILED) {
    perror("ShmPool::Create: mmap failed");
    return -1;
  }

#if MAP_POPULATE == 0 && defined(POSIX_MADV_WILLNEED)
  posix_madvise(base_, pool_size, POSIX_MADV_WILLNEED);
#endif

  std::memset(base_, 0, pool_size);
  return 0;
}

int ShmPool::MapFd(int fd) {
  struct stat statbuf;
  if (fstat(fd, &statbuf) != 0) {
    perror("ShmPool::MapFd: fstat failed");
    close(fd);
    return -1;
  }

  base_ = mmap(nullptr, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
               fd, 0);
  if (base_ == MAP_FAILED) {
    perror("ShmPool::MapFd: mmap failed");
    return -1;
  }

  fd_ = fd;
  pos_ = 0;
  size_ = static_cast<size_t>(statbuf.st_size);
  return 0;
}

int ShmPool::Map(const std::string &path) {
  int fd = open(path.c_str(), O_RDWR, 0666);
  if (fd == -1) {
    perror("ShmPool::Map: open failed");
    return -1;
  }

  path_ = path;
  if (MapFd(fd)) {
    close(fd);
    return -1;
  }
  return 0;
}

int ShmPool::Unmap() {
  if (base_ == nullptr) {
    return 0;
  }
  if (munmap(base_, size_)) {
    perror("ShmPool::Unmap: unmap failed");
    return -1;
  }
  close(fd_);
  fd_ = -1;
  base_ = nullptr;
  size_ = 0;
  return 0;
}

int ShmPool::Unlink() {
  if (path_.empty()) {
    return 0;
  }
  return unlink(path_.c_str());
}

BaseIf::BaseIf() = default;

size_t BaseIf::SHMSize(const BaseIfParams &params) {
  return params.in_num_entries * params.in_entries_size +
         params.out_num_entries * params.out_entries_size;
}

int BaseIf::Init(const BaseIfParams &params) {
  if (MustCheckSync(params.sync_mode) &&
      params.link_latency < params.sync_interval) {
    std::fprintf(stderr,
                 "BaseIf::Init: latency must be larger or equal to sync "
                 "interval\n");
    return -1;
  }

  *this = BaseIf();
  params_ = params;
  return 0;
}

int BaseIf::ManagerSetup(ShmPool *pool, size_t in_offset, size_t out_offset,
                         size_t in_entries, size_t out_entries,
                         size_t in_entry_size, size_t out_entry_size) {
  shm_ = pool;
  in_queue_ = static_cast<uint8_t *>(pool->base()) + in_offset;
  in_pos_ = 0;
  in_elen_ = in_entry_size;
  in_enum_ = in_entries;
  in_timestamp_ = 0;

  out_queue_ = static_cast<uint8_t *>(pool->base()) + out_offset;
  out_pos_ = 0;
  out_elen_ = out_entry_size;
  out_enum_ = out_entries;
  out_timestamp_ = 0;

  conn_state_ = ConnState::kOpen;
  listener_ = false;
  conn_fd_ = -1;
  listen_fd_ = -1;
  return 0;
}

int BaseIf::Accept() {
  int flags = (!params_.blocking_conn ? SOCK_NONBLOCK : 0);
  conn_fd_ = Accept4Compat(listen_fd_, nullptr, nullptr, flags);
  if (conn_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
    conn_state_ = ConnState::kAwaitHandshakeRxTx;
    return 0;
  }

  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return 1;
  }

  perror("BaseIf::Accept: accept4 failed");
  close(listen_fd_);
  listen_fd_ = -1;
  conn_state_ = ConnState::kClosed;
  return -1;
}

int BaseIf::Listen(ShmPool *pool) {
  if (params_.sock_path.size() >= sizeof(sockaddr_un::sun_path)) {
    std::fprintf(stderr,
                 "BaseIf::Listen: socket path %s is too long (exceeding %zu)\n",
                 params_.sock_path.c_str(),
                 sizeof(sockaddr_un::sun_path) - 1);
    errno = ENAMETOOLONG;
    return -1;
  }

  shm_ = pool;
  size_t in_len = params_.in_num_entries * params_.in_entries_size;
  size_t out_len = params_.out_num_entries * params_.out_entries_size;
  if (pool->pos() + in_len + out_len > pool->size()) {
    std::fprintf(stderr,
                 "BaseIf::Listen: not enough memory available in pool\n");
    return -1;
  }

  listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd_ == -1) {
    perror("BaseIf::Listen: socket failed");
    return -1;
  }

  if (!params_.blocking_conn) {
    int flags = fcntl(listen_fd_, F_GETFL, 0);
    if (flags == -1 || fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
      perror("BaseIf::Listen: fcntl set nonblock failed");
      close(listen_fd_);
      listen_fd_ = -1;
      return -1;
    }
  }

  sockaddr_un saun{};
  saun.sun_family = AF_UNIX;
  std::snprintf(saun.sun_path, sizeof(saun.sun_path), "%s",
                params_.sock_path.c_str());
  if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&saun), sizeof(saun))) {
    perror("BaseIf::Listen: bind failed");
    close(listen_fd_);
    listen_fd_ = -1;
    return -1;
  }

  if (listen(listen_fd_, 5)) {
    perror("BaseIf::Listen: listen failed");
    close(listen_fd_);
    listen_fd_ = -1;
    return -1;
  }

  in_queue_ = static_cast<uint8_t *>(pool->base()) + pool->pos();
  in_pos_ = 0;
  in_elen_ = params_.in_entries_size;
  in_enum_ = params_.in_num_entries;
  in_timestamp_ = 0;
  pool->set_pos(pool->pos() + in_len);

  out_queue_ = static_cast<uint8_t *>(pool->base()) + pool->pos();
  out_pos_ = 0;
  out_elen_ = params_.out_entries_size;
  out_enum_ = params_.out_num_entries;
  out_timestamp_ = 0;
  pool->set_pos(pool->pos() + out_len);

  conn_state_ = ConnState::kListening;
  listener_ = true;
  return (Accept() < 0 ? -1 : 0);
}

int BaseIf::Connect() {
  if (params_.sock_path.size() >= sizeof(sockaddr_un::sun_path)) {
    std::fprintf(stderr,
                 "BaseIf::Connect: socket path %s is too long (exceeding %zu)\n",
                 params_.sock_path.c_str(),
                 sizeof(sockaddr_un::sun_path) - 1);
    errno = ENAMETOOLONG;
    return -1;
  }

  listener_ = false;

  conn_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (conn_fd_ == -1) {
    perror("BaseIf::Connect: socket failed");
    return -1;
  }

  if (!params_.blocking_conn) {
    int flags = fcntl(conn_fd_, F_GETFL, 0);
    if (flags == -1 || fcntl(conn_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
      perror("BaseIf::Connect: fcntl set nonblock failed");
      close(conn_fd_);
      conn_fd_ = -1;
      return -1;
    }
  }

  sockaddr_un saun{};
  saun.sun_family = AF_UNIX;
  std::snprintf(saun.sun_path, sizeof(saun.sun_path), "%s",
                params_.sock_path.c_str());

  if (connect(conn_fd_, reinterpret_cast<sockaddr *>(&saun), sizeof(saun))) {
    if (errno == EINPROGRESS && !params_.blocking_conn) {
      conn_state_ = ConnState::kConnecting;
      return 0;
    }
    perror("BaseIf::Connect: connect failed");
    close(conn_fd_);
    conn_fd_ = -1;
    conn_state_ = ConnState::kClosed;
    return -1;
  }

  conn_state_ = ConnState::kAwaitHandshakeRxTx;
  return 0;
}

int BaseIf::Connected() {
  switch (conn_state_) {
    case ConnState::kListening:
      return Accept();
    case ConnState::kConnecting: {
      pollfd pfd{};
      pfd.fd = conn_fd_;
      pfd.events = POLLOUT;
      if (poll(&pfd, 1, 0) < 0) {
        perror("BaseIf::Connected: poll failed");
        conn_state_ = ConnState::kClosed;
        return -1;
      }

      int error = 0;
      socklen_t len = sizeof(error);
      if (getsockopt(conn_fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        perror("BaseIf::Connected: getsockopt failed");
        conn_state_ = ConnState::kClosed;
        return -1;
      }

      if (error) {
        errno = error;
        conn_state_ = ConnState::kClosed;
        return -1;
      }

      conn_state_ = ConnState::kAwaitHandshakeRxTx;
      return 0;
    }
    case ConnState::kAwaitHandshakeRxTx:
    case ConnState::kAwaitHandshakeRx:
    case ConnState::kAwaitHandshakeTx:
    case ConnState::kOpen:
      return 1;
    case ConnState::kClosed:
      return -1;
    default:
      std::fprintf(stderr, "BaseIf::Connected: unexpected conn state %u\n",
                   static_cast<unsigned>(conn_state_));
      return -1;
  }
}

int BaseIf::ConnFd() const {
  if (conn_state_ == ConnState::kListening) {
    return listen_fd_;
  }
  if (conn_state_ == ConnState::kConnecting) {
    return conn_fd_;
  }
  return -1;
}

int ConnsWait(const std::vector<BaseIf *> &base_ifs) {
  std::vector<pollfd> pfds;
  std::vector<size_t> ids;
  pfds.reserve(base_ifs.size());
  ids.reserve(base_ifs.size());

  for (size_t i = 0; i < base_ifs.size(); ++i) {
    BaseIf *base_if = base_ifs[i];
    if (base_if == nullptr) {
      continue;
    }
    int connected = base_if->Connected();
    if (connected == 1) {
      continue;
    }
    if (connected < 0) {
      return -1;
    }
    pfds.push_back(pollfd{});
    pfds.back().fd = base_if->ConnFd();
    pfds.back().events =
        base_if->params().blocking_conn ? POLLIN : POLLOUT;
    ids.push_back(i);
  }

  if (pfds.empty()) {
    return 0;
  }

  if (poll(pfds.data(), pfds.size(), -1) < 0) {
    perror("ConnsWait: poll failed");
    return -1;
  }

  for (size_t idx = 0; idx < pfds.size(); ++idx) {
    if (pfds[idx].revents & (POLLERR | POLLHUP)) {
      perror("ConnsWait: error event");
      return -1;
    }
    BaseIf *base_if = base_ifs[ids[idx]];
    int ret = base_if->Connected();
    if (ret <= 0) {
      perror("ConnsWait: connected failed");
      return -1;
    }
  }

  return 0;
}

int BaseIf::IntroSend(const void *payload, size_t payload_len) {
  if (conn_state_ != ConnState::kAwaitHandshakeRxTx &&
      conn_state_ != ConnState::kAwaitHandshakeTx) {
    std::fprintf(stderr,
                 "BaseIf::IntroSend: connection in unexpected state (%u)\n",
                 static_cast<unsigned>(conn_state_));
    return -1;
  }

  UbsimProtoListenerIntro intro{};
  UbsimProtoConnecterIntro cintro{};
  struct iovec iov[2];

  bool sync_optional = params_.sync_mode == SyncMode::kOptional;
  bool sync_required = params_.sync_mode == SyncMode::kRequired;

  if (listener_) {
    intro.version = UBSIM_PROTO_VERSION;
    intro.upper_layer_proto = params_.upper_layer_proto;
    intro.upper_layer_intro_off = sizeof(UbsimProtoListenerIntro);
    intro.l2c_offset = 0;
    intro.l2c_elen = params_.out_entries_size;
    intro.l2c_nentries = params_.out_num_entries;
    intro.c2l_offset = params_.out_entries_size * params_.out_num_entries;
    intro.c2l_elen = params_.in_entries_size;
    intro.c2l_nentries = params_.in_num_entries;

    if (sync_optional || sync_required) {
      intro.flags |= UBSIM_PROTO_FLAGS_LI_SYNC;
    }
    if (sync_required) {
      intro.flags |= UBSIM_PROTO_FLAGS_LI_SYNC_FORCE;
    }

    iov[0].iov_base = &intro;
    iov[0].iov_len = sizeof(intro);
  } else {
    cintro.version = UBSIM_PROTO_VERSION;
    cintro.upper_layer_proto = params_.upper_layer_proto;
    cintro.upper_layer_intro_off = sizeof(UbsimProtoConnecterIntro);

    if (sync_optional || sync_required) {
      cintro.flags |= UBSIM_PROTO_FLAGS_CO_SYNC;
    }
    if (sync_required) {
      cintro.flags |= UBSIM_PROTO_FLAGS_CO_SYNC_FORCE;
    }

    iov[0].iov_base = &cintro;
    iov[0].iov_len = sizeof(cintro);
  }

  iov[1].iov_base = const_cast<void *>(payload);
  iov[1].iov_len = payload_len;

  char cmsgbuf[CMSG_SPACE(sizeof(int))];
  msghdr msg{};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  if (listener_) {
    cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    int shm_fd = shm_ ? shm_->fd() : -1;
    std::memcpy(CMSG_DATA(cmsg), &shm_fd, sizeof(int));
    msg.msg_controllen = cmsg->cmsg_len;
  }

  ssize_t ret = sendmsg(conn_fd_, &msg, 0);
  if (ret < 0) {
    perror("BaseIf::IntroSend: sendmsg failed");
    return -1;
  }

  size_t expected = iov[0].iov_len + iov[1].iov_len;
  if (static_cast<size_t>(ret) != expected) {
    std::fprintf(stderr,
                 "BaseIf::IntroSend: sendmsg was short, sent=%zd expected=%zu\n",
                 ret, expected);
    return -1;
  }

  if (conn_state_ == ConnState::kAwaitHandshakeTx) {
    conn_state_ = ConnState::kOpen;
  } else if (conn_state_ == ConnState::kAwaitHandshakeRxTx) {
    conn_state_ = ConnState::kAwaitHandshakeRx;
  }

  return 0;
}

int BaseIf::IntroRecv(void *payload, size_t *payload_len) {
  if (conn_state_ != ConnState::kAwaitHandshakeRxTx &&
      conn_state_ != ConnState::kAwaitHandshakeRx) {
    std::fprintf(stderr,
                 "BaseIf::IntroRecv: connection in unexpected state (%u)\n",
                 static_cast<unsigned>(conn_state_));
    return -1;
  }

  UbsimProtoListenerIntro intro{};
  UbsimProtoConnecterIntro cintro{};
  struct iovec iov[2];

  if (listener_) {
    iov[0].iov_base = &cintro;
    iov[0].iov_len = sizeof(cintro);
  } else {
    iov[0].iov_base = &intro;
    iov[0].iov_len = sizeof(intro);
  }

  iov[1].iov_base = payload;
  iov[1].iov_len = *payload_len;

  char cmsgbuf[CMSG_SPACE(sizeof(int))];
  msghdr msg{};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  ssize_t ret = recvmsg(conn_fd_, &msg, 0);
  if (ret < 0) {
    perror("BaseIf::IntroRecv: recvmsg failed");
    return -1;
  }

  *payload_len = static_cast<size_t>(ret) - iov[0].iov_len;

  bool sync_force = false;
  uint64_t proto_version = 0;
  uint64_t upper_layer_proto = 0;

  if (listener_) {
    proto_version = cintro.version;
    upper_layer_proto = cintro.upper_layer_proto;
    sync_force = cintro.flags & UBSIM_PROTO_FLAGS_CO_SYNC_FORCE;
    sync_ = cintro.flags & UBSIM_PROTO_FLAGS_CO_SYNC;
  } else {
    proto_version = intro.version;
    upper_layer_proto = intro.upper_layer_proto;
    sync_force = intro.flags & UBSIM_PROTO_FLAGS_LI_SYNC_FORCE;
    sync_ = intro.flags & UBSIM_PROTO_FLAGS_LI_SYNC;
  }

  if (proto_version != UBSIM_PROTO_VERSION) {
    std::fprintf(stderr, "BaseIf::IntroRecv: unexpected version (%lx)\n",
                 proto_version);
    return -1;
  }

  if (upper_layer_proto != params_.upper_layer_proto) {
    std::fprintf(stderr,
                 "BaseIf::IntroRecv: peer's upper layer proto (%lx) != local "
                 "(%lx)\n",
                 upper_layer_proto, params_.upper_layer_proto);
    return -1;
  }

  if (sync_force && params_.sync_mode == SyncMode::kDisabled) {
    std::fprintf(stderr,
                 "BaseIf::IntroRecv: peer forced sync but disabled locally\n");
    return -1;
  }

  if (params_.sync_mode == SyncMode::kRequired && !sync_) {
    std::fprintf(stderr,
                 "BaseIf::IntroRecv: sync required locally, peer disabled\n");
    return -1;
  }

  if (!listener_) {
    if (intro.upper_layer_intro_off != iov[0].iov_len) {
      std::fprintf(stderr,
                   "BaseIf::IntroRecv: upper layer intro does not match\n");
      return -1;
    }

    if (msg.msg_controllen < sizeof(cmsgbuf)) {
      std::fprintf(stderr,
                   "BaseIf::IntroRecv: getting shm fd failed (short msg)\n");
      return -1;
    }

    cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr || cmsg->cmsg_type != SCM_RIGHTS) {
      std::fprintf(stderr, "BaseIf::IntroRecv: getting shm fd failed\n");
      return -1;
    }

    int shmfd = -1;
    std::memcpy(&shmfd, CMSG_DATA(cmsg), sizeof(int));
    if (shm_ == nullptr) {
      std::fprintf(stderr, "BaseIf::IntroRecv: shm pool not set\n");
      return -1;
    }
    if (shm_->MapFd(shmfd)) {
      std::fprintf(stderr, "BaseIf::IntroRecv: mapping shm failed\n");
      return -1;
    }

    if (intro.c2l_offset + intro.c2l_elen * intro.c2l_nentries > shm_->size()) {
      std::fprintf(stderr,
                   "BaseIf::IntroRecv: incoming queue outside shm pool\n");
      return -1;
    }
    if (intro.l2c_offset + intro.l2c_elen * intro.l2c_nentries > shm_->size()) {
      std::fprintf(stderr,
                   "BaseIf::IntroRecv: outgoing queue outside shm pool\n");
      return -1;
    }

    in_queue_ = static_cast<uint8_t *>(shm_->base()) + intro.c2l_offset;
    in_elen_ = intro.c2l_elen;
    in_enum_ = intro.c2l_nentries;
    out_queue_ = static_cast<uint8_t *>(shm_->base()) + intro.l2c_offset;
    out_elen_ = intro.l2c_elen;
    out_enum_ = intro.l2c_nentries;
  }

  if (conn_state_ == ConnState::kAwaitHandshakeRx) {
    conn_state_ = ConnState::kOpen;
  } else if (conn_state_ == ConnState::kAwaitHandshakeRxTx) {
    conn_state_ = ConnState::kAwaitHandshakeTx;
  }

  return 0;
}

int BaseIf::IntroFd() const {
  return conn_fd_;
}

int Establish(std::vector<EstablishData> *ifs) {
  if (ifs == nullptr) {
    return -1;
  }

  std::vector<pollfd> pfds;
  std::vector<size_t> ids;
  pfds.reserve(ifs->size());
  ids.reserve(ifs->size());

  for (size_t i = 0; i < ifs->size(); ++i) {
    BaseIf *base_if = (*ifs)[i].base_if;
    if (base_if == nullptr) {
      continue;
    }

    int connected = base_if->Connected();
    if (connected == 1) {
      continue;
    }
    if (connected < 0) {
      std::fprintf(stderr, "Establish: connecting %zu failed\n", i);
      return -1;
    }

    pfds.push_back(pollfd{});
    pfds.back().fd = base_if->ConnFd();
    pfds.back().events =
        base_if->params().blocking_conn ? POLLIN : POLLOUT;
    ids.push_back(i);
  }

  while (true) {
    size_t n_pfd = pfds.size();
    if (n_pfd == 0) {
      return 0;
    }

    if (poll(pfds.data(), n_pfd, -1) < 0) {
      perror("Establish: poll failed");
      return -1;
    }

    for (size_t idx = 0; idx < n_pfd; ++idx) {
      size_t i = ids[idx];
      BaseIf *base_if = (*ifs)[i].base_if;

      if ((base_if->params().blocking_conn && (pfds[idx].revents & POLLIN)) ||
          (!base_if->params().blocking_conn && (pfds[idx].revents & POLLOUT))) {
        int ret = base_if->Connected();
        if (ret < 0) {
          std::fprintf(stderr, "Establish: connecting %zu failed\n", i);
          return -1;
        }
      }

      if ((base_if->params().blocking_conn && (pfds[idx].revents & POLLOUT)) ||
          (!base_if->params().blocking_conn && (pfds[idx].revents & POLLIN))) {
        if (base_if->IntroSend((*ifs)[i].tx_intro, (*ifs)[i].tx_intro_len) != 0) {
          std::fprintf(stderr, "Establish: sending intro on %zu failed\n", i);
          return -1;
        }
      }

      if (pfds[idx].revents & POLLIN) {
        if (base_if->IntroRecv((*ifs)[i].rx_intro, &(*ifs)[i].rx_intro_len) != 0) {
          std::fprintf(stderr, "Establish: receiving intro on %zu failed\n", i);
          return -1;
        }
      }

      if (base_if->Connected() == 1) {
        pfds[idx] = pfds.back();
        pfds.pop_back();
        ids[idx] = ids.back();
        ids.pop_back();
        --idx;
        --n_pfd;
      }
    }
  }
}

void BaseIf::Close() {
  if (conn_state_ == ConnState::kListening && listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  } else if (conn_state_ == ConnState::kClosed) {
    return;
  }

  if (conn_state_ == ConnState::kOpen) {
    volatile UbsimProtoBaseMsg *msg = nullptr;
    while ((msg = OutAlloc(UINT64_MAX)) == nullptr) {
    }
    OutSend(msg, UBSIM_PROTO_MSG_TYPE_TERMINATE);
  }

  if (conn_fd_ >= 0) {
    close(conn_fd_);
    conn_fd_ = -1;
  }

  conn_state_ = ConnState::kClosed;
}

void BaseIf::Unlink() {
  if (!params_.sock_path.empty()) {
    unlink(params_.sock_path.c_str());
  }
}

uint8_t BaseIf::InType(volatile UbsimProtoBaseMsg *msg) const {
  return msg->header.own_type & ~UBSIM_PROTO_MSG_OWN_MASK;
}

volatile UbsimProtoBaseMsg *BaseIf::InPeek(uint64_t timestamp) {
  if (in_terminated_) {
    return nullptr;
  }

  auto *msg = static_cast<volatile UbsimProtoBaseMsg *>(
      static_cast<void *>(static_cast<uint8_t *>(in_queue_) +
                          in_pos_ * in_elen_));

  if ((msg->header.own_type & UBSIM_PROTO_MSG_OWN_MASK) !=
      UBSIM_PROTO_MSG_OWN_CON) {
    return nullptr;
  }

  if (sync_ && msg->header.timestamp > timestamp) {
    return nullptr;
  }

  return msg;
}

volatile UbsimProtoBaseMsg *BaseIf::InPoll(uint64_t timestamp) {
  volatile UbsimProtoBaseMsg *msg = InPeek(timestamp);
  if (msg == nullptr) {
    return nullptr;
  }

  if (InType(msg) == UBSIM_PROTO_MSG_TYPE_TERMINATE) {
    in_terminated_ = true;
  }

  return msg;
}

void BaseIf::InDone(volatile UbsimProtoBaseMsg *msg) {
  msg->header.own_type |= UBSIM_PROTO_MSG_OWN_PRO;
  in_pos_ = (in_pos_ + 1) % in_enum_;
}

uint64_t BaseIf::InTimestamp() const {
  volatile UbsimProtoBaseMsg *msg =
      static_cast<volatile UbsimProtoBaseMsg *>(
          static_cast<void *>(static_cast<uint8_t *>(in_queue_) +
                              in_pos_ * in_elen_));
  return msg->header.timestamp;
}

bool BaseIf::InTerminated() const {
  return in_terminated_;
}

volatile UbsimProtoBaseMsg *BaseIf::OutAlloc(uint64_t timestamp) {
  auto *msg = static_cast<volatile UbsimProtoBaseMsg *>(
      static_cast<void *>(static_cast<uint8_t *>(out_queue_) +
                          out_pos_ * out_elen_));

  if ((msg->header.own_type & UBSIM_PROTO_MSG_OWN_MASK) !=
      UBSIM_PROTO_MSG_OWN_PRO) {
    return nullptr;
  }

  if (sync_ && msg->header.timestamp > timestamp) {
    return nullptr;
  }

  msg->header.timestamp = timestamp + params_.link_latency;
  return msg;
}

void BaseIf::OutSend(volatile UbsimProtoBaseMsg *msg, uint8_t msg_type) {
  msg->header.own_type =
      UBSIM_PROTO_MSG_OWN_CON | (msg_type & UBSIM_PROTO_MSG_TYPE_MASK);
  out_pos_ = (out_pos_ + 1) % out_enum_;
  out_timestamp_ = msg->header.timestamp;
}

int BaseIf::OutSync(uint64_t timestamp) {
  if (!sync_) {
    return 0;
  }

  volatile UbsimProtoBaseMsg *msg = OutAlloc(timestamp);
  if (msg == nullptr) {
    return -1;
  }

  OutSend(msg, UBSIM_PROTO_MSG_TYPE_SYNC);
  return 0;
}

uint64_t BaseIf::OutNextSync() const {
  return out_timestamp_;
}

size_t BaseIf::OutMsgLen() const {
  return out_elen_;
}

bool BaseIf::SyncEnabled() const {
  return sync_;
}

}  // namespace ubsim
