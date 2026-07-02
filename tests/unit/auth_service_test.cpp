#include "shuati/auth/auth_service.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include "shuati/auth/password_hasher.h"
#include "shuati/auth/session_manager.h"
#include "shuati/auth/user_repository.h"

using namespace std::chrono_literals;

namespace {

struct AuthHarness {
  std::chrono::system_clock::time_point now{100s};
  std::shared_ptr<shuati::auth::InMemoryUserRepository> users =
      std::make_shared<shuati::auth::InMemoryUserRepository>();
  std::shared_ptr<shuati::auth::Sha256PasswordHasher> hasher =
      std::make_shared<shuati::auth::Sha256PasswordHasher>(
          [] { return std::string("salt"); });
  int nextToken = 0;
  std::shared_ptr<shuati::auth::SessionManager> sessions =
      std::make_shared<shuati::auth::SessionManager>(
          1h, [this] { return now; }, [this] {
            ++nextToken;
            return std::string("token-") + std::to_string(nextToken);
          });
  shuati::auth::AuthService service{users, hasher, sessions};
};

}  // namespace

TEST(AuthServiceTest, BootstrapsSuperAdminOnlyOnce) {
  AuthHarness h;

  const auto first = h.service.bootstrapSuperAdmin("root", "secret123");
  const auto second = h.service.bootstrapSuperAdmin("root", "secret123");

  ASSERT_TRUE(first.ok);
  EXPECT_EQ(first.user.role, shuati::auth::UserRole::SuperAdmin);
  EXPECT_FALSE(second.ok);
  EXPECT_EQ(second.error, shuati::auth::AuthError::AlreadyExists);
}

TEST(AuthServiceTest, RegistersUserAndRejectsDuplicateUsername) {
  AuthHarness h;

  const auto first = h.service.registerUser("alice", "password123");
  const auto second = h.service.registerUser("alice", "password123");

  ASSERT_TRUE(first.ok);
  EXPECT_EQ(first.user.username, "alice");
  EXPECT_EQ(first.user.role, shuati::auth::UserRole::User);
  EXPECT_FALSE(second.ok);
  EXPECT_EQ(second.error, shuati::auth::AuthError::AlreadyExists);
}

TEST(AuthServiceTest, RejectsShortPasswords) {
  AuthHarness h;

  const auto result = h.service.registerUser("alice", "short");

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, shuati::auth::AuthError::InvalidInput);
}

TEST(AuthServiceTest, LoginCreatesSessionAndBadPasswordFails) {
  AuthHarness h;
  ASSERT_TRUE(h.service.registerUser("alice", "password123").ok);

  const auto bad = h.service.login("alice", "wrong");
  const auto good = h.service.login("alice", "password123");

  EXPECT_FALSE(bad.ok);
  EXPECT_EQ(bad.error, shuati::auth::AuthError::InvalidCredentials);
  ASSERT_TRUE(good.ok);
  EXPECT_FALSE(good.token.empty());
  EXPECT_EQ(good.user.username, "alice");
}

TEST(AuthServiceTest, ExpiredLoginSessionCannotAccessCurrentUser) {
  AuthHarness h;
  h.sessions = std::make_shared<shuati::auth::SessionManager>(
      5s, [&h] { return h.now; }, [] { return std::string("short-token"); });
  h.service = shuati::auth::AuthService{h.users, h.hasher, h.sessions};
  ASSERT_TRUE(h.service.registerUser("alice", "password123").ok);
  const auto login = h.service.login("alice", "password123");
  ASSERT_TRUE(login.ok);

  h.now += 6s;

  const auto current = h.service.currentUser(login.token);
  EXPECT_FALSE(current.ok);
  EXPECT_EQ(current.error, shuati::auth::AuthError::Unauthorized);
}

TEST(AuthServiceTest, LogoutRevokesSession) {
  AuthHarness h;
  ASSERT_TRUE(h.service.registerUser("alice", "password123").ok);
  const auto login = h.service.login("alice", "password123");
  ASSERT_TRUE(login.ok);

  EXPECT_TRUE(h.service.logout(login.token).ok);
  EXPECT_FALSE(h.service.currentUser(login.token).ok);
}

TEST(AuthServiceTest, OnlySuperAdminCanChangeUserRoles) {
  AuthHarness h;
  const auto root = h.service.bootstrapSuperAdmin("root", "secret123");
  const auto alice = h.service.registerUser("alice", "password123");
  ASSERT_TRUE(root.ok);
  ASSERT_TRUE(alice.ok);
  const auto rootLogin = h.service.login("root", "secret123");
  const auto aliceLogin = h.service.login("alice", "password123");
  ASSERT_TRUE(rootLogin.ok);
  ASSERT_TRUE(aliceLogin.ok);

  const auto forbidden = h.service.updateUserRole(
      aliceLogin.token, alice.user.id, shuati::auth::UserRole::Admin);
  const auto promoted = h.service.updateUserRole(
      rootLogin.token, alice.user.id, shuati::auth::UserRole::Admin);

  EXPECT_FALSE(forbidden.ok);
  EXPECT_EQ(forbidden.error, shuati::auth::AuthError::Forbidden);
  ASSERT_TRUE(promoted.ok);
  EXPECT_EQ(promoted.user.role, shuati::auth::UserRole::Admin);
}

TEST(AuthServiceTest, SuperAdminCannotDemoteSelf) {
  AuthHarness h;
  const auto root = h.service.bootstrapSuperAdmin("root", "secret123");
  ASSERT_TRUE(root.ok);
  const auto rootLogin = h.service.login("root", "secret123");
  ASSERT_TRUE(rootLogin.ok);

  const auto result = h.service.updateUserRole(
      rootLogin.token, root.user.id, shuati::auth::UserRole::Admin);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, shuati::auth::AuthError::Forbidden);
}
