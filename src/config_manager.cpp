#include "config_manager.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace Config {

std::optional<ConfigData> ConfigManager::load(const std::string &filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        Logger::instance().error("Failed to open " + filename);
        return std::nullopt;
    }
    try {
        nlohmann::json j;
        in >> j;

        ConfigData cfg;

        if (!j.contains("pairs") || !j["pairs"].is_array()) {
            Logger::instance().error("Missing or invalid 'pairs' in " + filename);
            return std::nullopt;
        }
        for (const auto &item : j["pairs"]) {
            if (item.is_string()) {
                cfg.pairs.push_back(item.get<std::string>());
            }
        }

        if (!j.contains("log_level") || !j["log_level"].is_string()) {
            Logger::instance().error("Missing or invalid 'log_level' in " + filename);
            return std::nullopt;
        }
        std::string level = j["log_level"].get<std::string>();
        if (level == "INFO")
            cfg.log_level = LogLevel::Info;
        else if (level == "WARN" || level == "WARNING")
            cfg.log_level = LogLevel::Warning;
        else if (level == "ERROR")
            cfg.log_level = LogLevel::Error;
        else
            Logger::instance().warn("Unknown log level '" + level + "'");

        if (!j.contains("candles_limit") || !j["candles_limit"].is_number_unsigned()) {
            Logger::instance().error("Missing or invalid 'candles_limit' in " + filename);
            return std::nullopt;
        }
        cfg.candles_limit = j["candles_limit"].get<std::size_t>();

        if (!j.contains("enable_streaming") || !j["enable_streaming"].is_boolean()) {
            Logger::instance().error("Missing or invalid 'enable_streaming' in " + filename);
            return std::nullopt;
        }
        cfg.enable_streaming = j["enable_streaming"].get<bool>();

        if (!j.contains("signal") || !j["signal"].is_object()) {
            Logger::instance().error("Missing or invalid 'signal' in " + filename);
            return std::nullopt;
        }
        const auto &s = j["signal"];
        if (s.contains("type") && s["type"].is_string()) {
            cfg.signal.type = s["type"].get<std::string>();
        } else {
            Logger::instance().error("Missing or invalid 'signal.type' in " + filename);
            return std::nullopt;
        }
        if (s.contains("short_period") && s["short_period"].is_number_unsigned()) {
            cfg.signal.short_period = s["short_period"].get<std::size_t>();
        } else {
            Logger::instance().error("Missing or invalid 'signal.short_period' in " + filename);
            return std::nullopt;
        }
        if (s.contains("long_period") && s["long_period"].is_number_unsigned()) {
            cfg.signal.long_period = s["long_period"].get<std::size_t>();
        } else {
            Logger::instance().error("Missing or invalid 'signal.long_period' in " + filename);
            return std::nullopt;
        }
        if (s.contains("params") && s["params"].is_object()) {
            for (auto it = s["params"].begin(); it != s["params"].end(); ++it) {
                if (it.value().is_number()) {
                    cfg.signal.params[it.key()] = it.value().get<double>();
                }
            }
        }

        return cfg;
    } catch (const std::exception &e) {
        Logger::instance().error(std::string("Failed to parse ") + filename + ": " + e.what());
        return std::nullopt;
    }
}

bool ConfigManager::save_selected_pairs(const std::string &filename,
                                        const std::vector<std::string> &pairs) {
    nlohmann::json j;
    {
        std::ifstream in(filename);
        if (in.is_open()) {
            try {
                in >> j;
            } catch (const std::exception &e) {
                Logger::instance().warn(std::string("Failed to parse existing ") + filename + ": " + e.what());
            }
        }
    }
    j["pairs"] = pairs;
    std::ofstream out(filename);
    if (!out.is_open()) {
        Logger::instance().error("Failed to open " + filename + " for writing");
        return false;
    }
    out << j.dump(4);
    return true;
}

} // namespace Config

