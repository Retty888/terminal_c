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

