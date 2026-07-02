#include "shuati/judge/submission_service.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace {

shuati::judge::Actor user(std::int64_t id) {
  return {id, shuati::auth::UserRole::User};
}

shuati::judge::Actor admin(std::int64_t id) {
  return {id, shuati::auth::UserRole::Admin};
}

shuati::judge::JudgeRunResult acceptedResult() {
  shuati::judge::JudgeRunResult result;
  result.status = shuati::judge::SubmissionStatus::Accepted;
  result.totalTimeMs = 12;
  result.maxMemoryKb = 0;
  result.cases.push_back({1, shuati::judge::SubmissionStatus::Accepted, 12, 0,
                          "", "ok", ""});
  return result;
}

}  // namespace

TEST(SubmissionServiceTest, CreatesPendingCppSubmission) {
  auto repository =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>();
  shuati::judge::SubmissionService service(repository);

  const auto result =
      service.createSubmission(user(5), 9, "cpp", "int main(){return 0;}");

  ASSERT_TRUE(result.ok);
  EXPECT_EQ(result.submission.id, 1);
  EXPECT_EQ(result.submission.userId, 5);
  EXPECT_EQ(result.submission.problemId, 9);
  EXPECT_EQ(result.submission.status,
            shuati::judge::SubmissionStatus::Pending);
}

TEST(SubmissionServiceTest, ClaimsOnlyOnePendingSubmission) {
  auto repository =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>();
  shuati::judge::SubmissionService service(repository);
  ASSERT_TRUE(service.createSubmission(user(5), 9, "cpp", "int main(){}").ok);

  const auto first = service.claimNextPending("worker-1");
  const auto second = service.claimNextPending("worker-2");

  ASSERT_TRUE(first.ok);
  EXPECT_EQ(first.submission.status,
            shuati::judge::SubmissionStatus::Running);
  EXPECT_FALSE(second.ok);
  EXPECT_EQ(second.error, shuati::judge::SubmissionError::NoPendingTask);
}

TEST(SubmissionServiceTest, OwnerOrAdminCanViewSubmission) {
  auto repository =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>();
  shuati::judge::SubmissionService service(repository);
  const auto created =
      service.createSubmission(user(5), 9, "cpp", "int main(){}");
  ASSERT_TRUE(created.ok);

  const auto owner = service.getSubmission(user(5), created.submission.id);
  const auto other = service.getSubmission(user(6), created.submission.id);
  const auto byAdmin = service.getSubmission(admin(7), created.submission.id);

  EXPECT_TRUE(owner.ok);
  EXPECT_FALSE(other.ok);
  EXPECT_EQ(other.error, shuati::judge::SubmissionError::Forbidden);
  EXPECT_TRUE(byAdmin.ok);
}

TEST(SubmissionServiceTest, CompletesSubmissionWithJudgeResult) {
  auto repository =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>();
  shuati::judge::SubmissionService service(repository);
  const auto created =
      service.createSubmission(user(5), 9, "cpp", "int main(){}");
  ASSERT_TRUE(created.ok);
  ASSERT_TRUE(service.claimNextPending("worker-1").ok);

  const auto completed =
      service.completeSubmission(created.submission.id, acceptedResult());

  ASSERT_TRUE(completed.ok);
  EXPECT_EQ(completed.submission.status,
            shuati::judge::SubmissionStatus::Accepted);
  EXPECT_EQ(completed.submission.totalTimeMs, 12);
  ASSERT_EQ(completed.submission.cases.size(), 1U);
  EXPECT_EQ(completed.submission.cases[0].caseIndex, 1);
}

TEST(SubmissionServiceTest, RejectsTooFrequentSubmissionsBySameUser) {
  std::chrono::system_clock::time_point now{100s};
  auto repository =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>(
          [&now] { return now; });
  shuati::judge::SubmissionService service(
      repository, 64, 5s, [&now] { return now; });

  const auto first =
      service.createSubmission(user(5), 9, "cpp", "int main(){}");
  const auto second =
      service.createSubmission(user(5), 9, "cpp", "int main(){}");
  now += 6s;
  const auto third =
      service.createSubmission(user(5), 9, "cpp", "int main(){}");

  EXPECT_TRUE(first.ok);
  EXPECT_FALSE(second.ok);
  EXPECT_EQ(second.error, shuati::judge::SubmissionError::RateLimited);
  EXPECT_TRUE(third.ok);
}

TEST(SubmissionServiceTest, RecoversInterruptedSubmissionsToPending) {
  auto repository =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>();
  shuati::judge::SubmissionService service(repository);
  const auto created =
      service.createSubmission(user(5), 9, "cpp", "int main(){}");
  ASSERT_TRUE(created.ok);
  ASSERT_TRUE(service.claimNextPending("worker-1").ok);

  const auto recovered = service.recoverInterruptedSubmissions();
  const auto detail = service.getSubmission(user(5), created.submission.id);

  EXPECT_EQ(recovered, 1U);
  ASSERT_TRUE(detail.ok);
  EXPECT_EQ(detail.submission.status, shuati::judge::SubmissionStatus::Pending);
}

TEST(SubmissionServiceTest, CleansExpiredSourceButKeepsMetadata) {
  std::chrono::system_clock::time_point now{100s};
  auto repository =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>(
          [&now] { return now; });
  shuati::judge::SubmissionService service(
      repository, 64, 0s, [&now] { return now; });
  const auto created =
      service.createSubmission(user(5), 9, "cpp", "int main(){}");
  ASSERT_TRUE(created.ok);

  now += 25h;
  const auto cleaned = service.cleanupExpiredSources(24h);
  const auto detail = service.getSubmission(user(5), created.submission.id);

  EXPECT_EQ(cleaned, 1U);
  ASSERT_TRUE(detail.ok);
  EXPECT_TRUE(detail.submission.source.empty());
  EXPECT_NE(detail.submission.sourceDeletedAt.time_since_epoch().count(), 0);
  EXPECT_EQ(detail.submission.problemId, 9);
}
