#include "journal.h"
#include "core/logger.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>

TEST(JournalTest, Serialization) {
    ASSERT_EQ(Journal::side_to_string(Journal::Side::Buy), "BUY");
    EXPECT_EQ(Journal::side_from_string("SELL"), Journal::Side::Sell);

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
    ASSERT_TRUE(saved_json && saved_csv);

    Journal::Journal j2;
    bool loaded_json = j2.load_json(json_file);
    ASSERT_TRUE(loaded_json);
    ASSERT_EQ(j2.entries().size(), 2);
    EXPECT_EQ(j2.entries()[0].symbol, "BTC");
    EXPECT_EQ(j2.entries()[0].side, Journal::Side::Buy);
    EXPECT_EQ(j2.entries()[1].side, Journal::Side::Sell);

    Journal::Journal j3;
    bool loaded_csv = j3.load_csv(csv_file);
    ASSERT_TRUE(loaded_csv);
    ASSERT_EQ(j3.entries().size(), 2);
    EXPECT_EQ(j3.entries()[0].side, Journal::Side::Buy);
    EXPECT_EQ(j3.entries()[1].side, Journal::Side::Sell);

    std::filesystem::remove_all(dir);
}

TEST(JournalTest, LoadMissingFileCreatesFileWithoutError) {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "journal_missing";
    fs::remove_all(dir);
    fs::create_directories(dir);
    auto json_file = (dir / "journal.json").string();
    auto log_file = (dir / "log.txt").string();

    Core::Logger::instance().set_file(log_file);
    Core::Logger::instance().enable_console_output(false);
    Core::Logger::instance().set_min_level(Core::LogLevel::Error);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Journal::Journal j;
    bool loaded = j.load_json(json_file);
    EXPECT_TRUE(loaded);
    EXPECT_TRUE(fs::exists(json_file));

    std::ifstream in(json_file);
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "[]");
    in.close();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::ifstream log_in(log_file);
    bool empty = (log_in.peek() == std::ifstream::traits_type::eof());
    EXPECT_TRUE(empty);
    log_in.close();

    Core::Logger::instance().set_file("");
    fs::remove_all(dir);
}

