#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Journal {

enum class Side { Buy, Sell };

inline const char* side_to_string(Side s) {
    return s == Side::Buy ? "BUY" : "SELL";
}

inline Side side_from_string(const std::string& s) {
    return s == "SELL" ? Side::Sell : Side::Buy;
}

struct Entry {
    std::string symbol;
    Side side = Side::Buy;
    double price = 0.0;
    double quantity = 0.0;
    std::int64_t timestamp = 0; // milliseconds since epoch
};

class Journal {
public:
    void add_entry(const Entry& e) { m_entries.push_back(e); }
    const std::vector<Entry>& entries() const { return m_entries; }
    std::vector<Entry>& entries() { return m_entries; }

    bool load_json(const std::string& filename);
    bool save_json(const std::string& filename) const;

    bool load_csv(const std::string& filename);
    bool save_csv(const std::string& filename) const;

private:
    std::vector<Entry> m_entries;
};

} // namespace Journal

