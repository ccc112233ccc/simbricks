#ifndef UBSIM_BASE_MANAGER_HPP_
#define UBSIM_BASE_MANAGER_HPP_

#include <cstddef>
#include <string>

#include <ubsim/base/if.h>

namespace ubsim {

struct ManagerPort {
  std::string name;
  std::string channel_type;
  std::string shm_path;
  std::string mq_in_name;
  std::string mq_out_name;
  size_t in_offset = 0;
  size_t out_offset = 0;
  size_t in_entries = 0;
  size_t out_entries = 0;
  size_t in_entry_size = 0;
  size_t out_entry_size = 0;
};

class ManagerPortLoader {
 public:
  bool LoadFromFile(const std::string &path, const std::string &port_name,
                    ManagerPort *out) const;
  bool LoadFromEnv(const std::string &port_name, ManagerPort *out) const;
};

class ChannelAttachment {
 public:
  ChannelAttachment();
  bool Attach(BaseIf *base_if, BaseIfParams *params, const ManagerPort &port);

 private:
  ShmPool pool_;
};

}  // namespace ubsim

#endif  // UBSIM_BASE_MANAGER_HPP_
