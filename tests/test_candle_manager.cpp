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
        {"x", {"1970-01-01T00:00:00.000Z", "1970-01-01T00:01:00.000Z"}},
        {"y", {
            {10.0, 15.0, 5.0, 20.0},
            {12.0, 18.0, 8.0, 22.0}
        }}
    };
    EXPECT_EQ(json, expected);
    // Ensure pagination works
    auto paged = cm.load_candles_json("TEST", "1m", 1, 1);
    nlohmann::json expected_paged = {
        {"x", {"1970-01-01T00:01:00.000Z"}},
        {"y", {{12.0, 18.0, 8.0, 22.0}}}
    };
    EXPECT_EQ(paged, expected_paged);

    std::filesystem::remove_all(dir);
}

TEST(CandleManagerTest, LoadCandlesTradingViewFormat) {
    using namespace Core;
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "cm_tradingview_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    CandleManager cm(dir);

    std::vector<Candle> candles = {
        {0,10.0,20.0,5.0,15.0,1.0,59999,0.0,0,0.0,0.0,0.0},
        {60000,12.0,22.0,8.0,18.0,2.0,119999,0.0,0,0.0,0.0,0.0}
    };
    cm.save_candles("TEST","1m",candles);

    auto json = cm.load_candles_tradingview("TEST","1m");
    ASSERT_EQ(json.size(), 2);
    auto first = json[0];
    EXPECT_EQ(first["time"].get<long long>(), 0);
    EXPECT_DOUBLE_EQ(first["open"].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(first["high"].get<double>(), 20.0);
    EXPECT_DOUBLE_EQ(first["low"].get<double>(), 5.0);
    EXPECT_DOUBLE_EQ(first["close"].get<double>(), 15.0);

    std::filesystem::remove_all(dir);
}

TEST(CandleManagerTest, SaveLoadJsonFile) {
    using namespace Core;
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "cm_json_file_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    CandleManager cm(dir);

    std::vector<Candle> candles = {
        {1,1,1,1,1,1,1,1,1,1,1,0},
        {2,2,2,2,2,2,2,2,2,2,2,0}
    };

    ASSERT_TRUE(cm.save_candles_json("TEST","1m",candles));
    auto loaded = cm.load_candles_from_json("TEST","1m");
    ASSERT_EQ(loaded.size(), candles.size());
    EXPECT_EQ(loaded[1].open_time, candles[1].open_time);
    EXPECT_DOUBLE_EQ(loaded[1].open, candles[1].open);
    EXPECT_DOUBLE_EQ(loaded[1].close, candles[1].close);
    EXPECT_DOUBLE_EQ(loaded[1].volume, candles[1].volume);

    std::filesystem::remove_all(dir);
}

TEST(CandleManagerTest, AppendSkipsDuplicatesWithCorruptedIndex) {
    using namespace Core;
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "cm_corrupt_idx_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    CandleManager cm(dir);

    std::vector<Candle> base = {
        {1,1,1,1,1,1,1,1,1,1,1,0},
        {2,2,2,2,2,2,2,2,2,2,2,0}
    };
    ASSERT_TRUE(cm.save_candles("TEST","1m",base));

    // Corrupt index file
    std::filesystem::path idx_path = dir / "TEST_1m.idx";
    {
        std::ofstream idx(idx_path, std::ios::trunc);
        idx << "bad";
    }

    std::vector<Candle> more = {
        {2,2,2,2,2,2,2,2,2,2,2,0}, // duplicate
        {3,3,3,3,3,3,3,3,3,3,3,0}
    };
    ASSERT_TRUE(cm.append_candles("TEST","1m", more));

    auto loaded = cm.load_candles("TEST","1m");
    ASSERT_EQ(loaded.size(), 3);
    EXPECT_EQ(loaded.back().open_time, 3);

    long long idx_time = -1;
    std::ifstream idx(idx_path);
    idx >> idx_time;
    EXPECT_EQ(idx_time, 3);

    std::filesystem::remove_all(dir);
}

