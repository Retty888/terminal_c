#pragma once

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel { Info, Warning, Error };

class Logger {
public:
  static Logger &instance();
  void set_file(const std::string &filename,
                std::size_t max_size = 1024 * 1024);
  void enable_console_output(bool enable);
  void set_min_level(LogLevel level);
  void log(LogLevel level, const std::string &message);
  void info(const std::string &message);
  void warn(const std::string &message);
  void error(const std::string &message);

private:
  Logger() = default;
  std::ofstream out_;
  std::mutex mutex_;
  bool console_output_ = false;
  LogLevel min_level_ = LogLevel::Info;
  std::string filename_;
  std::size_t max_file_size_ = 1024 * 1024;
  std::string level_to_string(LogLevel level);
};

