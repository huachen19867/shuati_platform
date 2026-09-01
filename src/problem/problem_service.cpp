#include "shuati/problem/problem_service.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include "shuati/common/state_file.h"

namespace shuati::problem {
namespace {

std::string lowerCopy(const std::string& value) {
  std::string lowered = value;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return lowered;
}

bool containsCaseInsensitive(const std::string& text,
                             const std::string& keyword) {
  if (keyword.empty()) {
    return true;
  }
  return lowerCopy(text).find(lowerCopy(keyword)) != std::string::npos;
}

bool hasTag(const std::vector<std::string>& tags, const std::string& tag) {
  if (tag.empty()) {
    return true;
  }
  return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

Problem fromDraft(const ProblemDraft& draft, std::int64_t id,
                  std::int64_t createdBy,
                  std::chrono::system_clock::time_point createdAt) {
  Problem problem;
  problem.id = id;
  problem.title = draft.title;
  problem.statement = draft.statement;
  problem.inputDescription = draft.inputDescription;
  problem.outputDescription = draft.outputDescription;
  problem.samplesJson = draft.samplesJson;
  problem.difficulty = draft.difficulty;
  problem.tags = draft.tags;
  problem.createdBy = createdBy;
  problem.createdAt = createdAt;
  problem.updatedAt = std::chrono::system_clock::now();
  return problem;
}

}  // namespace

InMemoryProblemRepository::InMemoryProblemRepository(
    std::filesystem::path persistencePath)
    : persistencePath_(std::move(persistencePath)) {
  load();
}

void InMemoryProblemRepository::load() {
  if (persistencePath_.empty()) return;
  const auto content = shuati::common::readStateFile(persistencePath_);
  if (content.empty()) return;
  std::istringstream input(content);
  std::string line;
  std::getline(input, line);
  if (line != "SHUATI_PROBLEMS_V1") {
    throw std::runtime_error("unsupported problems state format");
  }
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto fields = shuati::common::splitStateLine(line);
    if (fields.size() < 11) {
      throw std::runtime_error("corrupt problems state record");
    }
    const auto difficulty = parseDifficulty(fields[4]);
    const auto tagCount = static_cast<std::size_t>(std::stoull(fields[10]));
    if (!difficulty.has_value() || fields.size() != 11 + tagCount) {
      throw std::runtime_error("corrupt problems state fields");
    }
    Problem problem;
    problem.id = std::stoll(fields[0]);
    problem.createdAt = shuati::common::timeFromEpochMilliseconds(
        std::stoll(fields[1]));
    problem.updatedAt = shuati::common::timeFromEpochMilliseconds(
        std::stoll(fields[2]));
    problem.createdBy = std::stoll(fields[3]);
    problem.difficulty = *difficulty;
    problem.title = shuati::common::hexDecode(fields[5]);
    problem.statement = shuati::common::hexDecode(fields[6]);
    problem.inputDescription = shuati::common::hexDecode(fields[7]);
    problem.outputDescription = shuati::common::hexDecode(fields[8]);
    problem.samplesJson = shuati::common::hexDecode(fields[9]);
    for (std::size_t i = 0; i < tagCount; ++i) {
      problem.tags.push_back(shuati::common::hexDecode(fields[11 + i]));
    }
    problemsById_[problem.id] = problem;
    nextId_ = std::max(nextId_, problem.id + 1);
  }
}

void InMemoryProblemRepository::persistLocked() const {
  if (persistencePath_.empty()) return;
  std::vector<Problem> problems;
  problems.reserve(problemsById_.size());
  for (const auto& item : problemsById_) problems.push_back(item.second);
  std::sort(problems.begin(), problems.end(), [](const auto& left, const auto& right) {
    return left.id < right.id;
  });
  std::ostringstream output;
  output << "SHUATI_PROBLEMS_V1\n";
  for (const auto& problem : problems) {
    output << problem.id << '\t'
           << shuati::common::epochMilliseconds(problem.createdAt) << '\t'
           << shuati::common::epochMilliseconds(problem.updatedAt) << '\t'
           << problem.createdBy << '\t' << toString(problem.difficulty) << '\t'
           << shuati::common::hexEncode(problem.title) << '\t'
           << shuati::common::hexEncode(problem.statement) << '\t'
           << shuati::common::hexEncode(problem.inputDescription) << '\t'
           << shuati::common::hexEncode(problem.outputDescription) << '\t'
           << shuati::common::hexEncode(problem.samplesJson) << '\t'
           << problem.tags.size();
    for (const auto& tag : problem.tags) {
      output << '\t' << shuati::common::hexEncode(tag);
    }
    output << '\n';
  }
  shuati::common::atomicWriteStateFile(persistencePath_, output.str());
}

Problem InMemoryProblemRepository::createProblem(const ProblemDraft& draft,
                                                 std::int64_t createdBy) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = std::chrono::system_clock::now();
  auto problem = fromDraft(draft, nextId_++, createdBy, now);
  problem.updatedAt = now;
  problemsById_[problem.id] = problem;
  persistLocked();
  return problem;
}

std::optional<Problem> InMemoryProblemRepository::updateProblem(
    std::int64_t id,
    const ProblemDraft& draft) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = problemsById_.find(id);
  if (it == problemsById_.end()) {
    return std::nullopt;
  }

  const auto createdBy = it->second.createdBy;
  const auto createdAt = it->second.createdAt;
  auto updated = fromDraft(draft, id, createdBy, createdAt);
  updated.updatedAt = std::chrono::system_clock::now();
  it->second = updated;
  persistLocked();
  return updated;
}

std::optional<Problem> InMemoryProblemRepository::findById(
    std::int64_t id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = problemsById_.find(id);
  if (it == problemsById_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<Problem> InMemoryProblemRepository::listProblems() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Problem> problems;
  problems.reserve(problemsById_.size());
  for (const auto& [id, problem] : problemsById_) {
    (void)id;
    problems.push_back(problem);
  }
  std::sort(problems.begin(), problems.end(),
            [](const Problem& left, const Problem& right) {
              return left.id < right.id;
            });
  return problems;
}

ProblemService::ProblemService(std::shared_ptr<IProblemRepository> problems)
    : problems_(std::move(problems)) {}

ProblemResult ProblemService::createProblem(const Actor& actor,
                                            const ProblemDraft& draft) {
  if (!shuati::auth::canAccessAdmin(actor.role)) {
    return failure(ProblemError::Forbidden);
  }
  if (!isValidDraft(draft)) {
    return failure(ProblemError::InvalidInput);
  }
  return ProblemResult{true, ProblemError::None, "ok",
                       problems_->createProblem(draft, actor.userId)};
}

ProblemResult ProblemService::updateProblem(const Actor& actor,
                                            std::int64_t problemId,
                                            const ProblemDraft& draft) {
  if (!shuati::auth::canAccessAdmin(actor.role)) {
    return failure(ProblemError::Forbidden);
  }
  if (!isValidDraft(draft) || problemId <= 0) {
    return failure(ProblemError::InvalidInput);
  }
  const auto updated = problems_->updateProblem(problemId, draft);
  if (!updated.has_value()) {
    return failure(ProblemError::NotFound);
  }
  return ProblemResult{true, ProblemError::None, "ok", *updated};
}

ProblemResult ProblemService::getProblem(std::int64_t problemId) const {
  if (problemId <= 0) {
    return failure(ProblemError::InvalidInput);
  }
  const auto found = problems_->findById(problemId);
  if (!found.has_value()) {
    return failure(ProblemError::NotFound);
  }
  return ProblemResult{true, ProblemError::None, "ok", *found};
}

ProblemListResult ProblemService::listProblems(
    const ProblemFilter& filter) const {
  ProblemListResult result;
  result.ok = true;
  result.message = "ok";

  for (const auto& problem : problems_->listProblems()) {
    if (!containsCaseInsensitive(problem.title, filter.keyword)) {
      continue;
    }
    if (filter.difficulty.has_value() &&
        problem.difficulty != *filter.difficulty) {
      continue;
    }
    if (!hasTag(problem.tags, filter.tag)) {
      continue;
    }
    result.problems.push_back(problem);
  }
  return result;
}

ProblemResult ProblemService::failure(ProblemError error) const {
  return ProblemResult{false, error, problemErrorMessage(error), Problem{}};
}

bool ProblemService::isValidDraft(const ProblemDraft& draft) const {
  return !draft.title.empty() && draft.title.size() <= 128 &&
         !draft.statement.empty() && !draft.inputDescription.empty() &&
         !draft.outputDescription.empty();
}

std::string toString(Difficulty difficulty) {
  switch (difficulty) {
    case Difficulty::Easy:
      return "easy";
    case Difficulty::Medium:
      return "medium";
    case Difficulty::Hard:
      return "hard";
  }
  return "easy";
}

std::optional<Difficulty> parseDifficulty(const std::string& value) {
  const auto lowered = lowerCopy(value);
  if (lowered == "easy") {
    return Difficulty::Easy;
  }
  if (lowered == "medium") {
    return Difficulty::Medium;
  }
  if (lowered == "hard") {
    return Difficulty::Hard;
  }
  return std::nullopt;
}

std::string problemErrorMessage(ProblemError error) {
  switch (error) {
    case ProblemError::None:
      return "ok";
    case ProblemError::InvalidInput:
      return "invalid problem input";
    case ProblemError::Forbidden:
      return "forbidden";
    case ProblemError::NotFound:
      return "problem not found";
  }
  return "unknown problem error";
}

}  // namespace shuati::problem
