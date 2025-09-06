#include "ui_manager.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32) && defined(EMBED_WEBVIEW)
#include <windows.h>
extern "C" HWND glfwGetWin32Window(GLFWwindow *window);
#endif
#if defined(_WIN32)
#include <objbase.h>
#endif
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
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
#include "core/logger.h"
#ifndef _WIN32
#include <sys/resource.h>
#endif
#include "imgui.h"
#include "imgui_impl_glfw.h"
#if defined(UI_BACKEND_DX11)
#include "imgui_impl_dx11.h"
#include "core/dx11_context.h"
#else
#include "imgui_impl_opengl3.h"
#endif
#include "implot.h"

namespace {

const char *ToolToString(UiManager::DrawTool t) {
  switch (t) {
  case UiManager::DrawTool::None:
    return "cross";
  case UiManager::DrawTool::Line:
    return "trend";
  case UiManager::DrawTool::HLine:
    return "hline";
  case UiManager::DrawTool::Ruler:
    return "ruler";
  case UiManager::DrawTool::Long:
    return "long";
  case UiManager::DrawTool::Short:
    return "short";
  case UiManager::DrawTool::Fibo:
    return "fibo";
  }
  return "cross";
}

UiManager::DrawTool ToolFromString(const std::string &s) {
  if (s == "trend")
    return UiManager::DrawTool::Line;
  if (s == "hline")
    return UiManager::DrawTool::HLine;
  if (s == "ruler")
    return UiManager::DrawTool::Ruler;
  if (s == "long")
    return UiManager::DrawTool::Long;
  if (s == "short")
    return UiManager::DrawTool::Short;
  if (s == "fibo")
    return UiManager::DrawTool::Fibo;
  return UiManager::DrawTool::None;
}

const char *SeriesTypeToString(UiManager::SeriesType t) {
  switch (t) {
  case UiManager::SeriesType::Line:
    return "LineSeries";
  case UiManager::SeriesType::Area:
    return "AreaSeries";
  case UiManager::SeriesType::Candlestick:
  default:
    return "CandlestickSeries";
  }
}

UiManager::SeriesType SeriesTypeFromString(const std::string &s) {
  if (s == "LineSeries")
    return UiManager::SeriesType::Line;
  if (s == "AreaSeries")
    return UiManager::SeriesType::Area;
  return UiManager::SeriesType::Candlestick;
}

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
    const double *it = std::lower_bound(xs, xs + count, rounded_x);
    int idx = -1;
    if (it != xs + count && *it == rounded_x)
      idx = static_cast<int>(it - xs);
    if (idx != -1) {
      ImGui::BeginTooltip();
      auto tp = std::chrono::system_clock::time_point(
          std::chrono::seconds(static_cast<long long>(xs[idx])));
      std::time_t tt = std::chrono::system_clock::to_time_t(tp);
      std::tm tm;
#if defined(_WIN32)
      gmtime_s(&tm, &tt);
#else
      gmtime_r(&tt, &tm);
#endif
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
  glfw_window_ = window;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  owns_imgui_context_ = true;
  ImGuiIO &io = ImGui::GetIO();
  if (io.Fonts->Fonts.empty()) {
    io.Fonts->AddFontDefault();
  }
  const auto ini_path =
#if defined(UI_BACKEND_DX11)
      Core::path_from_executable("imgui_dx11.ini");
#else
      Core::path_from_executable("imgui.ini");
#endif
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
  // Optional: reset layout via env, to avoid stale/off-screen positions
  if (const char* reset = std::getenv("CANDLE_RESET_LAYOUT")) {
    if (reset[0] == '1') {
      std::error_code ec;
      std::filesystem::remove(ini_path, ec);
      load_ini = false;
      if (status_callback_) status_callback_("Layout reset by CANDLE_RESET_LAYOUT=1");
    }
  }
  io.IniFilename = ini_path_str.c_str();
  if (load_ini) {
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
  }
  ImGui::StyleColorsDark();
  // Initialize platform backend
#if defined(UI_BACKEND_DX11)
  if (!ImGui_ImplGlfw_InitForOther(window, true)) {
#else
  if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
#endif
    Core::Logger::instance().error(
        "Failed to initialize ImGui GLFW backend");
    return false;
  }
#if defined(UI_BACKEND_DX11)
  if (!ImGui_ImplDX11_Init(Core::Dx11Context::instance().device(), Core::Dx11Context::instance().context())) {
    Core::Logger::instance().error("Failed to initialize ImGui DX11 backend");
    return false;
  }
  // Proactively create font/device objects to ensure TexID is valid
  if (!ImGui_ImplDX11_CreateDeviceObjects()) {
    Core::Logger::instance().error("ImGui DX11 CreateDeviceObjects failed");
  }
  else {
    Core::Logger::instance().info("ImGui DX11 device objects created");
  }
#if defined(HAVE_WEBVIEW)
  Core::Logger::instance().info("HAVE_WEBVIEW=1");
#else
  Core::Logger::instance().info("HAVE_WEBVIEW=0");
#endif
#if defined(EMBED_WEBVIEW)
  Core::Logger::instance().info("EMBED_WEBVIEW=1");
#else
  Core::Logger::instance().info("EMBED_WEBVIEW=0");
#endif
#else
  if (!ImGui_ImplOpenGL3_Init("#version 130")) {
    Core::Logger::instance().error("Failed to initialize ImGui OpenGL3 backend");
    return false;
  }
#endif
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
#if defined(UI_BACKEND_DX11)
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  // Ensure ImGui DX11 device objects (font texture) exist; otherwise nothing will render.
  if (ImGui::GetIO().Fonts && ImGui::GetIO().Fonts->TexID == (ImTextureID)0) {
    if (!ImGui_ImplDX11_CreateDeviceObjects()) {
      Core::Logger::instance().error("ImGui DX11 CreateDeviceObjects failed (late)");
    } else {
      Core::Logger::instance().info("ImGui DX11 device objects (late) created");
    }
  }
#else
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
#endif
  ImGui::NewFrame();
}

void UiManager::set_pairs(const std::vector<std::string> &pairs) {
  if (pairs != pair_strings_) {
    pair_strings_ = pairs;
    pair_items_.clear();
    pair_items_.reserve(pair_strings_.size());
    for (auto &p : pair_strings_) {
      pair_items_.push_back(p.c_str());
    }
  }
}

void UiManager::set_intervals(const std::vector<std::string> &intervals) {
  if (intervals != interval_strings_) {
    interval_strings_ = intervals;
    interval_items_.clear();
    interval_items_.reserve(interval_strings_.size());
    for (auto &i : interval_strings_) {
      interval_items_.push_back(i.c_str());
    }
#ifdef HAVE_WEBVIEW
    if (webview_) {
      nlohmann::json arr = nlohmann::json::array();
      for (const auto &s : interval_strings_) arr.push_back(s);
      std::string js = "setIntervals(" + arr.dump() + ");";
      webview_eval(static_cast<webview_t>(webview_), js.c_str());
      if (!current_interval_.empty()) {
        std::string js2 = "setActiveInterval('" + current_interval_ + "');";
        webview_eval(static_cast<webview_t>(webview_), js2.c_str());
      }
    }
#endif
  }
}

void UiManager::draw_chart_panel() {
  // Display the currently selected interval in the panel title so users can
  // easily confirm the timeframe of the data being shown. If the interval is
  // empty, fall back to the plain "Chart" title.
  std::string title = "Chart";
  if (!current_interval_.empty()) {
    title += " - " + current_interval_;
  }
  auto vp = ImGui::GetMainViewport();
  // Lay out: reserve a left panel and optional bottom strip for other windows.
  const float left_w = 360.0f;
  const float bottom_h = 260.0f;
  static int s_chart_frames = 0;
  ++s_chart_frames;
  ImGuiCond cond = (s_chart_frames < 60) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + left_w, vp->WorkPos.y), cond);
  ImGui::SetNextWindowSize(
      ImVec2(std::max(100.0f, vp->WorkSize.x - left_w),
             std::max(100.0f, vp->WorkSize.y - bottom_h)),
      cond);
  ImGui::Begin(title.c_str());

  int pair_index = 0;
  for (std::size_t i = 0; i < pair_strings_.size(); ++i) {
    if (pair_strings_[i] == current_pair_)
      pair_index = static_cast<int>(i);
  }
  if (ImGui::Combo("Pair", &pair_index, pair_items_.data(),
                   static_cast<int>(pair_items_.size()))) {
    if (pair_index >= 0 && pair_index < static_cast<int>(pair_strings_.size())) {
      current_pair_ = pair_strings_[pair_index];
      if (on_pair_changed_)
        on_pair_changed_(current_pair_);
    }
  }

  int interval_index = 0;
  for (std::size_t i = 0; i < interval_strings_.size(); ++i) {
    if (interval_strings_[i] == current_interval_)
      interval_index = static_cast<int>(i);
  }
  if (!interval_items_.empty()) {
    if (ImGui::Combo("Interval", &interval_index, interval_items_.data(),
                     static_cast<int>(interval_items_.size()))) {
      if (interval_index >= 0 &&
          interval_index < static_cast<int>(interval_strings_.size())) {
        current_interval_ = interval_strings_[interval_index];
        if (on_interval_changed_)
          on_interval_changed_(current_interval_);
        ImGuiIO &io = ImGui::GetIO();
        if (io.IniFilename)
          ImGui::SaveIniSettingsToDisk(io.IniFilename);
      }
    }
  } else {
    ImGui::Text("No intervals");
  }
  ImGui::SameLine();
  if (ImGui::Button("Fit")) {
    fit_next_plot_ = true;
  }
  int series_index = static_cast<int>(current_series_);
  const char *series_items[] = {"CandlestickSeries", "LineSeries",
                                "AreaSeries"};
  if (ImGui::Combo("Chart Type", &series_index, series_items,
                   static_cast<int>(IM_ARRAYSIZE(series_items)))) {
    current_series_ = static_cast<SeriesType>(series_index);
#ifdef HAVE_WEBVIEW
    if (webview_) {
      std::string js = "setActiveSeries('" +
                       std::string(SeriesTypeToString(current_series_)) + "');";
      post_js(js);
    }
#endif
  }
  ImGui::SetItemTooltip(
      "Selects how the chart is displayed: candlesticks, line, or area.");
  auto set_tool = [&](DrawTool t) {
    current_tool_ = t;
    drawing_first_point_ = false;
    editing_object_ = -1;
#ifdef HAVE_WEBVIEW
    if (webview_) {
      std::string js =
          "setActiveTool('" + std::string(ToolToString(current_tool_)) + "');";
      post_js(js);
    }
#endif
  };
  ImGui::BeginChild("ToolPanel", ImVec2(80, 0), true);
  if (ImGui::Button("Cross"))
    set_tool(DrawTool::None);
  if (ImGui::Button("Trend"))
    set_tool(DrawTool::Line);
  if (ImGui::Button("HLine"))
    set_tool(DrawTool::HLine);
  if (ImGui::Button("Ruler"))
    set_tool(DrawTool::Ruler);
  if (ImGui::Button("Long"))
    set_tool(DrawTool::Long);
  if (ImGui::Button("Short"))
    set_tool(DrawTool::Short);
  if (ImGui::Button("Fibo"))
    set_tool(DrawTool::Fibo);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("ChartChild", ImVec2(0, 0), false);
  // Determine the on-screen rect for embedding the WebView (Windows)
  ImVec2 screen_pos = ImGui::GetCursorScreenPos();
  ImVec2 avail = ImGui::GetContentRegionAvail();
#ifndef HAVE_WEBVIEW
  (void)screen_pos; (void)avail;
#endif
#ifdef HAVE_WEBVIEW
  const bool disable_webview = std::getenv("CANDLE_DISABLE_WEBVIEW") != nullptr;
  const bool external_webview = std::getenv("CANDLE_WEBVIEW_EXTERNAL") != nullptr;
  if (!disable_webview && !webview_ && !webview_missing_chart_ && !webview_init_failed_) {
    std::filesystem::path html_path;
    if (!chart_html_path_.empty()) {
      html_path = chart_html_path_;
    } else {
      html_path = Core::path_from_executable("chart.html");
    }
    if (std::filesystem::exists(html_path)) {
      void *parent = nullptr;
#if defined(_WIN32) && defined(EMBED_WEBVIEW)
      if (glfw_window_ && !external_webview) {
        HWND hwnd_parent = glfwGetWin32Window(glfw_window_);
        if (hwnd_parent) {
          // Force child-host for reliability (ignore env to avoid accidental parent-host)
          bool no_child = false;
          if (no_child) {
            parent = hwnd_parent;
            if (status_callback_) status_callback_("Using parent window as WebView host (no child)");
          } else {
            // Translate ImGui screen coords to parent client coords
            POINT clientTL{0,0};
            ClientToScreen(hwnd_parent, &clientTL);
            int x = static_cast<int>(screen_pos.x - clientTL.x);
            int y = static_cast<int>(screen_pos.y - clientTL.y);
            int w = std::max(1, static_cast<int>(avail.x));
            int h = std::max(1, static_cast<int>(avail.y));
            HWND host = CreateWindowExW(0, L"STATIC", L"",
                                        WS_CHILD | WS_VISIBLE,
                                        x, y, w, h, hwnd_parent, nullptr,
                                        GetModuleHandleW(nullptr), nullptr);
            if (host) {
              webview_host_hwnd_ = host;
              parent = host;
              if (status_callback_) {
                std::ostringstream oss;
                oss << "Created WebView host hwnd at (" << x << ", " << y
                    << ") size (" << w << "x" << h << ")";
                status_callback_(oss.str());
              }
            } else if (status_callback_) {
              status_callback_("Failed to create WebView host window");
            }
          }
        }
      }
#endif
      if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        com_initialized_ = true;
      }
      webview_ = webview_create(0, parent);
      if (webview_) {
          if (status_callback_)
            status_callback_("WebView created");
          
          webview_bind(
              static_cast<webview_t>(webview_), "appSetInterval",
            [](const char *seq, const char *req, void *arg) {
              auto self = static_cast<UiManager *>(arg);
              if (self->on_interval_changed_) {
                try {
                  auto j = nlohmann::json::parse(req);
                  if (j.is_array() && !j.empty())
                    self->on_interval_changed_(j[0].get<std::string>());
                } catch (...) {
                }
              }
              webview_return(static_cast<webview_t>(self->webview_), seq, 0,
                             "null");
            },
            this);
        webview_bind(
            static_cast<webview_t>(webview_), "appSetPair",
            [](const char *seq, const char *req, void *arg) {
              auto self = static_cast<UiManager *>(arg);
              if (self->on_pair_changed_) {
                try {
                  auto j = nlohmann::json::parse(req);
                  if (j.is_array() && !j.empty())
                    self->on_pair_changed_(j[0].get<std::string>());
                } catch (...) {
                }
              }
              webview_return(static_cast<webview_t>(self->webview_), seq, 0,
                             "null");
            },
            this);
        webview_bind(
            static_cast<webview_t>(webview_), "appStatus",
            [](const char *seq, const char *req, void *arg) {
              auto self = static_cast<UiManager *>(arg);
              if (self->status_callback_) {
                try {
                  auto j = nlohmann::json::parse(req);
                  if (j.is_array() && !j.empty())
                    self->status_callback_(j[0].get<std::string>());
                } catch (...) {
                }
              }
              webview_return(static_cast<webview_t>(self->webview_), seq, 0,
                             "null");
            },
            this);
        webview_bind(
            static_cast<webview_t>(webview_), "appSetTool",
            [](const char *seq, const char *req, void *arg) {
              auto self = static_cast<UiManager *>(arg);
              try {
                auto j = nlohmann::json::parse(req);
                if (j.is_array() && !j.empty())
                  self->current_tool_ =
                      ToolFromString(j[0].get<std::string>());
              } catch (...) {
              }
              webview_return(static_cast<webview_t>(self->webview_), seq, 0,
                             "null");
            },
            this);
        webview_bind(
            static_cast<webview_t>(webview_), "appSetSeries",
            [](const char *seq, const char *req, void *arg) {
              auto self = static_cast<UiManager *>(arg);
              try {
                auto j = nlohmann::json::parse(req);
                if (j.is_array() && !j.empty())
                  self->current_series_ =
                      SeriesTypeFromString(j[0].get<std::string>());
              } catch (...) {
              }
              webview_return(static_cast<webview_t>(self->webview_), seq, 0,
                             "null");
            },
            this);
        // Bind page-ready signal to push initial state after DOM/chart are ready
        webview_bind(
            static_cast<webview_t>(webview_), "appReady",
            [](const char *seq, const char * /*req*/, void *arg) {
              auto *self = static_cast<UiManager *>(arg);
              if (self->webview_ready_) {
                // Ignore duplicate ready notifications to avoid re-pushing initialization
                webview_return(static_cast<webview_t>(self->webview_), seq, 0, "{}");
                return;
              }
              self->webview_ready_ = true;
              {
                std::lock_guard<std::mutex> lock(self->ui_mutex_);
                if (!self->candles_.empty()) {
                  nlohmann::json arr = nlohmann::json::array();
                  for (const auto &c : self->candles_) {
                    arr.push_back({{"time", c.open_time / 1000},
                                   {"open", c.open},
                                   {"high", c.high},
                                   {"low", c.low},
                                   {"close", c.close}});
                  }
                  std::string js = "series.setData(" + arr.dump() + ");";
                  self->post_js(js);
                }
                if (!self->interval_strings_.empty()) {
                  nlohmann::json arr_iv = nlohmann::json::array();
                  for (const auto &s : self->interval_strings_) arr_iv.push_back(s);
                  std::string jsiv = "setIntervals(" + arr_iv.dump() + ");";
                  self->post_js(jsiv);
                }
                if (!self->current_interval_.empty()) {
                  std::string jsai = "setActiveInterval('" + self->current_interval_ + "');";
                  self->post_js(jsai);
                }
                if (self->price_line_) {
                  std::ostringstream oss;
                  oss << "chart.setPriceLine(" << *self->price_line_ << ");";
                  self->post_js(oss.str());
                }
                std::string js_series =
                    "setActiveSeries('" +
                    std::string(SeriesTypeToString(self->current_series_)) + "');";
                self->post_js(js_series);
                std::string js_tool =
                    "setActiveTool('" + std::string(ToolToString(self->current_tool_)) + "');";
                self->post_js(js_tool);
              }
              // Flush any queued JS that accumulated before readiness
              {
                std::vector<std::string> queued;
                {
                  std::lock_guard<std::mutex> lock(self->ui_mutex_);
                  queued.swap(self->pending_js_);
                }
                for (auto &cmd : queued) self->post_js(cmd);
              }
              if (auto *self2 = static_cast<UiManager *>(arg); self2 && self2->status_callback_) {
                self2->status_callback_("WebView initialized and data (if any) pushed");
              }
              webview_return(static_cast<webview_t>(static_cast<UiManager *>(arg)->webview_), seq, 0, "{}");
            },
            this);
        // Navigate after bindings are in place
        // Prefer inlining HTML+JS to avoid file:// quirks and improve readiness
        bool inline_ok = false;
        try {
          std::ifstream hf(html_path, std::ios::binary);
          if (hf) {
            std::string html((std::istreambuf_iterator<char>(hf)), std::istreambuf_iterator<char>());
            // Try to inline lightweight-charts if referenced by relative path
            auto dir = html_path.parent_path();
            std::string tag = "<script src=\"lightweight-charts.standalone.production.js\"></script>";
            auto pos = html.find(tag);
            if (pos != std::string::npos) {
              std::ifstream jf((dir / "lightweight-charts.standalone.production.js").string(), std::ios::binary);
              if (jf) {
                std::string js((std::istreambuf_iterator<char>(jf)), std::istreambuf_iterator<char>());
                std::string inline_tag = std::string("<script>\n") + js + "\n</script>";
                html.replace(pos, tag.size(), inline_tag);
              }
            }
            if (status_callback_) status_callback_("Loading inline chart HTML");
            if (webview_set_html(static_cast<webview_t>(webview_), html.c_str()) == 0) {
              inline_ok = true;
            }
          }
        } catch (...) {}
        if (!inline_ok) {
          chart_url_ = std::string("file://") + html_path.generic_string();
          if (status_callback_)
            status_callback_(std::string("Navigating to ") + chart_url_);
          if (std::getenv("CANDLE_WEBVIEW_TEST")) {
            const char *test = "data:text/html,%3Ch1%3EOK%3C/h1%3E";
            webview_navigate(static_cast<webview_t>(webview_), test);
          } else {
            webview_navigate(static_cast<webview_t>(webview_), chart_url_.c_str());
          }
        } else {
          chart_url_.clear();
        }
        webview_nav_time_ = std::chrono::steady_clock::now();
        last_nav_retry_time_.reset();
        nav_retry_count_ = 0;
        // Set initial size if hosting in parent (no child hwnd)
        int w = std::max(1, static_cast<int>(avail.x));
        int h = std::max(1, static_cast<int>(avail.y));
        if (webview_host_hwnd_ == nullptr) {
          webview_set_size(static_cast<webview_t>(webview_), w, h, WEBVIEW_HINT_NONE);
        }
        // Run a dedicated webview loop thread for robustness regardless of host mode
        webview_thread_ = std::jthread([this](std::stop_token) {
          webview_run(static_cast<webview_t>(webview_));
        });
      } else {
        webview_init_failed_ = true;
        if (status_callback_)
          status_callback_("WebView initialization failed");
      }
    } else {
      webview_missing_chart_ = true;
      if (status_callback_) {
        std::string p = html_path.empty() ? std::string("chart.html")
                                          : html_path.string();
        status_callback_("chart HTML not found: " + p);
      }
    }
  }
  if (webview_missing_chart_ || webview_init_failed_ || !webview_) {
    // When WebView is unavailable yet
    if (require_tv_chart_) {
      ImGui::Text("Loading TradingView chart... area %.0fx%.0f, candles: %zu", avail.x, avail.y, candles_.size());
      if (webview_init_failed_) ImGui::TextColored(ImVec4(1,0.6f,0,1), "WebView init failed; will retry.");
      if (webview_missing_chart_) ImGui::TextColored(ImVec4(1,0.6f,0,1), "Chart HTML not found.");
      ImGui::SameLine();
      if (ImGui::Button("Reload Chart")) {
        // Force reload attempt: clear state and let the setup re-run next frame
        webview_init_failed_ = false;
        webview_missing_chart_ = false;
        if (webview_) {
          webview_dispatch(static_cast<webview_t>(webview_), [](webview_t w, void*){ webview_terminate(w); }, nullptr);
          if (webview_thread_.joinable()) webview_thread_.join();
          webview_destroy(static_cast<webview_t>(webview_));
          webview_ = nullptr;
          webview_ready_ = false;
        }
        nav_retry_count_ = 0;
        last_nav_retry_time_.reset();
        webview_nav_time_.reset();
      }
    } else {
      ImGui::Text("Chart area %.0fx%.0f, candles: %zu", avail.x, avail.y, candles_.size());
      ImGui::SameLine();
      if (ImGui::Button("Fit")) { fit_next_plot_ = true; }
    }
    // Series selector (Candlestick / Line)
    {
      int series_idx = (current_series_ == SeriesType::Line) ? 1 : 0;
      const char* series_items[] = {"Candlestick", "Line"};
      ImGui::SameLine();
      ImGui::SetNextItemWidth(140);
      if (ImGui::Combo("##series", &series_idx, series_items, IM_ARRAYSIZE(series_items))) {
        current_series_ = (series_idx == 1) ? SeriesType::Line : SeriesType::Candlestick;
      }
    }
    // Interval selector sourced from available intervals
    if (!interval_strings_.empty()) {
      int active_idx = 0;
      for (int i = 0; i < (int)interval_strings_.size(); ++i) {
        if (interval_strings_[i] == current_interval_) { active_idx = i; break; }
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120);
      if (ImGui::Combo("##interval", &active_idx, [](void* data,int idx,const char** out){
            auto* vec = static_cast<std::vector<std::string>*>(data);
            if (idx < 0 || idx >= (int)vec->size()) return false;
            *out = (*vec)[idx].c_str(); return true;
          }, &interval_strings_, (int)interval_strings_.size())) {
        current_interval_ = interval_strings_[active_idx];
        if (on_interval_changed_) on_interval_changed_(current_interval_);
      }
    }
    // Fallback native chart (only if allowed)
    if (!require_tv_chart_ && !candles_.empty()) {
      std::vector<double> xs(candles_.size());
      std::vector<double> o(candles_.size());
      std::vector<double> h(candles_.size());
      std::vector<double> l(candles_.size());
      std::vector<double> c(candles_.size());
      double min_x = std::numeric_limits<double>::max();
      double max_x = std::numeric_limits<double>::lowest();
      double min_y = std::numeric_limits<double>::max();
      double max_y = std::numeric_limits<double>::lowest();
      for (size_t i = 0; i < candles_.size(); ++i) {
        xs[i] = static_cast<double>(candles_[i].open_time / 1000);
        o[i] = candles_[i].open;
        h[i] = candles_[i].high;
        l[i] = candles_[i].low;
        c[i] = candles_[i].close;
        min_x = std::min(min_x, xs[i]);
        max_x = std::max(max_x, xs[i]);
        min_y = std::min(min_y, l[i]);
        max_y = std::max(max_y, h[i]);
      }
      if (ImPlot::BeginPlot("Native Chart", ImVec2(-1, -1), 0)) {
        // Enable pan/zoom; initial fit once, Fit button forces Always
        ImPlot::SetupAxes("Time", "Price", ImPlotAxisFlags_NoMenus, ImPlotAxisFlags_NoMenus);
        // Pad ranges a bit for readability
        double dx = (max_x - min_x) * 0.02; if (dx <= 0) dx = 1.0;
        double dy = (max_y - min_y) * 0.05; if (dy <= 0) dy = 1.0;
        ImPlot::SetupAxesLimits(min_x - dx, max_x + dx, min_y - dy, max_y + dy, ImPlotCond_Once);
        if (fit_next_plot_) {
          ImPlot::SetupAxesLimits(min_x - dx, max_x + dx, min_y - dy, max_y + dy, ImPlotCond_Always);
          fit_next_plot_ = false;
        }
        if (current_series_ == SeriesType::Line) {
          ImPlot::PlotLine("close", xs.data(), c.data(), (int)xs.size());
        } else {
          PlotCandlestick("ohlc", xs.data(), o.data(), c.data(), l.data(), h.data(), static_cast<int>(xs.size()));
        }
        ImPlot::EndPlot();
      }
    } else if (!require_tv_chart_) {
      ImGui::TextUnformatted("No candles to display");
    }
  } else {
    // Keep embedded WebView child sized to the ImGui region
#if defined(_WIN32) && defined(EMBED_WEBVIEW)
    if (webview_host_hwnd_) {
      HWND hwnd_parent = glfwGetWin32Window(glfw_window_);
      if (hwnd_parent) {
        POINT clientTL{0,0};
        ClientToScreen(hwnd_parent, &clientTL);
        int x = static_cast<int>(screen_pos.x - clientTL.x);
        int y = static_cast<int>(screen_pos.y - clientTL.y);
        int w = std::max(1, static_cast<int>(avail.x));
        int h = std::max(1, static_cast<int>(avail.y));
        SetWindowPos(static_cast<HWND>(webview_host_hwnd_), HWND_TOP,
                     x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
      }
    }
#endif
    // If hosting in parent (no child hwnd), update WebView size to match panel
    if (webview_ && webview_host_hwnd_ == nullptr) {
      int w = std::max(1, static_cast<int>(avail.x));
      int h = std::max(1, static_cast<int>(avail.y));
      webview_set_size(static_cast<webview_t>(webview_), w, h, WEBVIEW_HINT_NONE);
    }
    // If WebView did not signal readiness, optionally retry navigate
    if (webview_ && !webview_ready_ && webview_nav_time_) {
      auto now = std::chrono::steady_clock::now();
      // Retry navigation while waiting
      if (!chart_url_.empty()) {
        auto should_retry = [&]() {
          if (!last_nav_retry_time_) return true;
          return (now - *last_nav_retry_time_) > std::chrono::milliseconds(nav_retry_interval_ms_);
        }();
        if (should_retry && nav_retry_count_ < nav_retry_max_) {
          if (status_callback_) {
            std::ostringstream oss; oss << "Re-navigating to chart (attempt " << (nav_retry_count_+1) << ")";
            status_callback_(oss.str());
          }
          webview_navigate(static_cast<webview_t>(webview_), chart_url_.c_str());
          last_nav_retry_time_ = now;
          ++nav_retry_count_;
        }
      } else {
        // If using inline HTML, re-apply HTML on retry
        auto should_retry = [&]() {
          if (!last_nav_retry_time_) return true;
          return (now - *last_nav_retry_time_) > std::chrono::milliseconds(nav_retry_interval_ms_);
        }();
        if (should_retry && nav_retry_count_ < nav_retry_max_) {
          if (status_callback_) status_callback_("Re-applying inline chart HTML");
          // This is a light op since HTML is already in memory only during create; fallback to file if needed on next frame
          // No cached HTML here; rely on JS readiness retry and status text in UI
          last_nav_retry_time_ = now;
          ++nav_retry_count_;
        }
      }
      if (now - *webview_nav_time_ > std::chrono::milliseconds(webview_ready_timeout_ms_)) {
        if (require_tv_chart_) {
          if (status_callback_) status_callback_("WebView not ready yet; continuing to wait per configuration");
          // keep waiting, don't tear down
        } else {
          if (status_callback_) status_callback_("WebView did not become ready, falling back to native chart");
          // Tear down current WebView
#ifdef HAVE_WEBVIEW
        webview_dispatch(
            static_cast<webview_t>(webview_),
            [](webview_t w, void *) { webview_terminate(w); }, nullptr);
        if (webview_thread_.joinable()) webview_thread_.join();
        webview_destroy(static_cast<webview_t>(webview_));
        webview_ = nullptr;
        webview_ready_ = false;
#if defined(_WIN32) && defined(EMBED_WEBVIEW)
        if (webview_host_hwnd_) {
          DestroyWindow(static_cast<HWND>(webview_host_hwnd_));
          webview_host_hwnd_ = nullptr;
        }
#endif
        webview_init_failed_ = true;
        webview_nav_time_.reset();
#endif
        }
      }
    }
  }
#endif
#ifndef HAVE_WEBVIEW
    ImGui::TextUnformatted("WebView support disabled");
#endif
  ImGui::EndChild();
  ImGui::End();
}

void UiManager::set_markers(const std::string &markers_json) {
  std::lock_guard<std::mutex> lock(ui_mutex_);
#ifdef HAVE_WEBVIEW
  if (webview_) {
    std::string js = "chart.setMarkers(" + markers_json + ");";
    post_js(js);
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
    post_js(oss.str());
  }
#endif
  price_line_ = price;
}

void UiManager::add_position(const Position &p) {
  std::lock_guard<std::mutex> lock(ui_mutex_);
#ifdef HAVE_WEBVIEW
  if (webview_) {
    nlohmann::json j = {
        {"id", p.id},       {"tool", p.is_long ? "long" : "short"},
        {"time1", p.time1}, {"price1", p.price1},
        {"time2", p.time2}, {"price2", p.price2}};
    std::string js = "addPosition(" + j.dump() + ");";
    post_js(js);
  }
#endif
  auto it = std::find_if(positions_.begin(), positions_.end(),
                         [&](const Position &x) { return x.id == p.id; });
  if (it == positions_.end())
    positions_.push_back(p);
  else
    *it = p;
}

void UiManager::update_position(const Position &p) { add_position(p); }

void UiManager::remove_position(int id) {
  std::lock_guard<std::mutex> lock(ui_mutex_);
#ifdef HAVE_WEBVIEW
  if (webview_) {
    std::ostringstream oss;
    oss << "removePosition(" << id << ");";
    post_js(oss.str());
  }
#endif
  positions_.erase(
      std::remove_if(positions_.begin(), positions_.end(),
                     [id](const Position &x) { return x.id == id; }),
      positions_.end());
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
    post_js(js);
  }
#endif
  candles_.clear();
  candles_.reserve(candles.size());
  candles_.insert(candles_.end(), candles.begin(), candles.end());
  cached_candle_.reset();
  if (!candles_.empty()) {
    fit_next_plot_ = true;
  }
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
    post_js(js);
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

void UiManager::set_require_tv_chart(bool require) {
  require_tv_chart_ = require;
}

void UiManager::set_webview_ready_timeout_ms(int ms) {
  if (ms > 0) webview_ready_timeout_ms_ = ms;
}

void UiManager::set_webview_throttle_ms(int ms) {
  if (ms > 0) throttle_interval_ = std::chrono::milliseconds(ms);
}

void UiManager::end_frame(GLFWwindow *window) {
  ImGui::Render();
  (void)window;
#if defined(UI_BACKEND_DX11)
  Core::Dx11Context::instance().begin_frame();
  // Optional visibility debug overlay (off by default). Enable by setting CANDLE_VIS_DEBUG=1
  static int s_dbg_vis = [](){ const char* e = std::getenv("CANDLE_VIS_DEBUG"); return (e && e[0] == '1') ? 1 : 0; }();
  if (s_dbg_vis) {
    Core::Dx11Context::instance().debug_draw_corner_marker(1.0f, 0.1f, 0.1f, 1.0f);
  }
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  // No forced fullscreen debug fill in normal mode
  Core::Dx11Context::instance().end_frame();
#else
  int display_w, display_h;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window);
#endif
}

void UiManager::shutdown() {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  if (shutdown_called_)
    return;
  shutdown_called_ = true;
#ifdef HAVE_WEBVIEW
  if (webview_) {
    webview_dispatch(
        static_cast<webview_t>(webview_),
        [](webview_t w, void *) { webview_terminate(w); }, nullptr);
    if (webview_thread_.joinable())
      webview_thread_.join();
    webview_destroy(static_cast<webview_t>(webview_));
    webview_ = nullptr;
    webview_ready_ = false;
  }
  #if defined(_WIN32) && defined(EMBED_WEBVIEW)
  if (webview_host_hwnd_) {
    DestroyWindow(static_cast<HWND>(webview_host_hwnd_));
    webview_host_hwnd_ = nullptr;
  }
  #endif
#if defined(_WIN32)
  if (com_initialized_) {
    CoUninitialize();
    com_initialized_ = false;
  }
#endif
#endif
  // Only tear down ImGui/ImPlot if we created them via setup().
  if (owns_imgui_context_) {
    ImPlot::DestroyContext();
    #if defined(UI_BACKEND_DX11)
      ImGui_ImplDX11_Shutdown();
      ImGui_ImplGlfw_Shutdown();
    #else
      ImGui_ImplOpenGL3_Shutdown();
      ImGui_ImplGlfw_Shutdown();
    #endif
    ImGui::DestroyContext();
    owns_imgui_context_ = false;
  }
}

#ifdef HAVE_WEBVIEW
void UiManager::set_chart_html_path(const std::string &path) {
  chart_html_path_ = path;
}

void UiManager::post_js(const std::string &js) {
  // Queue until the WebView has signaled readiness, then dispatch safely.
  if (!webview_ || !webview_thread_.joinable() || !webview_ready_) {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    pending_js_.push_back(js);
    return;
  }
  webview_dispatch(static_cast<webview_t>(webview_),
                   [](webview_t w, void *arg) {
                     std::unique_ptr<std::string> cmd(static_cast<std::string *>(arg));
                     webview_eval(w, cmd->c_str());
                   },
                   new std::string(js));
}
#else
void UiManager::set_chart_html_path(const std::string &) {}
#endif
