#include "candle_manager.h"

#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <charconv>
#include <string_view>
#include <array>
#include <nlohmann/json.hpp>
#include "logger.h"

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace Core {

namespace {

std::filesystem::path executable_dir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer, buffer + len).parent_path();
#elif __APPLE__
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
        return std::filesystem::path(path).parent_path();
    return std::filesystem::current_path();
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1)
        return std::filesystem::path(std::string(result, count)).parent_path();
    return std::filesystem::current_path();
#endif
}

std::filesystem::path resolve_config_path() {
    if (const char* env_cfg = std::getenv("CANDLE_CONFIG_PATH")) {
        return std::filesystem::path(env_cfg);
    }
    auto exe_dir = executable_dir();
    std::array<std::filesystem::path,3> candidates = {
        exe_dir / "config.json",
        exe_dir.parent_path() / "config.json",
        std::filesystem::current_path() / "config.json"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    return candidates[0];
}

std::filesystem::path resolve_data_dir() {
    if (const char* env_dir = std::getenv("CANDLE_DATA_DIR")) {
        std::filesystem::path dir(env_dir);
        std::filesystem::create_directories(dir);
        return dir;
    }

    auto cfg_path = resolve_config_path();
    nlohmann::json j;
    std::filesystem::path dir;

    if (std::ifstream cfg(cfg_path); cfg.is_open()) {
        try {
            cfg >> j;
            if (j.contains("data_dir") && j["data_dir"].is_string()) {
                dir = j["data_dir"].get<std::string>();
            }
        } catch (const std::exception& e) {
            Logger::instance().error("Failed to parse " + cfg_path.string() + ": " + e.what());
        }
    }

    if (dir.empty()) {
        const char* home = nullptr;
#ifdef _WIN32
        home = std::getenv("USERPROFILE");
#else
        home = std::getenv("HOME");
#endif
        if (home) {
            dir = std::filesystem::path(home) / "candle_data";
        } else {
            dir = std::filesystem::current_path() / "candle_data";
        }
        j["data_dir"] = dir.string();
        std::ofstream out(cfg_path);
        if (out.is_open()) {
            out << j.dump(4);
        }
    }

    std::filesystem::create_directories(dir);
    return dir;
}

} // unnamed namespace

CandleManager::CandleManager() : data_dir_(resolve_data_dir()) {}

CandleManager::CandleManager(const std::filesystem::path& dir) : data_dir_(dir) {
    std::filesystem::create_directories(data_dir_);
}

void CandleManager::set_data_dir(const std::filesystem::path& dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_dir_ = dir;
    std::filesystem::create_directories(data_dir_);
}

std::filesystem::path CandleManager::get_data_dir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_dir_;
}

std::filesystem::path CandleManager::get_candle_path(const std::string& symbol, const std::string& interval) const {
    std::filesystem::create_directories(data_dir_); // Ensure directory exists
    std::string filename = symbol + "_" + interval + ".csv";
    return data_dir_ / filename;
}

std::filesystem::path CandleManager::get_index_path(const std::string& symbol, const std::string& interval) const {
    std::filesystem::create_directories(data_dir_);
    std::string filename = symbol + "_" + interval + ".idx";
    return data_dir_ / filename;
}

long long CandleManager::read_last_open_time(const std::string& symbol, const std::string& interval) const {
    std::filesystem::path idx_path = get_index_path(symbol, interval);
    long long last = -1;
    if (std::filesystem::exists(idx_path)) {
        std::ifstream idx(idx_path);
        if (idx.is_open()) {
            idx >> last;
            return last;
        }
    }

    std::filesystem::path csv_path = get_candle_path(symbol, interval);
    if (std::filesystem::exists(csv_path)) {
        std::ifstream csv(csv_path);
        if (csv.is_open()) {
            std::string line, last_line;
            while (std::getline(csv, line)) {
                if (!line.empty()) last_line = line;
            }
            csv.close();
            if (!last_line.empty()) {
                std::string_view sv(last_line);
                size_t comma = sv.find(',');
                if (comma != std::string_view::npos)
                    sv = sv.substr(0, comma);
                long long value = -1;
                auto res = std::from_chars(sv.data(), sv.data() + sv.size(), value);
                if (res.ec == std::errc() && res.ptr == sv.data() + sv.size()) {
                    last = value;
                    std::ofstream idx_new(idx_path);
                    if (idx_new.is_open()) idx_new << last;
                } else {
                    Logger::instance().error("Failed to parse last open time: " + std::string(last_line));
                }
            }
        }
    }
    return last;
}

void CandleManager::write_last_open_time(const std::string& symbol, const std::string& interval, long long t) const {
    if (t < 0) return;
    std::filesystem::path idx_path = get_index_path(symbol, interval);
    std::ofstream idx(idx_path, std::ios::trunc);
    if (idx.is_open()) {
        idx << t;
    }
}

bool CandleManager::save_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path path_to_save = get_candle_path(symbol, interval);
    std::ofstream file(path_to_save);

    if (!file.is_open()) {
        Logger::instance().error("Could not open file for writing: " + path_to_save.string());
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
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path path_to_save = get_candle_path(symbol, interval);
    long long last_open_time = read_last_open_time(symbol, interval);

    std::ofstream file(path_to_save, std::ios::app);
    if (!file.is_open()) {
        Logger::instance().error("Could not open file for appending: " + path_to_save.string());
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
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path path = get_candle_path(symbol, interval);
    std::vector<Candle> candles;

    if (!std::filesystem::exists(path)) {
        return candles; // Return empty vector if file doesn't exist
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::instance().error("Could not open file for reading: " + path.string());
        return candles;
    }

    std::string line;
    std::getline(file, line); // Skip header

    auto parse_ll = [](std::string_view s, long long& out) {
        auto res = std::from_chars(s.data(), s.data() + s.size(), out);
        return res.ec == std::errc() && res.ptr == s.data() + s.size();
    };
    auto parse_int = [](std::string_view s, int& out) {
        auto res = std::from_chars(s.data(), s.data() + s.size(), out);
        return res.ec == std::errc() && res.ptr == s.data() + s.size();
    };
    auto parse_double = [](std::string_view s, double& out) {
        auto res = std::from_chars(s.data(), s.data() + s.size(), out);
        return res.ec == std::errc() && res.ptr == s.data() + s.size();
    };

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::string_view sv(line);
        std::array<std::string_view, 12> fields{};
        size_t start = 0;
        size_t idx = 0;
        while (idx < fields.size()) {
            size_t comma = sv.find(',', start);
            if (comma == std::string_view::npos) {
                fields[idx++] = sv.substr(start);
                break;
            }
            fields[idx++] = sv.substr(start, comma - start);
            start = comma + 1;
        }

        if (idx != fields.size()) {
            Logger::instance().warn("Malformed candle line: " + line);
            continue;
        }

        Candle candle;
        if (parse_ll(fields[0], candle.open_time) &&
            parse_double(fields[1], candle.open) &&
            parse_double(fields[2], candle.high) &&
            parse_double(fields[3], candle.low) &&
            parse_double(fields[4], candle.close) &&
            parse_double(fields[5], candle.volume) &&
            parse_ll(fields[6], candle.close_time) &&
            parse_double(fields[7], candle.quote_asset_volume) &&
            parse_int(fields[8], candle.number_of_trades) &&
            parse_double(fields[9], candle.taker_buy_base_asset_volume) &&
            parse_double(fields[10], candle.taker_buy_quote_asset_volume) &&
            parse_double(fields[11], candle.ignore)) {
            candles.push_back(candle);
        } else {
            Logger::instance().error("Failed to parse candle line: " + line);
        }
    }

    file.close();
    return candles;
}

std::vector<std::string> CandleManager::list_stored_data() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> stored_files;
    if (std::filesystem::exists(data_dir_) && std::filesystem::is_directory(data_dir_)) {
        for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
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

