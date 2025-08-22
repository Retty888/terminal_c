#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "ui/chart_window.h"

struct GLFWwindow;

// Manages ImGui initialization and per-frame rendering and hosts auxiliary
// UI panels such as the chart webview container.
class UiManager {
public:
  ~UiManager();
  bool setup(GLFWwindow *window);
  void begin_frame();
  // Draw docked panels each frame. Hosts the chart window and forwards
  // interval changes to the JavaScript side.
  void draw_chart_panel(const std::string &selected_interval);
  // Set callback to be invoked when the JS side notifies about a new interval
  // selection.
  void set_interval_callback(std::function<void(const std::string &)> cb);
  // Set callback for reporting status messages to the application.
  void set_status_callback(std::function<void(const std::string &)> cb);
  // Inform the JS side about the current interval during initialization.
  void set_initial_interval(const std::string &interval);
  void end_frame(GLFWwindow *window);
  void shutdown();

private:
  bool resources_available_ = true;
  bool chart_enabled_ = true;
  std::unique_ptr<ChartWindow> chart_window_;
  std::thread chart_thread_;
  std::string current_interval_;
  std::string chart_html_path_;
  std::string chart_js_path_;
  std::function<void(const std::string &)> on_interval_changed_;
  std::function<void(const std::string &)> status_callback_;
  std::string chart_error_;
  std::mutex chart_mutex_;
  bool shutdown_called_ = false;
};
