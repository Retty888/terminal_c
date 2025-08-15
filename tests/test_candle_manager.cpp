#include "core/candle_manager.h"
#include <vector>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

TEST(CandleManagerTest, SaveLoadAndAppend) {
    using namespace Core;
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "candle_manager_test";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);
    CandleManager::set_data_dir(test_dir);

    std::vector<Candle> candles;
    candles.emplace_back(1,1,1,1,1,1,1,1,1,1,1,0);
    bool saved = CandleManager::save_candles("TEST","1m",candles);
    EXPECT_TRUE(saved);

    auto loaded = CandleManager::load_candles("TEST","1m");
    ASSERT_EQ(loaded.size(), 1);
    EXPECT_EQ(loaded[0].close, 1);

    auto list = CandleManager::list_stored_data();
    EXPECT_FALSE(list.empty());

    std::vector<Candle> more;
    more.emplace_back(2,2,2,2,2,2,2,2,2,2,2,0);
    bool appended = CandleManager::append_candles("TEST","1m",more);
    EXPECT_TRUE(appended);

    loaded = CandleManager::load_candles("TEST","1m");
    ASSERT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded[1].open_time, 2);

    bool appended_dup = CandleManager::append_candles("TEST","1m",more);
    EXPECT_TRUE(appended_dup);
    loaded = CandleManager::load_candles("TEST","1m");
    EXPECT_EQ(loaded.size(), 2); // no duplicates

    std::filesystem::path idx_path = test_dir / "TEST_1m.idx";
    EXPECT_TRUE(std::filesystem::exists(idx_path));
    std::ifstream idx(idx_path);
    long long idx_time = -1;
    idx >> idx_time;
    EXPECT_EQ(idx_time, 2);
    idx.close();

    std::filesystem::remove(idx_path);
    bool appended_dup_no_idx = CandleManager::append_candles("TEST","1m",more);
    EXPECT_TRUE(appended_dup_no_idx);
    loaded = CandleManager::load_candles("TEST","1m");
    EXPECT_EQ(loaded.size(), 2);
    idx.open(idx_path);
    idx_time = -1;
    idx >> idx_time;
    EXPECT_EQ(idx_time, 2);
    idx.close();

    std::vector<Candle> next;
    next.emplace_back(3,3,3,3,3,3,3,3,3,3,3,0);
    bool appended_next = CandleManager::append_candles("TEST","1m",next);
    EXPECT_TRUE(appended_next);
    loaded = CandleManager::load_candles("TEST","1m");
    ASSERT_EQ(loaded.size(), 3);
    idx.open(idx_path);
    idx_time = -1;
    idx >> idx_time;
    EXPECT_EQ(idx_time, 3);
    idx.close();

    std::filesystem::remove_all(test_dir);
}

