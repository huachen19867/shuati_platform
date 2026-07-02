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

bool isFinalStatus(SubmissionStatus status) {
  return status != SubmissionStatus::Pending &&
         status != SubmissionStatus::Compiling &&
         status != SubmissionStatus::Running;
}

}  // namespace shuati::judge
