#include "journal.h"
#include <cassert>
#include <filesystem>

int main() {
    assert(std::string(Journal::side_to_string(Journal::Side::Buy)) == "BUY");
    assert(Journal::side_from_string("SELL") == Journal::Side::Sell);

    Journal::Journal j;
    Journal::Entry e1{"BTC", Journal::Side::Buy, 100.0, 1.5, 1000};
    Journal::Entry e2{"ETH", Journal::Side::Sell, 200.0, 2.0, 2000};
    j.add_entry(e1);
    j.add_entry(e2);
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "journal_test";
    std::filesystem::create_directories(dir);
    auto json_file = (dir / "journal.json").string();
    auto csv_file = (dir / "journal.csv").string();
    bool saved_json = j.save_json(json_file);
    bool saved_csv = j.save_csv(csv_file);
    assert(saved_json && saved_csv);
    Journal::Journal j2;
    bool loaded_json = j2.load_json(json_file);
    assert(loaded_json);
    assert(j2.entries().size() == 2);
    assert(j2.entries()[0].symbol == "BTC");
    assert(j2.entries()[0].side == Journal::Side::Buy);
    assert(j2.entries()[1].side == Journal::Side::Sell);
    Journal::Journal j3;
    bool loaded_csv = j3.load_csv(csv_file);
    assert(loaded_csv);
    assert(j3.entries().size() == 2);
    assert(j3.entries()[0].side == Journal::Side::Buy);
    assert(j3.entries()[1].side == Journal::Side::Sell);
    std::filesystem::remove_all(dir);
    return 0;
}
