#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shuati/judge/judge_types.h"
#include "shuati/problem/testcase_service.h"

namespace shuati::judge {

struct LocalCppRunnerConfig {
  std::string compiler = "g++";
  std::string tempDir = "data/judge_tmp";
  int compileTimeoutMs = 10000;
  int runTimeoutMs = 2000;
  int outputLimitKb = 1024;
  int compileMessageLimitKb = 8;
  int stderrLimitKb = 4;
};

struct JudgeRunRequest {
  std::int64_t submissionId = 0;
  std::string source;
  std::vector<shuati::problem::TestcaseMeta> testcases;
};

struct JudgeRunResult {
  SubmissionStatus status = SubmissionStatus::Pending;
  std::string compileMessage;
  int totalTimeMs = 0;
  int maxMemoryKb = 0;
  std::vector<JudgeCaseResult> cases;
};

class LocalCppRunner {
 public:
  explicit LocalCppRunner(LocalCppRunnerConfig config);

  JudgeRunResult judge(const JudgeRunRequest& request) const;

 private:
  LocalCppRunnerConfig config_;
};

std::string normalizeJudgeOutput(const std::string& output);

}  // namespace shuati::judge
