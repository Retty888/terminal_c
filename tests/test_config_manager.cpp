#include "config_manager.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

static std::string make_tmp(const char *name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

TEST(ConfigManagerTest, ReturnsNulloptOnCorruptedJson) {
    std::string tmp = make_tmp("corrupted_config.json");
    {
        std::ofstream out(tmp);
        out << "{ this is not json";
    }
    auto cfg = Config::ConfigManager::load(tmp);
    EXPECT_FALSE(cfg.has_value());
    std::filesystem::remove(tmp);
}

TEST(ConfigManagerTest, ReturnsNulloptOnMissingKeys) {
    std::string tmp = make_tmp("missing_keys_config.json");
    {
        std::ofstream out(tmp);
        out << R"({"log_level": "INFO"})";
    }
    auto cfg = Config::ConfigManager::load(tmp);
    EXPECT_FALSE(cfg.has_value());
    std::filesystem::remove(tmp);
}

