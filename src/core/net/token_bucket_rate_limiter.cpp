#include "token_bucket_rate_limiter.h"

#include <algorithm>
#include <thread>

namespace Core::Net {

TokenBucketRateLimiter::TokenBucketRateLimiter(
    std::size_t capacity, std::chrono::milliseconds refill_interval)
    : capacity_(capacity), refill_interval_(refill_interval),
      tokens_(static_cast<double>(capacity)),
namespace Core {

TokenBucketRateLimiter::TokenBucketRateLimiter(
    std::size_t capacity, std::chrono::milliseconds refill_interval)
    : capacity_(capacity), tokens_(capacity), refill_interval_(refill_interval),
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
  refill();
  while (tokens_ == 0) {
    auto next_time = last_refill_ + refill_interval_;
    cv_.wait_until(lock, next_time, [this] {
      refill();
      return tokens_ > 0;
    });