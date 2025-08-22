#pragma once

#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <chrono>

struct GLFWwindow;
namespace webview {
class webview;
}
namespace Core {
struct Candle;
}

// Manages ImGui initialization and per-frame rendering and hosts auxiliary
// UI panels. Currently only a placeholder chart panel is provided.
class UiManager {
public:
  ~UiManager();
  bool setup(GLFWwindow *window);
  void begin_frame();
  // Draw docked panels each frame. Currently hosts a placeholder chart panel.
  void draw_chart_panel(const std::string &selected_interval);
  // Pushes trade markers to the chart via series.setMarkers.
  void set_markers(const std::string &markers_json);
  // Draws/updates a price line for the currently open position.
  void set_price_line(double price);
  // Sends a new candle to the chart for real-time updates.
  void push_candle(const Core::Candle &candle);
  // Placeholder for future interval change notifications from embedded charts.
  void set_interval_callback(std::function<void(const std::string &)> cb);
  // Set callback for reporting status messages to the application.
  void set_status_callback(std::function<void(const std::string &)> cb);
  // Inform the UI about the current interval during initialization.
  void set_initial_interval(const std::string &interval);
  void end_frame(GLFWwindow *window);
  void shutdown();

private:
  bool chart_enabled_ = true;
  std::string current_interval_;
  std::function<void(const std::string &)> on_interval_changed_;
  std::function<void(const std::string &)> status_callback_;
  std::unique_ptr<webview::webview> chart_view_;
  bool shutdown_called_ = false;

  // Throttling for real-time candle pushes
  std::chrono::steady_clock::time_point last_push_time_{};
  std::chrono::milliseconds throttle_interval_{100};
  std::optional<Core::Candle> cached_candle_{};
};
