#pragma once

#include "candle.h"
#include <string>
#include <vector>
#include <filesystem>

namespace Core {

class CandleManager {
public:
    // Saves a vector of candles to a CSV file.
    static bool save_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles);

    // Appends new candles to an existing CSV file, skipping duplicates.
    static bool append_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles);

    // Loads candles from a CSV file into a vector.
    static std::vector<Candle> load_candles(const std::string& symbol, const std::string& interval);

    // Lists all locally stored candle data files (symbol_interval.csv).
    static std::vector<std::string> list_stored_data();

    // Allows runtime configuration of the candle data directory
    static void set_data_dir(const std::filesystem::path& dir);
    static std::filesystem::path get_data_dir();

private:
    // Helper function to get the full path for a candle CSV file.
    static std::filesystem::path get_candle_path(const std::string& symbol, const std::string& interval);
};

} // namespace Core
