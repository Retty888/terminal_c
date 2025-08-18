#include "core/candle_manager.h"
#include <vector>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

TEST(CandleManagerTest, SaveLoadAndAppend) {
    using namespace Core;
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "candle_manager_test";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);
    CandleManager cm(test_dir);

    std::vector<Candle> candles;
    candles.emplace_back(1,1,1,1,1,1,1,1,1,1,1,0);
    bool saved = cm.save_candles("TEST","1m",candles);
    EXPECT_TRUE(saved);

    auto loaded = cm.load_candles("TEST","1m");
    ASSERT_EQ(loaded.size(), 1);
    EXPECT_EQ(loaded[0].close, 1);

    auto list = cm.list_stored_data();
    EXPECT_FALSE(list.empty());

    std::vector<Candle> more;
    more.emplace_back(2,2,2,2,2,2,2,2,2,2,2,0);
    bool appended = cm.append_candles("TEST","1m",more);
    EXPECT_TRUE(appended);

    loaded = cm.load_candles("TEST","1m");
    ASSERT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded[1].open_time, 2);

    bool appended_dup = cm.append_candles("TEST","1m",more);
    EXPECT_TRUE(appended_dup);
    loaded = cm.load_candles("TEST","1m");
    EXPECT_EQ(loaded.size(), 2); // no duplicates

    std::filesystem::path idx_path = test_dir / "TEST_1m.idx";
    EXPECT_TRUE(std::filesystem::exists(idx_path));
    std::ifstream idx(idx_path);
    long long idx_time = -1;
    idx >> idx_time;
    EXPECT_EQ(idx_time, 2);
    idx.close();

    std::filesystem::remove(idx_path);
    bool appended_dup_no_idx = cm.append_candles("TEST","1m",more);
    EXPECT_TRUE(appended_dup_no_idx);
    loaded = cm.load_candles("TEST","1m");
    EXPECT_EQ(loaded.size(), 2);
    idx.open(idx_path);
    idx_time = -1;
    idx >> idx_time;
    EXPECT_EQ(idx_time, 2);
    idx.close();

    std::vector<Candle> next;
    next.emplace_back(3,3,3,3,3,3,3,3,3,3,3,0);
    bool appended_next = cm.append_candles("TEST","1m",next);
    EXPECT_TRUE(appended_next);
    loaded = cm.load_candles("TEST","1m");
    ASSERT_EQ(loaded.size(), 3);
    idx.open(idx_path);
    idx_time = -1;
    idx >> idx_time;
    EXPECT_EQ(idx_time, 3);
    idx.close();

    std::filesystem::remove_all(test_dir);
}

TEST(CandleManagerTest, LoadCandlesJsonReturnsOHLC) {
    using namespace Core;
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "cm_json_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    CandleManager cm(dir);

    std::vector<Candle> candles = {
        {0,10.0,20.0,5.0,15.0,0.0,59999,0.0,0,0.0,0.0,0.0},
        {60000,12.0,22.0,8.0,18.0,0.0,119999,0.0,0,0.0,0.0,0.0}
    };
    cm.save_candles("TEST","1m",candles);

    auto json = cm.load_candles_json("TEST","1m");
    nlohmann::json expected = {
        {"x", {0, 60000}},
        {"y", {
            {10.0, 15.0, 5.0, 20.0},
            {12.0, 18.0, 8.0, 22.0}
        }}
    };
    EXPECT_EQ(json, expected);

    std::filesystem::remove_all(dir);
}

