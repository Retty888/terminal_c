#pragma once

#include "candle.h"
#include <vector>
#include <string_view>

namespace Core {

void fill_missing(std::vector<Candle> &candles, long long interval_ms);

// Ensure candles are sorted by open_time ascending, deduplicated (keep last
// occurrence), and with sane high/low relative to open/close.
void normalize_candles(std::vector<Candle> &candles);

// Merge 'add' into 'base' by open_time; values from 'add' override on
// duplicates. Result is normalized and sorted.
void merge_candles(std::vector<Candle> &base, const std::vector<Candle> &add);

bool ParseLong(std::string_view s, long long &out);
bool ParseInt(std::string_view s, int &out);
bool ParseDouble(std::string_view s, double &out);

} // namespace Core

