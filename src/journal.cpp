#include "journal.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace Journal {

bool Journal::load_json(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) return false;
    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return false;
    }
    m_entries.clear();
    for (const auto& item : j) {
        Entry e;
        e.symbol = item.value("symbol", "");
        e.side = side_from_string(item.value("side", "BUY"));
        e.price = item.value("price", 0.0);
        e.quantity = item.value("quantity", 0.0);
        e.timestamp = item.value("timestamp", 0LL);
        m_entries.push_back(e);
    }
    return true;
}

bool Journal::save_json(const std::string& filename) const {
    std::ofstream f(filename);
    if (!f.is_open()) return false;
    nlohmann::json j = nlohmann::json::array();
    for (const auto& e : m_entries) {
        j.push_back({{"symbol", e.symbol}, {"side", side_to_string(e.side)}, {"price", e.price}, {"quantity", e.quantity}, {"timestamp", e.timestamp}});
    }
    f << j.dump(4);
    return true;
}

bool Journal::load_csv(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) return false;
    m_entries.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        Entry e;
        std::string field;
        std::getline(ss, e.symbol, ',');
        std::getline(ss, field, ',');
        e.side = side_from_string(field);
        try {
            std::getline(ss, field, ',');
            e.price = std::stod(field);
            std::getline(ss, field, ',');
            e.quantity = std::stod(field);
            std::getline(ss, field, ',');
            e.timestamp = std::stoll(field);
        } catch (const std::exception&) {
            return false;
        }
        m_entries.push_back(e);
    }
    return true;
}

bool Journal::save_csv(const std::string& filename) const {
    std::ofstream f(filename);
    if (!f.is_open()) return false;
    for (const auto& e : m_entries) {
        f << e.symbol << ',' << side_to_string(e.side) << ',' << e.price << ',' << e.quantity << ',' << e.timestamp << '\n';
    }
    return true;
}

} // namespace Journal

