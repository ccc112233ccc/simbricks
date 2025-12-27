#ifndef UBSIM_BASE_LOG_HPP_
#define UBSIM_BASE_LOG_HPP_

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace ubsim {

enum class LogLevel {
  kDebug,
  kInfo,
  kWarn,
  kError,
};

class Logger {
 public:
  static Logger &Instance() {
    static Logger instance;
    return instance;
  }

  void SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
  }

  void Log(LogLevel level, const std::string &component,
           const std::string &message) {
    if (level < level_) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream out;
    out << Timestamp() << " [" << LevelName(level) << "] " << component
        << ": " << message;
    std::cerr << out.str() << std::endl;
  }

 private:
  Logger() = default;

  std::string Timestamp() const {
    using std::chrono::system_clock;
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
  }

  const char *LevelName(LogLevel level) const {
    switch (level) {
      case LogLevel::kDebug:
        return "DEBUG";
      case LogLevel::kInfo:
        return "INFO";
      case LogLevel::kWarn:
        return "WARN";
      case LogLevel::kError:
        return "ERROR";
      default:
        return "LOG";
    }
  }

  LogLevel level_ = LogLevel::kInfo;
  std::mutex mutex_;
};

inline void LogDebug(const std::string &component, const std::string &message) {
  Logger::Instance().Log(LogLevel::kDebug, component, message);
}

inline void LogInfo(const std::string &component, const std::string &message) {
  Logger::Instance().Log(LogLevel::kInfo, component, message);
}

inline void LogWarn(const std::string &component, const std::string &message) {
  Logger::Instance().Log(LogLevel::kWarn, component, message);
}

inline void LogError(const std::string &component, const std::string &message) {
  Logger::Instance().Log(LogLevel::kError, component, message);
}

}  // namespace ubsim

#endif  // UBSIM_BASE_LOG_HPP_
