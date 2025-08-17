#include "config_schema.h"

namespace Config {

std::optional<ConfigData> ConfigSchema::parse(const nlohmann::json &j,
                                              std::string &error) {
    ConfigData cfg;
    if (j.contains("pairs")) {
        if (!j["pairs"].is_array()) {
            error = "'pairs' must be an array";
            return std::nullopt;
        }
        for (const auto &item : j["pairs"]) {
            if (item.is_string())
                cfg.pairs.push_back(item.get<std::string>());
            else {
                error = "'pairs' entries must be strings";
                return std::nullopt;
            }
        }
    }

    if (j.contains("log_level")) {
        if (!j["log_level"].is_string()) {
            error = "'log_level' must be a string";
            return std::nullopt;
        }
        std::string level = j["log_level"].get<std::string>();
        if (level == "INFO")
            cfg.log_level = LogLevel::Info;
        else if (level == "WARN" || level == "WARNING")
            cfg.log_level = LogLevel::Warning;
        else if (level == "ERROR")
            cfg.log_level = LogLevel::Error;
        else {
            error = "Unknown log level '" + level + "'";
            return std::nullopt;
        }
    }

    if (j.contains("candles_limit")) {
        if (!j["candles_limit"].is_number_unsigned()) {
            error = "'candles_limit' must be an unsigned number";
            return std::nullopt;
        }
        cfg.candles_limit = j["candles_limit"].get<std::size_t>();
    }

    if (j.contains("enable_streaming")) {
        if (!j["enable_streaming"].is_boolean()) {
            error = "'enable_streaming' must be a boolean";
            return std::nullopt;
        }
        cfg.enable_streaming = j["enable_streaming"].get<bool>();
    }

    if (j.contains("signal")) {
        if (!j["signal"].is_object()) {
            error = "'signal' must be an object";
            return std::nullopt;
        }
        const auto &s = j["signal"];
        if (s.contains("type")) {
            if (!s["type"].is_string()) {
                error = "'signal.type' must be a string";
                return std::nullopt;
            }
            cfg.signal.type = s["type"].get<std::string>();
        }
        if (s.contains("short_period")) {
            if (!s["short_period"].is_number_unsigned()) {
                error = "'signal.short_period' must be an unsigned number";
                return std::nullopt;
            }
            cfg.signal.short_period = s["short_period"].get<std::size_t>();
        }
        if (s.contains("long_period")) {
            if (!s["long_period"].is_number_unsigned()) {
                error = "'signal.long_period' must be an unsigned number";
                return std::nullopt;
            }
            cfg.signal.long_period = s["long_period"].get<std::size_t>();
        }
        if (s.contains("params")) {
            if (!s["params"].is_object()) {
                error = "'signal.params' must be an object";
                return std::nullopt;
            }
            for (auto it = s["params"].begin(); it != s["params"].end(); ++it) {
                if (!it.value().is_number()) {
                    error = "'signal.params." + it.key() + "' must be a number";
                    return std::nullopt;
                }
                cfg.signal.params[it.key()] = it.value().get<double>();
            }
        }
    }

    return cfg;
}

} // namespace Config

