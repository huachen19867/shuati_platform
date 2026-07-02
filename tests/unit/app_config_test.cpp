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
