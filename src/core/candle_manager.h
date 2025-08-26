#pragma once

#include "candle.h"
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <cstdint>

namespace Core {

class CandleManager {
public:
    CandleManager();
    explicit CandleManager(const std::filesystem::path& dir);

    // Saves a vector of candles to a CSV file. Optionally verifies the written data.
    bool save_candles(const std::string& symbol, const std::string& interval,
                      const std::vector<Candle>& candles, bool verify = true) const;

    // Appends new candles to an existing CSV file, skipping duplicates.
    bool append_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) const;

    // Validates existing candle data for a symbol/interval.
    bool validate_candles(const std::string& symbol, const std::string& interval) const;

    // Loads candles from a CSV file into a vector.
    std::vector<Candle> load_candles(const std::string& symbol, const std::string& interval) const;

    // Saves candles in JSON format to a separate file.
    bool save_candles_json(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) const;

    // Loads candles from a JSON file.
    std::vector<Candle> load_candles_from_json(const std::string& symbol, const std::string& interval) const;

    // Loads candles and converts them to JSON with timestamps "x" and
    // OHLC arrays "y" for candlestick charts. Supports basic pagination
    // via offset/limit to avoid serializing excessive data at once.
    nlohmann::json load_candles_json(const std::string& symbol,
                                     const std::string& interval,
                                     std::size_t offset = 0,
                                     std::size_t limit = 0) const;

    // Loads candles and converts them to TradingView compatible JSON
    // array with fields time/open/high/low/close/volume.
    nlohmann::json load_candles_tradingview(const std::string& symbol,
                                            const std::string& interval) const;

    // Removes all files with the given symbol prefix (symbol_*).
    bool remove_candles(const std::string& symbol) const;

    // Removes candle data for a specific symbol and interval.
    bool clear_interval(const std::string& symbol, const std::string& interval) const;

    // Returns size of the candle file for a symbol/interval in bytes.
    std::uintmax_t file_size(const std::string& symbol, const std::string& interval) const;

    // Lists all locally stored candle data files (symbol_interval.csv).
    std::vector<std::string> list_stored_data() const;

    // Allows runtime configuration of the candle data directory
    void set_data_dir(const std::filesystem::path& dir);
    std::filesystem::path get_data_dir() const;

private:
    // Helper functions to get the full path for candle CSV and index files.
    std::filesystem::path get_candle_path(const std::string& symbol, const std::string& interval) const;
    std::filesystem::path get_candle_json_path(const std::string& symbol, const std::string& interval) const;
    std::filesystem::path get_index_path(const std::string& symbol, const std::string& interval) const;
    long long read_last_open_time(const std::string& symbol, const std::string& interval) const;
    void write_last_open_time(const std::string& symbol, const std::string& interval, long long open_time) const;

    std::filesystem::path data_dir_;
    mutable std::mutex mutex_;
};

} // namespace Core
