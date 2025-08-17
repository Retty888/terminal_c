#include "token_bucket_rate_limiter.h"

#include <algorithm>
#include <thread>

namespace Core::Net {

TokenBucketRateLimiter::TokenBucketRateLimiter(
    std::size_t capacity, std::chrono::milliseconds refill_interval)
    : capacity_(capacity), refill_interval_(refill_interval),
      tokens_(static_cast<double>(capacity)),
      last_refill_(std::chrono::steady_clock::now()) {}

void TokenBucketRateLimiter::refill() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                      last_refill_);
  if (elapsed.count() <= 0)
    return;
  double new_tokens = static_cast<double>(elapsed.count()) /
                      static_cast<double>(refill_interval_.count());
  tokens_ = std::min<double>(capacity_, tokens_ + new_tokens);
  if (new_tokens > 0)
    last_refill_ = now;
}

void TokenBucketRateLimiter::acquire() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (true) {
    refill();
    if (tokens_ >= 1.0) {
      tokens_ -= 1.0;
      return;
    }
    auto now = std::chrono::steady_clock::now();
    auto next = last_refill_ + refill_interval_;
    auto wait_time = next - now;
    if (wait_time.count() > 0) {
      lock.unlock();
      std::this_thread::sleep_for(wait_time);
      lock.lock();
    } else {
      std::this_thread::yield();
    }
  }
}

} // namespace Core::Net
