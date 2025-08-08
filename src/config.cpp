#include "config.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

namespace Config {

std::vector<std::string> load_selected_pairs(const std::string& filename) {
    std::ifstream in(filename);
    std::vector<std::string> result;

    if (in.is_open()) {
        try {
            nlohmann::json j;
            in >> j;
            if (j.contains("pairs") && j["pairs"].is_array()) {
                for (const auto& item : j["pairs"]) {
                    result.push_back(item.get<std::string>());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse config.json: " << e.what() << std::endl;
        }
    }
    return result;
}

void save_selected_pairs(const std::string& filename, const std::vector<std::string>& pairs) {
    std::ofstream out(filename);
    if (out.is_open()) {
        nlohmann::json j;
        j["pairs"] = pairs;
        out << j.dump(4);
    }
}

}
