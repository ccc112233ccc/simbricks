#ifndef SIMBRICKS_CXX_COMMUNICATOR_SHM_COMMUNICATOR_H_
#define SIMBRICKS_CXX_COMMUNICATOR_SHM_COMMUNICATOR_H_

#include <string>
#include <memory>

#include "base/if.h" // Relative to lib/simbricks
#include "cxx/base/communicator.h" // Relative to lib/simbricks

namespace simbricks {
namespace communicator {

class ShmCommunicator : public base::Communicator {
 public:
  explicit ShmCommunicator(const std::string& sock_path,
                           const std::string& shm_path,
                           bool listener);
  ~ShmCommunicator() override;

  int Listen() override;
  int Connect() override;
  bool PollConnection() override;

  volatile void* OutAlloc(uint64_t timestamp) override;
  void OutSend(volatile void* msg, uint8_t msg_type) override;
  volatile void* InPoll(uint64_t timestamp) override;
  void InDone(volatile void* msg) override;

 private:
  SimbricksBaseIf base_if_;
  SimbricksBaseIfParams params_;
  SimbricksBaseIfSHMPool shm_pool_;

  bool listener_;
  std::string sock_path_;
  std::string shm_path_;
};

}  // namespace communicator
}  // namespace simbricks

#endif  // SIMBRICKS_CXX_COMMUNICATOR_SHM_COMMUNICATOR_H_
