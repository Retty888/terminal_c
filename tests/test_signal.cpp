#include "signal.h"
#include <vector>
#include <gtest/gtest.h>

TEST(SignalTest, SmaCrossoverAndAverage) {
    std::vector<Core::Candle> candles;
    double closes[] = {5,4,3,2,3,4};
    for (int i = 0; i < 6; ++i) {
        candles.emplace_back(i,0,0,0,closes[i],0,0,0,0,0,0,0);
    }
    int sig = Signal::sma_crossover_signal(candles,5,2,3);
    EXPECT_EQ(sig, 1);

    candles.emplace_back(6,0,0,0,3,0,0,0,0,0,0,0);
    candles.emplace_back(7,0,0,0,2,0,0,0,0,0,0,0);
    sig = Signal::sma_crossover_signal(candles,7,2,3);
    EXPECT_EQ(sig, -1);

    double sma = Signal::simple_moving_average(candles,7,3);
    EXPECT_NEAR(sma, 3.0, 1e-9);
}

