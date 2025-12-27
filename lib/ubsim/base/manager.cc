#include "lib/ubsim/base/manager.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "lib/ubsim/base/log.hpp"

namespace ubsim {
namespace {

bool ParseLine(const std::string &line, std::vector<std::string> *tokens) {
  tokens->clear();
  std::istringstream stream(line);
  std::string token;
  while (stream >> token) {
    if (!token.empty() && token[0] == '#') {
      break;
    }
    tokens->push_back(token);
  }
  return !tokens->empty();
}

}  // namespace

bool ManagerPortLoader::LoadFromFile(const std::string &path,
                                     const std::string &port_name,
                                     ManagerPort *out) const {
  std::ifstream file(path);
  if (!file.is_open()) {
    LogError("manager", "failed to open port file: " + path);
    return false;
  }

  std::string line;
  std::vector<std::string> tokens;
  while (std::getline(file, line)) {
    if (!ParseLine(line, &tokens)) {
      continue;
    }
    if (tokens[0] != port_name) {
      continue;
    }
    if (tokens.size() < 9) {
      LogError("manager", "malformed port entry for " + port_name);
      return false;
    }

    out->name = tokens[0];
    out->channel_type = tokens[1];
    out->shm_path = tokens[2];
    out->in_offset = std::stoull(tokens[3]);
    out->in_entries = std::stoull(tokens[4]);
    out->in_entry_size = std::stoull(tokens[5]);
    out->out_offset = std::stoull(tokens[6]);
    out->out_entries = std::stoull(tokens[7]);
    out->out_entry_size = std::stoull(tokens[8]);
    if (out->channel_type == "mq") {
      if (tokens.size() < 11) {
        LogError("manager", "missing mq queue names for " + port_name);
        return false;
      }
      out->mq_in_name = tokens[9];
      out->mq_out_name = tokens[10];
    }
    return true;
  }

  LogError("manager", "port not found: " + port_name);
  return false;
}

bool ManagerPortLoader::LoadFromEnv(const std::string &port_name,
                                    ManagerPort *out) const {
  const char *env = std::getenv("UBSIM_MANAGER_PORTS");
  if (env == nullptr) {
    LogError("manager", "UBSIM_MANAGER_PORTS not set");
    return false;
  }
  return LoadFromFile(env, port_name, out);
}

ChannelAttachment::ChannelAttachment() = default;

bool ChannelAttachment::Attach(BaseIf *base_if, BaseIfParams *params,
                               const ManagerPort &port) {
  if (port.channel_type != "shm_ring") {
    LogError("manager", "unsupported channel type: " + port.channel_type);
    return false;
  }

  if (base_if == nullptr || params == nullptr) {
    LogError("manager", "invalid base interface parameters");
    return false;
  }

  if (base_if->Init(*params)) {
    LogError("manager", "failed to init base interface");
    return false;
  }

  if (pool_.Map(port.shm_path) != 0) {
    LogError("manager", "failed to map shared memory: " + port.shm_path);
    return false;
  }

  params->in_num_entries = port.in_entries;
  params->out_num_entries = port.out_entries;
  params->in_entries_size = port.in_entry_size;
  params->out_entries_size = port.out_entry_size;
  base_if->params() = *params;

  return base_if->ManagerSetup(&pool_, port.in_offset, port.out_offset,
                               port.in_entries, port.out_entries,
                               port.in_entry_size,
                               port.out_entry_size) == 0;
}

}  // namespace ubsim
