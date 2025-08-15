#pragma once

#include <string>
#include <vector>
#include <map>

// Configuration file, can be expanded later

enum class LogLevel;

namespace Config {

std::vector<std::string> load_selected_pairs(const std::string& filename);
void save_selected_pairs(const std::string& filename, const std::vector<std::string>& pairs);
LogLevel load_min_log_level(const std::string& filename);
size_t load_candles_limit(const std::string& filename);

struct SignalConfig {
    std::string type{"sma_crossover"};
    std::size_t short_period{0};
    std::size_t long_period{0};
    std::map<std::string, double> params{};
};

SignalConfig load_signal_config(const std::string& filename);

} // namespace Config
