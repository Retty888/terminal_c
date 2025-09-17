#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/logger.h"

namespace Config {

struct SignalConfig {
  std::string type{"sma_crossover"};
  std::size_t short_period{0};
  std::size_t long_period{0};
  std::map<std::string, double> params{};
};

struct ConfigData {
  std::vector<std::string> pairs{};
  Core::LogLevel log_level{Core::LogLevel::Info};
  bool log_to_file{true};
  bool log_to_console{true};
  std::string log_file{"terminal.log"};
  std::size_t candles_limit{5000};
  bool enable_chart{true};
  std::string chart_html_path{"resources/chart.html"};
  bool enable_streaming{false};
  bool save_journal_csv{true};
  int http_timeout_ms{15000};
  // Do not require TradingView/WebView chart by default; fallback to ImPlot if WebView isn't ready quickly.
  bool require_tv_chart{false};
  // Faster fallback if WebView is slow to initialize.
  int webview_ready_timeout_ms{5000};
  // Limit JS updates pushed into WebView; default 2000ms.
  int webview_throttle_ms{2000};
  SignalConfig signal{};
  std::string primary_provider{"hyperliquid"};
  std::optional<std::string> fallback_provider{};
};

} // namespace Config

