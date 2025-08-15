#include "logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger &Logger::instance() {
  static Logger inst;
  return inst;
}

void Logger::set_file(const std::string &filename, std::size_t max_size) {
  std::lock_guard<std::mutex> lock(mutex_);
  filename_ = filename;
  max_file_size_ = max_size;
  namespace fs = std::filesystem;
  if (out_.is_open())
    out_.close();
  bool rotate = false;
  if (fs::exists(filename)) {
    auto size = fs::file_size(filename);
    auto last = fs::last_write_time(filename);
    auto last_sys =
        std::chrono::clock_cast<std::chrono::system_clock>(last);
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::time_point_cast<std::chrono::days>(now);
    auto file_day =
        std::chrono::time_point_cast<std::chrono::days>(last_sys);
    if (size >= max_file_size_ || file_day != today)
      rotate = true;
  }
  if (rotate) {
    auto t = std::time(nullptr);
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << filename << '.' << std::put_time(&tm, "%Y%m%d%H%M%S");
    fs::rename(filename, oss.str());
  }
  out_.open(filename, std::ios::app);
}

void Logger::enable_console_output(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  console_output_ = enable;
}

void Logger::set_min_level(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  min_level_ = level;
}

std::string Logger::level_to_string(LogLevel level) {
  switch (level) {
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warning:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  }
  return "";
}

void Logger::log(LogLevel level, const std::string &message) {
  if (static_cast<int>(level) < static_cast<int>(min_level_))
    return;
  using namespace std::chrono;
  auto now = system_clock::now();
  auto t = system_clock::to_time_t(now);
  std::lock_guard<std::mutex> lock(mutex_);
  std::tm tm;
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ["
      << level_to_string(level) << "] " << message << std::endl;
  auto formatted = oss.str();
  if (out_.is_open())
    out_ << formatted;
  if (console_output_)
    std::cout << formatted;
}

void Logger::info(const std::string &message) { log(LogLevel::Info, message); }

void Logger::warn(const std::string &message) { log(LogLevel::Warning, message); }

void Logger::error(const std::string &message) { log(LogLevel::Error, message); }

