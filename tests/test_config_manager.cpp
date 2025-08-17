#include "config_manager.h"

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>

static std::string make_tmp(const char *name) {
    std::string path = std::tmpnam(nullptr);
    path += name;
    return path;
}

TEST(ConfigManagerTest, ReturnsNulloptOnCorruptedJson) {
    std::string tmp = make_tmp("corrupted_config.json");
    {
        std::ofstream out(tmp);
        out << "{ this is not json";
    }
    auto cfg = Config::ConfigManager::load(tmp);
    EXPECT_FALSE(cfg.has_value());
    std::remove(tmp.c_str());
}

TEST(ConfigManagerTest, ReturnsNulloptOnMissingKeys) {
    std::string tmp = make_tmp("missing_keys_config.json");
    {
        std::ofstream out(tmp);
        out << R"({"log_level": "INFO"})";
    }
    auto cfg = Config::ConfigManager::load(tmp);
    EXPECT_FALSE(cfg.has_value());
    std::remove(tmp.c_str());
}

