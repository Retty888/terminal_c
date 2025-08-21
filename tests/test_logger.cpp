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

TEST(LoggerFilesystem, OpenFailure) {
    Core::Logger::instance().set_file("");
    Core::Logger::instance().enable_console_output(false);
    auto path = std::filesystem::path("/proc/forbidden.log");
    testing::internal::CaptureStderr();
    Core::Logger::instance().set_file(path.string());
    auto err = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(err.empty());
    Core::Logger::instance().info("test");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(std::filesystem::exists(path));
    Core::Logger::instance().set_file("");
}

TEST(LoggerFilesystem, RotateFailure) {
    Core::Logger::instance().set_file("");
    Core::Logger::instance().enable_console_output(false);
    auto path = std::filesystem::path("/proc/version");
    ASSERT_TRUE(std::filesystem::exists(path));
    testing::internal::CaptureStderr();
    Core::Logger::instance().set_file(path.string(), 0);
    auto err = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(err.empty());
    EXPECT_TRUE(std::filesystem::exists(path));
    Core::Logger::instance().set_file("");
}
