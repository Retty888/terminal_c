#include "core/data_dir.h"
#include "config_path.h"
#include "core/logger.h"
#include "core/path_utils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Core {

std::filesystem::path resolve_data_dir() {
    if (const char* env_dir = std::getenv("CANDLE_DATA_DIR")) {
        std::filesystem::path dir(env_dir);
        if (!dir.is_absolute()) {
            dir = Core::executable_dir().parent_path().parent_path() / dir;
        }
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
                if (!dir.is_absolute()) {
                    dir = Core::executable_dir().parent_path().parent_path() / dir;
                }
            }
        } catch (const std::exception& e) {
            Logger::instance().error("Failed to parse " + cfg_path.string() + ": " + e.what());
        }
    }

    if (dir.empty()) {
        dir = Core::executable_dir().parent_path().parent_path() / "candle_data";
        j["data_dir"] = "candle_data"; // Store relative path in config
        std::ofstream out(cfg_path);
        if (!out.is_open()) {
            Logger::instance().error("Failed to open " + cfg_path.string() + " for writing");
        } else {
            out << j.dump(4);
        }
    }

    std::filesystem::create_directories(dir);
    Logger::instance().info("Resolved data directory: " + dir.string());
    return dir;
}

} // namespace Core

