#include "core/candle.h"
#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "core/interval_utils.h"

TEST(SchedulerTest, AppendsCandleAtBoundary) {
  using namespace Core;
  std::vector<Candle> candles;
  std::string interval = "1m";
  auto period = parse_interval(interval);
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
