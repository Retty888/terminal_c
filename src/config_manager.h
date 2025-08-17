#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "logger.h"

namespace Config {

struct SignalConfig {
    std::string type{"sma_crossover"};
    std::size_t short_period{0};
    std::size_t long_period{0};
    std::map<std::string, double> params{};
};

struct ConfigData {
    std::vector<std::string> pairs{};
    LogLevel log_level{LogLevel::Info};
    std::size_t candles_limit{5000};
    bool enable_streaming{false};
    SignalConfig signal{};
};

class ConfigManager {
public:
    static std::optional<ConfigData> load(const std::string &filename);
    static bool save_selected_pairs(const std::string &filename,
                                    const std::vector<std::string> &pairs);
};

} // namespace Config

