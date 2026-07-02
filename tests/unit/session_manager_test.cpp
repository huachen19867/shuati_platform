#include "shuati/auth/session_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "shuati/auth/role.h"

using namespace std::chrono_literals;

TEST(SessionManagerTest, CreatedSessionCanBeResolvedBeforeExpiry) {
  auto now = std::chrono::system_clock::time_point{100s};
  shuati::auth::SessionManager sessions(
      10s, [&now] { return now; }, [] { return std::string("token-1"); });

  const auto created = sessions.createSession(42, shuati::auth::UserRole::Admin);
  const auto resolved = sessions.findSession(created.token);

  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->userId, 42);
  EXPECT_EQ(resolved->role, shuati::auth::UserRole::Admin);
  EXPECT_EQ(created.expiresAt, now + 10s);
}

TEST(SessionManagerTest, ExpiredSessionIsRejectedAndRemoved) {
  auto now = std::chrono::system_clock::time_point{100s};
  shuati::auth::SessionManager sessions(
      5s, [&now] { return now; }, [] { return std::string("token-2"); });

  const auto created = sessions.createSession(7, shuati::auth::UserRole::User);
  now += 6s;

  EXPECT_FALSE(sessions.findSession(created.token).has_value());
  EXPECT_EQ(sessions.activeCount(), 0U);
}

TEST(SessionManagerTest, RevokedSessionCannotBeResolved) {
  auto now = std::chrono::system_clock::time_point{100s};
  shuati::auth::SessionManager sessions(
      5s, [&now] { return now; }, [] { return std::string("token-3"); });

  const auto created =
      sessions.createSession(9, shuati::auth::UserRole::SuperAdmin);

  EXPECT_TRUE(sessions.revoke(created.token));
  EXPECT_FALSE(sessions.findSession(created.token).has_value());
}
