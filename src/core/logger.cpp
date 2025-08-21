#include "core/logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>

namespace Core {

Logger &Logger::instance() {
  static Logger inst;
  return inst;
}

Logger::Logger() { worker_ = std::thread([this] { process_queue(); }); }

Logger::~Logger() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
  }
  cv_.notify_all();
  if (worker_.joinable())
    worker_.join();
  if (out_.is_open())
    out_.close();
}

void Logger::set_file(const std::string &filename, std::size_t max_size) {
  std::lock_guard<std::mutex> lock(mutex_);
  filename_ = filename;
  max_file_size_ = max_size;
  namespace fs = std::filesystem;
  if (out_.is_open())
    out_.close();
  if (filename.empty())
    return;
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
    std::error_code ec;
    fs::rename(filename, oss.str(), ec);
    if (ec) {
      std::cerr << "Failed to rotate log file: " << ec.message() << std::endl;
      filename_.clear();
      return;
    }
  }
  try {
    out_.open(filename, std::ios::app);
    if (!out_.is_open())
      throw std::ios_base::failure("open failed");
  } catch (const std::exception &e) {
    std::cerr << "Failed to open log file: " << e.what() << std::endl;
    if (out_.is_open())
      out_.close();
    filename_.clear();
  }
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
  LogMessage msg{level, message, std::chrono::system_clock::now()};
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(msg));
  }
  cv_.notify_one();
}

void Logger::info(const std::string &message) { log(LogLevel::Info, message); }

void Logger::warn(const std::string &message) { log(LogLevel::Warning, message); }

void Logger::error(const std::string &message) { log(LogLevel::Error, message); }

void Logger::process_queue() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (running_ || !queue_.empty()) {
    cv_.wait(lock, [this] { return !running_ || !queue_.empty(); });
    while (!queue_.empty()) {
      auto msg = std::move(queue_.front());
      queue_.pop_front();
      auto level = msg.level;
      auto time = msg.time;
      auto text = std::move(msg.message);
      auto console = console_output_;
      auto out_open = out_.is_open();
      lock.unlock();
      std::tm tm;
      auto t = std::chrono::system_clock::to_time_t(time);
#if defined(_WIN32)
      localtime_s(&tm, &t);
#else
      localtime_r(&t, &tm);
#endif
      std::ostringstream oss;
      oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ["
          << level_to_string(level) << "] " << text << std::endl;
      auto formatted = oss.str();
      if (out_open) {
        out_ << formatted;
        out_.flush();
      }
      if (console)
        std::cout << formatted;
      lock.lock();
    }
  }
}

} // namespace Core
