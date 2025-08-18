#pragma once

#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

namespace Core {

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
  ~Logger();

private:
  Logger();
  struct LogMessage {
    LogLevel level;
    std::string message;
    std::chrono::system_clock::time_point time;
  };

  void process_queue();

  std::ofstream out_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<LogMessage> queue_;
  std::thread worker_;
  bool running_ = true;
  bool console_output_ = false;
  LogLevel min_level_ = LogLevel::Info;
  std::string filename_;
  std::size_t max_file_size_ = 1024 * 1024;
  std::string level_to_string(LogLevel level);
};

} // namespace Core

