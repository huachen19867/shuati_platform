#include "shuati/common/rate_limiter.h"

#include <algorithm>

namespace shuati::common {

FixedWindowRateLimiter::FixedWindowRateLimiter(
    int maxRequests,
    std::chrono::steady_clock::duration window,
    Clock clock)
    : maxRequests_(std::max(1, maxRequests)),
      window_(window),
      clock_(std::move(clock)) {}

bool FixedWindowRateLimiter::allow(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = clock_();
  auto& bucket = buckets_[key];
  if (bucket.count == 0 || now - bucket.windowStart >= window_) {
    bucket.windowStart = now;
    bucket.count = 0;
  }
  if (bucket.count >= maxRequests_) {
    return false;
  }
  ++bucket.count;
  return true;
}

void FixedWindowRateLimiter::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  buckets_.clear();
}

}  // namespace shuati::common
