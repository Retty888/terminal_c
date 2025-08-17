#pragma once

#include <string>
#include <vector>
#include <map>

#include "core/candle.h"
#include "journal.h"
#include "core/backtester.h"

void DrawChartWindow(
    const std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>& all_candles,
    std::string& active_pair,
    std::string& active_interval,
    const std::vector<std::string>& pair_list,
    const std::vector<std::string>& interval_list,
    bool show_on_chart,
    const std::vector<double>& buy_times,
    const std::vector<double>& buy_prices,
    const std::vector<double>& sell_times,
    const std::vector<double>& sell_prices,
    const Journal::Journal& journal,
    const Core::BacktestResult& last_result);

// Reset cached plot data for a specific trading pair and interval so that
// the next draw rebuilds it from the updated candle list.
void InvalidateCache(const std::string& pair, const std::string& interval);

