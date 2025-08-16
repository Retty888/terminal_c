#pragma once

#include "candle.h"
#include <future>
#include <string>
#include <vector>
#include <chrono>

namespace Core {

// Detailed error codes returned by DataFetcher functions.
enum class FetchError {
  None = 0,        // No error
  HttpError = 1,   // Non-200 HTTP status
  ParseError = 2,  // Failed to parse response
  NetworkError = 3 // Network level failure
};

struct KlinesResult {
  FetchError error{FetchError::None};
  int http_status{0};
  std::string message; // detailed message
  std::vector<Candle> candles; // fetched candles
};

struct SymbolsResult {
  FetchError error{FetchError::None};
  int http_status{0};
  std::string message;
  std::vector<std::string> symbols;
};

struct IntervalsResult {
  FetchError error{FetchError::None};
  int http_status{0};
  std::string message;
  std::vector<std::string> intervals;
};

class DataFetcher {
public:
  // Fetches kline (candle) data from a specified API (e.g., Binance).
  // symbol: Trading pair (e.g., "BTCUSDT")
  // interval: Time interval (e.g., "5m", "1h", "1d")
  // limit: Number of candles to fetch
  // max_retries: how many times to retry on failure
  // retry_delay: delay between retries
  // request_pause: enforced pause between requests to respect API limits
  static KlinesResult
  fetch_klines(const std::string &symbol, const std::string &interval,
               int limit = 500, int max_retries = 3,
               std::chrono::milliseconds retry_delay =
                   std::chrono::milliseconds(1000),
               std::chrono::milliseconds request_pause =
                   std::chrono::milliseconds(1100));

  // Asynchronously fetches kline data on a background thread.
  // Returns a future that becomes ready with the fetched candles once
  // the HTTP request completes. The function itself is thread-safe;
  // callers must ensure synchronization when modifying shared candle data
  // with the returned result.
  static std::future<KlinesResult>
  fetch_klines_async(const std::string &symbol, const std::string &interval,
                     int limit = 500, int max_retries = 3,
                     std::chrono::milliseconds retry_delay =
                         std::chrono::milliseconds(1000),
                     std::chrono::milliseconds request_pause =
                         std::chrono::milliseconds(1100));

  // Fetch list of all available trading symbols from the exchange.
  static SymbolsResult
  fetch_all_symbols(int max_retries = 3,
                    std::chrono::milliseconds retry_delay =
                        std::chrono::milliseconds(1000),
                    std::chrono::milliseconds request_pause =
                        std::chrono::milliseconds(1100));

  static IntervalsResult
  fetch_all_intervals(int max_retries = 3,
                      std::chrono::milliseconds retry_delay =
                          std::chrono::milliseconds(1000),
                      std::chrono::milliseconds request_pause =
                          std::chrono::milliseconds(1100));
};

} // namespace Core
