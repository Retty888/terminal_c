#include "core/backtester.h"
#include <cassert>
#include <vector>

// Mock strategy generating predetermined signals
class MockStrategy : public Core::IStrategy {
public:
    explicit MockStrategy(const std::vector<int>& sigs) : signals(sigs) {}
    int generate_signal(const std::vector<Core::Candle>& /*candles*/, size_t index) override {
        if (index < signals.size()) return signals[index];
        return 0;
    }
private:
    std::vector<int> signals;
};

int main() {
    using namespace Core;
    // Construct a minimal candle sequence
    std::vector<Candle> candles;
    double closes[] = {10, 11, 12, 11, 13, 12};
    for (size_t i = 0; i < 6; ++i) {
        candles.emplace_back(static_cast<long long>(i), 0, 0, 0, closes[i], 0, 0, 0, 0, 0, 0, 0);
    }

    // Deterministic signals: buy, hold, sell, buy, hold, sell
    std::vector<int> signals = {1, 0, -1, 1, 0, -1};
    MockStrategy strategy(signals);

    Backtester backtester(candles, strategy);
    BacktestResult result = backtester.run();

    // Verify trades
    assert(result.trades.size() == 2);
    assert(result.trades[0].entry_index == 0);
    assert(result.trades[0].exit_index == 2);
    assert(result.trades[0].entry_price == 10);
    assert(result.trades[0].exit_price == 12);
    assert(result.trades[0].pnl == 2);

    assert(result.trades[1].entry_index == 3);
    assert(result.trades[1].exit_index == 5);
    assert(result.trades[1].entry_price == 11);
    assert(result.trades[1].exit_price == 12);
    assert(result.trades[1].pnl == 1);

    // Verify PnL and win rate
    assert(result.total_pnl == 3);
    assert(result.win_rate == 1.0);

    // Verify equity curve
    std::vector<double> expected_equity = {0, 1, 2, 2, 4, 3};
    assert(result.equity_curve.size() == expected_equity.size());
    for (size_t i = 0; i < expected_equity.size(); ++i) {
        assert(result.equity_curve[i] == expected_equity[i]);
    }

    return 0;
}

