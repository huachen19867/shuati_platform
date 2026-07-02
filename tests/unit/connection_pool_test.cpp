#include "shuati/db/connection_pool.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(ConnectionPoolTest, AcquiresAndReleasesFixedResources) {
  std::vector<std::shared_ptr<int>> values{
      std::make_shared<int>(1),
      std::make_shared<int>(2),
  };
  shuati::db::FixedConnectionPool<int> pool(values);

  EXPECT_EQ(pool.size(), 2U);
  EXPECT_EQ(pool.available(), 2U);

  {
    auto first = pool.acquire(10ms);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(pool.available(), 1U);
    EXPECT_TRUE(**first == 1 || **first == 2);
  }

  EXPECT_EQ(pool.available(), 2U);
}

TEST(ConnectionPoolTest, AcquireTimesOutWhenPoolIsExhausted) {
  std::vector<std::shared_ptr<int>> values{std::make_shared<int>(1)};
  shuati::db::FixedConnectionPool<int> pool(values);

  auto held = pool.acquire(10ms);
  ASSERT_TRUE(held.has_value());

  const auto started = std::chrono::steady_clock::now();
  auto missing = pool.acquire(30ms);
  const auto elapsed = std::chrono::steady_clock::now() - started;

  EXPECT_FALSE(missing.has_value());
  EXPECT_GE(elapsed, 25ms);
}
