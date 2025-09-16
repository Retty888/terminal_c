#pragma once

#include "fetch_result.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace Core {

class IDataProvider {
public:
  virtual ~IDataProvider() = default;

  virtual KlinesResult fetch_klines(const std::string &symbol, const std::string &interval,
                                  int limit = 500, int max_retries = 3,
                                  std::chrono::milliseconds retry_delay =
                                      std::chrono::milliseconds(1000)) const = 0;

  // Fetch candles in an explicit time range [start_ms, end_ms].
  virtual KlinesResult fetch_range(const std::string &symbol, const std::string &interval,
                                   long long start_ms, long long end_ms,
                                   int max_retries = 3,
                                   std::chrono::milliseconds retry_delay =
                                       std::chrono::milliseconds(1000)) const = 0;

  virtual SymbolsResult fetch_all_symbols(int max_retries = 3,
                                        std::chrono::milliseconds retry_delay =
                                            std::chrono::milliseconds(1000),
                                        std::size_t top_n = 100) const = 0;

  virtual IntervalsResult fetch_intervals(int max_retries = 3,
                                          std::chrono::milliseconds retry_delay =
                                              std::chrono::milliseconds(1000)) const = 0;
};

} // namespace Core
