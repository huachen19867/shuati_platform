#pragma once

#include <string>

namespace shuati::judge {

enum class SubmissionStatus {
  Pending,
  Compiling,
  Running,
  Accepted,
  WrongAnswer,
  TimeLimitExceeded,
  MemoryLimitExceeded,
  RuntimeError,
  CompileError,
  OutputLimitExceeded,
  SystemError,
};

struct JudgeCaseResult {
  int caseIndex = 0;
  SubmissionStatus status = SubmissionStatus::Pending;
  int timeMs = 0;
  int memoryKb = 0;
  std::string errorType;
  std::string message;
  std::string stderrText;
};

std::string toString(SubmissionStatus status);
bool isFinalStatus(SubmissionStatus status);

}  // namespace shuati::judge
