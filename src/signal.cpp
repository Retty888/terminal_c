#include "signal.h"
#include <numeric>

namespace Signal {

double simple_moving_average(const std::vector<Core::Candle>& candles, std::size_t index, std::size_t period) {
    if (period == 0 || index >= candles.size() || index + 1 < period) {
        return 0.0;
    }
    const std::size_t start = index + 1 - period;
    const auto begin = candles.begin() + static_cast<long>(start);
    const auto end = candles.begin() + static_cast<long>(index) + 1;
    const double sum = std::accumulate(begin, end, 0.0, [](double acc, const Core::Candle& c) {
        return acc + c.close;
    });
    return sum / static_cast<double>(period);
}

int sma_crossover_signal(const std::vector<Core::Candle>& candles,
                         std::size_t index,
                         std::size_t short_period,
                         std::size_t long_period) {
    if (short_period == 0 || long_period == 0 || short_period >= long_period) {
        return 0;
    }
    if (index >= candles.size() || index + 1 < long_period) {
        return 0;
    }

    double short_prev = simple_moving_average(candles, index - 1, short_period);
    double long_prev  = simple_moving_average(candles, index - 1, long_period);
    double short_curr = simple_moving_average(candles, index, short_period);
    double long_curr  = simple_moving_average(candles, index, long_period);

    if (short_prev <= long_prev && short_curr > long_curr) {
        return 1; // Bullish crossover
    }
    if (short_prev >= long_prev && short_curr < long_curr) {
        return -1; // Bearish crossover
    }
    return 0;
}

} // namespace Signal

