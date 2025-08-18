#pragma once

#include <map>
#include <string>
#include <vector>

#include "app_context.h"
#include "core/candle.h"
#include "ui/signal_entry.h"

struct AppStatus;

void DrawSignalsWindow(
    std::string &strategy, int &short_period, int &long_period,
    double &oversold, double &overbought, bool &show_on_chart,
    std::vector<SignalEntry> &signal_entries,
    std::vector<AppContext::TradeEvent> &trades,
    const std::map<std::string,
                   std::map<std::string, std::vector<Core::Candle>>>
        &all_candles,
    const std::string &active_pair, const std::string &selected_interval,
    AppStatus &status);
