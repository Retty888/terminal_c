#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>

struct GLFWwindow;

class EChartsWindow;

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
  void end_frame(GLFWwindow *window);
  void shutdown();

private:
  std::unique_ptr<EChartsWindow> echarts_window_;
  std::thread echarts_thread_;
  std::string current_interval_;
  std::function<void(const std::string &)> on_interval_changed_;
};
