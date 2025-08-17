#pragma once

#include "candle.h"
#include "net/ihttp_client.h"
#include "net/irate_limiter.h"
#include <future>
#include <string>
#include <vector>
#include <chrono>
#include <cstddef>
#include <memory>

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
  DataFetcher(std::shared_ptr<IHttpClient> http_client,
              std::shared_ptr<IRateLimiter> rate_limiter);

  // Fetches kline (candle) data from a specified API (e.g., Binance).
  KlinesResult fetch_klines(const std::string &symbol, const std::string &interval,
                            int limit = 500, int max_retries = 3,
                            std::chrono::milliseconds retry_delay =
                                std::chrono::milliseconds(1000)) const;

  // Fetches kline data from an alternative API. Used as a fallback when the
  // primary exchange fails or for unsupported intervals (e.g. 5s/15s).
  KlinesResult fetch_klines_alt(const std::string &symbol,
                                const std::string &interval, int limit = 500,
                                int max_retries = 3,
                                std::chrono::milliseconds retry_delay =
                                    std::chrono::milliseconds(1000)) const;

  // Asynchronously fetches kline data on a background thread.
  std::future<KlinesResult> fetch_klines_async(
      const std::string &symbol, const std::string &interval, int limit = 500,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay =
          std::chrono::milliseconds(1000)) const;

  // Fetch list of all available trading symbols from the exchange.
  SymbolsResult fetch_all_symbols(
      int max_retries = 3,
      std::chrono::milliseconds retry_delay =
          std::chrono::milliseconds(1000),
      std::size_t top_n = 100) const;

  IntervalsResult fetch_all_intervals(
      int max_retries = 3,
      std::chrono::milliseconds retry_delay =
          std::chrono::milliseconds(1000)) const;

private:
  KlinesResult fetch_klines_from_api(const std::string &prefix,
                                     const std::string &symbol,
                                     const std::string &interval, int limit,
                                     int max_retries,
                                     std::chrono::milliseconds retry_delay) const;

  std::shared_ptr<IHttpClient> http_client_;
  std::shared_ptr<IRateLimiter> rate_limiter_;
};

} // namespace Core

