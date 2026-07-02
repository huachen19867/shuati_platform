#include "shuati/common/config.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::path writeTempConfig(const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("shuati-config-test-" +
                     std::to_string(std::chrono::steady_clock::now()
                                        .time_since_epoch()
                                        .count()) +
                     ".yaml");
  std::ofstream out(path);
  out << content;
  return path;
}

}  // namespace

TEST(ConfigTest, ParsesNestedSectionsAndScalars) {
  const auto path = writeTempConfig(R"CFG(
app:
  name: "shuati-platform"
  port: 8080
  debug: false

judge:
  workers: 4
  run_timeout_ms: 2000
)CFG");

  const auto config = shuati::common::Config::loadFromFile(path.string());

  EXPECT_EQ(config.getString("app.name"), "shuati-platform");
  EXPECT_EQ(config.getInt("app.port"), 8080);
  EXPECT_FALSE(config.getBool("app.debug"));
  EXPECT_EQ(config.getInt("judge.workers"), 4);
  EXPECT_EQ(config.getInt("judge.run_timeout_ms"), 2000);

  std::filesystem::remove(path);
}

TEST(ConfigTest, IgnoresCommentsAndTrimsWhitespace) {
  const auto path = writeTempConfig(R"CFG(
# Global comment
server:
  host:  0.0.0.0   # inline comment
  public_dir: "public"
)CFG");

  const auto config = shuati::common::Config::loadFromFile(path.string());

  EXPECT_EQ(config.getString("server.host"), "0.0.0.0");
  EXPECT_EQ(config.getString("server.public_dir"), "public");

  std::filesystem::remove(path);
}

TEST(ConfigTest, UsesDefaultsForMissingValues) {
  const auto path = writeTempConfig("app:\n  name: shuati-platform\n");

  const auto config = shuati::common::Config::loadFromFile(path.string());

  EXPECT_EQ(config.getString("missing.key", "fallback"), "fallback");
  EXPECT_EQ(config.getInt("missing.number", 42), 42);
  EXPECT_TRUE(config.getBool("missing.flag", true));

  std::filesystem::remove(path);
}

TEST(ConfigTest, ThrowsForMissingFile) {
  EXPECT_THROW(shuati::common::Config::loadFromFile("/tmp/no-such-shuati.yaml"),
               std::runtime_error);
}
