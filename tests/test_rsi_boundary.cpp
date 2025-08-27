#include "signal.h"
#include <gtest/gtest.h>
#include <vector>

TEST(RsiTest, HandlesBoundaryIndex) {
    std::vector<Core::Candle> candles;
    double closes[] = {1, 2, 3};
    for (int i = 0; i < 3; ++i) {
        candles.emplace_back(i, 0, 0, 0, closes[i], 0, 0, 0, 0, 0, 0, 0);
    }
    double rsi = Signal::relative_strength_index(candles, 2, 3);
    EXPECT_NEAR(rsi, 100.0, 1e-6);
}

