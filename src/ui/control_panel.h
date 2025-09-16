#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "core/candle.h"
#include "services/data_service.h"

struct AppStatus;

struct PairItem {
  std::string name;
  bool visible;
};

float DrawControlPanel(
    std::vector<PairItem> &pairs, std::vector<std::string> &selected_pairs,
    std::string &active_pair, const std::vector<std::string> &intervals,
    std::string &selected_interval,
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>
        &all_candles,
    const std::function<void()> &save_pairs,
    const std::vector<std::string> &exchange_pairs, AppStatus &status,
    std::mutex &status_mutex, DataService &data_service,
    const std::function<void(const std::string &)> &cancel_pair,
    bool &show_analytics_window, bool &show_journal_window,
    bool &show_backtest_window);
