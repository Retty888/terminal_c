#pragma once

#include <future>
#include <string>
#include <vector>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>

#include "core/candle.h"
#include "core/candle_manager.h"
#include "core/data_fetcher.h"
#include "core/net/cpr_http_client.h"
#include "core/net/token_bucket_rate_limiter.h"

// DataService groups configuration loading, candle storage and
// network operations used by the application.  At the moment it is
// mostly a thin wrapper around existing free functions.  This makes
// future refactoring easier and keeps main application logic focused
// on high level behaviour.
class DataService {
public:
  DataService();
  explicit DataService(const std::filesystem::path &data_dir);

  // Exchange data ---------------------------------------------------------
  Core::SymbolsResult fetch_all_symbols(
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000),
      std::size_t top_n = 100) const;
  Core::IntervalsResult fetch_intervals(
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000)) const;
  Core::KlinesResult fetch_klines(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000)) const;
  // Direct access to the alternative kline API.
  Core::KlinesResult fetch_klines_alt(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000)) const;
  std::future<Core::KlinesResult> fetch_klines_async(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000)) const;

  // Local storage ---------------------------------------------------------
  std::vector<Core::Candle> load_candles(const std::string &pair,
                                         const std::string &interval) const;
  void save_candles(const std::string &pair, const std::string &interval,
                    const std::vector<Core::Candle> &candles) const;
  void append_candles(const std::string &pair, const std::string &interval,
                      const std::vector<Core::Candle> &candles) const;
  std::vector<std::string> list_stored_data() const;

  Core::CandleManager &candle_manager() { return candle_manager_; }
  const Core::CandleManager &candle_manager() const { return candle_manager_; }

private:
  std::shared_ptr<Core::IHttpClient> http_client_;
  std::shared_ptr<Core::IRateLimiter> rate_limiter_;
  Core::DataFetcher fetcher_;
  Core::CandleManager candle_manager_;
};

