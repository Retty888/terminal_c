#include "ui_manager.h"

#include "imgui_internal.h"
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
#include "core/data_dir.h"
#include "core/interval_utils.h"
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
  case UiManager::DrawTool::VLine:
    return "vline";
  case UiManager::DrawTool::Rect:
    return "rect";
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
  if (s == "vline")
    return UiManager::DrawTool::VLine;
  if (s == "rect")
    return UiManager::DrawTool::Rect;
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
                     ImVec4 bullCol = ImVec4(0.19f, 0.88f, 0.63f, 1.0f),
                     ImVec4 bearCol = ImVec4(0.98f, 0.35f, 0.35f, 1.0f)) {
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
    // If not exact match, choose nearest neighbor
    if (idx == -1) {
      int i2 = static_cast<int>(it - xs);
      int i1 = i2 - 1;
      double best_d = 1e300; int best_i = -1;
      if (i1 >= 0) { double d = std::abs(xs[i1] - rounded_x); if (d < best_d) { best_d = d; best_i = i1; } }
      if (i2 >= 0 && i2 < count) { double d = std::abs(xs[i2] - rounded_x); if (d < best_d) { best_d = d; best_i = i2; } }
      idx = best_i;
    }
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
      oss << std::put_time(&tm, "%d.%m");
      ImGui::Text("Day:   %s", oss.str().c_str());
      ImGui::Text("Open:  %.2f", opens[idx]);
      ImGui::Text("Close: %.2f", closes[idx]);
      ImGui::Text("Low:   %.2f", lows[idx]);
      ImGui::Text("High:  %.2f", highs[idx]);
      ImGui::EndTooltip();

      // Highlight hovered candle body and wick
      ImU32 hcol = IM_COL32(20, 144, 245, 220); // blue accent
      // Body rectangle corners (top-left, bottom-right in pixels)
      double top_v = std::max(opens[idx], closes[idx]);
      double bot_v = std::min(opens[idx], closes[idx]);
      ImVec2 ra = ImPlot::PlotToPixels(xs[idx] - half_width, top_v);
      ImVec2 rb = ImPlot::PlotToPixels(xs[idx] + half_width, bot_v);
      draw_list->AddRect(ra, rb, hcol, 0.0f, 0, 2.0f);
      // Subtle fill to emphasize
      draw_list->AddRectFilled(ra, rb, IM_COL32(20,144,245,40));
      // Wick highlight
      ImVec2 lw = ImPlot::PlotToPixels(xs[idx], lows[idx]);
      ImVec2 hw = ImPlot::PlotToPixels(xs[idx], highs[idx]);
      draw_list->AddLine(lw, hw, hcol, 2.0f);
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

// Ensure candle arrays are valid for plotting: finite numbers, sane highs/lows,
// ascending or at least non-pathological X ordering; drop invalid points.
void BuildPlotArrays(const std::vector<Core::Candle>& in,
                     std::vector<double>& xs,
                     std::vector<double>& o,
                     std::vector<double>& h,
                     std::vector<double>& l,
                     std::vector<double>& c,
                     std::vector<double>& v) {
  xs.clear(); o.clear(); h.clear(); l.clear(); c.clear(); v.clear();
  xs.reserve(in.size()); o.reserve(in.size()); h.reserve(in.size());
  l.reserve(in.size()); c.reserve(in.size()); v.reserve(in.size());
  auto is_finite = [](double x){ return std::isfinite(x); };
  for (const auto& cd : in) {
    double xo = (double)(cd.open_time / 1000);
    double oo = cd.open, hh = cd.high, ll = cd.low, cc = cd.close, vv = cd.volume;
    if (!is_finite(xo) || !is_finite(oo) || !is_finite(hh) || !is_finite(ll) || !is_finite(cc) || !is_finite(vv))
      continue;
    // Fix swapped high/low if needed
    double mx = std::max({oo, cc, hh});
    double mn = std::min({oo, cc, ll});
    if (hh < mx) hh = mx;
    if (ll > mn) ll = mn;
    xs.push_back(xo); o.push_back(oo); h.push_back(hh); l.push_back(ll); c.push_back(cc); v.push_back(vv);
  }
  // If timestamps are unsorted, sort them with a simple index sort
  bool sorted = true;
  for (size_t i=1;i<xs.size();++i) if (xs[i] < xs[i-1]) { sorted = false; break; }
  if (!sorted) {
    std::vector<size_t> idx(xs.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b){ return xs[a] < xs[b]; });
    auto reorder = [&](std::vector<double>& arr){
      std::vector<double> tmp(arr.size());
      for (size_t i=0;i<idx.size();++i) tmp[i] = arr[idx[i]];
      arr.swap(tmp);
    };
    reorder(xs); reorder(o); reorder(h); reorder(l); reorder(c); reorder(v);
  }
}

// Apply Hyperliquid-inspired dark theme to ImPlot/ImGui
void ApplyHyperliquidPlotStyle() {
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;
  ImVec4* colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.19f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.16f, 0.17f, 0.18f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.23f, 0.24f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.25f, 0.26f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
  colors[ImGuiCol_Border]    = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
  colors[ImGuiCol_Button]    = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);
  colors[ImGuiCol_ButtonActive]  = ImVec4(0.26f, 0.28f, 0.32f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
  colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);

  ImPlotStyle &ps = ImPlot::GetStyle();
  ImVec4* pc = ps.Colors;
  pc[ImPlotCol_PlotBg]       = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
  pc[ImPlotCol_AxisText]     = ImVec4(0.80f, 0.82f, 0.85f, 1.0f);
  pc[ImPlotCol_AxisGrid]     = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
  pc[ImPlotCol_AxisBg]       = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
  pc[ImPlotCol_AxisBgHovered]= ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
  pc[ImPlotCol_LegendBg]     = ImVec4(0.08f, 0.08f, 0.09f, 0.90f);
  pc[ImPlotCol_LegendText]   = ImVec4(0.80f, 0.82f, 0.85f, 1.0f);
  pc[ImPlotCol_LegendBorder] = ImVec4(0.25f, 0.25f, 0.26f, 0.6f);
}

// High-contrast variant for accessibility
void ApplyHighContrastPlotStyle() {
  ApplyHyperliquidPlotStyle();
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.03f, 0.035f, 1.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_Border] = ImVec4(0.46f, 0.46f, 0.50f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.46f, 0.46f, 0.50f, 1.00f);
  ImPlotStyle &ps = ImPlot::GetStyle();
  ImVec4* pc = ps.Colors;
  pc[ImPlotCol_AxisGrid] = ImVec4(0.30f, 0.30f, 0.34f, 1.0f);
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

// Generate human-friendly time ticks over [min_x, max_x] in seconds since epoch
// and apply them to ImPlot X-axis. Chooses a "nice" step aiming ~8-12 ticks.
void SetupTimeAxisTicks(double min_x, double max_x, const std::string &interval, bool use_utc, bool show_seconds_pref) {
  if (!(max_x > min_x)) return;
  const double span = max_x - min_x;
  // Candidate steps in seconds
  static const long long kSteps[] = {
      5, 15, 30, 60, 300, 900, 1800, 3600, 2 * 3600, 4 * 3600, 12 * 3600, 24 * 3600};
  // Target around 10 ticks
  const double target = 10.0;
  long long best = 60; // default 1m
  double best_diff = 1e18;
  for (auto s : kSteps) {
    double n = span / (double)s;
    double d = std::abs(n - target);
    if (d < best_diff) { best_diff = d; best = s; }
  }
  // Build ticks
  std::vector<double> ticks;
  std::vector<std::string> labels;
  ticks.reserve((size_t)(span / best) + 3);
  labels.reserve(ticks.capacity());

  auto align = [&](long long t, long long step) -> long long {
    return (t / step) * step;
  };
  long long t0 = (long long)std::floor(min_x);
  long long start = align(t0, best);
  if (start < (long long)min_x) start += best;

  // Determine if we should include seconds in the label based on interval
  bool show_seconds = show_seconds_pref;
  try {
    // parse_interval returns std::chrono::milliseconds
    auto ms = Core::parse_interval(interval);
    if (!show_seconds_pref)
      show_seconds = (ms.count() < 60'000);
  } catch (...) {
    // fallback: use seconds only for sub-minute steps
    if (!show_seconds_pref)
      show_seconds = (best < 60);
  }

  for (long long t = start; t <= (long long)max_x + best; t += best) {
    ticks.push_back((double)t);
    std::time_t tt = (time_t)t;
    std::tm tm{};
#if defined(_WIN32)
    if (use_utc) gmtime_s(&tm, &tt); else localtime_s(&tm, &tt);
#else
    if (use_utc) gmtime_r(&tt, &tm); else localtime_r(&tt, &tm);
#endif
    char buf[20] = {0};
    if (best >= 24 * 3600) {
      std::strftime(buf, sizeof(buf), "%d.%m", &tm);
    } else if (best >= 3600) {
      std::strftime(buf, sizeof(buf), "%d.%m %H:%M", &tm);
    } else {
      std::strftime(buf, sizeof(buf), show_seconds ? "%H:%M:%S" : "%H:%M", &tm);
    }
    labels.emplace_back(buf);
  }
  std::vector<const char*> cstrs; cstrs.reserve(labels.size());
  for (auto &s : labels) cstrs.push_back(s.c_str());
  ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(), (int)ticks.size(), cstrs.data());
}

std::string FormatTimeLabel(double x_seconds, bool show_seconds, bool use_utc) {
  std::time_t tt = (time_t)std::llround(x_seconds);
  std::tm tm{};
#if defined(_WIN32)
  if (use_utc) gmtime_s(&tm, &tt); else localtime_s(&tm, &tt);
#else
  if (use_utc) gmtime_r(&tt, &tm); else localtime_r(&tt, &tm);
#endif
  char buf[32] = {0};
  std::strftime(buf, sizeof(buf), show_seconds ? "%d.%m %H:%M:%S" : "%d.%m %H:%M", &tm);
  return std::string(buf);
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
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  if (io.Fonts->Fonts.empty()) {
    io.Fonts->AddFontDefault();
  }
  // Try to load optional modern fonts from resources/fonts
  try {
    auto font_path = Core::path_from_executable("resources/fonts/Inter-Regular.ttf");
    auto font_semibold = Core::path_from_executable("resources/fonts/Inter-SemiBold.ttf");
    ImFontConfig cfg; cfg.OversampleH = 4; cfg.OversampleV = 4; cfg.PixelSnapH = true;
    if (std::filesystem::exists(font_path)) {
      ImFont* regular = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), 16.0f, &cfg);
      if (regular) io.FontDefault = regular;
    }
    if (std::filesystem::exists(font_semibold)) {
      io.Fonts->AddFontFromFileTTF(font_semibold.string().c_str(), 16.0f, &cfg);
    }
  } catch (...) {
    // ignore font loading errors; keep defaults
  }
  // Apply Hyperliquid-inspired style
  ApplyHyperliquidPlotStyle();
  ImGuiStyle& style = ImGui::GetStyle();
  style.Alpha = 1.0f;
  style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  style.Colors[ImGuiCol_TitleBg].w = 1.0f;
  style.Colors[ImGuiCol_TitleBgActive].w = 1.0f;
  // Disable advanced features by default (older ImGui may not define flags)
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
  // Temporarily disable loading ImGui ini to avoid invisible windows from stale layout
  load_ini = false;
  io.IniFilename = ini_path_str.c_str();
  if (load_ini) {
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
  }
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

  // Create a full-screen dockspace and build default layout once
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("MainDockSpaceHost", nullptr, host_flags);
    ImGui::PopStyleVar(2);
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0,0), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!layout_built_) {
      layout_built_ = true;
      ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, vp->Size);
      // Split: left (control panel) ~22%, bottom (logs/analysis) ~28%
      ImGuiID dock_main_id = dockspace_id;
      ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
      ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);
      // Dock windows
      ImGui::DockBuilderDockWindow("Control Panel", dock_id_left);
      ImGui::DockBuilderDockWindow("Chart", dock_main_id);
      ImGui::DockBuilderDockWindow("Journal", dock_id_down);
      ImGui::DockBuilderDockWindow("Backtest", dock_id_down);
      ImGui::DockBuilderDockWindow("Analytics", dock_id_down);
      ImGui::DockBuilderDockWindow("Signals", dock_id_down);
      // Make bottom area tabbed by merging siblings (DockBuilder will tab by docking multiple windows into same node)
      ImGui::DockBuilderFinish(dockspace_id);
    }
    ImGui::End();
  }

  // Draw UI chrome overlays each frame
  draw_top_bar();
  draw_status_bar();
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
  const char* kChartWindowName = "Chart";
  auto vp = ImGui::GetMainViewport();
  // Lay out: reserve a left panel and optional bottom strip for other windows.
  const float left_w = 360.0f;
  const float bottom_h = 260.0f;
  static int s_chart_frames = 0;
  ++s_chart_frames;
  ImGuiCond cond = (s_chart_frames < 60) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
  // If docking is enabled, let the dockspace control placement; otherwise use manual layout
  if (!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)) {
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + left_w, vp->WorkPos.y), cond);
    ImGui::SetNextWindowSize(
        ImVec2(std::max(100.0f, vp->WorkSize.x - left_w),
               std::max(100.0f, vp->WorkSize.y - bottom_h)),
        cond);
  }
  ImGui::Begin(kChartWindowName);
  if (!current_interval_.empty()) {
    ImGui::TextUnformatted(current_interval_.c_str());
    ImGui::Separator();
  }

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
  // Chart content (no nested child window to avoid ImGui stack mismatches)
  // Tool selector for native drawing
  {
    ImGui::SameLine();
    ImGui::SeparatorText("Tools");
    auto tool_button = [&](const char* name, DrawTool t){
      bool active = (current_tool_ == t);
      if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.7f,0.7f,1));
      if (ImGui::Button(name)) { current_tool_ = t; drawing_first_point_ = false; }
      if (active) ImGui::PopStyleColor();
      ImGui::SameLine();
    };
    tool_button("Cross", DrawTool::None);
    tool_button("Trend", DrawTool::Line);
    tool_button("HLine", DrawTool::HLine);
    tool_button("VLine", DrawTool::VLine);
    tool_button("Rect", DrawTool::Rect);
    tool_button("Ruler", DrawTool::Ruler);
    tool_button("Fibo", DrawTool::Fibo);
    if (ImGui::Button("Clear")) { draw_objects_.clear(); drawing_first_point_ = false; }
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snap_to_candles_);
    ImGui::SameLine();
    ImGui::Checkbox("UTC", &use_utc_time_);
    ImGui::SameLine();
    ImGui::Checkbox("Secs", &show_seconds_pref_);
    ImGui::SameLine();
    if (ImGui::Button("Save Drawings")) {
      try {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto &d : draw_objects_) {
          arr.push_back({
            {"type", ToolToString(d.type)},
            {"x1", d.x1}, {"y1", d.y1}, {"x2", d.x2}, {"y2", d.y2}
          });
        }
        auto base = Core::resolve_data_dir();
        std::filesystem::create_directories(base / "drawings");
        std::filesystem::path fp = base / "drawings" / (current_pair_ + "_" + current_interval_ + ".json");
        std::ofstream ofs(fp);
        ofs << arr.dump(2);
        if (status_callback_) status_callback_(std::string("Saved drawings: ") + fp.string());
      } catch (...) {
        if (status_callback_) status_callback_("Failed to save drawings");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Drawings")) {
      try {
        auto base = Core::resolve_data_dir();
        std::filesystem::path fp = base / "drawings" / (current_pair_ + "_" + current_interval_ + ".json");
        std::ifstream ifs(fp);
        if (ifs) {
          nlohmann::json arr = nlohmann::json::parse(ifs);
          draw_objects_.clear();
          for (auto &o : arr) {
            DrawObject d{};
            d.type = ToolFromString(o.value("type", std::string("trend")));
            d.x1 = o.value("x1", 0.0); d.y1 = o.value("y1", 0.0);
            d.x2 = o.value("x2", 0.0); d.y2 = o.value("y2", 0.0);
            draw_objects_.push_back(d);
          }
          if (status_callback_) status_callback_(std::string("Loaded drawings: ") + fp.string());
        }
      } catch (...) {
        if (status_callback_) status_callback_("Failed to load drawings");
      }
    }
  }
  ImGui::EndChild();
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
    // Single toolbar for drawing tools (no duplicates)
    {
      ImGui::SameLine();
      ImGui::SeparatorText("Tools");
      auto tool_button = [&](const char* name, DrawTool t){
        bool active = (current_tool_ == t);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f,0.35f,0.38f,1));
        if (ImGui::SmallButton(name)) { current_tool_ = t; drawing_first_point_ = false; editing_object_ = -1; }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
      };
      tool_button("Cross", DrawTool::None);
      tool_button("Trend", DrawTool::Line);
      tool_button("HLine", DrawTool::HLine);
      tool_button("VLine", DrawTool::VLine);
      tool_button("Rect", DrawTool::Rect);
      tool_button("Ruler", DrawTool::Ruler);
      tool_button("Fibo", DrawTool::Fibo);
      if (ImGui::SmallButton("Clear")) { draw_objects_.clear(); drawing_first_point_ = false; editing_object_ = -1; }
      ImGui::SameLine();
      ImGui::Checkbox("Snap", &snap_to_candles_);
    }

    // Fallback native chart (only if allowed)
    std::vector<Core::Candle> snapshot;
    {
      std::lock_guard<std::mutex> lk(ui_mutex_);
      snapshot = candles_;
    }
    if (!require_tv_chart_ && !snapshot.empty()) {
      std::vector<double> xs, o, h, l, c, v;
      BuildPlotArrays(snapshot, xs, o, h, l, c, v);
      double min_x = std::numeric_limits<double>::max();
      double max_x = std::numeric_limits<double>::lowest();
      double min_y = std::numeric_limits<double>::max();
      double max_y = std::numeric_limits<double>::lowest();
      for (size_t i = 0; i < xs.size(); ++i) { min_x = std::min(min_x, xs[i]); max_x = std::max(max_x, xs[i]); min_y = std::min(min_y, l[i]); max_y = std::max(max_y, h[i]); }
      if (ImPlot::BeginPlot("Native Chart", ImVec2(-1, -1), 0)) {
        ImPlot::SetupAxes("Time", "Price", ImPlotAxisFlags_NoMenus, ImPlotAxisFlags_NoMenus);
        double dx = (max_x - min_x) * 0.02; if (dx <= 0) dx = 1.0;
        double dy = (max_y - min_y) * 0.05; if (dy <= 0) dy = 1.0;
        ImPlot::SetupAxesLimits(min_x - dx, max_x + dx, min_y - dy, max_y + dy, ImPlotCond_Once);
        if (fit_next_plot_) {
          ImPlot::SetupAxesLimits(min_x - dx, max_x + dx, min_y - dy, max_y + dy, ImPlotCond_Always);
          fit_next_plot_ = false;
        }
        // Custom time ticks/labels on X
        SetupTimeAxisTicks(min_x, max_x, current_interval_, use_utc_time_, show_seconds_pref_);
        try {
          if (current_series_ == SeriesType::Line) {
            ImPlot::PlotLine("close", xs.data(), c.data(), (int)xs.size());
          } else {
            PlotCandlestick("ohlc", xs.data(), o.data(), c.data(), l.data(), h.data(), (int)xs.size());
          }
        } catch (...) {
          Core::Logger::instance().error("ImPlot draw exception (branch A)");
        }
        // Draw stored objects (trendlines, horizontal lines)
        {
          ImDrawList* draw = ImPlot::GetPlotDrawList();
          for (const auto& d : draw_objects_) {
            ImU32 col = IM_COL32(64,64,64,200);
            switch (d.type) {
              case DrawTool::HLine: {
                ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(min_x, d.y1));
                ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(max_x, d.y1));
                draw->AddLine(a, b, col, 2.0f);
                break;
              }
              case DrawTool::VLine: {
                ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(d.x1, min_y));
                ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(d.x1, max_y));
                draw->AddLine(a, b, col, 2.0f);
                break;
              }
              case DrawTool::Rect: {
                float x1 = (float)std::min(d.x1, d.x2);
                float x2 = (float)std::max(d.x1, d.x2);
                float y1 = (float)std::min(d.y1, d.y2);
                float y2 = (float)std::max(d.y1, d.y2);
                ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(x1, y1));
                ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(x2, y2));
                draw->AddRect(a, b, col, 0.f, 0, 2.0f);
                break;
              }
              case DrawTool::Fibo: {
                float x1 = (float)std::min(d.x1, d.x2);
                float x2 = (float)std::max(d.x1, d.x2);
                float y1 = (float)std::min(d.y1, d.y2);
                float y2 = (float)std::max(d.y1, d.y2);
                const float ratios[] = {0.f, 0.382f, 0.5f, 0.618f, 1.f};
                for (float r : ratios) {
                  float y = y1 + r * (y2 - y1);
                  ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(x1, y));
                  ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(x2, y));
                  draw->AddLine(a, b, col, 1.5f);
                  char buf[16]; std::snprintf(buf, sizeof(buf), "%.3g", r);
                  draw->AddText(ImVec2(a.x + 4, a.y - 12), IM_COL32(0,0,0,200), buf);
                }
                break;
              }
              case DrawTool::Ruler: {
                ImVec2 p1 = ImPlot::PlotToPixels(ImPlotPoint(d.x1, d.y1));
                ImVec2 p2 = ImPlot::PlotToPixels(ImPlotPoint(d.x2, d.y2));
                draw->AddLine(p1, p2, col, 2.0f);
                double dt = std::abs(d.x2 - d.x1);
                double dp = std::abs(d.y2 - d.y1);
                double pct = (d.y1 != 0.0) ? (dp / std::abs(d.y1)) * 100.0 : 0.0;
                int secs = (int)std::llround(dt);
                int hh = secs / 3600; secs %= 3600; int mm = secs / 60; int ss = secs % 60;
                char lab[96]; std::snprintf(lab, sizeof(lab), "dt=%02d:%02d:%02d  dp=%.4f  d%%=%.2f%%", hh, mm, ss, dp, pct);
                ImVec2 mid = ImVec2((p1.x + p2.x)*0.5f, (p1.y + p2.y)*0.5f);
                draw->AddText(ImVec2(mid.x + 6, mid.y - 6), IM_COL32(0,0,0,200), lab);
                break;
              }
              default: {
                ImVec2 p1 = ImPlot::PlotToPixels(ImPlotPoint(d.x1, d.y1));
                ImVec2 p2 = ImPlot::PlotToPixels(ImPlotPoint(d.x2, d.y2));
                draw->AddLine(p1, p2, col, 2.0f);
                break;
              }
            }
          }
          // In-progress preview
          if (drawing_first_point_) {
            ImPlotPoint mp = ImPlot::GetPlotMousePos();
            ImU32 col = IM_COL32(96,96,96,160);
            if (current_tool_ == DrawTool::HLine) {
              ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(min_x, temp_y_));
              ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(max_x, temp_y_));
              draw->AddLine(a, b, col, 2.0f);
            } else if (current_tool_ == DrawTool::VLine) {
              ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(temp_x_, min_y));
              ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(temp_x_, max_y));
              draw->AddLine(a, b, col, 2.0f);
            } else if (current_tool_ == DrawTool::Rect || current_tool_ == DrawTool::Fibo) {
              ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(temp_x_, temp_y_));
              ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(mp.x, mp.y));
              draw->AddRect(a, b, col, 0.f, 0, 2.0f);
            } else {
              ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(temp_x_, temp_y_));
              ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(mp.x, mp.y));
              draw->AddLine(a, b, col, 2.0f);
            }
          }
        }
        // Handle drawing interactions & selection
        if (ImPlot::IsPlotHovered()) {
          ImPlotPoint mp = ImPlot::GetPlotMousePos();
          // Snap to candles on X if enabled
          if (snap_to_candles_ && !xs.empty()) {
            long long sx = (long long)std::llround(mp.x);
            long long best_t = 0; long long best_d = LLONG_MAX;
            for (const auto &tx : xs) {
              long long ot = (long long)std::llround(tx);
              long long d = std::llabs(ot - sx);
              if (d < best_d) { best_d = d; best_t = ot; }
            }
            if (best_d <= 3600) mp.x = (double)best_t; // within an hour, snap
          }
          // Hover detection for selection
          hovered_object_ = -1;
          {
            const float max_dist = 6.0f; // pixels
            ImVec2 mpx = ImPlot::PlotToPixels(ImPlotPoint(mp.x, mp.y));
            for (int i = (int)draw_objects_.size() - 1; i >= 0; --i) {
              const auto &d = draw_objects_[i];
              ImVec2 a = ImPlot::PlotToPixels(ImPlotPoint(d.x1, d.y1));
              ImVec2 b = ImPlot::PlotToPixels(ImPlotPoint(d.x2, d.y2));
              // point-to-segment distance
              ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y);
              float ab2 = ab.x*ab.x + ab.y*ab.y;
              float t = ab2 > 0 ? ((mpx.x - a.x)*ab.x + (mpx.y - a.y)*ab.y)/ab2 : 0.f;
              t = std::clamp(t, 0.f, 1.f);
              ImVec2 p = ImVec2(a.x + t*ab.x, a.y + t*ab.y);
              float dx = p.x - mpx.x, dy = p.y - mpx.y;
              if (dx*dx + dy*dy <= max_dist*max_dist) { hovered_object_ = i; break; }
            }
          }
          if (hovered_object_ >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && current_tool_ == DrawTool::None) {
            editing_object_ = hovered_object_;
            dragging_object_ = true;
            drag_origin_ = draw_objects_[editing_object_];
          }
          if (dragging_object_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // move object preserving shape
            ImPlotPoint mp2 = mp;
            DrawObject &d = draw_objects_[editing_object_];
            double dxp = mp2.x - drag_origin_.x1;
            double dyp = mp2.y - drag_origin_.y1;
            d.x1 = drag_origin_.x1 + dxp; d.y1 = drag_origin_.y1 + dyp;
            d.x2 = drag_origin_.x2 + dxp; d.y2 = drag_origin_.y2 + dyp;
          }
          if (dragging_object_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            dragging_object_ = false;
          }
          if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (current_tool_ != DrawTool::None) {
              if (!drawing_first_point_) {
                temp_x_ = mp.x; temp_y_ = mp.y; drawing_first_point_ = true;
              } else {
                DrawObject d{}; d.type = current_tool_; d.x1 = temp_x_; d.y1 = temp_y_;
                if (current_tool_ == DrawTool::HLine) { d.y2 = temp_y_; d.x1 = min_x; d.x2 = max_x; }
                else if (current_tool_ == DrawTool::VLine) { d.x2 = temp_x_; d.y2 = temp_y_; }
                else { d.x2 = mp.x; d.y2 = mp.y; }
                draw_objects_.push_back(d); drawing_first_point_ = false;
              }
            }
          }
          if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            drawing_first_point_ = false;
          }
          if (ImGui::IsKeyPressed(ImGuiKey_Delete) && editing_object_ >= 0) {
            draw_objects_.erase(draw_objects_.begin() + editing_object_);
            editing_object_ = -1; hovered_object_ = -1; dragging_object_ = false;
          }
        }
        // Crosshair readout near cursor: nearest candle OHLCV
        if (ImPlot::IsPlotHovered()) {
          ImPlotPoint mp = ImPlot::GetPlotMousePos();
          // find nearest candle index by time
          int idx = -1;
          if (!xs.empty()) {
            double target = std::round(mp.x);
            auto it = std::lower_bound(xs.begin(), xs.end(), target);
            int cand = -1;
            if (it != xs.end() && *it == target) cand = int(it - xs.begin());
            else {
              int i2 = int(it - xs.begin());
              int i1 = i2 - 1;
              double best_d = 1e300; int best_i = -1;
              if (i1 >= 0) { double d = std::abs(xs[i1] - target); if (d < best_d) { best_d = d; best_i = i1; } }
              if (i2 >= 0 && i2 < (int)xs.size()) { double d = std::abs(xs[i2] - target); if (d < best_d) { best_d = d; best_i = i2; } }
              cand = best_i;
            }
            idx = cand;
          }
          if (idx >= 0) {
            bool show_sec = false;
            try { auto ms = Core::parse_interval(current_interval_); show_sec = (ms.count() < 60'000); } catch (...) {}
            if (show_seconds_pref_) show_sec = true;
            std::string when = FormatTimeLabel(xs[idx], show_sec, use_utc_time_);
            // Compose multi-line OHLCV box
            char l1[64], l2[64], l3[64], l4[64], l5[80], l6[80];
            std::snprintf(l1, sizeof(l1), "Time: %s", when.c_str());
            std::snprintf(l2, sizeof(l2), "Open:  %.6f", o[idx]);
            std::snprintf(l3, sizeof(l3), "High:  %.6f", h[idx]);
            std::snprintf(l4, sizeof(l4), "Low:   %.6f", l[idx]);
            std::snprintf(l5, sizeof(l5), "Close: %.6f", c[idx]);
            std::snprintf(l6, sizeof(l6), "Vol:   %.6f", v[idx]);
            // Draw box in top-right
            ImVec2 plot_pos = ImPlot::GetPlotPos();
            ImVec2 plot_size = ImPlot::GetPlotSize();
            ImDrawList* dl = ImPlot::GetPlotDrawList();
            ImVec2 pad = ImVec2(6, 4);
            float line_h = ImGui::GetTextLineHeight();
            // width = max line width
            float w = 0.0f;
            const char* lines[] = {l1,l2,l3,l4,l5,l6};
            for (auto s : lines) w = std::max(w, ImGui::CalcTextSize(s).x);
            float h_box = line_h * 6 + pad.y*2;
            ImVec2 p0 = ImVec2(plot_pos.x + plot_size.x - w - pad.x*2 - 6, plot_pos.y + 6);
            ImVec2 p1 = ImVec2(p0.x + w + pad.x*2, p0.y + h_box);
            dl->AddRectFilled(p0, p1, IM_COL32(255,255,255,220), 4.0f);
            dl->AddRect(p0, p1, IM_COL32(0,0,0,200), 4.0f);
            float y = p0.y + pad.y;
            for (auto s : lines) { dl->AddText(ImVec2(p0.x + pad.x, y), IM_COL32(0,0,0,255), s); y += line_h; }
          }
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
    // Native chart path when WebView is not compiled in
    ImGui::Text("Chart area %.0fx%.0f, candles: %zu", avail.x, avail.y, candles_.size());
    ImGui::SameLine();
    if (ImGui::Button("Fit")) { fit_next_plot_ = true; }
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
    // Interval selector
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
    std::vector<Core::Candle> snapshot2;
    {
      std::lock_guard<std::mutex> lk(ui_mutex_);
      snapshot2 = candles_;
    }
    if (!snapshot2.empty()) {
      std::vector<double> xs, o, h, l, c, v;
      BuildPlotArrays(snapshot2, xs, o, h, l, c, v);
      double min_x = std::numeric_limits<double>::max();
      double max_x = std::numeric_limits<double>::lowest();
      double min_y = std::numeric_limits<double>::max();
      double max_y = std::numeric_limits<double>::lowest();
      for (size_t i = 0; i < xs.size(); ++i) { min_x = std::min(min_x, xs[i]); max_x = std::max(max_x, xs[i]); min_y = std::min(min_y, l[i]); max_y = std::max(max_y, h[i]); }
      if (ImPlot::BeginPlot("Native Chart", ImVec2(-1, -1), 0)) {
        ImPlot::SetupAxes("Time", "Price", ImPlotAxisFlags_NoMenus, ImPlotAxisFlags_NoMenus);
        double dx = (max_x - min_x) * 0.02; if (dx <= 0) dx = 1.0;
        double dy = (max_y - min_y) * 0.05; if (dy <= 0) dy = 1.0;
        ImPlot::SetupAxesLimits(min_x - dx, max_x + dx, min_y - dy, max_y + dy, ImPlotCond_Once);
        if (fit_next_plot_) {
          ImPlot::SetupAxesLimits(min_x - dx, max_x + dx, min_y - dy, max_y + dy, ImPlotCond_Always);
          fit_next_plot_ = false;
        }
        // Custom time ticks/labels on X
        SetupTimeAxisTicks(min_x, max_x, current_interval_, use_utc_time_, show_seconds_pref_);
        try {
          if (current_series_ == SeriesType::Line) {
            ImPlot::PlotLine("close", xs.data(), c.data(), (int)xs.size());
          } else {
            PlotCandlestick("ohlc", xs.data(), o.data(), c.data(), l.data(), h.data(), (int)xs.size());
          }
        } catch (...) {
          Core::Logger::instance().error("ImPlot draw exception (branch B)");
        }
        // Crosshair readout near cursor: nearest candle OHLCV
        if (ImPlot::IsPlotHovered()) {
          ImPlotPoint mp = ImPlot::GetPlotMousePos();
          int idx = -1;
          if (!xs.empty()) {
            double target = std::round(mp.x);
            auto it = std::lower_bound(xs.begin(), xs.end(), target);
            int cand = -1;
            if (it != xs.end() && *it == target) cand = int(it - xs.begin());
            else {
              int i2 = int(it - xs.begin());
              int i1 = i2 - 1;
              double best_d = 1e300; int best_i = -1;
              if (i1 >= 0) { double d = std::abs(xs[i1] - target); if (d < best_d) { best_d = d; best_i = i1; } }
              if (i2 >= 0 && i2 < (int)xs.size()) { double d = std::abs(xs[i2] - target); if (d < best_d) { best_d = d; best_i = i2; } }
              cand = best_i;
            }
            idx = cand;
          }
          if (idx >= 0) {
            bool show_sec = false;
            try { auto ms = Core::parse_interval(current_interval_); show_sec = (ms.count() < 60'000); } catch (...) {}
            if (show_seconds_pref_) show_sec = true;
            std::string when = FormatTimeLabel(xs[idx], show_sec, use_utc_time_);
            char l1[64], l2[64], l3[64], l4[64], l5[80], l6[80];
            std::snprintf(l1, sizeof(l1), "Time: %s", when.c_str());
            std::snprintf(l2, sizeof(l2), "Open:  %.6f", o[idx]);
            std::snprintf(l3, sizeof(l3), "High:  %.6f", h[idx]);
            std::snprintf(l4, sizeof(l4), "Low:   %.6f", l[idx]);
            std::snprintf(l5, sizeof(l5), "Close: %.6f", c[idx]);
            std::snprintf(l6, sizeof(l6), "Vol:   %.6f", v[idx]);
            ImVec2 plot_pos = ImPlot::GetPlotPos();
            ImVec2 plot_size = ImPlot::GetPlotSize();
            ImDrawList* dl = ImPlot::GetPlotDrawList();
            ImVec2 pad = ImVec2(6, 4);
            float line_h = ImGui::GetTextLineHeight();
            float w = 0.0f; const char* lines[] = {l1,l2,l3,l4,l5,l6};
            for (auto s : lines) w = std::max(w, ImGui::CalcTextSize(s).x);
            float h_box = line_h * 6 + pad.y*2;
            ImVec2 p0 = ImVec2(plot_pos.x + plot_size.x - w - pad.x*2 - 6, plot_pos.y + 6);
            ImVec2 p1 = ImVec2(p0.x + w + pad.x*2, p0.y + h_box);
            dl->AddRectFilled(p0, p1, IM_COL32(255,255,255,220), 4.0f);
            dl->AddRect(p0, p1, IM_COL32(0,0,0,200), 4.0f);
            float y = p0.y + pad.y; for (auto s : lines) { dl->AddText(ImVec2(p0.x + pad.x, y), IM_COL32(0,0,0,255), s); y += line_h; }
          }
        }
        ImPlot::EndPlot();
      }
    } else {
      ImGui::TextUnformatted("No candles to display");
    }
#endif
  ImGui::End();
}

void UiManager::draw_top_bar() {
  ImGuiViewport* vp = ImGui::GetMainViewport();
  const float h = 34.0f;
  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y));
  ImGui::SetNextWindowSize(ImVec2(vp->Size.x, h));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus |
                           ImGuiWindowFlags_NoDocking;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("TopBar", nullptr, flags);
  ImGui::PopStyleVar(2);
  // Title with accent
  ImGui::PushStyleColor(ImGuiCol_Text, accent_color_);
  ImGui::TextUnformatted("TradingTerminal");
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::TextDisabled("· Hyperliquid");
  ImGui::SameLine();
  ImGui::Spacing(); ImGui::SameLine();
  // Theme toggle
  ImGui::TextDisabled("Theme"); ImGui::SameLine();
  if (ImGui::SmallButton(high_contrast_theme_ ? "High Contrast" : "Dark")) {
    high_contrast_theme_ = !high_contrast_theme_;
    if (high_contrast_theme_) ApplyHighContrastPlotStyle(); else ApplyHyperliquidPlotStyle();
  }
  ImGui::SameLine();
  // Time toggles
  ImGui::TextDisabled("UTC"); ImGui::SameLine();
  ImGui::Checkbox(" ", &use_utc_time_); ImGui::SameLine();
  ImGui::TextDisabled("Seconds"); ImGui::SameLine();
  ImGui::Checkbox("  ", &show_seconds_pref_);
  ImGui::SameLine();
  // Context
  if (!current_pair_.empty() && !current_interval_.empty()) {
    ImGui::Text("%s  %s", current_pair_.c_str(), current_interval_.c_str());
  }
  ImGui::End();
}

void UiManager::draw_status_bar() {
  ImGuiViewport* vp = ImGui::GetMainViewport();
  const float h = 24.0f;
  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - h));
  ImGui::SetNextWindowSize(ImVec2(vp->Size.x, h));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus |
                           ImGuiWindowFlags_NoDocking;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("StatusBar", nullptr, flags);
  ImGui::PopStyleVar(2);
  ImGui::TextDisabled("Pair:"); ImGui::SameLine();
  ImGui::TextUnformatted(current_pair_.empty() ? "-" : current_pair_.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("Interval:"); ImGui::SameLine();
  ImGui::TextUnformatted(current_interval_.empty() ? "-" : current_interval_.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("FPS:"); ImGui::SameLine();
  ImGui::Text("%.0f", ImGui::GetIO().Framerate);
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
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  // Post-ImGui sanity draw: small red fullscreen rect to validate pipeline
  Core::Dx11Context::instance().debug_draw_fullscreen_rect(1.0f, 0.0f, 0.0f, 0.25f);
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






