#include "interval_utils.h"

namespace Core {

std::chrono::milliseconds parse_interval(const std::string &interval) {
  if (interval.empty())
    return std::chrono::milliseconds(0);
  char unit = interval.back();
  long long value = 0;
  try {
    value = std::stoll(interval.substr(0, interval.size() - 1));
  } catch (...) {
    return std::chrono::milliseconds(0);
  }
  if (value < 0)
    return std::chrono::milliseconds(0);
  switch (unit) {
  case 's':
    return std::chrono::milliseconds(value * 1000LL);
  case 'm':
    return std::chrono::milliseconds(value * 60LL * 1000LL);
  case 'h':
    return std::chrono::milliseconds(value * 60LL * 60LL * 1000LL);
  case 'd':
    return std::chrono::milliseconds(value * 24LL * 60LL * 60LL * 1000LL);
  case 'w':
    return std::chrono::milliseconds(value * 7LL * 24LL * 60LL * 60LL * 1000LL);
  default:
    return std::chrono::milliseconds(0);
  }
}

} // namespace Core

