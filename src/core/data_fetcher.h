#pragma once

#include "candle.h"
#include <string>
#include <vector>

namespace Core {

class DataFetcher {
public:
    // Fetches kline (candle) data from a specified API (e.g., Binance).
    // symbol: Trading pair (e.g., "BTCUSDT")
    // interval: Time interval (e.g., "5m", "1h", "1d")
    // limit: Number of candles to fetch
    static std::vector<Candle> fetch_klines(const std::string& symbol, const std::string& interval, int limit = 500);
};

} // namespace Core
