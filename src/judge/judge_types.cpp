#include "shuati/judge/judge_types.h"

namespace shuati::judge {

std::string toString(SubmissionStatus status) {
  switch (status) {
    case SubmissionStatus::Pending:
      return "Pending";
    case SubmissionStatus::Compiling:
      return "Compiling";
    case SubmissionStatus::Running:
      return "Running";
    case SubmissionStatus::Accepted:
      return "Accepted";
    case SubmissionStatus::WrongAnswer:
      return "WrongAnswer";
    case SubmissionStatus::TimeLimitExceeded:
      return "TimeLimitExceeded";
    case SubmissionStatus::MemoryLimitExceeded:
      return "MemoryLimitExceeded";
    case SubmissionStatus::RuntimeError:
      return "RuntimeError";
    case SubmissionStatus::CompileError:
      return "CompileError";
    case SubmissionStatus::OutputLimitExceeded:
      return "OutputLimitExceeded";
    case SubmissionStatus::SystemError:
      return "SystemError";
  }
  return "SystemError";
}

std::optional<SubmissionStatus> parseSubmissionStatus(const std::string& value) {
  if (value == "Pending") return SubmissionStatus::Pending;
  if (value == "Compiling") return SubmissionStatus::Compiling;
  if (value == "Running") return SubmissionStatus::Running;
  if (value == "Accepted") return SubmissionStatus::Accepted;
  if (value == "WrongAnswer") return SubmissionStatus::WrongAnswer;
  if (value == "TimeLimitExceeded") return SubmissionStatus::TimeLimitExceeded;
  if (value == "MemoryLimitExceeded") return SubmissionStatus::MemoryLimitExceeded;
  if (value == "RuntimeError") return SubmissionStatus::RuntimeError;
  if (value == "CompileError") return SubmissionStatus::CompileError;
  if (value == "OutputLimitExceeded") return SubmissionStatus::OutputLimitExceeded;
  if (value == "SystemError") return SubmissionStatus::SystemError;
  return std::nullopt;
}

bool isFinalStatus(SubmissionStatus status) {
  return status != SubmissionStatus::Pending &&
         status != SubmissionStatus::Compiling &&
         status != SubmissionStatus::Running;
}

}  // namespace shuati::judge
