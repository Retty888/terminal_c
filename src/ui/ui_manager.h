#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "ui/echarts_window.h"

struct GLFWwindow;

// Manages ImGui initialization and per-frame rendering and hosts auxiliary
// UI panels such as the ECharts webview container.
class UiManager {
public:
  ~UiManager();
  bool setup(GLFWwindow *window);
  void begin_frame();
  // Draw docked panels each frame. Currently hosts the ECharts window and
  // forwards interval changes to the JavaScript side.
  void draw_echarts_panel(const std::string &selected_interval);
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
  std::unique_ptr<EChartsWindow> echarts_window_;
  std::thread echarts_thread_;
  std::string current_interval_;
  std::function<void(const std::string &)> on_interval_changed_;
  std::function<void(const std::string &)> status_callback_;
  std::string echarts_error_;
  std::mutex echarts_mutex_;
};
