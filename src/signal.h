#pragma once

#include "core/candle.h"
#include <vector>

namespace Signal {

// Calculates simple moving average of candle close prices.
double simple_moving_average(const std::vector<Core::Candle>& candles, std::size_t index, std::size_t period);

// Generates a trading signal based on SMA crossover.
// Returns 1 when short SMA crosses above long SMA,
// -1 when it crosses below, and 0 otherwise.
int sma_crossover_signal(const std::vector<Core::Candle>& candles,
                         std::size_t index,
                         std::size_t short_period,
                         std::size_t long_period);

} // namespace Signal

