#include "candle_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>


namespace Core {

namespace {

std::filesystem::path resolve_data_dir() {
    if (const char* env_dir = std::getenv("CANDLE_DATA_DIR")) {
        return std::filesystem::path(env_dir);
    }

    std::ifstream cfg("config.json");
    if (cfg.is_open()) {
        std::string content((std::istreambuf_iterator<char>(cfg)), std::istreambuf_iterator<char>());
        auto pos = content.find("\"data_dir\"");
        if (pos != std::string::npos) {
            pos = content.find(':', pos);
            if (pos != std::string::npos) {
                auto start = content.find('"', pos);
                auto end = content.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos && end > start) {
                    return std::filesystem::path(content.substr(start + 1, end - start - 1));
                }
            }
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

std::filesystem::path data_dir = resolve_data_dir();

std::filesystem::path get_index_path(const std::string& symbol, const std::string& interval) {
    std::filesystem::create_directories(data_dir);
    std::string filename = symbol + "_" + interval + ".idx";
    return data_dir / filename;
}

long long read_last_open_time(const std::string& symbol, const std::string& interval) {
    std::filesystem::path idx_path = get_index_path(symbol, interval);
    long long last = -1;
    if (std::filesystem::exists(idx_path)) {
        std::ifstream idx(idx_path);
        if (idx.is_open()) {
            idx >> last;
            return last;
        }
    }

    std::filesystem::path csv_path = data_dir / (symbol + "_" + interval + ".csv");
    if (std::filesystem::exists(csv_path)) {
        std::ifstream csv(csv_path);
        if (csv.is_open()) {
            std::string line, last_line;
            while (std::getline(csv, line)) {
                if (!line.empty()) last_line = line;
            }
            csv.close();
            if (!last_line.empty()) {
                std::stringstream ss(last_line);
                std::string first;
                if (std::getline(ss, first, ',')) {
                    try {
                        last = std::stoll(first);
                        std::ofstream idx_new(idx_path);
                        if (idx_new.is_open()) idx_new << last;
                    } catch (...) {
                        last = -1;
                    }
                }
            }
        }
    }
    return last;
}

void write_last_open_time(const std::string& symbol, const std::string& interval, long long t) {
    if (t < 0) return;
    std::filesystem::path idx_path = get_index_path(symbol, interval);
    std::ofstream idx(idx_path, std::ios::trunc);
    if (idx.is_open()) {
        idx << t;
    }
}

} // namespace

std::filesystem::path CandleManager::get_candle_path(const std::string& symbol, const std::string& interval) {
    std::filesystem::create_directories(data_dir); // Ensure directory exists
    std::string filename = symbol + "_" + interval + ".csv";
    return data_dir / filename;
}

void CandleManager::set_data_dir(const std::filesystem::path& dir) {
    data_dir = dir;
}

std::filesystem::path CandleManager::get_data_dir() {
    return data_dir;
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
    file.setf(std::ios::fixed);
    file << std::setprecision(8);

    // Write candle data
    for (const auto& candle : candles) {
        file << candle.open_time << ","
             << candle.open << ","
             << candle.high << ","
             << candle.low << ","
             << candle.close << ","
             << candle.volume << ","
             << candle.close_time << ","
             << candle.quote_asset_volume << ","
             << candle.number_of_trades << ","
             << candle.taker_buy_base_asset_volume << ","
             << candle.taker_buy_quote_asset_volume << ","
             << candle.ignore << "\n";
    }

    file.close();

    if (!candles.empty()) {
        write_last_open_time(symbol, interval, candles.back().open_time);
    }
    return true;
}

bool CandleManager::append_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) {
    std::filesystem::path path_to_save = get_candle_path(symbol, interval);
    long long last_open_time = read_last_open_time(symbol, interval);

    std::ofstream file(path_to_save, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for appending: " << path_to_save << std::endl;
        return false;
    }

    // If file newly created, write header
    if (std::filesystem::file_size(path_to_save) == 0) {
        file << "open_time,open,high,low,close,volume,close_time,quote_asset_volume,number_of_trades,taker_buy_base_asset_volume,taker_buy_quote_asset_volume,ignore\n";
    }

    for (const auto& candle : candles) {
        if (candle.open_time > last_open_time) {
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
            last_open_time = candle.open_time;
        }
    }

    file.close();
    write_last_open_time(symbol, interval, last_open_time);
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
    if (std::filesystem::exists(data_dir) && std::filesystem::is_directory(data_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".csv") {
                std::string filename = entry.path().filename().string();
                // Assuming filename format is SYMBOL_INTERVAL.csv
                // Extract SYMBOL and INTERVAL
                size_t last_underscore = filename.rfind('_');
                size_t dot_csv = filename.rfind(".csv");
                if (last_underscore != std::string::npos && dot_csv != std::string::npos && last_underscore < dot_csv) {
                    std::string symbol = filename.substr(0, last_underscore);
                    std::string interval = filename.substr(last_underscore + 1, dot_csv - (last_underscore + 1));
                    stored_files.push_back(symbol + " (" + interval + ")");
                } else {
                    stored_files.push_back(filename + " (unknown format)");
                }
            }
        }
    }
    return stored_files;
}

} // namespace Core
