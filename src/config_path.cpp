#include "config_path.h"
#include "core/path_utils.h"

#include <array>
#include <cstdlib>

std::filesystem::path resolve_config_path(const std::filesystem::path &filename) {
    if (filename.is_absolute()) {
        return filename;
    }
    if (filename.filename() != "config.json") {
        return std::filesystem::absolute(filename);
    }
    if (const char *env_cfg = std::getenv("CANDLE_CONFIG_PATH")) {
        return std::filesystem::path(env_cfg);
    }
    auto exe_dir = Core::executable_dir();
    std::array<std::filesystem::path, 3> candidates = {
        exe_dir / filename,
        exe_dir.parent_path() / filename,
        std::filesystem::current_path() / filename,
    };
    for (const auto &c : candidates) {
        if (std::filesystem::exists(c))
            return c;
    }
    return candidates[0];
}

