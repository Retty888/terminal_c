#include "core/data_fetcher.h"
#include <gtest/gtest.h>
#include <chrono>

TEST(DataFetcherTest, LatestCandleNoLag) {
    using namespace Core;
    long long interval_ms = 60LL * 1000LL; // 1 minute
    auto res = DataFetcher::fetch_klines(
        "BTCUSDT", "1m", 1, 3,
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(0));

    if (res.error != FetchError::None) {
        GTEST_SKIP() << "Network error";
    }
    ASSERT_EQ(res.candles.size(), 1u);

    auto now = std::chrono::system_clock::now();
    long long current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now.time_since_epoch())
                               .count();
    long long expected_open = current_ms / interval_ms * interval_ms - interval_ms;
    EXPECT_EQ(res.candles[0].open_time, expected_open);
    EXPECT_EQ(res.candles[0].close_time, expected_open + interval_ms - 1);
}

TEST(DataFetcherTest, AsyncLatestCandleNoLag) {
    using namespace Core;
    long long interval_ms = 60LL * 1000LL; // 1 minute
    auto fut = DataFetcher::fetch_klines_async(
        "BTCUSDT", "1m", 1, 3,
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(0));
    auto res = fut.get();

    if (res.error != FetchError::None) {
        GTEST_SKIP() << "Network error";
    }
    ASSERT_EQ(res.candles.size(), 1u);

    auto now = std::chrono::system_clock::now();
    long long current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now.time_since_epoch())
                               .count();
    long long expected_open = current_ms / interval_ms * interval_ms - interval_ms;
    EXPECT_EQ(res.candles[0].open_time, expected_open);
    EXPECT_EQ(res.candles[0].close_time, expected_open + interval_ms - 1);
}
