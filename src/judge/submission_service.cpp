#include "shuati/judge/submission_service.h"

#include <algorithm>

namespace shuati::judge {
namespace {

Submission withTimestamps(Submission submission) {
  const auto now = std::chrono::system_clock::now();
  submission.createdAt = now;
  submission.updatedAt = now;
  return submission;
}

}  // namespace

Submission InMemorySubmissionRepository::createSubmission(
    std::int64_t userId,
    std::int64_t problemId,
    const std::string& language,
    const std::string& source) {
  std::lock_guard<std::mutex> lock(mutex_);
  Submission submission;
  submission.id = nextId_++;
  submission.userId = userId;
  submission.problemId = problemId;
  submission.language = language;
  submission.source = source;
  submission.status = SubmissionStatus::Pending;
  submission = withTimestamps(submission);
  submissionsById_[submission.id] = submission;
  return submission;
}

std::optional<Submission> InMemorySubmissionRepository::findById(
    std::int64_t id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = submissionsById_.find(id);
  if (it == submissionsById_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<Submission> InMemorySubmissionRepository::claimNextPending(
    const std::string& workerId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto best = submissionsById_.end();
  for (auto it = submissionsById_.begin(); it != submissionsById_.end(); ++it) {
    if (it->second.status != SubmissionStatus::Pending) {
      continue;
    }
    if (best == submissionsById_.end() || it->second.id < best->second.id) {
      best = it;
    }
  }
  if (best == submissionsById_.end()) {
    return std::nullopt;
  }
  best->second.status = SubmissionStatus::Running;
  best->second.workerId = workerId;
  best->second.updatedAt = std::chrono::system_clock::now();
  return best->second;
}

std::optional<Submission> InMemorySubmissionRepository::completeSubmission(
    std::int64_t id,
    const JudgeRunResult& result) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = submissionsById_.find(id);
  if (it == submissionsById_.end()) {
    return std::nullopt;
  }
  it->second.status = result.status;
  it->second.compileMessage = result.compileMessage;
  it->second.totalTimeMs = result.totalTimeMs;
  it->second.maxMemoryKb = result.maxMemoryKb;
  it->second.cases = result.cases;
  it->second.updatedAt = std::chrono::system_clock::now();
  return it->second;
}

std::vector<Submission> InMemorySubmissionRepository::listSubmissions() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Submission> submissions;
  submissions.reserve(submissionsById_.size());
  for (const auto& [id, submission] : submissionsById_) {
    (void)id;
    submissions.push_back(submission);
  }
  std::sort(submissions.begin(), submissions.end(),
            [](const Submission& left, const Submission& right) {
              return left.id < right.id;
            });
  return submissions;
}

SubmissionService::SubmissionService(
    std::shared_ptr<ISubmissionRepository> submissions,
    int sourceSizeLimitKb)
    : submissions_(std::move(submissions)),
      sourceSizeLimitKb_(sourceSizeLimitKb) {}

SubmissionResult SubmissionService::createSubmission(
    const Actor& actor,
    std::int64_t problemId,
    const std::string& language,
    const std::string& source) {
  if (actor.userId <= 0 || problemId <= 0 || language != "cpp" ||
      source.empty() ||
      source.size() >
          static_cast<std::size_t>(sourceSizeLimitKb_) * 1024U) {
    return failure(SubmissionError::InvalidInput);
  }
  return SubmissionResult{
      true, SubmissionError::None, "ok",
      submissions_->createSubmission(actor.userId, problemId, language, source)};
}

SubmissionResult SubmissionService::claimNextPending(
    const std::string& workerId) {
  if (workerId.empty()) {
    return failure(SubmissionError::InvalidInput);
  }
  const auto claimed = submissions_->claimNextPending(workerId);
  if (!claimed.has_value()) {
    return failure(SubmissionError::NoPendingTask);
  }
  return SubmissionResult{true, SubmissionError::None, "ok", *claimed};
}

SubmissionResult SubmissionService::completeSubmission(
    std::int64_t submissionId,
    const JudgeRunResult& judgeResult) {
  if (submissionId <= 0 || !isFinalStatus(judgeResult.status)) {
    return failure(SubmissionError::InvalidInput);
  }
  const auto completed =
      submissions_->completeSubmission(submissionId, judgeResult);
  if (!completed.has_value()) {
    return failure(SubmissionError::NotFound);
  }
  return SubmissionResult{true, SubmissionError::None, "ok", *completed};
}

SubmissionResult SubmissionService::getSubmission(
    const Actor& actor,
    std::int64_t submissionId) const {
  if (submissionId <= 0) {
    return failure(SubmissionError::InvalidInput);
  }
  const auto found = submissions_->findById(submissionId);
  if (!found.has_value()) {
    return failure(SubmissionError::NotFound);
  }
  if (found->userId != actor.userId && !shuati::auth::canAccessAdmin(actor.role)) {
    return failure(SubmissionError::Forbidden);
  }
  return SubmissionResult{true, SubmissionError::None, "ok", *found};
}

SubmissionListResult SubmissionService::listSubmissions(
    const Actor& actor) const {
  SubmissionListResult result;
  result.ok = true;
  result.message = "ok";
  for (const auto& submission : submissions_->listSubmissions()) {
    if (submission.userId == actor.userId ||
        shuati::auth::canAccessAdmin(actor.role)) {
      result.submissions.push_back(submission);
    }
  }
  return result;
}

SubmissionResult SubmissionService::failure(SubmissionError error) const {
  return SubmissionResult{false, error, submissionErrorMessage(error), {}};
}

SubmissionListResult SubmissionService::listFailure(
    SubmissionError error) const {
  return SubmissionListResult{false, error, submissionErrorMessage(error), {}};
}

std::string submissionErrorMessage(SubmissionError error) {
  switch (error) {
    case SubmissionError::None:
      return "ok";
    case SubmissionError::InvalidInput:
      return "invalid submission input";
    case SubmissionError::Forbidden:
      return "forbidden";
    case SubmissionError::NotFound:
      return "submission not found";
    case SubmissionError::NoPendingTask:
      return "no pending judge task";
  }
  return "unknown submission error";
}

}  // namespace shuati::judge
