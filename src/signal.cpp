#include "signal_manager.h"
#include <cmath>
#include <numeric>
#include <limits>

namespace Core {

std::vector<double> SignalManager::calculate_sma(const std::vector<Candle>& candles, int period) {
    std::vector<double> result(candles.size(), std::numeric_limits<double>::quiet_NaN());
    if (period <= 0 || candles.size() < static_cast<size_t>(period))
        return result;

    double sum = 0.0;
    for (size_t i = 0; i < candles.size(); ++i) {
        sum += candles[i].close;
        if (i >= static_cast<size_t>(period))
            sum -= candles[i - period].close;
        if (i >= static_cast<size_t>(period - 1))
            result[i] = sum / period;
    }
    return result;
}

std::vector<double> SignalManager::calculate_rsi(const std::vector<Candle>& candles, int period) {
    std::vector<double> rsi(candles.size(), std::numeric_limits<double>::quiet_NaN());
    if (period <= 0 || candles.size() <= static_cast<size_t>(period))
        return rsi;

    std::vector<double> gains(candles.size(), 0.0);
    std::vector<double> losses(candles.size(), 0.0);

    for (size_t i = 1; i < candles.size(); ++i) {
        double diff = candles[i].close - candles[i - 1].close;
        if (diff > 0) {
            gains[i] = diff;
        } else {
            losses[i] = -diff;
        }
    }

    double avg_gain = std::accumulate(gains.begin() + 1, gains.begin() + period + 1, 0.0) / period;
    double avg_loss = std::accumulate(losses.begin() + 1, losses.begin() + period + 1, 0.0) / period;

    if (avg_loss == 0) {
        rsi[period] = 100.0;
    } else {
        double rs = avg_gain / avg_loss;
        rsi[period] = 100.0 - (100.0 / (1.0 + rs));
    }

    for (size_t i = period + 1; i < candles.size(); ++i) {
        avg_gain = (avg_gain * (period - 1) + gains[i]) / period;
        avg_loss = (avg_loss * (period - 1) + losses[i]) / period;

        if (avg_loss == 0) {
            rsi[i] = 100.0;
        } else {
            double rs = avg_gain / avg_loss;
            rsi[i] = 100.0 - (100.0 / (1.0 + rs));
        }
    }

    return rsi;
}

std::vector<bool> SignalManager::generate_buy_signal(const std::vector<Candle>& candles,
                                                     const std::vector<double>& sma,
                                                     const std::vector<double>& rsi) {
    size_t n = candles.size();
    std::vector<bool> signals(n, false);
    for (size_t i = 0; i < n && i < sma.size() && i < rsi.size(); ++i) {
        if (!std::isnan(sma[i]) && !std::isnan(rsi[i]) &&
            candles[i].close > sma[i] && rsi[i] < 30.0) {
            signals[i] = true;
        }
    }
    return signals;
}

std::vector<bool> SignalManager::generate_sell_signal(const std::vector<Candle>& candles,
                                                      const std::vector<double>& sma,
                                                      const std::vector<double>& rsi) {
    size_t n = candles.size();
    std::vector<bool> signals(n, false);
    for (size_t i = 0; i < n && i < sma.size() && i < rsi.size(); ++i) {
        if (!std::isnan(sma[i]) && !std::isnan(rsi[i]) &&
            candles[i].close < sma[i] && rsi[i] > 70.0) {
            signals[i] = true;
        }
    }
    return signals;
}

} // namespace Core

