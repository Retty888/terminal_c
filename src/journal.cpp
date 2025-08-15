#include "journal.h"
#include <fstream>
#include <charconv>
#include <string_view>
#include <array>
#include <nlohmann/json.hpp>
#include "logger.h"

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
        std::string_view sv(line);
        std::array<std::string_view, 5> fields{};
        size_t start = 0;
        size_t idx = 0;
        while (idx < fields.size()) {
            size_t comma = sv.find(',', start);
            if (comma == std::string_view::npos) {
                fields[idx++] = sv.substr(start);
                break;
            }
            fields[idx++] = sv.substr(start, comma - start);
            start = comma + 1;
        }
        if (idx != fields.size()) {
            Logger::instance().error("Malformed journal line: " + line);
            continue;
        }

        Entry e;
        e.symbol = std::string(fields[0]);
        e.side = side_from_string(std::string(fields[1]));

        auto parse_double = [](std::string_view s, double& out) {
            auto res = std::from_chars(s.data(), s.data() + s.size(), out);
            return res.ec == std::errc() && res.ptr == s.data() + s.size();
        };
        auto parse_ll = [](std::string_view s, std::int64_t& out) {
            auto res = std::from_chars(s.data(), s.data() + s.size(), out);
            return res.ec == std::errc() && res.ptr == s.data() + s.size();
        };

        if (!parse_double(fields[2], e.price) ||
            !parse_double(fields[3], e.quantity) ||
            !parse_ll(fields[4], e.timestamp)) {
            Logger::instance().error("Failed to parse journal line: " + line);
            continue;
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

