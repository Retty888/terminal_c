#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include "core/candle.h"

// Serialize candle data into object with "x" timestamps and "y" OHLC arrays
// matching ECharts candlestick expectations.
nlohmann::json SerializeCandles(const std::vector<Core::Candle>& candles);

