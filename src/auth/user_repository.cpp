#include "shuati/auth/user_repository.h"

#include <algorithm>

namespace shuati::auth {

std::optional<UserRecord> InMemoryUserRepository::createUser(
    const std::string& username,
    const std::string& passwordHash,
    UserRole role) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (idsByUsername_.find(username) != idsByUsername_.end()) {
    return std::nullopt;
  }

  UserRecord user;
  user.id = nextId_++;
  user.username = username;
  user.passwordHash = passwordHash;
  user.role = role;
  user.createdAt = std::chrono::system_clock::now();

  idsByUsername_[username] = user.id;
  usersById_[user.id] = user;
  return user;
}

std::optional<UserRecord> InMemoryUserRepository::findByUsername(
    const std::string& username) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto idIt = idsByUsername_.find(username);
  if (idIt == idsByUsername_.end()) {
    return std::nullopt;
  }
  return usersById_.at(idIt->second);
}

std::optional<UserRecord> InMemoryUserRepository::findById(
    std::int64_t id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = usersById_.find(id);
  if (it == usersById_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<UserRecord> InMemoryUserRepository::updateRole(
    std::int64_t id,
    UserRole role) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = usersById_.find(id);
  if (it == usersById_.end()) {
    return std::nullopt;
  }
  it->second.role = role;
  return it->second;
}

std::vector<UserRecord> InMemoryUserRepository::listUsers() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<UserRecord> users;
  users.reserve(usersById_.size());
  for (const auto& [id, user] : usersById_) {
    (void)id;
    users.push_back(user);
  }
  std::sort(users.begin(), users.end(),
            [](const UserRecord& left, const UserRecord& right) {
              return left.id < right.id;
            });
  return users;
}

bool InMemoryUserRepository::hasSuperAdmin() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::any_of(usersById_.begin(), usersById_.end(),
                     [](const auto& item) {
                       return item.second.role == UserRole::SuperAdmin;
                     });
}

}  // namespace shuati::auth
