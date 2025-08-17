#include "core/interval_utils.h"
#include <gtest/gtest.h>

using Core::parse_interval;

TEST(IntervalUtilsTest, ParsesValidIntervals) {
  EXPECT_EQ(parse_interval("1s"), std::chrono::milliseconds(1000));
  EXPECT_EQ(parse_interval("3m"), std::chrono::milliseconds(3 * 60 * 1000));
  EXPECT_EQ(parse_interval("2h"),
            std::chrono::milliseconds(2 * 60 * 60 * 1000LL));
  EXPECT_EQ(parse_interval("1d"),
            std::chrono::milliseconds(24 * 60 * 60 * 1000LL));
  EXPECT_EQ(parse_interval("1w"),
            std::chrono::milliseconds(7 * 24 * 60 * 60 * 1000LL));
}

TEST(IntervalUtilsTest, HandlesInvalidIntervals) {
  EXPECT_EQ(parse_interval(""), std::chrono::milliseconds(0));
  EXPECT_EQ(parse_interval("10x"), std::chrono::milliseconds(0));
  EXPECT_EQ(parse_interval("abc"), std::chrono::milliseconds(0));
  EXPECT_EQ(parse_interval("5"), std::chrono::milliseconds(0));
  EXPECT_EQ(parse_interval("-5m"), std::chrono::milliseconds(0));
}

