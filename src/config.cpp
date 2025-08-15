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

}
