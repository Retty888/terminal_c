#pragma once

#include "app_context.h"
#include "services/data_service.h"
#include "services/journal_service.h"
#include "ui/ui_manager.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct AppStatus {
  float candle_progress = 0.0f;
  std::string analysis_message = "Idle";
  std::string signal_message = "Idle";
  std::string error_message;
  std::deque<std::string> log;
};

// The App class owns the services and drives the main event loop.
class App {
public:
  App();
  ~App();
  // Runs the application. Returns the exit code.
  int run();
  const AppStatus &status() const { return status_; }
  void add_status(const std::string &msg);

private:
  bool init_window();
  void setup_imgui();
  void load_config();
  void process_events();
  void render_ui();
  void cleanup();
  void update_next_fetch_time(long long candidate);
  void schedule_retry(long long now_ms, const std::string &msg = "");

  std::unique_ptr<AppContext> ctx_;
  DataService data_service_;
  JournalService journal_service_;
  AppStatus status_;
  mutable std::mutex status_mutex_;
  GLFWwindow *window_ = nullptr;
  UiManager ui_manager_;
};
