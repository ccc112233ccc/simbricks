#ifndef SIMBRICKS_CXX_BASE_COMMUNICATOR_H_
#define SIMBRICKS_CXX_BASE_COMMUNICATOR_H_

#include <cstddef>
#include <cstdint>

namespace simbricks {
namespace base {

class Communicator {
 public:
  virtual ~Communicator() = default;
  virtual int Listen() = 0;
  virtual int Connect() = 0;
  virtual bool PollConnection() = 0;
  virtual volatile void* OutAlloc(uint64_t timestamp) = 0;
  virtual void OutSend(volatile void* msg, uint8_t msg_type) = 0;
  virtual volatile void* InPoll(uint64_t timestamp) = 0;
  virtual void InDone(volatile void* msg) = 0;
};

}  // namespace base
}  // namespace simbricks

#endif  // SIMBRICKS_CXX_BASE_COMMUNICATOR_H_
