#define __cpp_lib_format 0
#define GTEST_HAS_PTHREAD 0
#include <gtest/gtest.h>
#include "ui/echarts_serializer.h"

TEST(EChartsSerialization, ProducesExpectedJson) {
    std::vector<Core::Candle> candles = {
        {0, 10.0, 20.0, 5.0, 15.0, 0.0, 0, 0.0, 0, 0.0, 0.0, 0.0},
        {60, 12.0, 22.0, 8.0, 18.0, 0.0, 0, 0.0, 0, 0.0, 0.0, 0.0}
    };
    auto json = SerializeCandles(candles);
    nlohmann::json expected = {
        {"x", {0, 60}},
        {"y", {
            {10.0, 15.0, 5.0, 20.0},
            {12.0, 18.0, 8.0, 22.0}
        }}
    };
    EXPECT_EQ(json, expected);
}

