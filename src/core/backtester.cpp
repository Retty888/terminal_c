#include "backtester.h"
#include <numeric>

namespace Core {

Backtester::Backtester(const std::vector<Candle>& candles, IStrategy& strategy)
    : m_candles(candles), m_strategy(strategy) {}

BacktestResult Backtester::run() {
    BacktestResult result;
    result.equity_curve.reserve(m_candles.size());
    bool in_position = false;
    double entry_price = 0.0;
    size_t entry_index = 0;
    double total_pnl = 0.0;
    size_t wins = 0;

    for (size_t i = 0; i < m_candles.size(); ++i) {
        int signal = m_strategy.generate_signal(m_candles, i);
        if (!in_position && signal > 0) {
            // enter long
            in_position = true;
            entry_price = m_candles[i].close;
            entry_index = i;
        } else if (in_position && signal < 0) {
            // exit long
            double exit_price = m_candles[i].close;
            double pnl = exit_price - entry_price;
            result.trades.push_back({entry_index, i, entry_price, exit_price, pnl});
            total_pnl += pnl;
            if (pnl > 0) ++wins;
            in_position = false;
            entry_price = 0.0;
            entry_index = 0;
        }

        // mark-to-market equity
        double equity = total_pnl;
        if (in_position) {
            equity += m_candles[i].close - entry_price;
        }
        result.equity_curve.push_back(equity);
    }

    // If still in a position, close it at the last available price
    if (in_position && !m_candles.empty()) {
        double exit_price = m_candles.back().close;
        double pnl = exit_price - entry_price;
        result.trades.push_back({entry_index, m_candles.size() - 1, entry_price, exit_price, pnl});
        total_pnl += pnl;
        if (pnl > 0) {
            ++wins;
        }
        if (!result.equity_curve.empty()) {
            result.equity_curve.back() = total_pnl;
        }
    }

    result.total_pnl = total_pnl;
    if (!result.trades.empty()) {
        result.win_rate = static_cast<double>(wins) / result.trades.size();
    }
    return result;
}

} // namespace Core

