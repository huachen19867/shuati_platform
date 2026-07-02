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

namespace shuati::problem {

enum class Difficulty {
  Easy,
  Medium,
  Hard,
};

enum class ProblemError {
  None,
  InvalidInput,
  Forbidden,
  NotFound,
};

struct Actor {
  std::int64_t userId = 0;
  shuati::auth::UserRole role = shuati::auth::UserRole::User;
};

struct ProblemDraft {
  std::string title;
  std::string statement;
  std::string inputDescription;
  std::string outputDescription;
  std::string samplesJson = "[]";
  Difficulty difficulty = Difficulty::Easy;
  std::vector<std::string> tags;
};

struct Problem {
  std::int64_t id = 0;
  std::string title;
  std::string statement;
  std::string inputDescription;
  std::string outputDescription;
  std::string samplesJson = "[]";
  Difficulty difficulty = Difficulty::Easy;
  std::vector<std::string> tags;
  std::int64_t createdBy = 0;
  std::chrono::system_clock::time_point createdAt{};
  std::chrono::system_clock::time_point updatedAt{};
};

struct ProblemFilter {
  std::string keyword;
  std::optional<Difficulty> difficulty;
  std::string tag;
};

struct ProblemResult {
  bool ok = false;
  ProblemError error = ProblemError::None;
  std::string message;
  Problem problem;
};

struct ProblemListResult {
  bool ok = false;
  ProblemError error = ProblemError::None;
  std::string message;
  std::vector<Problem> problems;
};

class IProblemRepository {
 public:
  virtual ~IProblemRepository() = default;

  virtual Problem createProblem(const ProblemDraft& draft,
                                std::int64_t createdBy) = 0;
  virtual std::optional<Problem> updateProblem(std::int64_t id,
                                               const ProblemDraft& draft) = 0;
  virtual std::optional<Problem> findById(std::int64_t id) const = 0;
  virtual std::vector<Problem> listProblems() const = 0;
};

class InMemoryProblemRepository : public IProblemRepository {
 public:
  Problem createProblem(const ProblemDraft& draft,
                        std::int64_t createdBy) override;
  std::optional<Problem> updateProblem(std::int64_t id,
                                       const ProblemDraft& draft) override;
  std::optional<Problem> findById(std::int64_t id) const override;
  std::vector<Problem> listProblems() const override;

 private:
  mutable std::mutex mutex_;
  std::int64_t nextId_ = 1;
  std::unordered_map<std::int64_t, Problem> problemsById_;
};

class ProblemService {
 public:
  explicit ProblemService(std::shared_ptr<IProblemRepository> problems);

  ProblemResult createProblem(const Actor& actor, const ProblemDraft& draft);
  ProblemResult updateProblem(const Actor& actor,
                              std::int64_t problemId,
                              const ProblemDraft& draft);
  ProblemResult getProblem(std::int64_t problemId) const;
  ProblemListResult listProblems(const ProblemFilter& filter) const;

 private:
  ProblemResult failure(ProblemError error) const;
  bool isValidDraft(const ProblemDraft& draft) const;

  std::shared_ptr<IProblemRepository> problems_;
};

std::string toString(Difficulty difficulty);
std::optional<Difficulty> parseDifficulty(const std::string& value);
std::string problemErrorMessage(ProblemError error);

}  // namespace shuati::problem
