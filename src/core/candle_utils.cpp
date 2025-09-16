#include "candle_utils.h"

#include <utility>
#include <charconv>
#include <algorithm>

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

static void fix_high_low(Candle &c) {
  double mx = std::max({c.open, c.close, c.high});
  double mn = std::min({c.open, c.close, c.low});
  if (c.high < mx) c.high = mx;
  if (c.low > mn) c.low = mn;
}

void normalize_candles(std::vector<Candle> &candles) {
  if (candles.empty()) return;
  // Sort by open_time; stable to keep later duplicates later
  std::stable_sort(candles.begin(), candles.end(),
                   [](const Candle &a, const Candle &b) { return a.open_time < b.open_time; });
  // Dedup keeping last occurrence
  std::vector<Candle> out;
  out.reserve(candles.size());
  long long prev_ts = std::numeric_limits<long long>::min();
  for (size_t i = 0; i < candles.size(); ++i) {
    if (!out.empty() && out.back().open_time == candles[i].open_time) {
      out.back() = candles[i];
    } else {
      out.push_back(candles[i]);
    }
  }
  for (auto &c : out) fix_high_low(c);
  candles.swap(out);
}

void merge_candles(std::vector<Candle> &base, const std::vector<Candle> &add) {
  if (add.empty()) { normalize_candles(base); return; }
  std::vector<Candle> merged;
  merged.reserve(base.size() + add.size());
  merged.insert(merged.end(), base.begin(), base.end());
  merged.insert(merged.end(), add.begin(), add.end());
  normalize_candles(merged);
  base.swap(merged);
}

bool ParseLong(std::string_view s, long long &out) {
  auto res = std::from_chars(s.data(), s.data() + s.size(), out);
  return res.ec == std::errc() && res.ptr == s.data() + s.size();
}

bool ParseInt(std::string_view s, int &out) {
  auto res = std::from_chars(s.data(), s.data() + s.size(), out);
  return res.ec == std::errc() && res.ptr == s.data() + s.size();
}

bool ParseDouble(std::string_view s, double &out) {
  auto res = std::from_chars(s.data(), s.data() + s.size(), out);
  return res.ec == std::errc() && res.ptr == s.data() + s.size();
}

} // namespace Core

