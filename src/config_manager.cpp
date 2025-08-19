#include "config_manager.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "config_schema.h"
#include "config_path.h"

namespace Config {

std::optional<ConfigData> ConfigManager::load(const std::string &filename) {
    auto path = resolve_config_path(filename);
    std::ifstream in(path);
    if (!in.is_open()) {
        Core::Logger::instance().error("Failed to open " + path.string());
        return std::nullopt;
    }
    try {
        nlohmann::json j;
        in >> j;

        std::string error;
        auto cfg = ConfigSchema::parse(j, error);
        if (!cfg) {
            Core::Logger::instance().error(error + " in " + path.string());
            return std::nullopt;
        }
        return cfg;
    } catch (const std::exception &e) {
        Core::Logger::instance().error(std::string("Failed to parse ") + path.string() + ": " + e.what());
        return std::nullopt;
    }
}

bool ConfigManager::save_selected_pairs(const std::string &filename,
                                        const std::vector<std::string> &pairs) {
    auto path = resolve_config_path(filename);
    nlohmann::json j;
    {
        std::ifstream in(path);
        if (in.is_open()) {
            try {
                in >> j;
            } catch (const std::exception &e) {
                Core::Logger::instance().warn(std::string("Failed to parse existing ") + path.string() + ": " + e.what());
            }
        }
    }
    j["pairs"] = pairs;
    std::ofstream out(path);
    if (!out.is_open()) {
        Core::Logger::instance().error("Failed to open " + path.string() + " for writing");
        return false;
    }
    out << j.dump(4);
    return true;
}

} // namespace Config

