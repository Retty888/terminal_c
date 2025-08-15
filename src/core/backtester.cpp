#include "backtester.h"
#include <algorithm>
#include <cmath>
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

    // Calculate average win and loss
    double win_sum = 0.0;
    double loss_sum = 0.0;
    size_t win_count = 0;
    size_t loss_count = 0;
    for (const auto& t : result.trades) {
        if (t.pnl > 0) {
            win_sum += t.pnl;
            ++win_count;
        } else if (t.pnl < 0) {
            loss_sum += -t.pnl;
            ++loss_count;
        }
    }
    if (win_count > 0) {
        result.avg_win = win_sum / static_cast<double>(win_count);
    }
    if (loss_count > 0) {
        result.avg_loss = loss_sum / static_cast<double>(loss_count);
    }

    // Calculate max drawdown
    double peak = result.equity_curve.empty() ? 0.0 : result.equity_curve[0];
    double max_dd = 0.0;
    for (double equity : result.equity_curve) {
        peak = std::max(peak, equity);
        max_dd = std::max(max_dd, peak - equity);
    }
    result.max_drawdown = max_dd;

    // Calculate Sharpe ratio
    if (result.equity_curve.size() > 1) {
        std::vector<double> returns;
        returns.reserve(result.equity_curve.size() - 1);
        for (size_t i = 1; i < result.equity_curve.size(); ++i) {
            returns.push_back(result.equity_curve[i] - result.equity_curve[i - 1]);
        }
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) /
                      static_cast<double>(returns.size());
        double sq_sum = 0.0;
        for (double r : returns) {
            double diff = r - mean;
            sq_sum += diff * diff;
        }
        double variance = sq_sum / static_cast<double>(returns.size());
        double stddev = std::sqrt(variance);
        if (stddev != 0.0) {
            result.sharpe_ratio = (mean / stddev) *
                                  std::sqrt(static_cast<double>(returns.size()));
        }
    }

    return result;
}

} // namespace Core

