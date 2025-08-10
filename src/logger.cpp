#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>

Logger &Logger::instance() {
  static Logger inst;
  return inst;
}

void Logger::set_file(const std::string &filename) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (out_.is_open())
    out_.close();
  out_.open(filename, std::ios::app);
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
  using namespace std::chrono;
  auto now = system_clock::now();
  auto t = system_clock::to_time_t(now);
  auto tm = *std::localtime(&t);
  std::lock_guard<std::mutex> lock(mutex_);
  if (out_.is_open()) {
    out_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ["
         << level_to_string(level) << "] " << message << std::endl;
  }
}

void Logger::info(const std::string &message) { log(LogLevel::Info, message); }

void Logger::warn(const std::string &message) { log(LogLevel::Warning, message); }

void Logger::error(const std::string &message) { log(LogLevel::Error, message); }

