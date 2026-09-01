#include "shuati/judge/submission_service.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "shuati/common/state_file.h"

namespace shuati::judge {
namespace {

Submission withTimestamps(Submission submission,
                          std::chrono::system_clock::time_point now) {
  submission.createdAt = now;
  submission.updatedAt = now;
  return submission;
}

}  // namespace

InMemorySubmissionRepository::InMemorySubmissionRepository(
    Clock clock,
    std::filesystem::path persistencePath)
    : clock_(std::move(clock)), persistencePath_(std::move(persistencePath)) {
  load();
}

void InMemorySubmissionRepository::load() {
  if (persistencePath_.empty()) return;
  const auto content = shuati::common::readStateFile(persistencePath_);
  if (content.empty()) return;
  std::istringstream input(content);
  std::string line;
  std::getline(input, line);
  if (line != "SHUATI_SUBMISSIONS_V1") {
    throw std::runtime_error("unsupported submissions state format");
  }
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto fields = shuati::common::splitStateLine(line);
    if (fields.size() != 15 || fields[0] != "S") {
      throw std::runtime_error("corrupt submission state record");
    }
    const auto status = parseSubmissionStatus(fields[5]);
    if (!status.has_value()) {
      throw std::runtime_error("corrupt submission status");
    }
    Submission submission;
    submission.id = std::stoll(fields[1]);
    submission.userId = std::stoll(fields[2]);
    submission.problemId = std::stoll(fields[3]);
    submission.language = shuati::common::hexDecode(fields[4]);
    submission.status = *status;
    submission.workerId = shuati::common::hexDecode(fields[6]);
    submission.compileMessage = shuati::common::hexDecode(fields[7]);
    submission.totalTimeMs = std::stoi(fields[8]);
    submission.maxMemoryKb = std::stoi(fields[9]);
    submission.createdAt = shuati::common::timeFromEpochMilliseconds(
        std::stoll(fields[10]));
    submission.updatedAt = shuati::common::timeFromEpochMilliseconds(
        std::stoll(fields[11]));
    submission.sourceDeletedAt = shuati::common::timeFromEpochMilliseconds(
        std::stoll(fields[12]));
    submission.source = shuati::common::hexDecode(fields[13]);
    const auto caseCount = static_cast<std::size_t>(std::stoull(fields[14]));
    for (std::size_t i = 0; i < caseCount; ++i) {
      if (!std::getline(input, line)) {
        throw std::runtime_error("missing submission case state record");
      }
      const auto caseFields = shuati::common::splitStateLine(line);
      if (caseFields.size() != 8 || caseFields[0] != "C") {
        throw std::runtime_error("corrupt submission case state record");
      }
      const auto caseStatus = parseSubmissionStatus(caseFields[2]);
      if (!caseStatus.has_value()) {
        throw std::runtime_error("corrupt submission case status");
      }
      JudgeCaseResult caseResult;
      caseResult.caseIndex = std::stoi(caseFields[1]);
      caseResult.status = *caseStatus;
      caseResult.timeMs = std::stoi(caseFields[3]);
      caseResult.memoryKb = std::stoi(caseFields[4]);
      caseResult.errorType = shuati::common::hexDecode(caseFields[5]);
      caseResult.message = shuati::common::hexDecode(caseFields[6]);
      caseResult.stderrText = shuati::common::hexDecode(caseFields[7]);
      submission.cases.push_back(std::move(caseResult));
    }
    submissionsById_[submission.id] = submission;
    nextId_ = std::max(nextId_, submission.id + 1);
  }
}

void InMemorySubmissionRepository::persistLocked() const {
  if (persistencePath_.empty()) return;
  std::vector<Submission> submissions;
  submissions.reserve(submissionsById_.size());
  for (const auto& item : submissionsById_) submissions.push_back(item.second);
  std::sort(submissions.begin(), submissions.end(), [](const auto& left, const auto& right) {
    return left.id < right.id;
  });
  std::ostringstream output;
  output << "SHUATI_SUBMISSIONS_V1\n";
  for (const auto& submission : submissions) {
    output << "S\t" << submission.id << '\t' << submission.userId << '\t'
           << submission.problemId << '\t'
           << shuati::common::hexEncode(submission.language) << '\t'
           << toString(submission.status) << '\t'
           << shuati::common::hexEncode(submission.workerId) << '\t'
           << shuati::common::hexEncode(submission.compileMessage) << '\t'
           << submission.totalTimeMs << '\t' << submission.maxMemoryKb << '\t'
           << shuati::common::epochMilliseconds(submission.createdAt) << '\t'
           << shuati::common::epochMilliseconds(submission.updatedAt) << '\t'
           << shuati::common::epochMilliseconds(submission.sourceDeletedAt) << '\t'
           << shuati::common::hexEncode(submission.source) << '\t'
           << submission.cases.size() << '\n';
    for (const auto& caseResult : submission.cases) {
      output << "C\t" << caseResult.caseIndex << '\t'
             << toString(caseResult.status) << '\t' << caseResult.timeMs << '\t'
             << caseResult.memoryKb << '\t'
             << shuati::common::hexEncode(caseResult.errorType) << '\t'
             << shuati::common::hexEncode(caseResult.message) << '\t'
             << shuati::common::hexEncode(caseResult.stderrText) << '\n';
    }
  }
  shuati::common::atomicWriteStateFile(persistencePath_, output.str());
}

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
  persistLocked();
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
  persistLocked();
  return best->second;
}

std::optional<Submission> InMemorySubmissionRepository::claimPendingById(
    std::int64_t id,
    const std::string& workerId) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = submissionsById_.find(id);
  if (it == submissionsById_.end() ||
      it->second.status != SubmissionStatus::Pending) {
    return std::nullopt;
  }
  it->second.status = SubmissionStatus::Running;
  it->second.workerId = workerId;
  it->second.updatedAt = now();
  persistLocked();
  return it->second;
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
  persistLocked();
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
  if (recovered > 0) persistLocked();
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
  if (cleaned > 0) persistLocked();
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
  if (actor.userId <= 0 || problemId <= 0 ||
      (language != "cpp" && language != "c") ||
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

SubmissionResult SubmissionService::claimPendingById(
    std::int64_t submissionId,
    const std::string& workerId) {
  if (submissionId <= 0 || workerId.empty()) {
    return failure(SubmissionError::InvalidInput);
  }
  const auto claimed = submissions_->claimPendingById(submissionId, workerId);
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
