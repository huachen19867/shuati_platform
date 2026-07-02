#include "shuati/app/app_config.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path writeTempConfig(const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("shuati-app-config-test-" +
                     std::to_string(std::chrono::steady_clock::now()
                                        .time_since_epoch()
                                        .count()) +
                     ".yaml");
  std::ofstream out(path);
  out << content;
  return path;
}

}  // namespace

TEST(AppConfigTest, LoadsConfiguredP1Values) {
  const auto path = writeTempConfig(R"CFG(
app:
  name: "custom-oj"
  environment: "test"
server:
  host: "127.0.0.1"
  port: 18080
  public_dir: "custom_public"
logs:
  level: "debug"
  access: "tmp/access.log"
  error: "tmp/error.log"
  judge: "tmp/judge.log"
)CFG");

  const auto config = shuati::app::AppConfig::loadFromFile(path.string());

  EXPECT_EQ(config.appName, "custom-oj");
  EXPECT_EQ(config.environment, "test");
  EXPECT_EQ(config.server.host, "127.0.0.1");
  EXPECT_EQ(config.server.port, 18080);
  EXPECT_EQ(config.server.publicDir, "custom_public");
  EXPECT_EQ(config.logs.level, shuati::common::LogLevel::Debug);
  EXPECT_EQ(config.logs.accessPath, "tmp/access.log");
  EXPECT_EQ(config.logs.errorPath, "tmp/error.log");
  EXPECT_EQ(config.logs.judgePath, "tmp/judge.log");

  std::filesystem::remove(path);
}

TEST(AppConfigTest, UsesP1DefaultsForMissingOptionalValues) {
  const auto path = writeTempConfig("app:\n  name: minimal\n");

  const auto config = shuati::app::AppConfig::loadFromFile(path.string());

  EXPECT_EQ(config.appName, "minimal");
  EXPECT_EQ(config.environment, "development");
  EXPECT_EQ(config.server.host, "0.0.0.0");
  EXPECT_EQ(config.server.port, 8080);
  EXPECT_EQ(config.server.publicDir, "public");
  EXPECT_EQ(config.logs.level, shuati::common::LogLevel::Info);
  EXPECT_EQ(config.logs.accessPath, "logs/access.log");
  EXPECT_EQ(config.logs.errorPath, "logs/error.log");
  EXPECT_EQ(config.logs.judgePath, "logs/judge.log");

  std::filesystem::remove(path);
}

TEST(AppConfigTest, LoadsDatabaseSecurityAndBootstrapValues) {
  const auto path = writeTempConfig(R"CFG(
database:
  host: "db.internal"
  port: 3307
  name: "oj_test"
  user: "oj_user"
  password: "oj_pass"
  pool_size: 8
  acquire_timeout_ms: 1500
security:
  session_ttl_hours: 24
  submit_interval_seconds: 3
  upload_max_mb: 32
bootstrap:
  super_admin:
    enabled: true
    username: "boss"
    password: "boss-password"
)CFG");

  const auto config = shuati::app::AppConfig::loadFromFile(path.string());

  EXPECT_EQ(config.database.host, "db.internal");
  EXPECT_EQ(config.database.port, 3307);
  EXPECT_EQ(config.database.name, "oj_test");
  EXPECT_EQ(config.database.user, "oj_user");
  EXPECT_EQ(config.database.password, "oj_pass");
  EXPECT_EQ(config.database.poolSize, 8);
  EXPECT_EQ(config.database.acquireTimeoutMs, 1500);
  EXPECT_EQ(config.security.sessionTtlHours, 24);
  EXPECT_EQ(config.security.submitIntervalSeconds, 3);
  EXPECT_EQ(config.security.uploadMaxMb, 32);
  EXPECT_TRUE(config.superAdmin.enabled);
  EXPECT_EQ(config.superAdmin.username, "boss");
  EXPECT_EQ(config.superAdmin.password, "boss-password");

  std::filesystem::remove(path);
}

TEST(AppConfigTest, LoadsJudgeAndStorageValues) {
  const auto path = writeTempConfig(R"CFG(
judge:
  docker_image: "custom-cpp-judge:latest"
  workers: 3
  source_size_limit_kb: 96
  compile_timeout_ms: 12000
  run_timeout_ms: 3000
  memory_limit_mb: 256
  output_limit_kb: 2048
  compile_message_limit_kb: 16
  stderr_limit_kb: 6
  temp_dir: "tmp/judge"
storage:
  testcase_dir: "tmp/testcases"
  submission_dir: "tmp/submissions"
  source_retention_hours: 12
)CFG");

  const auto config = shuati::app::AppConfig::loadFromFile(path.string());

  EXPECT_EQ(config.judge.dockerImage, "custom-cpp-judge:latest");
  EXPECT_EQ(config.judge.workers, 3);
  EXPECT_EQ(config.judge.sourceSizeLimitKb, 96);
  EXPECT_EQ(config.judge.compileTimeoutMs, 12000);
  EXPECT_EQ(config.judge.runTimeoutMs, 3000);
  EXPECT_EQ(config.judge.memoryLimitMb, 256);
  EXPECT_EQ(config.judge.outputLimitKb, 2048);
  EXPECT_EQ(config.judge.compileMessageLimitKb, 16);
  EXPECT_EQ(config.judge.stderrLimitKb, 6);
  EXPECT_EQ(config.judge.tempDir, "tmp/judge");
  EXPECT_EQ(config.storage.testcaseDir, "tmp/testcases");
  EXPECT_EQ(config.storage.submissionDir, "tmp/submissions");
  EXPECT_EQ(config.storage.sourceRetentionHours, 12);

  std::filesystem::remove(path);
}

TEST(AppConfigTest, UsesConservativeJudgeAndStorageDefaults) {
  const auto path = writeTempConfig("app:\n  name: minimal\n");

  const auto config = shuati::app::AppConfig::loadFromFile(path.string());

  EXPECT_EQ(config.judge.dockerImage, "shuati-cpp-judge:latest");
  EXPECT_EQ(config.judge.workers, 4);
  EXPECT_EQ(config.judge.sourceSizeLimitKb, 64);
  EXPECT_EQ(config.judge.compileTimeoutMs, 10000);
  EXPECT_EQ(config.judge.runTimeoutMs, 2000);
  EXPECT_EQ(config.judge.memoryLimitMb, 128);
  EXPECT_EQ(config.judge.outputLimitKb, 1024);
  EXPECT_EQ(config.judge.compileMessageLimitKb, 8);
  EXPECT_EQ(config.judge.stderrLimitKb, 4);
  EXPECT_EQ(config.judge.tempDir, "data/judge_tmp");
  EXPECT_EQ(config.storage.testcaseDir, "data/testcases");
  EXPECT_EQ(config.storage.submissionDir, "data/submissions");
  EXPECT_EQ(config.storage.sourceRetentionHours, 24);

  std::filesystem::remove(path);
}
