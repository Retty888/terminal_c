#pragma once

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel { Info, Warning, Error };

class Logger {
public:
  static Logger &instance();
  void set_file(const std::string &filename);
  void log(LogLevel level, const std::string &message);
  void info(const std::string &message);
  void warn(const std::string &message);
  void error(const std::string &message);

private:
  Logger() = default;
  std::ofstream out_;
  std::mutex mutex_;
  std::string level_to_string(LogLevel level);
};

