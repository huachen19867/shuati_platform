#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
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
  RateLimited,
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
  std::chrono::system_clock::time_point sourceDeletedAt{};
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
  using Clock = std::function<std::chrono::system_clock::time_point()>;

  explicit InMemorySubmissionRepository(
      Clock clock = [] { return std::chrono::system_clock::now(); });

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
  std::size_t recoverInterruptedSubmissions();
  std::size_t cleanupSourcesOlderThan(
      std::chrono::system_clock::time_point cutoff);
  std::vector<Submission> listSubmissions() const override;

 private:
  std::chrono::system_clock::time_point now() const;

  mutable std::mutex mutex_;
  Clock clock_;
  std::int64_t nextId_ = 1;
  std::unordered_map<std::int64_t, Submission> submissionsById_;
};

class SubmissionService {
 public:
  using Clock = std::function<std::chrono::system_clock::time_point()>;

  explicit SubmissionService(std::shared_ptr<ISubmissionRepository> submissions,
                             int sourceSizeLimitKb = 64,
                             std::chrono::seconds submitInterval =
                                 std::chrono::seconds(0),
                             Clock clock = [] {
                               return std::chrono::system_clock::now();
                             });

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
  std::size_t recoverInterruptedSubmissions();
  std::size_t cleanupExpiredSources(std::chrono::hours retention);

 private:
  SubmissionResult failure(SubmissionError error) const;
  SubmissionListResult listFailure(SubmissionError error) const;

  std::shared_ptr<ISubmissionRepository> submissions_;
  int sourceSizeLimitKb_ = 64;
  std::chrono::seconds submitInterval_{0};
  Clock clock_;
  mutable std::mutex limiterMutex_;
  std::unordered_map<std::int64_t, std::chrono::system_clock::time_point>
      lastSubmissionByUser_;
};

std::string submissionErrorMessage(SubmissionError error);

}  // namespace shuati::judge
