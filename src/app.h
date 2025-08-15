#pragma once

#include "services/data_service.h"
#include "services/journal_service.h"

#include <mutex>
#include <string>
#include <vector>

struct AppStatus {
  float candle_progress = 0.0f;
  std::string analysis_message = "Idle";
  std::string signal_message = "Idle";
  std::string error_message;
  std::vector<std::string> log;
};

// The App class owns the services and drives the main event loop.
class App {
public:
  // Runs the application. Returns the exit code.
  int run();
  const AppStatus &status() const { return status_; }
  void add_status(const std::string &msg);

private:
  DataService data_service_;
  JournalService journal_service_;
  AppStatus status_;
  mutable std::mutex status_mutex_;
};

