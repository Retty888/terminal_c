#include "core/logger.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

TEST(LoggerAsync, NonBlockingLogging) {
    auto tmp = std::filesystem::temp_directory_path() / "async_log_test.log";
    Core::Logger::instance().set_file(tmp.string());
    Core::Logger::instance().enable_console_output(false);
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        Core::Logger::instance().info("message" + std::to_string(i));
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(duration, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::ifstream in(tmp);
    std::string line;
    int count = 0;
    while (std::getline(in, line))
        ++count;
    EXPECT_EQ(count, 1000);
    in.close();
    Core::Logger::instance().set_file("");
    std::filesystem::remove(tmp);
}
