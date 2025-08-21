#pragma once

#include <optional>
#include <string>
#include <vector>

#include "config_types.h"

namespace Config {

class ConfigManager {
public:
  static std::optional<ConfigData> load(const std::string &filename);
  static bool save_selected_pairs(const std::string &filename,
                                  const std::vector<std::string> &pairs);
};

} // namespace Config
