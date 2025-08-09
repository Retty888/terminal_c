#pragma once

#include <string>
#include <map>
#include <vector>

#include "core/candle.h"

void DrawAnalyticsWindow(
    const std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>& all_candles,
    const std::string& active_pair,
    const std::string& selected_interval);

