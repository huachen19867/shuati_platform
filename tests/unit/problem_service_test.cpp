#include "shuati/problem/problem_service.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

shuati::problem::ProblemDraft sumDraft() {
  shuati::problem::ProblemDraft draft;
  draft.title = "Two Sum";
  draft.statement = "Read two integers and print their sum.";
  draft.inputDescription = "Two integers a and b.";
  draft.outputDescription = "The sum of a and b.";
  draft.samplesJson = R"([{"input":"1 2\n","output":"3\n"}])";
  draft.difficulty = shuati::problem::Difficulty::Easy;
  draft.tags = {"math", "intro"};
  return draft;
}

shuati::problem::Actor adminActor() {
  return {1, shuati::auth::UserRole::Admin};
}

shuati::problem::Actor userActor() {
  return {2, shuati::auth::UserRole::User};
}

}  // namespace

TEST(ProblemServiceTest, AdminCanCreateAndUpdateProblem) {
  auto repository = std::make_shared<shuati::problem::InMemoryProblemRepository>();
  shuati::problem::ProblemService service(repository);

  const auto created = service.createProblem(adminActor(), sumDraft());
  ASSERT_TRUE(created.ok);
  EXPECT_EQ(created.problem.id, 1);
  EXPECT_EQ(created.problem.title, "Two Sum");
  EXPECT_EQ(created.problem.createdBy, 1);

  auto updatedDraft = sumDraft();
  updatedDraft.title = "A Plus B";
  updatedDraft.tags = {"math"};
  const auto updated =
      service.updateProblem(adminActor(), created.problem.id, updatedDraft);

  ASSERT_TRUE(updated.ok);
  EXPECT_EQ(updated.problem.title, "A Plus B");
  EXPECT_EQ(updated.problem.tags.size(), 1U);
  EXPECT_EQ(updated.problem.tags[0], "math");
}

TEST(ProblemServiceTest, NormalUserCannotManageProblems) {
  auto repository = std::make_shared<shuati::problem::InMemoryProblemRepository>();
  shuati::problem::ProblemService service(repository);

  const auto result = service.createProblem(userActor(), sumDraft());

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, shuati::problem::ProblemError::Forbidden);
}

TEST(ProblemServiceTest, ListProblemsFiltersByKeywordDifficultyAndTag) {
  auto repository = std::make_shared<shuati::problem::InMemoryProblemRepository>();
  shuati::problem::ProblemService service(repository);
  ASSERT_TRUE(service.createProblem(adminActor(), sumDraft()).ok);

  auto graphDraft = sumDraft();
  graphDraft.title = "Shortest Path";
  graphDraft.difficulty = shuati::problem::Difficulty::Medium;
  graphDraft.tags = {"graph"};
  ASSERT_TRUE(service.createProblem(adminActor(), graphDraft).ok);

  shuati::problem::ProblemFilter filter;
  filter.keyword = "sum";
  filter.difficulty = shuati::problem::Difficulty::Easy;
  filter.tag = "math";
  const auto result = service.listProblems(filter);

  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.problems.size(), 1U);
  EXPECT_EQ(result.problems[0].title, "Two Sum");
}

TEST(ProblemServiceTest, DetailReturnsNotFoundForMissingProblem) {
  auto repository = std::make_shared<shuati::problem::InMemoryProblemRepository>();
  shuati::problem::ProblemService service(repository);

  const auto result = service.getProblem(404);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, shuati::problem::ProblemError::NotFound);
}
