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

    // Scenario: open position at the end should be closed automatically
    {
        std::vector<Candle> candles2;
        double closes2[] = {10, 12, 11};
        for (size_t i = 0; i < 3; ++i) {
            candles2.emplace_back(static_cast<long long>(i), 0, 0, 0, closes2[i], 0, 0, 0, 0, 0, 0, 0);
        }

        std::vector<int> signals2 = {1, 0, 0};
        MockStrategy strategy2(signals2);

        Backtester backtester2(candles2, strategy2);
        BacktestResult result2 = backtester2.run();

        // A single trade that exits on the last candle
        assert(result2.trades.size() == 1);
        assert(result2.trades[0].entry_index == 0);
        assert(result2.trades[0].exit_index == 2);
        assert(result2.trades[0].entry_price == 10);
        assert(result2.trades[0].exit_price == 11);
        assert(result2.trades[0].pnl == 1);

        // PnL and win rate
        assert(result2.total_pnl == 1);
        assert(result2.win_rate == 1.0);

        // Equity curve should reflect the final realized PnL
        std::vector<double> expected_equity2 = {0, 2, 1};
        assert(result2.equity_curve.size() == expected_equity2.size());
        for (size_t i = 0; i < expected_equity2.size(); ++i) {
            assert(result2.equity_curve[i] == expected_equity2[i]);
        }
    }

    return 0;
}

