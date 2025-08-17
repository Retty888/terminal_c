#pragma once

#include <vector>
#include <string>
#include <map>

#include "core/candle.h"
#include "app.h"
#include "ui/signal_entry.h"

void DrawSignalsWindow(
    std::string& strategy,
    int& short_period,
    int& long_period,
    double& oversold,
    double& overbought,
    bool& show_on_chart,
    std::vector<SignalEntry>& signal_entries,
    std::vector<App::AppContext::TradeEvent>& trades,
    const std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>& all_candles,
    const std::string& active_pair,
    const std::string& selected_interval,
    AppStatus& status);

