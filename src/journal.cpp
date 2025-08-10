#include "journal.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace Journal {

bool Journal::load_json(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    m_entries.clear();
    size_t pos = 0;
    auto trim = [](const std::string& s) {
        size_t b = s.find_first_not_of(" \n\r\t\"");
        size_t e = s.find_last_not_of(" \n\r\t\"");
        if (b == std::string::npos || e == std::string::npos) return std::string();
        return s.substr(b, e - b + 1);
    };
    while ((pos = content.find('{', pos)) != std::string::npos) {
        size_t end = content.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = content.substr(pos + 1, end - pos - 1);
        Entry e;
        std::istringstream ss(obj);
        std::string kv;
        while (std::getline(ss, kv, ',')) {
            auto colon = kv.find(':');
            if (colon == std::string::npos) continue;
            std::string key = trim(kv.substr(0, colon));
            std::string value = trim(kv.substr(colon + 1));
            if (key == "symbol") e.symbol = value;
            else if (key == "side") e.side = side_from_string(value);
            else if (key == "price") e.price = std::stod(value);
            else if (key == "quantity") e.quantity = std::stod(value);
            else if (key == "timestamp") e.timestamp = std::stoll(value);
        }
        m_entries.push_back(e);
        pos = end + 1;
    }
    return true;
}

bool Journal::save_json(const std::string& filename) const {
    std::ofstream f(filename);
    if (!f.is_open()) return false;
    f << "[\n";
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto& e = m_entries[i];
        f << "  {\"symbol\":\"" << e.symbol << "\",";
        f << "\"side\":\"" << side_to_string(e.side) << "\",";
        f << "\"price\":" << e.price << ",";
        f << "\"quantity\":" << e.quantity << ",";
        f << "\"timestamp\":" << e.timestamp << "}";
        if (i + 1 < m_entries.size()) f << ',';
        f << "\n";
    }
    f << "]";
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

