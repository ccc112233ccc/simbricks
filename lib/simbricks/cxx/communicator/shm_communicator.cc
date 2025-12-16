#include "cxx/communicator/shm_communicator.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdexcept>
#include <cstring> // For memset, strerror
#include <cerrno>  // For errno

// C library include is implicitly handled by the header

namespace simbricks {
namespace communicator {

ShmCommunicator::ShmCommunicator(const std::string& sock_path,
                                 const std::string& shm_path,
                                 bool listener)
    : listener_(listener),
      sock_path_(sock_path),
      shm_path_(shm_path) {
  memset(&base_if_, 0, sizeof(base_if_));
  memset(&params_, 0, sizeof(params_));
  memset(&shm_pool_, 0, sizeof(shm_pool_));

  SimbricksBaseIfDefaultParams(&params_);
  params_.sock_path = sock_path_.c_str();

  if (SimbricksBaseIfInit(&base_if_, &params_) != 0) {
    throw std::runtime_error("SimbricksBaseIfInit failed");
  }
}

ShmCommunicator::~ShmCommunicator() {
  SimbricksBaseIfClose(&base_if_);
  if (listener_) {
    SimbricksBaseIfSHMPoolUnmap(&shm_pool_);
    SimbricksBaseIfSHMPoolUnlink(&shm_pool_);
  }
}

int ShmCommunicator::Listen() {
  if (!listener_) return -1;

  size_t shm_size = SimbricksBaseIfSHMSize(&params_);
  if (SimbricksBaseIfSHMPoolCreate(&shm_pool_, shm_path_.c_str(), shm_size) != 0) {
    fprintf(stderr, "ShmCommunicator::Listen: SimbricksBaseIfSHMPoolCreate failed: %s\n", strerror(errno));
    return -1;
  }

  return SimbricksBaseIfListen(&base_if_, &shm_pool_);
}

int ShmCommunicator::Connect() {
  if (listener_) return -1;
  return SimbricksBaseIfConnect(&base_if_);
}

bool ShmCommunicator::PollConnection() {
    if (base_if_.conn_state == kConnOpen) {
        return true;
    }

    // Non-blocking check
    if (SimbricksBaseIfConnected(&base_if_) != 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        // A real error occurred
        return false;
    }

    // If connected, proceed with handshake
    if (base_if_.conn_state >= kConnAwaitHandshakeRx) { // AwaitHandshake and beyond
        if (base_if_.conn_state == kConnAwaitHandshakeRxTx ||
            base_if_.conn_state == kConnAwaitHandshakeTx) {

            if (SimbricksBaseIfIntroSend(&base_if_, nullptr, 0) != 0)
                return false;
        }

        if (base_if_.conn_state == kConnAwaitHandshakeRxTx ||
            base_if_.conn_state == kConnAwaitHandshakeRx) {

            size_t len = 0;
            int recv_ret = SimbricksBaseIfIntroRecv(&base_if_, nullptr, &len);
            if (recv_ret < 0)
                return false;
        }
    }

    return base_if_.conn_state == kConnOpen;
}

volatile void* ShmCommunicator::OutAlloc(uint64_t timestamp) {
  return SimbricksBaseIfOutAlloc(&base_if_, timestamp);
}

void ShmCommunicator::OutSend(volatile void* msg, uint8_t msg_type) {
  SimbricksBaseIfOutSend(&base_if_,
                         (volatile union SimbricksProtoBaseMsg*)msg,
                                                  msg_type);
}

volatile void* ShmCommunicator::InPoll(uint64_t timestamp) {
  return SimbricksBaseIfInPoll(&base_if_, timestamp);
}

void ShmCommunicator::InDone(volatile void* msg) {
  SimbricksBaseIfInDone(&base_if_,
                        (volatile union SimbricksProtoBaseMsg*)msg);
}

}  // namespace communicator
}  // namespace simbricks
