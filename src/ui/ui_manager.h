#pragma once

#include "core/candle.h"
#include "imgui.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <string>
#include <vector>

struct GLFWwindow;

// Manages ImGui initialization and per-frame rendering and hosts auxiliary
// UI panels. Currently only a placeholder chart panel is provided.
class UiManager {
public:
  ~UiManager();
  bool setup(GLFWwindow *window);
  void begin_frame();
  // Draw docked panels each frame.
  void draw_chart_panel(const std::vector<std::string> &pairs,
                        const std::vector<std::string> &intervals);
  // Pushes trade markers to the chart.
  void set_markers(const std::string &markers_json);
  // Draws/updates a price line for the currently open position.
  void set_price_line(double price);
  // Replaces all chart candles with the provided collection.
  void set_candles(const std::vector<Core::Candle> &candles);
  // Sends a new candle to the chart for real-time updates.
  void push_candle(const Core::Candle &candle);
  // Provides callback to forward candle JSON to the chart.
  std::function<void(const std::string &)> candle_callback();
  // Placeholder for future interval change notifications from embedded charts.
  void set_interval_callback(std::function<void(const std::string &)> cb);
  // Notify when the active trading pair changes.
  void set_pair_callback(std::function<void(const std::string &)> cb);
  // Set callback for reporting status messages to the application.
  void set_status_callback(std::function<void(const std::string &)> cb);
  // Inform the UI about the current interval during initialization.
  void set_initial_interval(const std::string &interval);
  // Inform the UI about the current pair during initialization.
  void set_initial_pair(const std::string &pair);
  void end_frame(GLFWwindow *window);
  void shutdown();

private:
  std::vector<Core::Candle> candles_;
  enum class DrawTool { None, Line, HLine, Ruler, Long, Short };
  struct DrawObject {
    DrawTool type;
    double x1;
    double y1;
    double x2;
    double y2;
  };
  struct Marker {
    double time;
    bool above;
    ImVec4 color;
    std::string text;
  };
  std::vector<Marker> markers_;
  DrawTool current_tool_ = DrawTool::None;
  std::vector<DrawObject> draw_objects_;
  bool drawing_first_point_ = false;
  int editing_object_ = -1;
  double temp_x_ = 0.0;
  double temp_y_ = 0.0;
  int context_object_ = -1;
  std::optional<double> price_line_;
  std::string current_interval_;
  std::string current_pair_;
  std::function<void(const std::string &)> on_interval_changed_;
  std::function<void(const std::string &)> on_pair_changed_;
  std::function<void(const std::string &)> status_callback_;
  bool shutdown_called_ = false;
  mutable std::mutex ui_mutex_;

  // Throttling for real-time candle pushes
  std::chrono::steady_clock::time_point last_push_time_{};
  std::chrono::milliseconds throttle_interval_{100};
  std::optional<Core::Candle> cached_candle_{};

#ifdef HAVE_WEBVIEW
  void *webview_ = nullptr;
  std::jthread webview_thread_{};
  bool webview_ready_ = false;
#endif
};
