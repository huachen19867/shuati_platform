#include "shuati/problem/testcase_service.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path tempRoot() {
  return std::filesystem::temp_directory_path() /
         ("shuati-testcases-" +
          std::to_string(std::chrono::steady_clock::now()
                             .time_since_epoch()
                             .count()));
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

}  // namespace

TEST(TestcaseServiceTest, ReplacesWholePackageWithPairedInOutFiles) {
  const auto root = tempRoot();
  shuati::problem::TestcaseService service(root.string());

  const auto first = service.replaceTestcases(
      7, {{"1.in", "1 2\n"}, {"1.out", "3\n"}, {"2.in", "4 5\n"},
          {"2.out", "9\n"}});
  ASSERT_TRUE(first.ok) << first.message;
  ASSERT_EQ(first.testcases.size(), 2U);
  EXPECT_TRUE(std::filesystem::exists(root / "problem_7" / "1.in"));
  EXPECT_EQ(readFile(root / "problem_7" / "2.out"), "9\n");

  const auto second =
      service.replaceTestcases(7, {{"1.in", "10 20\n"}, {"1.out", "30\n"}});
  ASSERT_TRUE(second.ok) << second.message;
  EXPECT_EQ(second.testcases.size(), 1U);
  EXPECT_FALSE(std::filesystem::exists(root / "problem_7" / "2.in"));
  EXPECT_EQ(readFile(root / "problem_7" / "1.in"), "10 20\n");

  std::filesystem::remove_all(root);
}

TEST(TestcaseServiceTest, RejectsMissingOutputPairWithDetailedMessage) {
  const auto root = tempRoot();
  shuati::problem::TestcaseService service(root.string());

  const auto result = service.replaceTestcases(7, {{"1.in", "1 2\n"}});

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, shuati::problem::TestcaseError::InvalidPackage);
  EXPECT_NE(result.message.find("missing 1.out"), std::string::npos);

  std::filesystem::remove_all(root);
}

TEST(TestcaseServiceTest, RejectsUnsafeRelativeNames) {
  const auto root = tempRoot();
  shuati::problem::TestcaseService service(root.string());

  const auto result =
      service.replaceTestcases(7, {{"../1.in", "1\n"}, {"1.out", "1\n"}});

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, shuati::problem::TestcaseError::InvalidPackage);
  EXPECT_NE(result.message.find("unsafe path"), std::string::npos);

  std::filesystem::remove_all(root);
}

TEST(TestcaseServiceTest, ListsStoredTestcasesInCaseOrder) {
  const auto root = tempRoot();
  shuati::problem::TestcaseService service(root.string());
  ASSERT_TRUE(service.replaceTestcases(
                         7, {{"2.in", "2\n"}, {"2.out", "2\n"},
                             {"1.in", "1\n"}, {"1.out", "1\n"}})
                  .ok);

  const auto result = service.listTestcases(7);

  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.testcases.size(), 2U);
  EXPECT_EQ(result.testcases[0].caseIndex, 1);
  EXPECT_EQ(result.testcases[1].caseIndex, 2);

  std::filesystem::remove_all(root);
}
