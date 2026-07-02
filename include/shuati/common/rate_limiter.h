#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace shuati::common {

class FixedWindowRateLimiter {
 public:
  using Clock = std::function<std::chrono::steady_clock::time_point()>;

  FixedWindowRateLimiter(int maxRequests,
                         std::chrono::steady_clock::duration window,
                         Clock clock = [] {
                           return std::chrono::steady_clock::now();
                         });

  bool allow(const std::string& key);
  void clear();

 private:
  struct Bucket {
    std::chrono::steady_clock::time_point windowStart{};
    int count = 0;
  };

  int maxRequests_ = 1;
  std::chrono::steady_clock::duration window_;
  Clock clock_;
  std::unordered_map<std::string, Bucket> buckets_;
  std::mutex mutex_;
};

}  // namespace shuati::common
