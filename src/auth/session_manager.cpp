#include "shuati/auth/session_manager.h"

#include "shuati/common/crypto.h"

namespace shuati::auth {
namespace {

std::string tokenHash(const std::string& token) {
  return shuati::common::sha256Hex("session:" + token);
}

}  // namespace

SessionManager::SessionManager(std::chrono::seconds ttl)
    : SessionManager(ttl,
                     [] { return std::chrono::system_clock::now(); },
                     [] { return shuati::common::randomHex(32); }) {}

SessionManager::SessionManager(std::chrono::seconds ttl,
                               Clock clock,
                               TokenGenerator tokenGenerator)
    : ttl_(ttl),
      clock_(std::move(clock)),
      tokenGenerator_(std::move(tokenGenerator)) {}

CreatedSession SessionManager::createSession(std::int64_t userId,
                                             UserRole role) {
  std::lock_guard<std::mutex> lock(mutex_);
  removeExpiredLocked();

  const auto token = tokenGenerator_();
  const auto expiresAt = clock_() + ttl_;
  sessionsByTokenHash_[tokenHash(token)] = SessionData{userId, role, expiresAt};
  return CreatedSession{token, expiresAt};
}

std::optional<SessionData> SessionManager::findSession(
    const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  removeExpiredLocked();

  const auto it = sessionsByTokenHash_.find(tokenHash(token));
  if (it == sessionsByTokenHash_.end()) {
    return std::nullopt;
  }
  if (it->second.expiresAt <= clock_()) {
    sessionsByTokenHash_.erase(it);
    return std::nullopt;
  }
  return it->second;
}

bool SessionManager::revoke(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  return sessionsByTokenHash_.erase(tokenHash(token)) > 0;
}

std::size_t SessionManager::activeCount() {
  std::lock_guard<std::mutex> lock(mutex_);
  removeExpiredLocked();
  return sessionsByTokenHash_.size();
}

std::chrono::seconds SessionManager::ttl() const {
  return ttl_;
}

void SessionManager::removeExpiredLocked() {
  const auto now = clock_();
  for (auto it = sessionsByTokenHash_.begin();
       it != sessionsByTokenHash_.end();) {
    if (it->second.expiresAt <= now) {
      it = sessionsByTokenHash_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace shuati::auth
