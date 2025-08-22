#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include "core/candle.h"

// Serialize candle data into object with "x" timestamps and "y" OHLC arrays
// matching ECharts candlestick expectations.
nlohmann::json SerializeCandles(const std::vector<Core::Candle>& candles);

// Serialize candle data for TradingView Lightweight Charts. Produces an array
// of objects: { time: <unix-sec>, open, high, low, close }.
nlohmann::json SerializeCandlesTV(const std::vector<Core::Candle>& candles);

