#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "shuati/auth/role.h"
#include "shuati/judge/local_cpp_runner.h"

namespace shuati::judge {

enum class SubmissionError {
  None,
  InvalidInput,
  Forbidden,
  NotFound,
  NoPendingTask,
};

struct Actor {
  std::int64_t userId = 0;
  shuati::auth::UserRole role = shuati::auth::UserRole::User;
};

struct Submission {
  std::int64_t id = 0;
  std::int64_t userId = 0;
  std::int64_t problemId = 0;
  std::string language = "cpp";
  std::string source;
  SubmissionStatus status = SubmissionStatus::Pending;
  std::string workerId;
  std::string compileMessage;
  int totalTimeMs = 0;
  int maxMemoryKb = 0;
  std::chrono::system_clock::time_point createdAt{};
  std::chrono::system_clock::time_point updatedAt{};
  std::vector<JudgeCaseResult> cases;
};

struct SubmissionResult {
  bool ok = false;
  SubmissionError error = SubmissionError::None;
  std::string message;
  Submission submission;
};

struct SubmissionListResult {
  bool ok = false;
  SubmissionError error = SubmissionError::None;
  std::string message;
  std::vector<Submission> submissions;
};

class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;

  virtual Submission createSubmission(std::int64_t userId,
                                      std::int64_t problemId,
                                      const std::string& language,
                                      const std::string& source) = 0;
  virtual std::optional<Submission> findById(std::int64_t id) const = 0;
  virtual std::optional<Submission> claimNextPending(
      const std::string& workerId) = 0;
  virtual std::optional<Submission> completeSubmission(
      std::int64_t id,
      const JudgeRunResult& result) = 0;
  virtual std::vector<Submission> listSubmissions() const = 0;
};

class InMemorySubmissionRepository : public ISubmissionRepository {
 public:
  Submission createSubmission(std::int64_t userId,
                              std::int64_t problemId,
                              const std::string& language,
                              const std::string& source) override;
  std::optional<Submission> findById(std::int64_t id) const override;
  std::optional<Submission> claimNextPending(
      const std::string& workerId) override;
  std::optional<Submission> completeSubmission(
      std::int64_t id,
      const JudgeRunResult& result) override;
  std::vector<Submission> listSubmissions() const override;

 private:
  mutable std::mutex mutex_;
  std::int64_t nextId_ = 1;
  std::unordered_map<std::int64_t, Submission> submissionsById_;
};

class SubmissionService {
 public:
  explicit SubmissionService(std::shared_ptr<ISubmissionRepository> submissions,
                             int sourceSizeLimitKb = 64);

  SubmissionResult createSubmission(const Actor& actor,
                                    std::int64_t problemId,
                                    const std::string& language,
                                    const std::string& source);
  SubmissionResult claimNextPending(const std::string& workerId);
  SubmissionResult completeSubmission(std::int64_t submissionId,
                                      const JudgeRunResult& judgeResult);
  SubmissionResult getSubmission(const Actor& actor,
                                 std::int64_t submissionId) const;
  SubmissionListResult listSubmissions(const Actor& actor) const;

 private:
  SubmissionResult failure(SubmissionError error) const;
  SubmissionListResult listFailure(SubmissionError error) const;

  std::shared_ptr<ISubmissionRepository> submissions_;
  int sourceSizeLimitKb_ = 64;
};

std::string submissionErrorMessage(SubmissionError error);

}  // namespace shuati::judge
