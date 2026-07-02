#include "shuati/auth/auth_service.h"

#include <algorithm>
#include <cctype>

namespace shuati::auth {
namespace {

User toPublicUser(const UserRecord& record) {
  return User{record.id, record.username, record.role};
}

UserResult failure(AuthError error) {
  return UserResult{false, error, authErrorMessage(error), User{}};
}

LoginResult loginFailure(AuthError error) {
  LoginResult result;
  result.ok = false;
  result.error = error;
  result.message = authErrorMessage(error);
  return result;
}

UserListResult listFailure(AuthError error) {
  UserListResult result;
  result.ok = false;
  result.error = error;
  result.message = authErrorMessage(error);
  return result;
}

bool validUsername(const std::string& username) {
  if (username.size() < 3 || username.size() > 32) {
    return false;
  }
  return std::all_of(username.begin(), username.end(),
                     [](unsigned char ch) {
                       return std::isalnum(ch) || ch == '_';
                     });
}

bool validPassword(const std::string& password) {
  return password.size() >= 8 && password.size() <= 128;
}

}  // namespace

AuthService::AuthService(
    std::shared_ptr<IUserRepository> users,
    std::shared_ptr<Sha256PasswordHasher> passwordHasher,
    std::shared_ptr<SessionManager> sessions)
    : users_(std::move(users)),
      passwordHasher_(std::move(passwordHasher)),
      sessions_(std::move(sessions)) {}

UserResult AuthService::bootstrapSuperAdmin(const std::string& username,
                                            const std::string& password) {
  if (users_->hasSuperAdmin()) {
    return failure(AuthError::AlreadyExists);
  }
  return createUser(username, password, UserRole::SuperAdmin);
}

UserResult AuthService::registerUser(const std::string& username,
                                     const std::string& password) {
  return createUser(username, password, UserRole::User);
}

LoginResult AuthService::login(const std::string& username,
                               const std::string& password) {
  const auto found = users_->findByUsername(username);
  if (!found.has_value() ||
      !passwordHasher_->verifyPassword(password, found->passwordHash)) {
    return loginFailure(AuthError::InvalidCredentials);
  }

  const auto session = sessions_->createSession(found->id, found->role);
  LoginResult result;
  result.ok = true;
  result.error = AuthError::None;
  result.message = "ok";
  result.user = toPublicUser(*found);
  result.token = session.token;
  result.expiresAt = session.expiresAt;
  return result;
}

UserResult AuthService::logout(const std::string& token) {
  if (token.empty()) {
    return failure(AuthError::Unauthorized);
  }
  sessions_->revoke(token);
  UserResult result;
  result.ok = true;
  result.message = "ok";
  return result;
}

UserResult AuthService::currentUser(const std::string& token) {
  return actorFromToken(token);
}

UserListResult AuthService::listUsers(const std::string& actorToken) {
  const auto actor = actorFromToken(actorToken);
  if (!actor.ok) {
    return listFailure(actor.error);
  }
  if (!canAccessAdmin(actor.user.role)) {
    return listFailure(AuthError::Forbidden);
  }

  UserListResult result;
  result.ok = true;
  result.message = "ok";
  for (const auto& user : users_->listUsers()) {
    result.users.push_back(toPublicUser(user));
  }
  return result;
}

UserResult AuthService::updateUserRole(const std::string& actorToken,
                                       std::int64_t targetUserId,
                                       UserRole newRole) {
  const auto actor = actorFromToken(actorToken);
  if (!actor.ok) {
    return actor;
  }
  if (!canManageRoles(actor.user.role)) {
    return failure(AuthError::Forbidden);
  }
  if (actor.user.id == targetUserId && newRole != UserRole::SuperAdmin) {
    return failure(AuthError::Forbidden);
  }

  const auto updated = users_->updateRole(targetUserId, newRole);
  if (!updated.has_value()) {
    return failure(AuthError::NotFound);
  }
  return UserResult{true, AuthError::None, "ok", toPublicUser(*updated)};
}

UserResult AuthService::createUser(const std::string& username,
                                   const std::string& password,
                                   UserRole role) {
  if (!validUsername(username) || !validPassword(password)) {
    return failure(AuthError::InvalidInput);
  }
  const auto passwordHash = passwordHasher_->hashPassword(password);
  const auto created = users_->createUser(username, passwordHash, role);
  if (!created.has_value()) {
    return failure(AuthError::AlreadyExists);
  }
  return UserResult{true, AuthError::None, "ok", toPublicUser(*created)};
}

UserResult AuthService::actorFromToken(const std::string& token) {
  if (token.empty()) {
    return failure(AuthError::Unauthorized);
  }
  const auto session = sessions_->findSession(token);
  if (!session.has_value()) {
    return failure(AuthError::Unauthorized);
  }
  const auto found = users_->findById(session->userId);
  if (!found.has_value()) {
    return failure(AuthError::Unauthorized);
  }
  return UserResult{true, AuthError::None, "ok", toPublicUser(*found)};
}

std::string authErrorMessage(AuthError error) {
  switch (error) {
    case AuthError::None:
      return "ok";
    case AuthError::InvalidInput:
      return "invalid username or password";
    case AuthError::AlreadyExists:
      return "user already exists";
    case AuthError::InvalidCredentials:
      return "invalid username or password";
    case AuthError::Unauthorized:
      return "unauthorized or session expired";
    case AuthError::Forbidden:
      return "forbidden";
    case AuthError::NotFound:
      return "user not found";
  }
  return "unknown auth error";
}

}  // namespace shuati::auth
