#include "shuati/auth/password_hasher.h"

#include <gtest/gtest.h>

#include "shuati/common/crypto.h"

TEST(PasswordHasherTest, HashesNewPasswordsWithBcrypt) {
  shuati::auth::PasswordHasher hasher([] {
    return std::string("0123456789abcdef");
  });

  const auto hash = hasher.hashPassword("password123");

  EXPECT_EQ(hash.rfind("$2b$", 0), 0U);
  EXPECT_TRUE(hasher.verifyPassword("password123", hash));
  EXPECT_FALSE(hasher.verifyPassword("wrong-password", hash));
}

TEST(PasswordHasherTest, VerifiesLegacySha256HashesForMigration) {
  const auto digest =
      shuati::common::sha256Hex(std::string("salt") + ":password123");
  const auto legacy = std::string("sha256$salt$") + digest;
  shuati::auth::PasswordHasher hasher([] {
    return std::string("0123456789abcdef");
  });

  EXPECT_TRUE(hasher.verifyPassword("password123", legacy));
  EXPECT_FALSE(hasher.verifyPassword("wrong-password", legacy));
}
