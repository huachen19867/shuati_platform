#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "shuati/auth/role.h"

namespace shuati::auth {

struct CreatedSession {
  std::string token;
  std::chrono::system_clock::time_point expiresAt{};
};

struct SessionData {
  std::int64_t userId = 0;
  UserRole role = UserRole::User;
  std::chrono::system_clock::time_point expiresAt{};
};

class SessionManager {
 public:
  using Clock = std::function<std::chrono::system_clock::time_point()>;
  using TokenGenerator = std::function<std::string()>;

  explicit SessionManager(std::chrono::seconds ttl);
  SessionManager(std::chrono::seconds ttl,
                 Clock clock,
                 TokenGenerator tokenGenerator);

  CreatedSession createSession(std::int64_t userId, UserRole role);
  std::optional<SessionData> findSession(const std::string& token);
  bool revoke(const std::string& token);
  std::size_t activeCount();
  std::chrono::seconds ttl() const;

 private:
  void removeExpiredLocked();

  std::chrono::seconds ttl_;
  Clock clock_;
  TokenGenerator tokenGenerator_;
  std::unordered_map<std::string, SessionData> sessionsByTokenHash_;
  std::mutex mutex_;
};

}  // namespace shuati::auth
