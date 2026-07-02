#include "shuati/judge/local_cpp_runner.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path tempRoot(const std::string& name) {
  return std::filesystem::temp_directory_path() /
         ("shuati-runner-" + name + "-" +
          std::to_string(std::chrono::steady_clock::now()
                             .time_since_epoch()
                             .count()));
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << content;
}

shuati::problem::TestcaseMeta testcaseMeta(const std::filesystem::path& dir,
                                           int index,
                                           const std::string& input,
                                           const std::string& output) {
  const auto inputPath = dir / (std::to_string(index) + ".in");
  const auto outputPath = dir / (std::to_string(index) + ".out");
  writeFile(inputPath, input);
  writeFile(outputPath, output);
  shuati::problem::TestcaseMeta meta;
  meta.caseIndex = index;
  meta.inputPath = inputPath.generic_string();
  meta.outputPath = outputPath.generic_string();
  meta.inputSize = std::filesystem::file_size(inputPath);
  meta.outputSize = std::filesystem::file_size(outputPath);
  return meta;
}

shuati::judge::LocalCppRunnerConfig runnerConfig(
    const std::filesystem::path& tempDir) {
  shuati::judge::LocalCppRunnerConfig config;
  config.tempDir = tempDir.generic_string();
  config.compileTimeoutMs = 10000;
  config.runTimeoutMs = 2000;
  config.outputLimitKb = 64;
  config.compileMessageLimitKb = 1;
  config.stderrLimitKb = 1;
  return config;
}

}  // namespace

TEST(LocalCppRunnerTest, AcceptsCorrectCppAgainstMultipleCases) {
  const auto root = tempRoot("accepted");
  const auto caseDir = root / "cases";
  std::vector<shuati::problem::TestcaseMeta> testcases{
      testcaseMeta(caseDir, 1, "1 2\n", "3\n"),
      testcaseMeta(caseDir, 2, "10 20\n", "30\n")};
  shuati::judge::LocalCppRunner runner(runnerConfig(root / "tmp"));

  const auto result = runner.judge({101, R"CPP(
#include <iostream>
int main() {
  long long a, b;
  while (std::cin >> a >> b) {
    std::cout << a + b << "\n";
  }
  return 0;
}
)CPP",
                                    testcases});

  EXPECT_EQ(result.status, shuati::judge::SubmissionStatus::Accepted);
  ASSERT_EQ(result.cases.size(), 2U);
  EXPECT_EQ(result.cases[0].status,
            shuati::judge::SubmissionStatus::Accepted);
  EXPECT_GE(result.cases[0].timeMs, 0);
  EXPECT_EQ(result.cases[0].memoryKb, 0);

  std::filesystem::remove_all(root);
}

TEST(LocalCppRunnerTest, ReportsWrongAnswer) {
  const auto root = tempRoot("wa");
  const auto caseDir = root / "cases";
  std::vector<shuati::problem::TestcaseMeta> testcases{
      testcaseMeta(caseDir, 1, "1 2\n", "3\n")};
  shuati::judge::LocalCppRunner runner(runnerConfig(root / "tmp"));

  const auto result = runner.judge({102, R"CPP(
#include <iostream>
int main() {
  std::cout << 42 << "\n";
  return 0;
}
)CPP",
                                    testcases});

  EXPECT_EQ(result.status, shuati::judge::SubmissionStatus::WrongAnswer);
  ASSERT_EQ(result.cases.size(), 1U);
  EXPECT_EQ(result.cases[0].status,
            shuati::judge::SubmissionStatus::WrongAnswer);
  EXPECT_EQ(result.cases[0].errorType, "WrongAnswer");

  std::filesystem::remove_all(root);
}

TEST(LocalCppRunnerTest, TruncatesCompileError) {
  const auto root = tempRoot("ce");
  const auto caseDir = root / "cases";
  std::vector<shuati::problem::TestcaseMeta> testcases{
      testcaseMeta(caseDir, 1, "1\n", "1\n")};
  shuati::judge::LocalCppRunner runner(runnerConfig(root / "tmp"));
  const std::string longError(4096, 'A');

  const auto result =
      runner.judge({103, "#error \"" + longError + "\"\nint main(){}\n",
                    testcases});

  EXPECT_EQ(result.status, shuati::judge::SubmissionStatus::CompileError);
  EXPECT_FALSE(result.compileMessage.empty());
  EXPECT_LE(result.compileMessage.size(), 1024U);
  EXPECT_TRUE(result.cases.empty());

  std::filesystem::remove_all(root);
}
