#pragma once

#include "candle.h"
#include <vector>

namespace Core {

void fill_missing(std::vector<Candle> &candles, long long interval_ms);

} // namespace Core

