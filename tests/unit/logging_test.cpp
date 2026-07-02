#include "shuati/common/logging.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::path makeTempPath(const std::string& name) {
  return std::filesystem::temp_directory_path() /
         ("shuati-" + name + "-" +
          std::to_string(std::chrono::steady_clock::now()
                             .time_since_epoch()
                             .count()) +
          ".log");
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

}  // namespace

TEST(LogLevelTest, ParsesLevelNamesCaseInsensitively) {
  EXPECT_EQ(shuati::common::parseLogLevel("debug"),
            shuati::common::LogLevel::Debug);
  EXPECT_EQ(shuati::common::parseLogLevel("INFO"),
            shuati::common::LogLevel::Info);
  EXPECT_EQ(shuati::common::parseLogLevel("Warn"),
            shuati::common::LogLevel::Warn);
  EXPECT_EQ(shuati::common::parseLogLevel("error"),
            shuati::common::LogLevel::Error);
}

TEST(LogLevelTest, RejectsUnknownLevelName) {
  EXPECT_THROW(shuati::common::parseLogLevel("trace"), std::invalid_argument);
}

TEST(FileLoggerTest, FiltersMessagesBelowConfiguredLevel) {
  const auto path = makeTempPath("level-filter");
  shuati::common::FileLogger logger(path.string(),
                                    shuati::common::LogLevel::Warn);

  logger.log(shuati::common::LogLevel::Debug, "hidden debug");
  logger.log(shuati::common::LogLevel::Info, "hidden info");
  logger.log(shuati::common::LogLevel::Warn, "visible warn");
  logger.log(shuati::common::LogLevel::Error, "visible error");

  const auto content = readFile(path);
  EXPECT_EQ(content.find("hidden debug"), std::string::npos);
  EXPECT_EQ(content.find("hidden info"), std::string::npos);
  EXPECT_NE(content.find("[WARN] visible warn"), std::string::npos);
  EXPECT_NE(content.find("[ERROR] visible error"), std::string::npos);

  std::filesystem::remove(path);
}

TEST(FileLoggerTest, CreatesParentDirectoriesForLogFile) {
  const auto base = std::filesystem::temp_directory_path() /
                    ("shuati-logs-" +
                     std::to_string(std::chrono::steady_clock::now()
                                        .time_since_epoch()
                                        .count()));
  const auto path = base / "nested" / "access.log";
  shuati::common::FileLogger logger(path.string(),
                                    shuati::common::LogLevel::Info);

  logger.info("request accepted");

  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_NE(readFile(path).find("[INFO] request accepted"), std::string::npos);

  std::filesystem::remove_all(base);
}
