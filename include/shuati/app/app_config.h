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

struct DatabaseConfig {
  std::string host = "127.0.0.1";
  int port = 3306;
  std::string name = "shuati_platform";
  std::string user = "shuati";
  std::string password = "change-me";
  int poolSize = 4;
  int acquireTimeoutMs = 3000;
};

struct SecurityConfig {
  int sessionTtlHours = 168;
  int submitIntervalSeconds = 5;
  int uploadMaxMb = 20;
};

struct SuperAdminConfig {
  bool enabled = true;
  std::string username = "root";
  std::string password = "change-me-now";
};

struct AppConfig {
  std::string appName = "shuati-platform";
  std::string environment = "development";
  ServerConfig server;
  DatabaseConfig database;
  LogConfig logs;
  SecurityConfig security;
  SuperAdminConfig superAdmin;

  static AppConfig loadFromFile(const std::string& path);
};

}  // namespace shuati::app
