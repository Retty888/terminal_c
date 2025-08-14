#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "core/candle.h"

// DataService groups configuration loading, candle storage and
// network operations used by the application.  At the moment it is
// mostly a thin wrapper around existing free functions.  This makes
// future refactoring easier and keeps main application logic focused
// on high level behaviour.
class DataService {
public:
  // Configuration ---------------------------------------------------------
  std::vector<std::string> load_selected_pairs(const std::string &filename) const;
  void save_selected_pairs(const std::string &filename,
                           const std::vector<std::string> &pairs) const;

  // Exchange data ---------------------------------------------------------
  std::optional<std::vector<std::string>> fetch_all_symbols() const;
  std::optional<std::vector<Core::Candle>> fetch_klines(
      const std::string &symbol, const std::string &interval, int limit) const;
  std::future<std::optional<std::vector<Core::Candle>>> fetch_klines_async(
      const std::string &symbol, const std::string &interval, int limit) const;

  // Local storage ---------------------------------------------------------
  std::vector<Core::Candle> load_candles(const std::string &pair,
                                         const std::string &interval) const;
  void save_candles(const std::string &pair, const std::string &interval,
                    const std::vector<Core::Candle> &candles) const;
  void append_candles(const std::string &pair, const std::string &interval,
                      const std::vector<Core::Candle> &candles) const;
};

