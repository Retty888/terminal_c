#include "candle_manager.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <charconv>
#include <string_view>
#include <system_error>
#include "core/logger.h"
#include "interval_utils.h"
#include "core/data_dir.h"
#include "candle_utils.h"

namespace Core {


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

std::filesystem::path CandleManager::get_candle_json_path(const std::string& symbol, const std::string& interval) const {
    std::filesystem::create_directories(data_dir_);
    std::string filename = symbol + "_" + interval + ".json";
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

bool CandleManager::save_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) const {
    {
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
    }

    if (!candles.empty()) {
        auto loaded = load_candles(symbol, interval);
        if (loaded.size() >= candles.size()) {
            const auto& orig = candles.back();
            const auto& read = loaded[candles.size() - 1];
            if (orig.open_time != read.open_time ||
                orig.open != read.open ||
                orig.high != read.high ||
                orig.low != read.low ||
                orig.close != read.close ||
                orig.volume != read.volume) {
                Logger::instance().warn("Mismatch after save/load for " + symbol + " " + interval);
            }
        } else {
            Logger::instance().warn("Loaded fewer candles than saved for " + symbol + " " + interval);
        }
    }
    return true;
}

bool CandleManager::append_candles(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) const {
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

bool CandleManager::validate_candles(const std::string& symbol, const std::string& interval) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path path = get_candle_path(symbol, interval);
    if (!std::filesystem::exists(path)) {
        return true;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::instance().error("Could not open file for validation: " + path.string());
        return false;
    }

    std::string line;
    std::getline(file, line); // skip header

    auto parse_ll = [](std::string_view s, long long &out) {
        auto res = std::from_chars(s.data(), s.data() + s.size(), out);
        return res.ec == std::errc() && res.ptr == s.data() + s.size();
    };
    auto parse_int = [](std::string_view s, int &out) {
        auto res = std::from_chars(s.data(), s.data() + s.size(), out);
        return res.ec == std::errc() && res.ptr == s.data() + s.size();
    };
    auto parse_double = [](std::string_view s, double &out) {
        auto res = std::from_chars(s.data(), s.data() + s.size(), out);
        return res.ec == std::errc() && res.ptr == s.data() + s.size();
    };

    long long prev_open = -1;
    long long interval_ms = parse_interval(interval).count();
    if (interval_ms <= 0) {
        Logger::instance().warn("Could not determine interval '" + interval + "' for " + symbol);
        return false;
    }

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
            return false;
        }

        Candle c{};
        if (!(parse_ll(fields[0], c.open_time) &&
              parse_double(fields[1], c.open) &&
              parse_double(fields[2], c.high) &&
              parse_double(fields[3], c.low) &&
              parse_double(fields[4], c.close) &&
              parse_double(fields[5], c.volume) &&
              parse_ll(fields[6], c.close_time) &&
              parse_double(fields[7], c.quote_asset_volume) &&
              parse_int(fields[8], c.number_of_trades) &&
              parse_double(fields[9], c.taker_buy_base_asset_volume) &&
              parse_double(fields[10], c.taker_buy_quote_asset_volume) &&
              parse_double(fields[11], c.ignore))) {
            return false;
        }

        if (prev_open != -1 && c.open_time != prev_open + interval_ms) {
            Logger::instance().warn("Non-monotonic candle sequence in " + path.string());
            return false;
        }
        prev_open = c.open_time;
    }

    return true;
}

bool CandleManager::save_candles_json(const std::string& symbol, const std::string& interval, const std::vector<Candle>& candles) const {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::filesystem::path path_to_save = get_candle_json_path(symbol, interval);
        std::ofstream file(path_to_save);
        if (!file.is_open()) {
            Logger::instance().error("Could not open JSON file for writing: " + path_to_save.string());
            return false;
        }
        nlohmann::json j = nlohmann::json::array();
        for (const auto& c : candles) {
            j.push_back({
                {"open_time", c.open_time},
                {"open", c.open},
                {"high", c.high},
                {"low", c.low},
                {"close", c.close},
                {"volume", c.volume},
                {"close_time", c.close_time},
                {"quote_asset_volume", c.quote_asset_volume},
                {"number_of_trades", c.number_of_trades},
                {"taker_buy_base_asset_volume", c.taker_buy_base_asset_volume},
                {"taker_buy_quote_asset_volume", c.taker_buy_quote_asset_volume},
                {"ignore", c.ignore}
            });
        }
        file << j.dump();
        file.close();
        if (!candles.empty()) {
            write_last_open_time(symbol, interval, candles.back().open_time);
        }
    }

    if (!candles.empty()) {
        auto loaded = load_candles_from_json(symbol, interval);
        if (loaded.size() >= candles.size()) {
            const auto& orig = candles.back();
            const auto& read = loaded[candles.size() - 1];
            if (orig.open_time != read.open_time ||
                orig.open != read.open ||
                orig.high != read.high ||
                orig.low != read.low ||
                orig.close != read.close ||
                orig.volume != read.volume) {
                Logger::instance().warn("Mismatch after JSON save/load for " + symbol + " " + interval);
            }
        } else {
            Logger::instance().warn("Loaded fewer candles than saved (JSON) for " + symbol + " " + interval);
        }
    }
    return true;
}

std::vector<Candle> CandleManager::load_candles_from_json(const std::string& symbol, const std::string& interval) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path path = get_candle_json_path(symbol, interval);
    std::vector<Candle> candles;
    if (!std::filesystem::exists(path)) {
        return candles;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::instance().error("Could not open JSON file for reading: " + path.string());
        return candles;
    }
    nlohmann::json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Failed to parse JSON: ") + e.what());
        return candles;
    }
    for (const auto& item : j) {
        Candle c;
        c.open_time = item.value("open_time", 0LL);
        c.open = item.value("open", 0.0);
        c.high = item.value("high", 0.0);
        c.low = item.value("low", 0.0);
        c.close = item.value("close", 0.0);
        c.volume = item.value("volume", 0.0);
        c.close_time = item.value("close_time", 0LL);
        c.quote_asset_volume = item.value("quote_asset_volume", 0.0);
        c.number_of_trades = item.value("number_of_trades", 0);
        c.taker_buy_base_asset_volume = item.value("taker_buy_base_asset_volume", 0.0);
        c.taker_buy_quote_asset_volume = item.value("taker_buy_quote_asset_volume", 0.0);
        c.ignore = item.value("ignore", 0.0);
        candles.push_back(c);
    }
    file.close();

    auto interval_ms = parse_interval(interval).count();
    if (interval_ms > 0) {
        Core::fill_missing(candles, interval_ms);
    } else {
        Logger::instance().warn("Could not determine interval '" + interval + "' for " + symbol);
    }

    return candles;
}

std::vector<Candle> CandleManager::load_candles(const std::string& symbol, const std::string& interval) const {
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

    auto interval_ms = parse_interval(interval).count();
    if (interval_ms > 0) {
        Core::fill_missing(candles, interval_ms);
    } else {
        Logger::instance().warn("Could not determine interval '" + interval + "' for " + symbol);
    }

    return candles;
}

nlohmann::json CandleManager::load_candles_json(const std::string& symbol,
                                                const std::string& interval,
                                                std::size_t offset,
                                                std::size_t limit) const {
    auto candles = load_candles(symbol, interval);
    nlohmann::json x = nlohmann::json::array();
    nlohmann::json y = nlohmann::json::array();

    if (offset >= candles.size()) {
        return nlohmann::json{{"x", std::move(x)}, {"y", std::move(y)}};
    }

    std::size_t end = limit > 0 ? std::min(offset + limit, candles.size()) : candles.size();
    for (std::size_t i = offset; i < end; ++i) {
        const auto& c = candles[i];
        long long ms = c.open_time;
        std::time_t sec = ms / 1000;
        int millis = static_cast<int>(ms % 1000);
        std::tm tm = *std::gmtime(&sec);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%FT%T") << '.'
            << std::setw(3) << std::setfill('0') << millis << 'Z';
        x.push_back(oss.str());
        y.push_back({c.open, c.close, c.low, c.high});
    }
    return nlohmann::json{{"x", std::move(x)}, {"y", std::move(y)}};
}

nlohmann::json CandleManager::load_candles_tradingview(const std::string& symbol,
                                                        const std::string& interval) const {
    auto candles = load_candles(symbol, interval);
    nlohmann::json data = nlohmann::json::array();
    for (const auto& c : candles) {
        data.push_back({{"time", c.open_time / 1000},
                        {"open", c.open},
                        {"high", c.high},
                        {"low", c.low},
                        {"close", c.close},
                        {"volume", c.volume}});
    }
    return data;
}


bool CandleManager::remove_candles(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    bool success = true;
    if (std::filesystem::exists(data_dir_) && std::filesystem::is_directory(data_dir_)) {
        std::string prefix = symbol + "_";
        for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
            if (!entry.is_regular_file()) continue;
            const auto& path = entry.path();
            std::string filename = path.filename().string();
            if (filename.rfind(prefix, 0) == 0) {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                if (ec) {
                    Logger::instance().warn("Failed to remove " + path.string() + ": " + ec.message());
                    success = false;
                }
            }
        }
    }
    return success;
}

bool CandleManager::clear_interval(const std::string& symbol, const std::string& interval) const {
    std::lock_guard<std::mutex> lock(mutex_);
    bool success = true;
    std::error_code ec;
    std::filesystem::path csv_path = get_candle_path(symbol, interval);
    if (std::filesystem::exists(csv_path)) {
        std::filesystem::remove(csv_path, ec);
        if (ec) {
            Logger::instance().warn("Failed to remove " + csv_path.string() + ": " + ec.message());
            success = false;
        }
    }

    ec.clear();
    std::filesystem::path idx_path = get_index_path(symbol, interval);
    if (std::filesystem::exists(idx_path)) {
        std::filesystem::remove(idx_path, ec);
        if (ec) {
            Logger::instance().warn("Failed to remove " + idx_path.string() + ": " + ec.message());
            success = false;
        }
    }

    return success;
}

std::uintmax_t CandleManager::file_size(const std::string& symbol, const std::string& interval) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path csv_path = get_candle_path(symbol, interval);
    std::error_code ec;
    if (std::filesystem::exists(csv_path)) {
        auto size = std::filesystem::file_size(csv_path, ec);
        if (!ec)
            return size;
    }
    return 0;
}

std::vector<std::string> CandleManager::list_stored_data() const {
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

