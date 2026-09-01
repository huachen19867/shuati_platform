#include "shuati/auth/user_repository.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "shuati/common/state_file.h"

namespace shuati::auth {

InMemoryUserRepository::InMemoryUserRepository(
    std::filesystem::path persistencePath)
    : persistencePath_(std::move(persistencePath)) {
  load();
}

void InMemoryUserRepository::load() {
  if (persistencePath_.empty()) return;
  const auto content = shuati::common::readStateFile(persistencePath_);
  if (content.empty()) return;
  std::istringstream input(content);
  std::string line;
  std::getline(input, line);
  if (line != "SHUATI_USERS_V1") {
    throw std::runtime_error("unsupported users state format");
  }
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto fields = shuati::common::splitStateLine(line);
    if (fields.size() != 5) {
      throw std::runtime_error("corrupt users state record");
    }
    UserRecord user;
    user.id = std::stoll(fields[0]);
    user.createdAt = shuati::common::timeFromEpochMilliseconds(
        std::stoll(fields[1]));
    user.role = parseRole(fields[2]);
    user.username = shuati::common::hexDecode(fields[3]);
    user.passwordHash = shuati::common::hexDecode(fields[4]);
    usersById_[user.id] = user;
    idsByUsername_[user.username] = user.id;
    nextId_ = std::max(nextId_, user.id + 1);
  }
}

void InMemoryUserRepository::persistLocked() const {
  if (persistencePath_.empty()) return;
  std::vector<UserRecord> users;
  users.reserve(usersById_.size());
  for (const auto& item : usersById_) users.push_back(item.second);
  std::sort(users.begin(), users.end(), [](const auto& left, const auto& right) {
    return left.id < right.id;
  });
  std::ostringstream output;
  output << "SHUATI_USERS_V1\n";
  for (const auto& user : users) {
    output << user.id << '\t'
           << shuati::common::epochMilliseconds(user.createdAt) << '\t'
           << toString(user.role) << '\t'
           << shuati::common::hexEncode(user.username) << '\t'
           << shuati::common::hexEncode(user.passwordHash) << '\n';
  }
  shuati::common::atomicWriteStateFile(persistencePath_, output.str());
}

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
  persistLocked();
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
  persistLocked();
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
