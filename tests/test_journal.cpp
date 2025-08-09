#include "journal.h"
#include <cassert>
#include <filesystem>

int main() {
    Journal::Journal j;
    Journal::Entry e{"BTC", "BUY", 100.0, 1.5, 1000};
    j.add_entry(e);
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
    assert(j2.entries().size() == 1);
    assert(j2.entries()[0].symbol == "BTC");
    Journal::Journal j3;
    bool loaded_csv = j3.load_csv(csv_file);
    assert(loaded_csv);
    assert(j3.entries().size() == 1);
    assert(j3.entries()[0].side == "BUY");
    std::filesystem::remove_all(dir);
    return 0;
}
