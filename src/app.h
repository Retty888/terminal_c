#pragma once

#include "app_context.h"
#include "core/glfw_context.h"
#include "services/data_service.h"
#include "services/journal_service.h"
#include "ui/ui_manager.h"

#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/logger.h"

struct GLFWwindow;

struct AppStatus {
  float candle_progress = 0.0f;
  std::string error_message;
  std::deque<std::string> log;
  static constexpr size_t kMaxLogEntries = 200;
};

// The App class owns the services and drives the main event loop.
class App {
public:
  App();
  ~App();
  // Runs the application. Returns the exit code.
  int run();
  const AppStatus &status() const { return status_; }
  void add_status(const std::string &msg,
                  Core::LogLevel level = Core::LogLevel::Info,
                  std::chrono::system_clock::time_point time =
                      std::chrono::system_clock::now());
  void clear_failed_fetches();
  void set_error_message(const std::string &msg);
  AppStatus get_status_snapshot() const;

private:
  bool init_window();
  void setup_imgui();
  void load_config();
  void process_events();
  void render_ui();
  void cleanup();
  void update_next_fetch_time(long long candidate);
  void schedule_retry(long long now_ms, std::chrono::milliseconds delay,
                      const std::string &msg = "");
  void load_pairs(std::vector<std::string> &pair_names);
  void load_existing_candles();
  void start_initial_fetch_and_streams();
  void schedule_http_updates(std::chrono::milliseconds period,
                             long long now_ms);
  void handle_http_updates();
  void update_candle_progress();
  void render_status_window();
  void render_main_windows();
  void handle_active_pair_change();
  void update_available_intervals();
  void start_fetch_thread();
  void stop_fetch_thread();

  std::unique_ptr<AppContext> ctx_;
  DataService data_service_;
  JournalService journal_service_;
  AppStatus status_;
  mutable std::mutex status_mutex_;
  struct WindowDeleter {
    void operator()(GLFWwindow *window) const;
  };
  std::unique_ptr<Core::GlfwContext> glfw_context_;
  std::unique_ptr<GLFWwindow, WindowDeleter> window_{nullptr};
  UiManager ui_manager_;
  std::jthread fetch_thread_;
};
