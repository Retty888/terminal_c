#pragma once

#include <chrono>

namespace Core {

class IRateLimiter {
public:
  virtual ~IRateLimiter() = default;
  virtual void acquire() = 0;
};

} // namespace Core

