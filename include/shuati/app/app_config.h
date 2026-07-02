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

struct JudgeConfig {
  std::string dockerImage = "shuati-cpp-judge:latest";
  int workers = 4;
  int sourceSizeLimitKb = 64;
  int compileTimeoutMs = 10000;
  int runTimeoutMs = 2000;
  int memoryLimitMb = 128;
  int outputLimitKb = 1024;
  int compileMessageLimitKb = 8;
  int stderrLimitKb = 4;
  std::string tempDir = "data/judge_tmp";
};

struct StorageConfig {
  std::string testcaseDir = "data/testcases";
  std::string submissionDir = "data/submissions";
  int sourceRetentionHours = 24;
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
  JudgeConfig judge;
  StorageConfig storage;
  SuperAdminConfig superAdmin;

  static AppConfig loadFromFile(const std::string& path);
};

}  // namespace shuati::app
