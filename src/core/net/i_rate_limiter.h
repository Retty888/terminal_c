#pragma once

#include <chrono>

namespace Core::Net {
class IRateLimiter {
public:
  virtual ~IRateLimiter() = default;
  virtual void acquire() = 0;
};
}
