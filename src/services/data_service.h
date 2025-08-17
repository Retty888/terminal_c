#pragma once

#include <future>
#include <string>
#include <vector>
#include <chrono>
#include <cstddef>
#include <filesystem>

#include "core/candle.h"
#include "core/data_fetcher.h"

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
      std::chrono::milliseconds request_pause =
          std::chrono::milliseconds(1100),
      std::size_t top_n = 100) const;
  Core::IntervalsResult fetch_intervals(
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000),
      std::chrono::milliseconds request_pause =
          std::chrono::milliseconds(1100)) const;
  Core::KlinesResult fetch_klines(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000),
      std::chrono::milliseconds request_pause =
          std::chrono::milliseconds(1100)) const;
  // Direct access to the alternative kline API.
  Core::KlinesResult fetch_klines_alt(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000),
      std::chrono::milliseconds request_pause =
          std::chrono::milliseconds(1100)) const;
  std::future<Core::KlinesResult> fetch_klines_async(
      const std::string &symbol, const std::string &interval, int limit,
      int max_retries = 3,
      std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000),
      std::chrono::milliseconds request_pause =
          std::chrono::milliseconds(1100)) const;

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
  Core::CandleManager candle_manager_;
};

