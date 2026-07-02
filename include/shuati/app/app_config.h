#pragma once

#include <string>

#include "shuati/common/logging.h"

namespace shuati::app {

struct ServerConfig {
  std::string host = "0.0.0.0";
  int port = 8080;
  std::string publicDir = "public";
};

struct LogConfig {
  shuati::common::LogLevel level = shuati::common::LogLevel::Info;
  std::string accessPath = "logs/access.log";
  std::string errorPath = "logs/error.log";
  std::string judgePath = "logs/judge.log";
};

struct AppConfig {
  std::string appName = "shuati-platform";
  std::string environment = "development";
  ServerConfig server;
  LogConfig logs;

  static AppConfig loadFromFile(const std::string& path);
};

}  // namespace shuati::app
