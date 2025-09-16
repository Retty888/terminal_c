#pragma once

#include <vector>
#include <optional>
#include <cstdint>
#include <string>

namespace Core { struct Candle; }

namespace llintraday {

struct Record {
    int64_t ll_time_ms = 0;
    double  ll_price = 0;
    std::optional<int64_t> prev_ph_time_ms;
    std::optional<double>  prev_ph_price;

    std::optional<int64_t> hh_time_ms;
    std::optional<int>     mins_ll_to_hh;

    std::optional<int64_t> ema200_cross_time_ms;
    std::optional<int>     mins_ll_to_ema200;

    std::optional<int64_t> retest_time_ms;
    std::optional<int>     mins_ll_to_retest;
};

struct SeriesStats {
    int count = 0;
    std::optional<double> median_min, p25_min, p75_min, mean_min, max_min;
};

struct Summary {
    SeriesStats mins_ll_to_hh;
    SeriesStats mins_ll_to_ema200;
    SeriesStats mins_ll_to_retest;
    int left = 3, right = 3;
    int ema_fast = 50, ema_slow = 200;
    double retest_eps = 0.001;
    int lookahead_min = 720;
    size_t rows_used = 0;
};

struct Params {
    int left = 3;
    int right = 3;
    int ema_fast = 50;
    int ema_slow = 200;
    double retest_eps = 0.001;  // 0.1%
    int lookahead_min = 720;    // 12h
};

struct Result {
    std::vector<Record> records;
    Summary summary;
};

Result analyze_core_candles(const std::vector<Core::Candle>& v, const Params& P);
bool write_records_csv(const std::string& path, const std::vector<Record>& R);
bool write_summary_json(const std::string& path, const Summary& S);

} // namespace llintraday

