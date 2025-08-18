#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include "core/candle.h"

// Serialize candle data into format suitable for ECharts candlestick series.
nlohmann::json SerializeCandles(const std::vector<Core::Candle>& candles);

