#include "shuati/auth/role.h"

#include <gtest/gtest.h>

#include <stdexcept>

TEST(RoleTest, ParsesAndSerializesRoles) {
  EXPECT_EQ(shuati::auth::parseRole("user"), shuati::auth::UserRole::User);
  EXPECT_EQ(shuati::auth::parseRole("admin"), shuati::auth::UserRole::Admin);
  EXPECT_EQ(shuati::auth::parseRole("super_admin"),
            shuati::auth::UserRole::SuperAdmin);

  EXPECT_EQ(shuati::auth::toString(shuati::auth::UserRole::User), "user");
  EXPECT_EQ(shuati::auth::toString(shuati::auth::UserRole::Admin), "admin");
  EXPECT_EQ(shuati::auth::toString(shuati::auth::UserRole::SuperAdmin),
            "super_admin");
}

TEST(RoleTest, RejectsUnknownRole) {
  EXPECT_THROW(shuati::auth::parseRole("root"), std::invalid_argument);
}

TEST(RoleTest, SeparatesAdminAccessFromSuperAdminRoleManagement) {
  EXPECT_FALSE(shuati::auth::canAccessAdmin(shuati::auth::UserRole::User));
  EXPECT_TRUE(shuati::auth::canAccessAdmin(shuati::auth::UserRole::Admin));
  EXPECT_TRUE(
      shuati::auth::canAccessAdmin(shuati::auth::UserRole::SuperAdmin));

  EXPECT_FALSE(shuati::auth::canManageRoles(shuati::auth::UserRole::User));
  EXPECT_FALSE(shuati::auth::canManageRoles(shuati::auth::UserRole::Admin));
  EXPECT_TRUE(
      shuati::auth::canManageRoles(shuati::auth::UserRole::SuperAdmin));
}
