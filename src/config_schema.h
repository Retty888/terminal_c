#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "config_types.h"

namespace Config {

inline constexpr const char *kDefaultChartHtmlPath = "resources/chart.html";
inline constexpr const char *kDefaultChartJsPath =
    "third_party/lightweight-charts/lightweight-charts.standalone.production.js";

struct ConfigSchema {
  static std::optional<ConfigData> parse(const nlohmann::json &j,
                                         std::string &error);
};

} // namespace Config
