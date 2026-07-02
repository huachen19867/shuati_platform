#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "shuati/auth/role.h"

namespace shuati::auth {

struct UserRecord {
  std::int64_t id = 0;
  std::string username;
  std::string passwordHash;
  UserRole role = UserRole::User;
  std::chrono::system_clock::time_point createdAt{};
};

class IUserRepository {
 public:
  virtual ~IUserRepository() = default;

  virtual std::optional<UserRecord> createUser(
      const std::string& username,
      const std::string& passwordHash,
      UserRole role) = 0;
  virtual std::optional<UserRecord> findByUsername(
      const std::string& username) const = 0;
  virtual std::optional<UserRecord> findById(std::int64_t id) const = 0;
  virtual std::optional<UserRecord> updateRole(std::int64_t id,
                                               UserRole role) = 0;
  virtual std::vector<UserRecord> listUsers() const = 0;
  virtual bool hasSuperAdmin() const = 0;
};

class InMemoryUserRepository : public IUserRepository {
 public:
  std::optional<UserRecord> createUser(const std::string& username,
                                       const std::string& passwordHash,
                                       UserRole role) override;
  std::optional<UserRecord> findByUsername(
      const std::string& username) const override;
  std::optional<UserRecord> findById(std::int64_t id) const override;
  std::optional<UserRecord> updateRole(std::int64_t id,
                                       UserRole role) override;
  std::vector<UserRecord> listUsers() const override;
  bool hasSuperAdmin() const override;

 private:
  mutable std::mutex mutex_;
  std::int64_t nextId_ = 1;
  std::unordered_map<std::int64_t, UserRecord> usersById_;
  std::unordered_map<std::string, std::int64_t> idsByUsername_;
};

}  // namespace shuati::auth
