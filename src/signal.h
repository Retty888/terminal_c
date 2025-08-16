#pragma once

#include "core/candle.h"
#include <vector>

namespace Signal {

// Calculates simple moving average of candle close prices.
[[nodiscard]] double simple_moving_average(const std::vector<Core::Candle>& candles, std::size_t index, std::size_t period);

// Generates a trading signal based on SMA crossover.
// Returns 1 when short SMA crosses above long SMA,
// -1 when it crosses below, and 0 otherwise.
[[nodiscard]] int sma_crossover_signal(const std::vector<Core::Candle>& candles,
                                       std::size_t index,
                                       std::size_t short_period,
                                       std::size_t long_period);

// Calculates exponential moving average of candle close prices.
[[nodiscard]] double exponential_moving_average(const std::vector<Core::Candle>& candles,
                                               std::size_t index,
                                               std::size_t period);

// Generates signal based on price crossing EMA.
[[nodiscard]] int ema_signal(const std::vector<Core::Candle>& candles,
                             std::size_t index,
                             std::size_t period);

// Calculates Relative Strength Index.
[[nodiscard]] double relative_strength_index(const std::vector<Core::Candle>& candles,
                                             std::size_t index,
                                             std::size_t period);

// Generates signal based on RSI thresholds.
[[nodiscard]] int rsi_signal(const std::vector<Core::Candle>& candles,
                             std::size_t index,
                             std::size_t period,
                             double oversold,
                             double overbought);

struct MACDResult {
    double macd;
    double signal;
    double histogram;
};

// Calculates Moving Average Convergence Divergence (MACD).
[[nodiscard]] MACDResult macd(const std::vector<Core::Candle>& candles,
                              std::size_t index,
                              std::size_t fast_period,
                              std::size_t slow_period,
                              std::size_t signal_period);

// Generates signal based on MACD line crossing the signal line.
[[nodiscard]] int macd_signal(const std::vector<Core::Candle>& candles,
                              std::size_t index,
                              std::size_t fast_period,
                              std::size_t slow_period,
                              std::size_t signal_period);

} // namespace Signal

