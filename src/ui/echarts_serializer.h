#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include "core/candle.h"

// Serialize candle data into object with "x" timestamps and "y" OHLC arrays
// matching ECharts candlestick expectations.
nlohmann::json SerializeCandles(const std::vector<Core::Candle>& candles);

// Serialize candles for TradingView Lightweight Charts. Each element is an
// object with Unix-second "time" and OHLC fields.
nlohmann::json SerializeCandlesTV(const std::vector<Core::Candle>& candles);

