#include <gtest/gtest.h>
#include "core/candle_manager.h"
#include <filesystem>
#include <fstream>

// Test fixture for CandleManager tests
class CandleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test data
        test_dir = std::filesystem::temp_directory_path() / "candle_manager_tests";
        std::filesystem::create_directories(test_dir);
        cm = new Core::CandleManager(test_dir);
    }

    void TearDown() override {
        // Clean up the temporary directory
        delete cm;
        std::filesystem::remove_all(test_dir);
    }

    Core::CandleManager* cm;
    std::filesystem::path test_dir;
};

TEST_F(CandleManagerTest, SaveAndLoad) {
    std::vector<Core::Candle> candles;
    candles.push_back(Core::Candle(1672531200000, 100.0, 110.0, 90.0, 105.0, 1000.0, 1672531259999));
    candles.push_back(Core::Candle(1672531260000, 105.0, 115.0, 100.0, 110.0, 1200.0, 1672531319999));

    cm->save_candles("BTCUSDT", "1m", candles);

    std::vector<Core::Candle> loaded_candles = cm->load_candles("BTCUSDT", "1m");

    ASSERT_EQ(candles.size(), loaded_candles.size());
    for (size_t i = 0; i < candles.size(); ++i) {
        EXPECT_EQ(candles[i].open_time, loaded_candles[i].open_time);
        EXPECT_EQ(candles[i].open, loaded_candles[i].open);
        EXPECT_EQ(candles[i].high, loaded_candles[i].high);
        EXPECT_EQ(candles[i].low, loaded_candles[i].low);
        EXPECT_EQ(candles[i].close, loaded_candles[i].close);
        EXPECT_EQ(candles[i].volume, loaded_candles[i].volume);
    }
}

TEST_F(CandleManagerTest, Append) {
    std::vector<Core::Candle> initial_candles;
    initial_candles.push_back(Core::Candle(1672531200000, 100.0, 110.0, 90.0, 105.0, 1000.0, 1672531259999));

    cm->save_candles("BTCUSDT", "1m", initial_candles);

    std::vector<Core::Candle> new_candles;
    new_candles.push_back(Core::Candle(1672531260000, 105.0, 115.0, 100.0, 110.0, 1200.0, 1672531319999));

    cm->append_candles("BTCUSDT", "1m", new_candles);

    std::vector<Core::Candle> loaded_candles = cm->load_candles("BTCUSDT", "1m");

    ASSERT_EQ(initial_candles.size() + new_candles.size(), loaded_candles.size());
}

TEST_F(CandleManagerTest, Validate) {
    // Create a valid candle file
    std::vector<Core::Candle> valid_candles;
    valid_candles.push_back(Core::Candle(1672531200000, 100.0, 110.0, 90.0, 105.0, 1000.0, 1672531259999));
    valid_candles.push_back(Core::Candle(1672531260000, 105.0, 115.0, 100.0, 110.0, 1200.0, 1672531319999));
    cm->save_candles("VALID", "1m", valid_candles);

    EXPECT_TRUE(cm->validate_candles("VALID", "1m"));

    // Create an invalid candle file
    std::vector<Core::Candle> invalid_candles;
    invalid_candles.push_back(Core::Candle(1672531200000, 100.0, 110.0, 90.0, 105.0, 1000.0, 1672531259999));
    invalid_candles.push_back(Core::Candle(1672531200000, 105.0, 115.0, 100.0, 110.0, 1200.0, 1672531319999)); // Duplicate open_time
    cm->save_candles("INVALID", "1m", invalid_candles);

    EXPECT_FALSE(cm->validate_candles("INVALID", "1m"));
}