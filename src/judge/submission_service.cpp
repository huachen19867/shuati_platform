#include "shuati/judge/submission_service.h"

#include <algorithm>

namespace shuati::judge {
namespace {

Submission withTimestamps(Submission submission,
                          std::chrono::system_clock::time_point now) {
  submission.createdAt = now;
  submission.updatedAt = now;
  return submission;
}

}  // namespace

InMemorySubmissionRepository::InMemorySubmissionRepository(Clock clock)
    : clock_(std::move(clock)) {}

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
  submission = withTimestamps(submission, now());
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
  best->second.updatedAt = now();
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
  it->second.updatedAt = now();
  return it->second;
}

std::size_t InMemorySubmissionRepository::recoverInterruptedSubmissions() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::size_t recovered = 0;
  for (auto& [id, submission] : submissionsById_) {
    (void)id;
    if (submission.status == SubmissionStatus::Compiling ||
        submission.status == SubmissionStatus::Running) {
      submission.status = SubmissionStatus::Pending;
      submission.workerId.clear();
      submission.updatedAt = now();
      ++recovered;
    }
  }
  return recovered;
}

std::size_t InMemorySubmissionRepository::cleanupSourcesOlderThan(
    std::chrono::system_clock::time_point cutoff) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::size_t cleaned = 0;
  for (auto& [id, submission] : submissionsById_) {
    (void)id;
    if (!submission.source.empty() && submission.createdAt <= cutoff) {
      submission.source.clear();
      submission.sourceDeletedAt = now();
      submission.updatedAt = submission.sourceDeletedAt;
      ++cleaned;
    }
  }
  return cleaned;
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

std::chrono::system_clock::time_point InMemorySubmissionRepository::now() const {
  return clock_();
}

SubmissionService::SubmissionService(
    std::shared_ptr<ISubmissionRepository> submissions,
    int sourceSizeLimitKb,
    std::chrono::seconds submitInterval,
    Clock clock)
    : submissions_(std::move(submissions)),
      sourceSizeLimitKb_(sourceSizeLimitKb),
      submitInterval_(submitInterval),
      clock_(std::move(clock)) {}

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
  if (submitInterval_.count() > 0) {
    std::lock_guard<std::mutex> lock(limiterMutex_);
    const auto now = clock_();
    const auto it = lastSubmissionByUser_.find(actor.userId);
    if (it != lastSubmissionByUser_.end() &&
        now - it->second < submitInterval_) {
      return failure(SubmissionError::RateLimited);
    }
    lastSubmissionByUser_[actor.userId] = now;
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

std::size_t SubmissionService::recoverInterruptedSubmissions() {
  auto* inMemory =
      dynamic_cast<InMemorySubmissionRepository*>(submissions_.get());
  if (inMemory == nullptr) {
    return 0;
  }
  return inMemory->recoverInterruptedSubmissions();
}

std::size_t SubmissionService::cleanupExpiredSources(
    std::chrono::hours retention) {
  auto* inMemory =
      dynamic_cast<InMemorySubmissionRepository*>(submissions_.get());
  if (inMemory == nullptr) {
    return 0;
  }
  return inMemory->cleanupSourcesOlderThan(clock_() - retention);
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
    case SubmissionError::RateLimited:
      return "too many submissions, please try again later";
  }
  return "unknown submission error";
}

}  // namespace shuati::judge
