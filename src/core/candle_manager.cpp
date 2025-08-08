#include "candle_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <nlohmann/json.hpp>

namespace Core {

namespace {

std::filesystem::path resolve_data_dir() {
    if (const char* env_dir = std::getenv("CANDLE_DATA_DIR")) {
        return std::filesystem::path(env_dir);
    }

    std::ifstream cfg("config.json");
    if (cfg.is_open()) {
        try {
            nlohmann::json j;
            cfg >> j;
            if (j.contains("data_dir") && j["data_dir"].is_string()) {
                return std::filesystem::path(j["data_dir"].get<std::string>());
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse config.json: " << e.what() << std::endl;
        }
    }

    const char* home = nullptr;
#ifdef _WIN32
    home = std::getenv("USERPROFILE");
#else
    home = std::getenv("HOME");
#endif
    if (home) {
        return std::filesystem::path(home) / "candle_data";
    }
    return std::filesystem::current_path() / "candle_data";
}

const std::filesystem::path DATA_DIR = resolve_data_dir();

} // namespace

std::filesystem::path CandleManager::get_candle_path(const std::string& symbol, const std::string& interval) {
    std::filesystem::create_directories(DATA_DIR); // Ensure directory exists
    std::string filename = symbol + "_" + interval + ".csv";
    return DATA_DIR / filename;
}

bool CandleManager::save_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) {
    std::filesystem::path path_to_save = get_candle_path(symbol, interval);
    std::ofstream file(path_to_save);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << path_to_save << std::endl;
        return false;
    }

    // Write header
    file << "open_time,open,high,low,close,volume,close_time,quote_asset_volume,number_of_trades,taker_buy_base_asset_volume,taker_buy_quote_asset_volume,ignore\n";

    // Write candle data
    for (const auto& candle : candles) {
        file << candle.open_time << ","
             << std::fixed << std::setprecision(8) << candle.open << ","
             << std::fixed << std::setprecision(8) << candle.high << ","
             << std::fixed << std::setprecision(8) << candle.low << ","
             << std::fixed << std::setprecision(8) << candle.close << ","
             << std::fixed << std::setprecision(8) << candle.volume << ","
             << candle.close_time << ","
             << std::fixed << std::setprecision(8) << candle.quote_asset_volume << ","
             << candle.number_of_trades << ","
             << std::fixed << std::setprecision(8) << candle.taker_buy_base_asset_volume << ","
             << std::fixed << std::setprecision(8) << candle.taker_buy_quote_asset_volume << ","
             << std::fixed << std::setprecision(8) << candle.ignore << "\n";
    }

    file.close();
    return true;
}

std::vector<Candle> CandleManager::load_candles(const std::string& symbol, const std::string& interval) {
    std::filesystem::path path = get_candle_path(symbol, interval);
    std::vector<Candle> candles;

    if (!std::filesystem::exists(path)) {
        // std::cerr << "Warning: Candle file not found: " << path << std::endl;
        return candles; // Return empty vector if file doesn't exist
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << path << std::endl;
        return candles;
    }

    std::string line;
    std::getline(file, line); // Skip header

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> seglist;

        while(std::getline(ss, segment, ',')) {
            seglist.push_back(segment);
        }

        // Ensure we have enough segments for a full candle
        if (seglist.size() == 12) {
            try {
                Candle candle;
                candle.open_time = std::stoll(seglist[0]);
                candle.open = std::stod(seglist[1]);
                candle.high = std::stod(seglist[2]);
                candle.low = std::stod(seglist[3]);
                candle.close = std::stod(seglist[4]);
                candle.volume = std::stod(seglist[5]);
                candle.close_time = std::stoll(seglist[6]);
                candle.quote_asset_volume = std::stod(seglist[7]);
                candle.number_of_trades = std::stoi(seglist[8]);
                candle.taker_buy_base_asset_volume = std::stod(seglist[9]);
                candle.taker_buy_quote_asset_volume = std::stod(seglist[10]);
                candle.ignore = std::stod(seglist[11]);
                candles.push_back(candle);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing candle line: " << line << " - " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Warning: Malformed candle line (expected 12 segments): " << line << std::endl;
        }
    }

    file.close();
    return candles;
}

std::vector<std::string> CandleManager::list_stored_data() {
    std::vector<std::string> stored_files;
    if (std::filesystem::exists(DATA_DIR) && std::filesystem::is_directory(DATA_DIR)) {
        for (const auto& entry : std::filesystem::directory_iterator(DATA_DIR)) {
            if (entry.is_regular_file() && entry.path().extension() == ".csv") {
                std::string filename = entry.path().filename().string();
                // Assuming filename format is SYMBOL_INTERVAL.csv
                // Extract SYMBOL and INTERVAL
                size_t last_underscore = filename.rfind('_');
                size_t dot_csv = filename.rfind(".csv");
                if (last_underscore != std::string::npos && dot_csv != std::string::npos && last_underscore < dot_csv) {
                    std::string symbol = filename.substr(0, last_underscore);
                    std::string interval = filename.substr(last_underscore + 1, dot_csv - (last_underscore + 1));
                    std::cout << "Processing file: " << filename << ", Extracted: Symbol=" << symbol << ", Interval=" << interval << std::endl; // Debug print
                    stored_files.push_back(symbol + " (" + interval + ")");
                } else {
                    std::cout << "Processing file: " << filename << ", Unknown format." << std::endl; // Debug print
                    stored_files.push_back(filename + " (unknown format)");
                }
            }
        }
    }
    return stored_files;
}

} // namespace Core
