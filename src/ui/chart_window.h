#pragma once

#include <string>
#include <vector>
#include <map>

#include "core/candle.h"
#include "journal.h"
#include "core/backtester.h"

void DrawChartWindow(
    const std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>& all_candles,
    const std::string& active_pair,
    const std::string& active_interval,
    bool show_on_chart,
    const std::vector<double>& buy_times,
    const std::vector<double>& buy_prices,
    const std::vector<double>& sell_times,
    const std::vector<double>& sell_prices,
    const Journal::Journal& journal,
    const Core::BacktestResult& last_result);

