#include "shuati/problem/testcase_service.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

namespace shuati::problem {
namespace {

struct ParsedName {
  int caseIndex = 0;
  std::string extension;
};

struct PairContent {
  std::optional<std::string> input;
  std::optional<std::string> output;
};

bool isDigits(const std::string& value) {
  return !value.empty() &&
         std::all_of(value.begin(), value.end(), [](unsigned char ch) {
           return ch >= '0' && ch <= '9';
         });
}

std::optional<ParsedName> parseName(const std::string& name) {
  if (name.find('/') != std::string::npos ||
      name.find('\\') != std::string::npos ||
      name.find("..") != std::string::npos) {
    return std::nullopt;
  }

  const auto dot = name.rfind('.');
  if (dot == std::string::npos) {
    return std::nullopt;
  }
  const auto stem = name.substr(0, dot);
  const auto extension = name.substr(dot);
  if (!isDigits(stem) || (extension != ".in" && extension != ".out")) {
    return std::nullopt;
  }
  const auto index = std::stoi(stem);
  if (index <= 0) {
    return std::nullopt;
  }
  return ParsedName{index, extension};
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  out << content;
}

TestcaseMeta metaFor(const std::filesystem::path& dir, int caseIndex) {
  const auto inputPath = dir / (std::to_string(caseIndex) + ".in");
  const auto outputPath = dir / (std::to_string(caseIndex) + ".out");
  TestcaseMeta meta;
  meta.caseIndex = caseIndex;
  meta.inputPath = inputPath.generic_string();
  meta.outputPath = outputPath.generic_string();
  meta.inputSize = std::filesystem::file_size(inputPath);
  meta.outputSize = std::filesystem::file_size(outputPath);
  return meta;
}

}  // namespace

TestcaseService::TestcaseService(std::string testcaseRoot)
    : testcaseRoot_(std::move(testcaseRoot)) {}

TestcasePackageResult TestcaseService::replaceTestcases(
    std::int64_t problemId,
    const std::vector<TestcaseFile>& files) const {
  if (problemId <= 0 || files.empty()) {
    return failure(TestcaseError::InvalidPackage,
                   "testcase package must not be empty");
  }

  std::map<int, PairContent> pairs;
  for (const auto& file : files) {
    const auto parsed = parseName(file.name);
    if (!parsed.has_value()) {
      return failure(TestcaseError::InvalidPackage,
                     "unsafe path or invalid testcase name: " + file.name);
    }

    auto& pair = pairs[parsed->caseIndex];
    if (parsed->extension == ".in") {
      if (pair.input.has_value()) {
        return failure(TestcaseError::InvalidPackage,
                       "duplicate input file: " + file.name);
      }
      pair.input = file.content;
    } else {
      if (pair.output.has_value()) {
        return failure(TestcaseError::InvalidPackage,
                       "duplicate output file: " + file.name);
      }
      pair.output = file.content;
    }
  }

  for (const auto& [caseIndex, pair] : pairs) {
    if (!pair.input.has_value()) {
      return failure(TestcaseError::InvalidPackage,
                     "missing " + std::to_string(caseIndex) + ".in");
    }
    if (!pair.output.has_value()) {
      return failure(TestcaseError::InvalidPackage,
                     "missing " + std::to_string(caseIndex) + ".out");
    }
  }

  try {
    const std::filesystem::path dir(problemDir(problemId));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    TestcasePackageResult result;
    result.ok = true;
    result.message = "ok";
    for (const auto& [caseIndex, pair] : pairs) {
      writeFile(dir / (std::to_string(caseIndex) + ".in"), *pair.input);
      writeFile(dir / (std::to_string(caseIndex) + ".out"), *pair.output);
      result.testcases.push_back(metaFor(dir, caseIndex));
    }
    return result;
  } catch (const std::exception& ex) {
    return failure(TestcaseError::IoError, ex.what());
  }
}

TestcasePackageResult TestcaseService::listTestcases(
    std::int64_t problemId) const {
  if (problemId <= 0) {
    return failure(TestcaseError::InvalidPackage, "invalid problem id");
  }

  const std::filesystem::path dir(problemDir(problemId));
  TestcasePackageResult result;
  result.ok = true;
  result.message = "ok";
  if (!std::filesystem::exists(dir)) {
    return result;
  }

  std::vector<int> indexes;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto parsed = parseName(entry.path().filename().string());
    if (parsed.has_value() && parsed->extension == ".in") {
      indexes.push_back(parsed->caseIndex);
    }
  }
  std::sort(indexes.begin(), indexes.end());
  for (const auto index : indexes) {
    const auto outputPath = dir / (std::to_string(index) + ".out");
    if (std::filesystem::exists(outputPath)) {
      result.testcases.push_back(metaFor(dir, index));
    }
  }
  return result;
}

TestcasePackageResult TestcaseService::failure(
    TestcaseError error,
    const std::string& message) const {
  return TestcasePackageResult{false, error, message, {}};
}

std::string TestcaseService::problemDir(std::int64_t problemId) const {
  return (std::filesystem::path(testcaseRoot_) /
          ("problem_" + std::to_string(problemId)))
      .generic_string();
}

std::string testcaseErrorMessage(TestcaseError error) {
  switch (error) {
    case TestcaseError::None:
      return "ok";
    case TestcaseError::InvalidPackage:
      return "invalid testcase package";
    case TestcaseError::IoError:
      return "testcase storage error";
  }
  return "unknown testcase error";
}

}  // namespace shuati::problem
