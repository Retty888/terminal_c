#pragma once

#include "irate_limiter.h"
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace Core {

class TokenBucketRateLimiter : public IRateLimiter {
public:
  TokenBucketRateLimiter(std::size_t capacity,
                         std::chrono::milliseconds refill_interval);
  void acquire() override;

private:
  void refill();

  const std::size_t capacity_;
  std::size_t tokens_;
  const std::chrono::milliseconds refill_interval_;
  std::chrono::steady_clock::time_point last_refill_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

} // namespace Core

