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

double exponential_moving_average(const std::vector<Core::Candle>& candles,
                                  std::size_t index,
                                  std::size_t period) {
    if (period == 0 || index >= candles.size() || index + 1 < period) {
        return 0.0;
    }
    const double k = 2.0 / (static_cast<double>(period) + 1.0);
    // Start with SMA for the first period
    std::size_t start = index + 1 - period;
    double ema = simple_moving_average(candles, index - (period > 1 ? 1 : 0), period);
    for (std::size_t i = start; i <= index; ++i) {
        ema = (candles[i].close - ema) * k + ema;
    }
    return ema;
}

int ema_signal(const std::vector<Core::Candle>& candles,
               std::size_t index,
               std::size_t period) {
    if (index == 0) {
        return 0;
    }
    double prev_ema = exponential_moving_average(candles, index - 1, period);
    double curr_ema = exponential_moving_average(candles, index, period);
    double prev_price = candles[index - 1].close;
    double curr_price = candles[index].close;
    if (prev_price <= prev_ema && curr_price > curr_ema) {
        return 1;
    }
    if (prev_price >= prev_ema && curr_price < curr_ema) {
        return -1;
    }
    return 0;
}

  double relative_strength_index(const std::vector<Core::Candle>& candles,
                                 std::size_t index,
                                 std::size_t period) {
    if (period == 0 || index >= candles.size() || index < period) {
        return 0.0;
    }
    double gain = 0.0;
    double loss = 0.0;
    for (std::size_t i = index + 1 - period; i <= index; ++i) {
        double change = candles[i].close - candles[i - 1].close;
        if (change > 0) {
            gain += change;
        } else {
            loss -= change;
        }
    }
    double avg_gain = gain / static_cast<double>(period);
    double avg_loss = loss / static_cast<double>(period);
    if (avg_loss == 0.0) {
        return 100.0;
    }
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

int rsi_signal(const std::vector<Core::Candle>& candles,
               std::size_t index,
               std::size_t period,
               double oversold,
               double overbought) {
    double rsi = relative_strength_index(candles, index, period);
    if (rsi < oversold) {
        return 1;
    }
    if (rsi > overbought) {
        return -1;
    }
      return 0;
  }

  // Calculates the MACD line (EMA(fast) - EMA(slow)).
  double macd_line(const std::vector<Core::Candle>& candles,
                   std::size_t index,
                   std::size_t fast_period,
                   std::size_t slow_period) {
      if (fast_period == 0 || slow_period == 0 || fast_period >= slow_period) {
          return 0.0;
      }
      if (index >= candles.size() || index + 1 < slow_period) {
          return 0.0;
      }
      double fast = exponential_moving_average(candles, index, fast_period);
      double slow = exponential_moving_average(candles, index, slow_period);
      return fast - slow;
  }

  // Calculates the signal line of MACD (EMA of MACD values).
  double macd_signal_line(const std::vector<Core::Candle>& candles,
                          std::size_t index,
                          std::size_t fast_period,
                          std::size_t slow_period,
                          std::size_t signal_period) {
      if (signal_period == 0 || fast_period == 0 || slow_period == 0 ||
          fast_period >= slow_period) {
          return 0.0;
      }
      if (index >= candles.size() ||
          index + 1 < slow_period + signal_period - 1) {
          return 0.0;
      }

      std::size_t start = index + 1 - signal_period;
      std::vector<double> macd_vals;
      macd_vals.reserve(signal_period);
      for (std::size_t i = start; i <= index; ++i) {
          macd_vals.push_back(macd_line(candles, i, fast_period, slow_period));
      }

      const double k = 2.0 / (static_cast<double>(signal_period) + 1.0);
      double signal = macd_vals.front();
      for (std::size_t i = 1; i < macd_vals.size(); ++i) {
          signal = (macd_vals[i] - signal) * k + signal;
      }
      return signal;
  }

  MACDResult macd(const std::vector<Core::Candle>& candles,
                  std::size_t index,
                  std::size_t fast_period,
                  std::size_t slow_period,
                  std::size_t signal_period) {
      if (fast_period == 0 || slow_period == 0 || signal_period == 0 ||
          fast_period >= slow_period) {
          return {0.0, 0.0, 0.0};
      }
      if (index >= candles.size() ||
          index + 1 < slow_period + signal_period - 1) {
          return {0.0, 0.0, 0.0};
      }

      double macd_val = macd_line(candles, index, fast_period, slow_period);
      double signal = macd_signal_line(candles, index, fast_period, slow_period, signal_period);
      double histogram = macd_val - signal;
      return {macd_val, signal, histogram};
  }

int macd_signal(const std::vector<Core::Candle>& candles,
                std::size_t index,
                std::size_t fast_period,
                std::size_t slow_period,
                std::size_t signal_period) {
    if (index == 0) {
        return 0;
    }
    MACDResult prev = macd(candles, index - 1, fast_period, slow_period, signal_period);
    MACDResult curr = macd(candles, index, fast_period, slow_period, signal_period);
    if (prev.macd <= prev.signal && curr.macd > curr.signal) {
        return 1;
    }
    if (prev.macd >= prev.signal && curr.macd < curr.signal) {
        return -1;
    }
    return 0;
}

} // namespace Signal

