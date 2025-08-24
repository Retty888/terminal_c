#include "ui_manager.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdio>
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

void PlotCandlestick(const char *label_id, const double *xs,
                     const double *opens, const double *closes,
                     const double *lows, const double *highs, int count,
                     bool tooltip = true, float width_percent = 0.25f,
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
    const double *it = std::lower_bound(xs, xs + count, mouse.x);
    int idx = -1;
    if (it == xs + count) {
      idx = count - 1;
    } else if (it == xs) {
      if (*it == mouse.x)
        idx = 0;
    } else {
      if (*it > mouse.x)
        --it;
      idx = static_cast<int>(it - xs);
    }
    int idx = BinarySearch(xs, 0, count - 1, rounded_x);
    if (idx != -1) {
      ImGui::BeginTooltip();
      auto tp = std::chrono::system_clock::time_point(
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
    ImU32 color = ImGui::GetColorU32(opens[i] > closes[i] ? bearCol : bullCol);
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

void AddCandle(std::vector<Core::Candle> &candles, const Core::Candle &candle) {
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

void UiManager::draw_chart_panel(const std::vector<std::string> &pairs,
                                 const std::vector<std::string> &intervals) {
  // Display the currently selected interval in the panel title so users can
  // easily confirm the timeframe of the data being shown. If the interval is
  // empty, fall back to the plain "Chart" title.
  std::string title = "Chart";
  if (!current_interval_.empty()) {
    title += " - " + current_interval_;
  }
  ImGui::Begin(title.c_str());

  int pair_index = 0;
  std::vector<const char *> pair_items;
  pair_items.reserve(pairs.size());
  for (std::size_t i = 0; i < pairs.size(); ++i) {
    pair_items.push_back(pairs[i].c_str());
    if (pairs[i] == current_pair_)
      pair_index = static_cast<int>(i);
  }
  if (ImGui::Combo("Pair", &pair_index, pair_items.data(),
                   static_cast<int>(pair_items.size()))) {
    if (pair_index >= 0 && pair_index < static_cast<int>(pairs.size())) {
      current_pair_ = pairs[pair_index];
      if (on_pair_changed_)
        on_pair_changed_(current_pair_);
    }
  }

  int interval_index = 0;
  std::vector<const char *> interval_items;
  interval_items.reserve(intervals.size());
  for (std::size_t i = 0; i < intervals.size(); ++i) {
    interval_items.push_back(intervals[i].c_str());
    if (intervals[i] == current_interval_)
      interval_index = static_cast<int>(i);
  }
  if (ImGui::Combo("Interval", &interval_index, interval_items.data(),
                   static_cast<int>(interval_items.size()))) {
    if (interval_index >= 0 &&
        interval_index < static_cast<int>(intervals.size())) {
      current_interval_ = intervals[interval_index];
      if (on_interval_changed_)
        on_interval_changed_(current_interval_);
    }
  }
  int tool_index = static_cast<int>(current_tool_);
  const char *tool_items[] = {"None", "Line", "HLine"};
  if (ImGui::Combo("Tool", &tool_index, tool_items,
                   static_cast<int>(IM_ARRAYSIZE(tool_items)))) {
    current_tool_ = static_cast<DrawTool>(tool_index);
    drawing_first_point_ = false;
    editing_object_ = -1;
  }
#ifdef HAVE_WEBVIEW
  if (!webview_) {
    webview_ = webview_create(0, nullptr);
    auto html_path = Core::path_from_executable("chart.html");
    std::string url = std::string("file://") + html_path.generic_string();
    webview_navigate(static_cast<webview_t>(webview_), url.c_str());
    webview_bind(
        static_cast<webview_t>(webview_), "setInterval",
        [](webview_t w, const char *seq, const char *req, void *arg) {
          auto self = static_cast<UiManager *>(arg);
          if (self->on_interval_changed_) {
            try {
              auto j = nlohmann::json::parse(req);
              if (j.is_array() && !j.empty())
                self->on_interval_changed_(j[0].get<std::string>());
            } catch (...) {
            }
          }
          webview_return(w, seq, 0, nullptr);
        },
        this);
    webview_bind(
        static_cast<webview_t>(webview_), "setPair",
        [](webview_t w, const char *seq, const char *req, void *arg) {
          auto self = static_cast<UiManager *>(arg);
          if (self->on_pair_changed_) {
            try {
              auto j = nlohmann::json::parse(req);
              if (j.is_array() && !j.empty())
                self->on_pair_changed_(j[0].get<std::string>());
            } catch (...) {
            }
          }
          webview_return(w, seq, 0, nullptr);
        },
        this);
    webview_bind(
        static_cast<webview_t>(webview_), "status",
        [](webview_t w, const char *seq, const char *req, void *arg) {
          auto self = static_cast<UiManager *>(arg);
          if (self->status_callback_) {
            try {
              auto j = nlohmann::json::parse(req);
              if (j.is_array() && !j.empty())
                self->status_callback_(j[0].get<std::string>());
            } catch (...) {
            }
          }
          webview_return(w, seq, 0, nullptr);
        },
        this);
    webview_thread_ = std::jthread([
      this
    ](std::stop_token) { webview_run(static_cast<webview_t>(webview_)); });
    webview_ready_ = true;
    {
      std::lock_guard<std::mutex> lock(ui_mutex_);
      if (!candles_.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto &c : candles_) {
          arr.push_back({{"time", c.open_time / 1000},
                         {"open", c.open},
                         {"high", c.high},
                         {"low", c.low},
                         {"close", c.close}});
        }
        std::string js = "series.setData(" + arr.dump() + ");";
        webview_eval(static_cast<webview_t>(webview_), js.c_str());
      }
      if (price_line_) {
        std::ostringstream oss;
        oss << "chart.setPriceLine(" << *price_line_ << ");";
        webview_eval(static_cast<webview_t>(webview_), oss.str().c_str());
      }
    }
  }
  ImGui::TextUnformatted("WebView chart running in external window.");
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
    ImPlot::SetupAxisFormat(ImAxis_X1, "%Y-%m-%d %H:%M");
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
      double x_min = xs.front();
      double x_max = xs.back();
      ImPlotPoint mouse = ImPlot::GetPlotMousePos();
      if (ImPlot::IsPlotHovered()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          if (editing_object_ >= 0) {
            auto &obj = draw_objects_[editing_object_];
            if (obj.type == DrawTool::Line) {
              if (!drawing_first_point_) {
                obj.x1 = mouse.x;
                obj.y1 = mouse.y;
                drawing_first_point_ = true;
              } else {
                obj.x2 = mouse.x;
                obj.y2 = mouse.y;
                drawing_first_point_ = false;
                editing_object_ = -1;
              }
            } else if (obj.type == DrawTool::HLine) {
              obj.y1 = obj.y2 = mouse.y;
              obj.x1 = x_min;
              obj.x2 = x_max;
              editing_object_ = -1;
            }
          } else {
            if (current_tool_ == DrawTool::Line) {
              if (!drawing_first_point_) {
                temp_x_ = mouse.x;
                temp_y_ = mouse.y;
                drawing_first_point_ = true;
              } else {
                draw_objects_.push_back(
                    {DrawTool::Line, temp_x_, temp_y_, mouse.x, mouse.y});
                drawing_first_point_ = false;
              }
            } else if (current_tool_ == DrawTool::HLine) {
              draw_objects_.push_back(
                  {DrawTool::HLine, x_min, mouse.y, x_max, mouse.y});
            }
          }
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          int idx = -1;
          ImVec2 mp = ImPlot::PlotToPixels(mouse);
          const float threshold = 5.0f;
          for (std::size_t i = 0; i < draw_objects_.size(); ++i) {
            const auto &o = draw_objects_[i];
            ImVec2 p1 = ImPlot::PlotToPixels(o.x1, o.y1);
            ImVec2 p2 = ImPlot::PlotToPixels(o.x2, o.y2);
            if (o.type == DrawTool::HLine) {
              if (std::fabs(mp.y - p1.y) < threshold) {
                idx = static_cast<int>(i);
                break;
              }
            } else if (o.type == DrawTool::Line) {
              double dx = p2.x - p1.x;
              double dy = p2.y - p1.y;
              double len2 = dx * dx + dy * dy;
              if (len2 > 0.0) {
                double t = ((mp.x - p1.x) * dx + (mp.y - p1.y) * dy) / len2;
                t = std::clamp(t, 0.0, 1.0);
                double projx = p1.x + t * dx;
                double projy = p1.y + t * dy;
                double dist2 = (mp.x - projx) * (mp.x - projx) +
                               (mp.y - projy) * (mp.y - projy);
                if (std::sqrt(dist2) < threshold) {
                  idx = static_cast<int>(i);
                  break;
                }
              }
            }
          }
          if (idx != -1) {
            context_object_ = idx;
            ImGui::OpenPopup("DrawObjContext");
          }
        }
      }
      if (ImGui::BeginPopup("DrawObjContext")) {
        if (ImGui::MenuItem("Edit")) {
          editing_object_ = context_object_;
          drawing_first_point_ = false;
        }
        if (ImGui::MenuItem("Delete")) {
          if (context_object_ >= 0 &&
              context_object_ < static_cast<int>(draw_objects_.size())) {
            draw_objects_.erase(draw_objects_.begin() + context_object_);
          }
          editing_object_ = -1;
        }
        ImGui::EndPopup();
      }
      for (std::size_t i = 0; i < draw_objects_.size(); ++i) {
        auto &o = draw_objects_[i];
        if (o.type == DrawTool::Line) {
          double lx[2] = {o.x1, o.x2};
          double ly[2] = {o.y1, o.y2};
          ImPlot::PlotLine((std::string("L") + std::to_string(i)).c_str(), lx,
                           ly, 2);
        } else if (o.type == DrawTool::HLine) {
          double lx[2] = {o.x1, o.x2};
          double ly[2] = {o.y1, o.y1};
          ImPlot::PlotLine((std::string("H") + std::to_string(i)).c_str(), lx,
                           ly, 2);
          char buf[32];
          std::snprintf(buf, sizeof(buf), "%.2f", o.y1);
          ImPlot::Annotation((o.x1 + o.x2) / 2.0, o.y1, ImVec4(1, 1, 1, 1),
                             ImVec2(0, -5.0f), true, buf);
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
#ifdef HAVE_WEBVIEW
  if (webview_) {
    std::string js = "chart.setMarkers(" + markers_json + ");";
    webview_eval(static_cast<webview_t>(webview_), js.c_str());
  }
#endif
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
#ifdef HAVE_WEBVIEW
  if (webview_) {
    std::ostringstream oss;
    oss << "chart.setPriceLine(" << price << ");";
    webview_eval(static_cast<webview_t>(webview_), oss.str().c_str());
  }
#endif
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

void UiManager::set_candles(const std::vector<Core::Candle> &candles) {
  std::lock_guard<std::mutex> lock(ui_mutex_);
#ifdef HAVE_WEBVIEW
  if (webview_) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &c : candles) {
      arr.push_back({{"time", c.open_time / 1000},
                     {"open", c.open},
                     {"high", c.high},
                     {"low", c.low},
                     {"close", c.close}});
    }
    std::string js = "series.setData(" + arr.dump() + ");";
    webview_eval(static_cast<webview_t>(webview_), js.c_str());
  }
#endif
  candles_.clear();
  candles_.reserve(candles.size());
  candles_.insert(candles_.end(), candles.begin(), candles.end());
  cached_candle_.reset();
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
#ifdef HAVE_WEBVIEW
  if (webview_) {
    nlohmann::json j = {{"time", candle.open_time / 1000},
                        {"open", candle.open},
                        {"high", candle.high},
                        {"low", candle.low},
                        {"close", candle.close}};
    std::string js = "updateCandle(" + j.dump() + ");";
    webview_eval(static_cast<webview_t>(webview_), js.c_str());
  }
#endif
}

void UiManager::set_interval_callback(
    std::function<void(const std::string &)> cb) {
  on_interval_changed_ = std::move(cb);
}

void UiManager::set_pair_callback(std::function<void(const std::string &)> cb) {
  on_pair_changed_ = std::move(cb);
}

void UiManager::set_status_callback(
    std::function<void(const std::string &)> cb) {
  status_callback_ = std::move(cb);
}

void UiManager::set_initial_interval(const std::string &interval) {
  current_interval_ = interval;
}

void UiManager::set_initial_pair(const std::string &pair) {
  current_pair_ = pair;
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
#ifdef HAVE_WEBVIEW
  if (webview_) {
    webview_dispatch(static_cast<webview_t>(webview_),
                     [](webview_t w, void *) { webview_terminate(w); },
                     nullptr);
    if (webview_thread_.joinable())
      webview_thread_.join();
    webview_destroy(static_cast<webview_t>(webview_));
    webview_ = nullptr;
    webview_ready_ = false;
  }
#endif
  ImPlot::DestroyContext();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
