#pragma once

#include "candle.h"
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>

namespace Core {

class CandleManager {
public:
    CandleManager();
    explicit CandleManager(const std::filesystem::path& dir);

    // Saves a vector of candles to a CSV file.
    bool save_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles);

    // Appends new candles to an existing CSV file, skipping duplicates.
    bool append_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles);

    // Loads candles from a CSV file into a vector.
    std::vector<Candle> load_candles(const std::string& symbol, const std::string& interval);

    // Lists all locally stored candle data files (symbol_interval.csv).
    std::vector<std::string> list_stored_data();

    // Allows runtime configuration of the candle data directory
    void set_data_dir(const std::filesystem::path& dir);
    std::filesystem::path get_data_dir() const;

private:
    // Helper functions to get the full path for candle CSV and index files.
    std::filesystem::path get_candle_path(const std::string& symbol, const std::string& interval) const;
    std::filesystem::path get_index_path(const std::string& symbol, const std::string& interval) const;
    long long read_last_open_time(const std::string& symbol, const std::string& interval) const;
    void write_last_open_time(const std::string& symbol, const std::string& interval, long long open_time) const;

    std::filesystem::path data_dir_;
    mutable std::mutex mutex_;
};

} // namespace Core
