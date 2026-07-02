#include "shuati/common/logging.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace shuati::common {
namespace {

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

std::string timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t rawTime = std::chrono::system_clock::to_time_t(now);
  std::tm localTime{};

#if defined(_WIN32)
  localtime_s(&localTime, &rawTime);
#else
  localtime_r(&rawTime, &localTime);
#endif

  std::ostringstream out;
  out << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
  return out.str();
}

}  // namespace

LogLevel parseLogLevel(const std::string& value) {
  const auto normalized = lower(value);
  if (normalized == "debug") {
    return LogLevel::Debug;
  }
  if (normalized == "info") {
    return LogLevel::Info;
  }
  if (normalized == "warn" || normalized == "warning") {
    return LogLevel::Warn;
  }
  if (normalized == "error") {
    return LogLevel::Error;
  }
  throw std::invalid_argument("unknown log level: " + value);
}

std::string toString(LogLevel level) {
  switch (level) {
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warn:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
  }
  return "UNKNOWN";
}

FileLogger::FileLogger(std::string path, LogLevel minLevel)
    : path_(std::move(path)), minLevel_(minLevel) {
  const std::filesystem::path logPath(path_);
  const auto parent = logPath.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  output_.open(path_, std::ios::app);
  if (!output_.is_open()) {
    throw std::runtime_error("failed to open log file: " + path_);
  }
}

void FileLogger::log(LogLevel level, const std::string& message) {
  if (!shouldLog(level)) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  output_ << timestamp() << " [" << toString(level) << "] " << message
          << '\n';
  output_.flush();
}

void FileLogger::debug(const std::string& message) {
  log(LogLevel::Debug, message);
}

void FileLogger::info(const std::string& message) {
  log(LogLevel::Info, message);
}

void FileLogger::warn(const std::string& message) {
  log(LogLevel::Warn, message);
}

void FileLogger::error(const std::string& message) {
  log(LogLevel::Error, message);
}

const std::string& FileLogger::path() const {
  return path_;
}

LogLevel FileLogger::minLevel() const {
  return minLevel_;
}

bool FileLogger::shouldLog(LogLevel level) const {
  return static_cast<int>(level) >= static_cast<int>(minLevel_);
}

}  // namespace shuati::common
