#include "shuati/auth/role.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace shuati::auth {
namespace {

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

}  // namespace

UserRole parseRole(const std::string& value) {
  const auto normalized = lower(value);
  if (normalized == "user") {
    return UserRole::User;
  }
  if (normalized == "admin") {
    return UserRole::Admin;
  }
  if (normalized == "super_admin" || normalized == "superadmin") {
    return UserRole::SuperAdmin;
  }
  throw std::invalid_argument("unknown user role: " + value);
}

std::string toString(UserRole role) {
  switch (role) {
    case UserRole::User:
      return "user";
    case UserRole::Admin:
      return "admin";
    case UserRole::SuperAdmin:
      return "super_admin";
  }
  return "user";
}

bool canAccessAdmin(UserRole role) {
  return role == UserRole::Admin || role == UserRole::SuperAdmin;
}

bool canManageRoles(UserRole role) {
  return role == UserRole::SuperAdmin;
}

}  // namespace shuati::auth
