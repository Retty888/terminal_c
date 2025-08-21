#pragma once

#include <map>
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
  std::string echarts_js_path{"third_party/echarts/echarts.min.js"};
  bool enable_streaming{false};
  SignalConfig signal{};
  std::string primary_provider{"binance"};
  std::string fallback_provider{"gateio"};
};

} // namespace Config

