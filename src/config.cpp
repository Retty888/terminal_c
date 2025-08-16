#include "config.h"
#include "logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

namespace Config {

std::vector<std::string> load_selected_pairs(const std::string& filename) {
    std::ifstream in(filename);
    std::vector<std::string> result;

    if (in.is_open()) {
        try {
            nlohmann::json j;
            in >> j;
            if (j.contains("pairs") && j["pairs"].is_array()) {
                for (const auto& item : j["pairs"]) {
                    result.push_back(item.get<std::string>());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse config.json: " << e.what() << std::endl;
        }
    }
    return result;
}

void save_selected_pairs(const std::string& filename, const std::vector<std::string>& pairs) {
    nlohmann::json j;
    {
        std::ifstream in(filename);
        if (in.is_open()) {
            try {
                in >> j;
            } catch (...) {
            }
        }
    }
    j["pairs"] = pairs;
    std::ofstream out(filename);
    if (out.is_open()) {
        out << j.dump(4);
    } else {
        std::cerr << "Failed to open " << filename << " for writing" << std::endl;
    }
}

LogLevel load_min_log_level(const std::string& filename) {
    std::ifstream in(filename);
    if (in.is_open()) {
        try {
            nlohmann::json j;
            in >> j;
            if (j.contains("log_level") && j["log_level"].is_string()) {
                std::string level = j["log_level"].get<std::string>();
                if (level == "INFO")
                    return LogLevel::Info;
                if (level == "WARN" || level == "WARNING")
                    return LogLevel::Warning;
                if (level == "ERROR")
                    return LogLevel::Error;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse config.json: " << e.what() << std::endl;
        }
    }
    return LogLevel::Info;
}

size_t load_candles_limit(const std::string& filename) {
    std::ifstream in(filename);
    if (in.is_open()) {
        try {
            nlohmann::json j;
            in >> j;
            if (j.contains("candles_limit") && j["candles_limit"].is_number_unsigned()) {
                return j["candles_limit"].get<size_t>();
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse config.json: " << e.what() << std::endl;
        }
    }
    return 5000;
}

bool load_streaming_enabled(const std::string& filename) {
    std::ifstream in(filename);
    if (in.is_open()) {
        try {
            nlohmann::json j; in >> j;
            if (j.contains("enable_streaming") && j["enable_streaming"].is_boolean()) {
                return j["enable_streaming"].get<bool>();
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse " << filename << ": " << e.what() << std::endl;
        }
    }
    return false;
}

SignalConfig load_signal_config(const std::string& filename) {
    std::ifstream in(filename);
    SignalConfig cfg;
    if (in.is_open()) {
        try {
            nlohmann::json j;
            in >> j;
            if (j.contains("signal") && j["signal"].is_object()) {
                const auto& s = j["signal"];
                if (s.contains("type") && s["type"].is_string()) {
                    cfg.type = s["type"].get<std::string>();
                }
                if (s.contains("short_period") && s["short_period"].is_number_unsigned()) {
                    cfg.short_period = s["short_period"].get<std::size_t>();
                }
                if (s.contains("long_period") && s["long_period"].is_number_unsigned()) {
                    cfg.long_period = s["long_period"].get<std::size_t>();
                }
                if (s.contains("params") && s["params"].is_object()) {
                    for (auto it = s["params"].begin(); it != s["params"].end(); ++it) {
                        if (it.value().is_number()) {
                            cfg.params[it.key()] = it.value().get<double>();
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse config.json: " << e.what() << std::endl;
        }
    }
    return cfg;
}

} // namespace Config

