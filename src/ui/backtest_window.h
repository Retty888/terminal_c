#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/candle.h"

// DrawBacktestWindow renders a window allowing backtesting on the
// currently selected pair and interval. It displays summary statistics
// such as PnL, win rate and the equity curve.
void DrawBacktestWindow(
    const std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>& all_candles,
    const std::string& active_pair,
    const std::string& selected_interval);

