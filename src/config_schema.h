#pragma once

#include "config_manager.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace Config {

struct ConfigSchema {
    static std::optional<ConfigData> parse(const nlohmann::json &j,
                                           std::string &error);
};

} // namespace Config

