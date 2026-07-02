#pragma once

#include <string>

namespace shuati::auth {

enum class UserRole {
  User = 0,
  Admin = 1,
  SuperAdmin = 2,
};

UserRole parseRole(const std::string& value);
std::string toString(UserRole role);
bool canAccessAdmin(UserRole role);
bool canManageRoles(UserRole role);

}  // namespace shuati::auth
