#include "token_bucket_rate_limiter.h"

namespace Core {

TokenBucketRateLimiter::TokenBucketRateLimiter(
    std::size_t capacity, std::chrono::milliseconds refill_interval)
    : capacity_(capacity), tokens_(capacity), refill_interval_(refill_interval),
      last_refill_(std::chrono::steady_clock::now()) {}

void TokenBucketRateLimiter::refill() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - last_refill_;
  auto new_tokens = static_cast<std::size_t>(elapsed / refill_interval_);
  if (new_tokens > 0) {
    tokens_ = std::min(capacity_, tokens_ + new_tokens);
    last_refill_ += refill_interval_ * new_tokens;
    cv_.notify_all();
  }
}

void TokenBucketRateLimiter::acquire() {
  std::unique_lock<std::mutex> lock(mutex_);
  refill();
  while (tokens_ == 0) {
    auto next_time = last_refill_ + refill_interval_;
    cv_.wait_until(lock, next_time, [this] {
      refill();
      return tokens_ > 0;
    });
  }
  --tokens_;
}

} // namespace Core

