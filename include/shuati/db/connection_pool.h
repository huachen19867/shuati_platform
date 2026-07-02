#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>

namespace shuati::db {

template <typename T>
class FixedConnectionPool {
 public:
  class Lease {
   public:
    Lease() = default;
    Lease(FixedConnectionPool* pool, std::shared_ptr<T> value)
        : pool_(pool), value_(std::move(value)) {}
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;
    Lease(Lease&& other) noexcept
        : pool_(other.pool_), value_(std::move(other.value_)) {
      other.pool_ = nullptr;
    }
    Lease& operator=(Lease&& other) noexcept {
      if (this != &other) {
        release();
        pool_ = other.pool_;
        value_ = std::move(other.value_);
        other.pool_ = nullptr;
      }
      return *this;
    }
    ~Lease() {
      release();
    }

    T& operator*() const {
      return *value_;
    }

    T* operator->() const {
      return value_.get();
    }

    T& get() const {
      return *value_;
    }

   private:
    void release() {
      if (pool_ && value_) {
        pool_->release(std::move(value_));
        pool_ = nullptr;
      }
    }

    FixedConnectionPool* pool_ = nullptr;
    std::shared_ptr<T> value_;
  };

  explicit FixedConnectionPool(const std::vector<std::shared_ptr<T>>& values) {
    for (const auto& value : values) {
      available_.push(value);
    }
    size_ = available_.size();
  }

  std::optional<Lease> acquire(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool ready = condition_.wait_for(lock, timeout, [&] {
      return !available_.empty();
    });
    if (!ready) {
      return std::nullopt;
    }

    auto value = available_.front();
    available_.pop();
    return Lease(this, std::move(value));
  }

  std::size_t size() const {
    return size_;
  }

  std::size_t available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_.size();
  }

 private:
  void release(std::shared_ptr<T> value) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      available_.push(std::move(value));
    }
    condition_.notify_one();
  }

  std::size_t size_ = 0;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::queue<std::shared_ptr<T>> available_;
};

}  // namespace shuati::db
