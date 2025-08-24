#include "ui_manager.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#ifdef HAVE_WEBVIEW
#include "webview.h"
#endif

#include "core/path_utils.h"
#ifndef _WIN32
#include <sys/resource.h>
#endif
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

namespace {
template <typename T>
int BinarySearch(const T *arr, int l, int r, T x) {
  if (r >= l) {
    int mid = l + (r - l) / 2;
    if (arr[mid] == x)
      return mid;
    if (arr[mid] > x)
      return BinarySearch(arr, l, mid - 1, x);
    return BinarySearch(arr, mid + 1, r, x);
  }
  return -1;
}

void PlotCandlestick(const char *label_id, const double *xs, const double *opens,
                     const double *closes, const double *lows,
                     const double *highs, int count, bool tooltip = true,
                     float width_percent = 0.25f,
                     ImVec4 bullCol = ImVec4(0, 1, 0, 1),
                     ImVec4 bearCol = ImVec4(1, 0, 0, 1)) {
  (void)label_id;
  ImDrawList *draw_list = ImPlot::GetPlotDrawList();
  double half_width =
      count > 1 ? (xs[1] - xs[0]) * width_percent : width_percent;

  if (ImPlot::IsPlotHovered() && tooltip) {
    ImPlotPoint mouse = ImPlot::GetPlotMousePos();
    double rounded_x = std::round(mouse.x);
    float tool_l =
        ImPlot::PlotToPixels(rounded_x - half_width * 1.5, mouse.y).x;
    float tool_r =
        ImPlot::PlotToPixels(rounded_x + half_width * 1.5, mouse.y).x;
    float tool_t = ImPlot::GetPlotPos().y;
    float tool_b = tool_t + ImPlot::GetPlotSize().y;
    ImPlot::PushPlotClipRect();
    draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b),
                             IM_COL32(128, 128, 128, 64));
    ImPlot::PopPlotClipRect();
    int idx = BinarySearch(xs, 0, count - 1, rounded_x);
    if (idx != -1) {
      ImGui::BeginTooltip();
      auto tp =
          std::chrono::system_clock::time_point(
              std::chrono::seconds(static_cast<long long>(xs[idx])));
      std::time_t tt = std::chrono::system_clock::to_time_t(tp);
      std::tm tm = *std::gmtime(&tt);
      std::ostringstream oss;
      oss << std::put_time(&tm, "%Y-%m-%d");
      ImGui::Text("Day:   %s", oss.str().c_str());
      ImGui::Text("Open:  $%.2f", opens[idx]);
      ImGui::Text("Close: $%.2f", closes[idx]);
      ImGui::Text("Low:   $%.2f", lows[idx]);
      ImGui::Text("High:  $%.2f", highs[idx]);
      ImGui::EndTooltip();
    }
  }

  for (int i = 0; i < count; ++i) {
    ImVec2 open_pos = ImPlot::PlotToPixels(xs[i] - half_width, opens[i]);
    ImVec2 close_pos = ImPlot::PlotToPixels(xs[i] + half_width, closes[i]);
    ImVec2 low_pos = ImPlot::PlotToPixels(xs[i], lows[i]);
    ImVec2 high_pos = ImPlot::PlotToPixels(xs[i], highs[i]);
    ImU32 color =
        ImGui::GetColorU32(opens[i] > closes[i] ? bearCol : bullCol);
    draw_list->AddLine(low_pos, high_pos, color);
    draw_list->AddRectFilled(open_pos, close_pos, color);
  }
}

ImVec4 ColorFromHex(const std::string &hex) {
  unsigned int c = 0;
  if (hex.size() >= 7 && hex[0] == '#') {
    std::stringstream ss;
    ss << std::hex << hex.substr(1);
    ss >> c;
  }
  float r = ((c >> 16) & 0xFF) / 255.0f;
  float g = ((c >> 8) & 0xFF) / 255.0f;
  float b = (c & 0xFF) / 255.0f;
  return ImVec4(r, g, b, 1.0f);
}

void AddCandle(std::vector<Core::Candle> &candles,
               const Core::Candle &candle) {
  if (!candles.empty() && candles.back().open_time == candle.open_time)
    candles.back() = candle;
  else
    candles.push_back(candle);
}
} // namespace

UiManager::~UiManager() { shutdown(); }

bool UiManager::setup(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGui::LoadIniSettingsFromMemory("");
  const auto ini_path = Core::path_from_executable("imgui.ini");
  std::filesystem::create_directories(ini_path.parent_path());
  static std::string ini_path_str = ini_path.string();
  bool load_ini = true;
  if (std::filesystem::exists(ini_path)) {
    std::ifstream file(ini_path);
    std::string line;
    while (std::getline(file, line)) {
      if (line.rfind("Size=", 0) == 0) {
        int w = 0;
        int h = 0;
        if (std::sscanf(line.c_str(), "Size=%d,%d", &w, &h) == 2) {
          if (w < 100 || h < 100) {
            std::error_code ec;
            std::filesystem::remove(ini_path, ec);
            load_ini = false;
          }
        }
        break;
      }
    }
  }
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = ini_path_str.c_str();
  if (load_ini) {
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
  }
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");
  return true;
}

void UiManager::begin_frame() {
  {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    if (cached_candle_) {
      auto now = std::chrono::steady_clock::now();
      if (now - last_push_time_ >= throttle_interval_) {
        last_push_time_ = now;
        AddCandle(candles_, *cached_candle_);
        cached_candle_.reset();
      }
    }
  }
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void UiManager::draw_chart_panel(const std::string &selected_interval) {
  (void)selected_interval;
  ImGui::Begin("Chart");
#ifdef HAVE_WEBVIEW
  ImGui::TextUnformatted("WebView chart unavailable in this environment.");
#else
  ImGui::TextUnformatted(
      "WebView support disabled; displaying fallback candlestick chart.");
  std::vector<double> xs, opens, closes, lows, highs;
  {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    xs.reserve(candles_.size());
    opens.reserve(candles_.size());
    closes.reserve(candles_.size());
    lows.reserve(candles_.size());
    highs.reserve(candles_.size());
    for (const auto &c : candles_) {
      xs.push_back(c.open_time / 1000.0);
      opens.push_back(c.open);
      closes.push_back(c.close);
      lows.push_back(c.low);
      highs.push_back(c.high);
    }
  }
  if (ImPlot::BeginPlot("Candles", ImVec2(-1, -1))) {
    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
    ImPlot::SetupAxisFormat(ImAxis_X1, "%Y-%m-%d");
    if (!xs.empty()) {
      PlotCandlestick("price", xs.data(), opens.data(), closes.data(),
                      lows.data(), highs.data(), (int)xs.size());
      if (price_line_) {
        double xline[2] = {xs.front(), xs.back()};
        double yline[2] = {*price_line_, *price_line_};
        ImPlot::PlotLine("PriceLine", xline, yline, 2);
      }
      for (const auto &m : markers_) {
        auto it = std::find_if(candles_.begin(), candles_.end(),
                               [&](const Core::Candle &c) {
                                 return c.open_time / 1000.0 == m.time;
                               });
        if (it != candles_.end()) {
          double price = m.above ? it->high : it->low;
          ImPlot::Annotation(m.time, price, m.color,
                             ImVec2(0, m.above ? -10.0f : 10.0f), true,
                             m.text.c_str());
        }
      }
    }
    ImPlot::EndPlot();
  }
#endif
  ImGui::End();
}

void UiManager::set_markers(const std::string &markers_json) {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  markers_.clear();
  try {
    auto arr = nlohmann::json::parse(markers_json);
    for (auto &m : arr) {
      Marker mk{};
      mk.time = m.value("time", 0.0);
      std::string pos = m.value("position", "aboveBar");
      mk.above = pos != "belowBar";
      mk.text = m.value("text", "");
      mk.color = ColorFromHex(m.value("color", "#FF0000"));
      markers_.push_back(std::move(mk));
    }
  } catch (...) {
    // ignore parse errors
  }
}

void UiManager::set_price_line(double price) {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  price_line_ = price;
}

std::function<void(const std::string &)> UiManager::candle_callback() {
  return [this](const std::string &json) {
    try {
      auto j = nlohmann::json::parse(json);
      Core::Candle c{};
      c.open_time = j.value("time", 0ULL) * 1000ULL;
      c.open = j.value("open", 0.0);
      c.high = j.value("high", 0.0);
      c.low = j.value("low", 0.0);
      c.close = j.value("close", 0.0);
      c.volume = j.value("volume", 0.0);
      push_candle(c);
    } catch (...) {
      // ignore parse errors
    }
  };
}

void UiManager::push_candle(const Core::Candle &candle) {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  auto now = std::chrono::steady_clock::now();
  if (now - last_push_time_ < throttle_interval_) {
    cached_candle_ = candle;
    return;
  }
  last_push_time_ = now;
  AddCandle(candles_, candle);
}

void UiManager::set_interval_callback(
    std::function<void(const std::string &)> cb) {
  on_interval_changed_ = std::move(cb);
}

void UiManager::set_status_callback(
    std::function<void(const std::string &)> cb) {
  status_callback_ = std::move(cb);
}

void UiManager::set_initial_interval(const std::string &interval) {
  current_interval_ = interval;
}

void UiManager::end_frame(GLFWwindow *window) {
  ImGui::Render();
  int display_w, display_h;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window);
}

void UiManager::shutdown() {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  if (shutdown_called_)
    return;
  shutdown_called_ = true;
  ImPlot::DestroyContext();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

