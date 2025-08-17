#pragma once

#include "candle.h"
#include "net/i_http_client.h"
#include "net/i_rate_limiter.h"
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
  DataFetcher(std::shared_ptr<Net::IHttpClient> http_client,
              std::shared_ptr<Net::IRateLimiter> rate_limiter);

  KlinesResult fetch_klines(const std::string &symbol,
                            const std::string &interval, int limit = 500,
                            int max_retries = 3,
                            std::chrono::milliseconds retry_delay =
                                std::chrono::milliseconds(1000));

  KlinesResult fetch_klines_alt(const std::string &symbol,
                                const std::string &interval, int limit = 500,
                                int max_retries = 3,
                                std::chrono::milliseconds retry_delay =
                                    std::chrono::milliseconds(1000));

  std::future<KlinesResult>
  fetch_klines_async(const std::string &symbol, const std::string &interval,
                     int limit = 500, int max_retries = 3,
                     std::chrono::milliseconds retry_delay =
                         std::chrono::milliseconds(1000));

  SymbolsResult fetch_all_symbols(
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000),
      std::size_t top_n = 100);

  IntervalsResult fetch_all_intervals(
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000));

private:
  std::shared_ptr<Net::IHttpClient> http_client_;
  std::shared_ptr<Net::IRateLimiter> rate_limiter_;
};

} // namespace Core
