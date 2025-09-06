#pragma once

#include <future>
#include <string>
#include <vector>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <cstdint>
#include <optional>
#include <functional>

#include "core/candle.h"
#include "core/candle_manager.h"
#include "core/data_fetcher.h"
#include "core/net/cpr_http_client.h"
#include "core/net/token_bucket_rate_limiter.h"
#include "config_types.h"

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
  Core::KlinesResult fetch_range(
      const std::string &symbol, const std::string &interval,
      long long start_time, long long end_time, int max_retries = 3,
      std::chrono::milliseconds retry_delay =
          std::chrono::milliseconds(1000)) const;
  std::future<Core::KlinesResult> fetch_klines_async(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000)) const;

  // Local storage ---------------------------------------------------------
  std::vector<Core::Candle> load_candles(const std::string &pair,
                                         const std::string &interval) const;
  void save_candles(const std::string &pair, const std::string &interval,
                    const std::vector<Core::Candle> &candles) const;

  // JSON storage helpers ---------------------------------------------------
  std::vector<Core::Candle> load_candles_json(const std::string &pair,
                                              const std::string &interval) const;
  void save_candles_json(const std::string &pair, const std::string &interval,
                         const std::vector<Core::Candle> &candles) const;
  void append_candles(const std::string &pair, const std::string &interval,
                      const std::vector<Core::Candle> &candles) const;
  bool remove_candles(const std::string &pair) const;
  bool clear_interval(const std::string &pair, const std::string &interval) const;
  bool reload_candles(const std::string &pair, const std::string &interval) const;
  std::vector<std::string> list_stored_data() const;

  std::uintmax_t get_file_size(const std::string &pair,
                               const std::string &interval) const;

  Core::CandleManager &candle_manager() { return candle_manager_; }
  const Core::CandleManager &candle_manager() const { return candle_manager_; }

  // Configuration accessors -----------------------------------------------
  std::string primary_provider() const;

private:
  Core::KlinesResult FetchRangeImpl(
      const std::string &url,
      const std::function<std::vector<Core::Candle>(const std::string &)> &parser,
      int max_retries,
      std::chrono::milliseconds retry_delay) const;
  const Config::ConfigData &config() const;
  std::shared_ptr<Core::IHttpClient> http_client_;
  std::shared_ptr<Core::IRateLimiter> rate_limiter_;
  Core::DataFetcher fetcher_;
  Core::CandleManager candle_manager_;
  mutable std::optional<Config::ConfigData> config_cache_;
};

