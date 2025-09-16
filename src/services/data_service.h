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
#include <map>

#include "core/candle.h"
#include "core/candle_manager.h"
#include "core/net/idata_provider.h"
#include "core/net/cpr_http_client.h"
#include "core/net/token_bucket_rate_limiter.h"
#include "config_types.h"

class DataService {
public:
  DataService();
  explicit DataService(const std::filesystem::path &data_dir);

  void register_provider(const std::string &name, std::shared_ptr<Core::IDataProvider> provider);
  void set_active_provider(const std::string &name);
  std::vector<std::string> get_provider_names() const;
  std::string get_active_provider_name() const;

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
  Core::KlinesResult fetch_range(
      const std::string &symbol, const std::string &interval,
      long long start_ms, long long end_ms,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000)) const;
  std::future<Core::KlinesResult> fetch_klines_async(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000)) const;

  std::vector<Core::Candle> load_candles(const std::string &pair,
                                         const std::string &interval) const;
  void append_candles(const std::string &pair, const std::string &interval,
                    const std::vector<Core::Candle> &candles) const;
  void overwrite_candles(const std::string &pair, const std::string &interval,
                         const std::vector<Core::Candle> &candles) const;
  // Save only when data actually changed (count or last timestamp) and not too frequently (debounce).
  bool save_if_changed(const std::string &pair, const std::string &interval,
                       const std::vector<Core::Candle> &candles) const;

  // Convenience wrappers used by UI
  bool clear_interval(const std::string &pair, const std::string &interval) const {
    return candle_manager_.clear_interval(pair, interval);
  }
  std::uintmax_t get_file_size(const std::string &pair, const std::string &interval) const {
    return candle_manager_.file_size(pair, interval);
  }
  bool reload_candles(const std::string &pair, const std::string &interval) const;

  // Fetch and merge any recent candles missing after the last stored open_time
  // up to now; persists to storage. Returns true if any data was written.
  bool top_up_recent(const std::string &pair, const std::string &interval) const;

  // Ensure at least target_count candles exist by backfilling older history
  // (or reloading if none). Persists to storage. Returns true on success.
  bool ensure_limit(const std::string &pair, const std::string &interval,
                    std::size_t target_count) const;

  std::vector<std::string> list_stored_data() const;

  bool remove_candles(const std::string &pair) const { return candle_manager_.remove_candles(pair); }

  Core::CandleManager &candle_manager() { return candle_manager_; }
  const Core::CandleManager &candle_manager() const { return candle_manager_; }

private:
  const Config::ConfigData &config() const;
  std::shared_ptr<Core::IHttpClient> http_client_;
  std::shared_ptr<Core::IRateLimiter> rate_limiter_;
  std::map<std::string, std::shared_ptr<Core::IDataProvider>> providers_;
  std::string active_provider_;
  Core::CandleManager candle_manager_;
  mutable std::optional<Config::ConfigData> config_cache_;

  // Debounce + change detection for saves
  mutable std::map<std::string, std::pair<std::size_t, long long>> last_saved_state_;
  mutable std::map<std::string, std::chrono::steady_clock::time_point> last_saved_time_;
  const std::chrono::milliseconds save_debounce_{3000};
  // Allow persistence only after initial warm-up to avoid write storms on launch
  std::chrono::steady_clock::time_point persist_allowed_after_{};
};
