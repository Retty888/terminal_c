#include "core/data_dir.h"
#include "config_path.h"
#include "core/logger.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Core {

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

} // namespace Core

