#pragma once

#include <filesystem>

// Resolves the path to the configuration file. If the provided filename is an
// absolute path or differs from the default "config.json", it is returned as
// is (converted to an absolute path if necessary). Otherwise, the function
// searches common locations and respects the CANDLE_CONFIG_PATH environment
// variable.
std::filesystem::path resolve_config_path(const std::filesystem::path &filename = "config.json");

