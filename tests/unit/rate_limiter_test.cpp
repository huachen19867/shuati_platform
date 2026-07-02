#include "shuati/common/rate_limiter.h"

#include <gtest/gtest.h>

#include <chrono>

using namespace std::chrono_literals;

TEST(RateLimiterTest, RejectsRequestsAfterWindowLimit) {
  std::chrono::steady_clock::time_point now{0s};
  shuati::common::FixedWindowRateLimiter limiter(
      2, 1min, [&now] { return now; });

  EXPECT_TRUE(limiter.allow("register:127.0.0.1"));
  EXPECT_TRUE(limiter.allow("register:127.0.0.1"));
  EXPECT_FALSE(limiter.allow("register:127.0.0.1"));
}

TEST(RateLimiterTest, ResetsAfterWindowExpires) {
  std::chrono::steady_clock::time_point now{0s};
  shuati::common::FixedWindowRateLimiter limiter(
      1, 1min, [&now] { return now; });

  EXPECT_TRUE(limiter.allow("login:127.0.0.1"));
  EXPECT_FALSE(limiter.allow("login:127.0.0.1"));

  now += 61s;

  EXPECT_TRUE(limiter.allow("login:127.0.0.1"));
}
