#ifndef SIMBRICKS_CXX_BASE_ENDPOINT_H_
#define SIMBRICKS_CXX_BASE_ENDPOINT_H_

#include <memory>
#include <functional>

#include "cxx/base/communicator.h"

union SimbricksProtoBaseMsg;

namespace simbricks {
namespace base {

class Endpoint {
 public:
  explicit Endpoint(std::unique_ptr<Communicator> comm)
      : comm_(std::move(comm)) {}

  int Listen() { return comm_->Listen(); }
  int Connect() { return comm_->Connect(); }
  bool PollConnection() { return comm_->PollConnection(); }

  template <typename T>
  bool Send(uint64_t timestamp, uint8_t msg_type, std::function<void(volatile T*)> preparer) {
    volatile void* raw_msg = comm_->OutAlloc(timestamp);
    if (!raw_msg) return false;
    volatile T* msg = static_cast<volatile T*>(raw_msg);
    preparer(msg);
    comm_->OutSend(msg, msg_type);
    return true;
  }

  template <typename T>
  bool Receive(uint64_t timestamp, std::function<void(volatile T*)> handler) {
    volatile void* raw_msg = comm_->InPoll(timestamp);
    if (!raw_msg) return false;
    volatile T* msg = static_cast<volatile T*>(raw_msg);
    handler(msg);
    comm_->InDone(msg);
    return true;
  }

 private:
  std::unique_ptr<Communicator> comm_;
};

}  // namespace base
}  // namespace simbricks

#endif  // SIMBRICKS_CXX_BASE_ENDPOINT_H_
