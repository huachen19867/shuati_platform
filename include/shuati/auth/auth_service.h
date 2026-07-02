#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shuati/auth/password_hasher.h"
#include "shuati/auth/session_manager.h"
#include "shuati/auth/user_repository.h"

namespace shuati::auth {

enum class AuthError {
  None,
  InvalidInput,
  AlreadyExists,
  InvalidCredentials,
  Unauthorized,
  Forbidden,
  NotFound,
};

struct User {
  std::int64_t id = 0;
  std::string username;
  UserRole role = UserRole::User;
};

struct UserResult {
  bool ok = false;
  AuthError error = AuthError::None;
  std::string message;
  User user;
};

struct LoginResult : UserResult {
  std::string token;
  std::chrono::system_clock::time_point expiresAt{};
};

struct UserListResult {
  bool ok = false;
  AuthError error = AuthError::None;
  std::string message;
  std::vector<User> users;
};

class AuthService {
 public:
  AuthService(std::shared_ptr<IUserRepository> users,
              std::shared_ptr<Sha256PasswordHasher> passwordHasher,
              std::shared_ptr<SessionManager> sessions);

  UserResult bootstrapSuperAdmin(const std::string& username,
                                 const std::string& password);
  UserResult registerUser(const std::string& username,
                          const std::string& password);
  LoginResult login(const std::string& username, const std::string& password);
  UserResult logout(const std::string& token);
  UserResult currentUser(const std::string& token);
  UserListResult listUsers(const std::string& actorToken);
  UserResult updateUserRole(const std::string& actorToken,
                            std::int64_t targetUserId,
                            UserRole newRole);

 private:
  UserResult createUser(const std::string& username,
                        const std::string& password,
                        UserRole role);
  UserResult actorFromToken(const std::string& token);

  std::shared_ptr<IUserRepository> users_;
  std::shared_ptr<Sha256PasswordHasher> passwordHasher_;
  std::shared_ptr<SessionManager> sessions_;
};

std::string authErrorMessage(AuthError error);

}  // namespace shuati::auth
