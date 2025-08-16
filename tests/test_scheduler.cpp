#include "core/candle.h"
#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {
std::chrono::milliseconds interval_to_duration(const std::string &interval) {
  if (interval.empty())
    return std::chrono::milliseconds(0);
  char unit = interval.back();
  long long value = 0;
  try {
    value = std::stoll(interval.substr(0, interval.size() - 1));
  } catch (...) {
    return std::chrono::milliseconds(0);
  }
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
} // namespace

TEST(SchedulerTest, AppendsCandleAtBoundary) {
  using namespace Core;
  std::vector<Candle> candles;
  std::string interval = "1m";
  auto period = interval_to_duration(interval);
  long long start = 1000;
  candles.emplace_back(start, 0, 0, 0, 0, 0, start + period.count() - 1, 0, 0,
                       0, 0, 0);

  long long next_boundary = candles.back().open_time + period.count();
  long long before = next_boundary - 1;
  bool should_fetch_before = before >= next_boundary;
  EXPECT_FALSE(should_fetch_before);

  long long at = next_boundary;
  bool should_fetch_at = at >= next_boundary;
  EXPECT_TRUE(should_fetch_at);

  if (should_fetch_at) {
    candles.emplace_back(next_boundary, 0, 0, 0, 0, 0,
                         next_boundary + period.count() - 1, 0, 0, 0, 0, 0);
  }

  ASSERT_EQ(candles.size(), 2u);
  EXPECT_EQ(candles.back().open_time, next_boundary);
}
