#include "signal.h"
#include "services/signal_bot.h"
#include "config.h"
#include <vector>
#include <fstream>
#include <filesystem>
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

TEST(SignalBotTest, GeneratesSignalFromConfig) {
    std::vector<Core::Candle> candles;
    double closes[] = {5,4,3,2,3,4};
    for (int i = 0; i < 6; ++i) {
        candles.emplace_back(i,0,0,0,closes[i],0,0,0,0,0,0,0);
    }
    Config::SignalConfig cfg{ "sma_crossover", 2, 3 };
    SignalBot bot(cfg);
    int sig = bot.generate_signal(candles,5);
    EXPECT_EQ(sig, 1);
}

TEST(ConfigTest, LoadSignalConfig) {
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "signal_config_test.json";
    {
        std::ofstream out(tmp);
        out << R"({
  "signal": {
    "type": "sma_crossover",
    "short_period": 2,
    "long_period": 3
  }
})";
    }
    Config::SignalConfig cfg = Config::load_signal_config(tmp.string());
    EXPECT_EQ(cfg.type, "sma_crossover");
    EXPECT_EQ(cfg.short_period, 2u);
    EXPECT_EQ(cfg.long_period, 3u);
    std::filesystem::remove(tmp);
}

TEST(SignalIndicators, CalculatesEmaAndRsi) {
    std::vector<Core::Candle> candles;
    double closes[] = {1,2,3,4,5};
    for (int i = 0; i < 5; ++i) {
        candles.emplace_back(i,0,0,0,closes[i],0,0,0,0,0,0,0);
    }
    double ema = Signal::exponential_moving_average(candles,4,3);
    EXPECT_NEAR(ema, 4.25, 1e-2);
    double rsi = Signal::relative_strength_index(candles,4,3);
    EXPECT_NEAR(rsi, 100.0, 1e-6);
}

TEST(SignalIndicators, CalculatesMacd) {
    std::vector<Core::Candle> candles;
    double closes[] = {1,2,3,3,2,1};
    for (int i = 0; i < 6; ++i) {
        candles.emplace_back(i,0,0,0,closes[i],0,0,0,0,0,0,0);
    }
    Signal::MACDResult res = Signal::macd(candles,5,2,3,2);
    EXPECT_NEAR(res.macd, -0.3194444444, 1e-6);
    EXPECT_NEAR(res.signal, -0.2546296296, 1e-6);
    EXPECT_NEAR(res.histogram, -0.0648148148, 1e-6);
    for (int i = 1; i <= 50; ++i) {
        candles.emplace_back(i,0,0,0,i,0,0,0,0,0,0,0);
    }
    auto m = Signal::macd(candles, 49, 12, 26, 9);
    EXPECT_NEAR(m.macd, 5.1017391484568435, 1e-6);
    EXPECT_NEAR(m.signal, 5.1017391484568524, 1e-6);
    EXPECT_NEAR(m.histogram, 0.0, 1e-6);
}

