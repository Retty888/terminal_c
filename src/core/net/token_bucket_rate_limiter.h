#pragma once

#include "i_rate_limiter.h"
#include <chrono>
#include <mutex>

namespace Core::Net {
class TokenBucketRateLimiter : public IRateLimiter {
public:
  TokenBucketRateLimiter(std::size_t capacity,
                         std::chrono::milliseconds refill_interval);
  void acquire() override;

private:
  std::size_t capacity_;
  std::chrono::milliseconds refill_interval_;
  double tokens_;
  std::chrono::steady_clock::time_point last_refill_;
  std::mutex mutex_;
  void refill();
};
}
