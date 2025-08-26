#pragma once

#include "candle.h"
#include <vector>
#include <string_view>

namespace Core {

void fill_missing(std::vector<Candle> &candles, long long interval_ms);

bool ParseLong(std::string_view s, long long &out);
bool ParseInt(std::string_view s, int &out);
bool ParseDouble(std::string_view s, double &out);

} // namespace Core

