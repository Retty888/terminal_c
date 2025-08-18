#include "candle_utils.h"

#include <utility>

namespace Core {

void fill_missing(std::vector<Candle> &candles, long long interval_ms) {
  if (candles.size() < 2 || interval_ms <= 0)
    return;
  std::vector<Candle> filled;
  filled.reserve(candles.size());
  for (std::size_t i = 0; i + 1 < candles.size(); ++i) {
    const auto &cur = candles[i];
    const auto &next = candles[i + 1];
    filled.push_back(cur);
    long long expected = cur.open_time + interval_ms;
    while (expected < next.open_time) {
      filled.emplace_back(expected, cur.close, cur.close, cur.close, cur.close,
                         0.0, expected + interval_ms - 1, 0.0, 0, 0.0, 0.0,
                         0.0);
      expected += interval_ms;
    }
  }
  filled.push_back(candles.back());
  candles = std::move(filled);
}

} // namespace Core

