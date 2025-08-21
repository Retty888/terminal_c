#pragma once

#include "config_manager.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace Config {

inline constexpr const char *kDefaultChartHtmlPath = "resources/chart.html";
inline constexpr const char *kDefaultEchartsJsPath =
    "third_party/echarts/echarts.min.js";

struct ConfigSchema {
  static std::optional<ConfigData> parse(const nlohmann::json &j,
                                         std::string &error);
};

} // namespace Config
