#pragma once

#include "core/candle.h"
#include "imgui.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct GLFWwindow;

// Manages ImGui initialization and per-frame rendering and hosts auxiliary
// UI panels. Currently only a placeholder chart panel is provided.
class UiManager {
public:
  ~UiManager();
  bool setup(GLFWwindow *window);
  void begin_frame();
  // Update available trading pairs. Only rebuilds internal arrays if the
  // provided list differs from the currently cached one.
  void set_pairs(const std::vector<std::string> &pairs);
  // Update available intervals. Only rebuilds internal arrays if the provided
  // list differs from the currently cached one.
  void set_intervals(const std::vector<std::string> &intervals);
  // Draw docked panels each frame.
  void draw_chart_panel();
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
  // Require TradingView/WebView chart; if true, never fall back to ImPlot.
  void set_require_tv_chart(bool require);
  // Timeout (ms) to wait for WebView readiness before considering fallback.
  void set_webview_ready_timeout_ms(int ms);
  void set_webview_throttle_ms(int ms);
  void end_frame(GLFWwindow *window);
  void shutdown();
  // Provide the absolute or executable-relative path to chart HTML.
  void set_chart_html_path(const std::string &path);

  enum class DrawTool { None, Line, HLine, VLine, Rect, Ruler, Long, Short, Fibo };
  enum class SeriesType { Candlestick, Line, Area };

  struct Position {
    int id;
    bool is_long;
    double time1;
    double price1;
    double time2;
    double price2;
  };
  void add_position(const Position &p);
  void update_position(const Position &p);
  void remove_position(int id);

private:
  // UI polish state
  bool high_contrast_theme_ = false;
  ImVec4 accent_color_ = ImVec4(0.08f, 0.56f, 0.96f, 1.0f); // blue accent

  std::vector<Core::Candle> candles_;
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
  std::vector<Position> positions_;
  int next_position_id_ = 1;
  DrawTool current_tool_ = DrawTool::None;
  SeriesType current_series_ = SeriesType::Candlestick;
  std::vector<DrawObject> draw_objects_;
  bool drawing_first_point_ = false;
  int editing_object_ = -1;
  int hovered_object_ = -1;
  bool dragging_object_ = false;
  double temp_x_ = 0.0;
  double temp_y_ = 0.0;
  DrawObject drag_origin_{};
  int context_object_ = -1;
  std::optional<double> price_line_;
  std::string current_interval_;
  std::string current_pair_;
  bool fit_next_plot_ = false;
  bool use_utc_time_ = false;
  bool show_seconds_pref_ = false; // show seconds on axis/cursor when true (otherwise derive from interval)
  bool snap_to_candles_ = true;
  std::function<void(const std::string &)> on_interval_changed_;
  std::function<void(const std::string &)> on_pair_changed_;
  std::function<void(const std::string &)> status_callback_;
  bool shutdown_called_ = false;
  bool owns_imgui_context_ = false;
  bool layout_built_ = false;
  mutable std::mutex ui_mutex_;
  bool require_tv_chart_ = false;
  int webview_ready_timeout_ms_ = 5000;

  // Cached data for pair and interval selection combos
  std::vector<std::string> pair_strings_;
  std::vector<const char *> pair_items_;
  std::vector<std::string> interval_strings_;
  std::vector<const char *> interval_items_;

  // Throttling for real-time candle pushes
  std::chrono::steady_clock::time_point last_push_time_{};
  // Throttle real-time push updates; default 500ms for smoother live candles.
  std::chrono::milliseconds throttle_interval_{500};
  std::optional<Core::Candle> cached_candle_{};

#ifdef HAVE_WEBVIEW
  void *webview_ = nullptr;
  std::jthread webview_thread_{};
  bool webview_ready_ = false;
  bool webview_missing_chart_ = false;
  bool webview_init_failed_ = false;
  std::string chart_html_path_{};
  std::string chart_url_{};
  std::optional<std::chrono::steady_clock::time_point> webview_nav_time_{};
  std::optional<std::chrono::steady_clock::time_point> last_nav_retry_time_{};
  int nav_retry_interval_ms_ = 2000;
  int nav_retry_max_ = 60;
  int nav_retry_count_ = 0;
#if defined(_WIN32)
  bool com_initialized_ = false;
#endif
#if defined(_WIN32)
  void *webview_host_hwnd_ = nullptr; // HWND for embedded WebView child
#endif
  // Queue for JavaScript commands posted before WebView is ready or to marshal
  // execution onto the WebView UI thread safely.
  std::vector<std::string> pending_js_;
  void post_js(const std::string &js);
#endif

  GLFWwindow *glfw_window_ = nullptr;

  // Internal helpers for UI chrome
  void draw_top_bar();
  void draw_status_bar();
};
