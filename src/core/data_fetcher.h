#pragma once

#include "candle.h"
#include <string>
#include <vector>
#include <future>

namespace Core {

class DataFetcher {
public:
    // Fetches kline (candle) data from a specified API (e.g., Binance).
    // symbol: Trading pair (e.g., "BTCUSDT")
    // interval: Time interval (e.g., "5m", "1h", "1d")
    // limit: Number of candles to fetch
    static std::vector<Candle> fetch_klines(const std::string& symbol, const std::string& interval, int limit = 500);

    // Asynchronously fetches kline data on a background thread.
    // Returns a future that becomes ready with the fetched candles once
    // the HTTP request completes. The function itself is thread-safe;
    // callers must ensure synchronization when modifying shared candle data
    // with the returned result.
    static std::future<std::vector<Candle>> fetch_klines_async(const std::string& symbol, const std::string& interval, int limit = 500);
};

} // namespace Core
