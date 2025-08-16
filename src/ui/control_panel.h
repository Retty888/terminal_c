#pragma once

#include <vector>
#include <string>
#include <map>
#include <functional>

#include "core/candle.h"
#include "app.h"

struct PairItem {
    std::string name;
    bool visible;
};

void DrawControlPanel(
    std::vector<PairItem>& pairs,
    std::vector<std::string>& selected_pairs,
    std::string& active_pair,
    const std::vector<std::string>& intervals,
    std::string& selected_interval,
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>& all_candles,
    const std::function<void()>& save_pairs,
    const std::vector<std::string>& exchange_pairs,
    const AppStatus& status,
    const std::function<void(const std::string&)>& cancel_pair);

