#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace shuati::problem {

enum class TestcaseError {
  None,
  InvalidPackage,
  IoError,
};

struct TestcaseFile {
  std::string name;
  std::string content;
};

struct TestcaseMeta {
  int caseIndex = 0;
  std::string inputPath;
  std::string outputPath;
  std::uintmax_t inputSize = 0;
  std::uintmax_t outputSize = 0;
};

struct TestcaseLimits {
  std::size_t maxFiles = 100;
  std::uintmax_t maxPackageBytes = 20U * 1024U * 1024U;
};

struct TestcasePackageResult {
  bool ok = false;
  TestcaseError error = TestcaseError::None;
  std::string message;
  std::vector<TestcaseMeta> testcases;
};

class TestcaseService {
 public:
  explicit TestcaseService(std::string testcaseRoot,
                           TestcaseLimits limits = TestcaseLimits{});

  TestcasePackageResult replaceTestcases(
      std::int64_t problemId,
      const std::vector<TestcaseFile>& files) const;
  TestcasePackageResult listTestcases(std::int64_t problemId) const;

 private:
  TestcasePackageResult failure(TestcaseError error,
                                const std::string& message) const;
  std::string problemDir(std::int64_t problemId) const;

  std::string testcaseRoot_;
  TestcaseLimits limits_;
};

std::string testcaseErrorMessage(TestcaseError error);

}  // namespace shuati::problem
