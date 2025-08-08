#pragma once

#include <string>
#include <vector>

// Configuration file, can be expanded later

namespace Config {

std::vector<std::string> load_selected_pairs(const std::string& filename);
void save_selected_pairs(const std::string& filename, const std::vector<std::string>& pairs);

} // namespace Config
