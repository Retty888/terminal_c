#include "core/backtester.h"
#include <vector>
#include <cmath>
#include <gtest/gtest.h>

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

TEST(BacktesterTest, BasicScenario) {
    using namespace Core;
    std::vector<Candle> candles;
    double closes[] = {10, 11, 12, 11, 13, 12};
    for (size_t i = 0; i < 6; ++i) {
        candles.emplace_back(static_cast<long long>(i), 0, 0, 0, closes[i], 0, 0, 0, 0, 0, 0, 0);
    }
    std::vector<int> signals = {1, 0, -1, 1, 0, -1};
    MockStrategy strategy(signals);

    Backtester backtester(candles, strategy);
    BacktestResult result = backtester.run();

    ASSERT_EQ(result.trades.size(), 2);
    EXPECT_EQ(result.trades[0].entry_index, 0);
    EXPECT_EQ(result.trades[0].exit_index, 2);
    EXPECT_EQ(result.trades[0].entry_price, 10);
    EXPECT_EQ(result.trades[0].exit_price, 12);
    EXPECT_EQ(result.trades[0].pnl, 2);

    EXPECT_EQ(result.trades[1].entry_index, 3);
    EXPECT_EQ(result.trades[1].exit_index, 5);
    EXPECT_EQ(result.trades[1].entry_price, 11);
    EXPECT_EQ(result.trades[1].exit_price, 12);
    EXPECT_EQ(result.trades[1].pnl, 1);

    EXPECT_EQ(result.total_pnl, 3);
    EXPECT_DOUBLE_EQ(result.win_rate, 1.0);

    std::vector<double> expected_equity = {0, 1, 2, 2, 4, 3};
    ASSERT_EQ(result.equity_curve.size(), expected_equity.size());
    for (size_t i = 0; i < expected_equity.size(); ++i) {
        EXPECT_DOUBLE_EQ(result.equity_curve[i], expected_equity[i]);
    }

    EXPECT_DOUBLE_EQ(result.max_drawdown, 1.0);
    double expected_sharpe = 3 * std::sqrt(5.0) / std::sqrt(26.0);
    EXPECT_NEAR(result.sharpe_ratio, expected_sharpe, 1e-9);
    EXPECT_DOUBLE_EQ(result.avg_win, 1.5);
    EXPECT_DOUBLE_EQ(result.avg_loss, 0.0);
}

TEST(BacktesterTest, OpenPositionClosedAutomatically) {
    using namespace Core;
    std::vector<Candle> candles;
    double closes[] = {10, 12, 11};
    for (size_t i = 0; i < 3; ++i) {
        candles.emplace_back(static_cast<long long>(i), 0, 0, 0, closes[i], 0, 0, 0, 0, 0, 0, 0);
    }
    std::vector<int> signals = {1, 0, 0};
    MockStrategy strategy(signals);

    Backtester backtester(candles, strategy);
    BacktestResult result = backtester.run();

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].entry_index, 0);
    EXPECT_EQ(result.trades[0].exit_index, 2);
    EXPECT_EQ(result.trades[0].entry_price, 10);
    EXPECT_EQ(result.trades[0].exit_price, 11);
    EXPECT_EQ(result.trades[0].pnl, 1);

    EXPECT_EQ(result.total_pnl, 1);
    EXPECT_DOUBLE_EQ(result.win_rate, 1.0);

    std::vector<double> expected_equity = {0, 2, 1};
    ASSERT_EQ(result.equity_curve.size(), expected_equity.size());
    for (size_t i = 0; i < expected_equity.size(); ++i) {
        EXPECT_DOUBLE_EQ(result.equity_curve[i], expected_equity[i]);
    }

    EXPECT_DOUBLE_EQ(result.max_drawdown, 1.0);
    double expected_sharpe = std::sqrt(2.0) / 3.0;
    EXPECT_NEAR(result.sharpe_ratio, expected_sharpe, 1e-9);
    EXPECT_DOUBLE_EQ(result.avg_win, 1.0);
    EXPECT_DOUBLE_EQ(result.avg_loss, 0.0);
}

