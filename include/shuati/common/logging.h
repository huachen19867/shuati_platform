#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace shuati::common {

enum class LogLevel {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3,
};

LogLevel parseLogLevel(const std::string& value);
std::string toString(LogLevel level);

class FileLogger {
 public:
  FileLogger(std::string path, LogLevel minLevel);

  void log(LogLevel level, const std::string& message);
  void debug(const std::string& message);
  void info(const std::string& message);
  void warn(const std::string& message);
  void error(const std::string& message);

  [[nodiscard]] const std::string& path() const;
  [[nodiscard]] LogLevel minLevel() const;

 private:
  [[nodiscard]] bool shouldLog(LogLevel level) const;

  std::string path_;
  LogLevel minLevel_;
  std::ofstream output_;
  std::mutex mutex_;
};

}  // namespace shuati::common
