#pragma once

#include "candle.h"
#include <vector>
#include <memory>

namespace Core {

// Simple strategy interface
class IStrategy {
public:
    virtual ~IStrategy() = default;
    // Return 1 for buy, -1 for sell, 0 for hold
    virtual int generate_signal(const std::vector<Candle>& candles, size_t index) = 0;
};

struct Trade {
    size_t entry_index{};
    size_t exit_index{};
    double entry_price{};
    double exit_price{};
    double pnl{};
};

struct BacktestResult {
    std::vector<Trade> trades;
    std::vector<double> equity_curve; // cumulative PnL over time
    double total_pnl = 0.0;
    double win_rate = 0.0;
};

class Backtester {
public:
    Backtester(const std::vector<Candle>& candles, IStrategy& strategy);
    BacktestResult run();

private:
    const std::vector<Candle>& m_candles;
    IStrategy& m_strategy;
};

} // namespace Core

