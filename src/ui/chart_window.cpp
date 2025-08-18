#include "ui/chart_window.h"

#include "imgui.h"
#include "implot.h"
#include "plot/candlestick.h"
#include "signal.h"

#include "core/logger.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <limits>
#include <map>
#include <string>

using namespace Core;

namespace {

struct CandleDataCache {
  std::vector<double> times;
  std::vector<double> opens;
  std::vector<double> highs;
  std::vector<double> lows;
  std::vector<double> closes;
  std::vector<double> volumes;
  std::vector<double> indicator_times;
  std::map<int, std::vector<double>> sma;
  std::map<int, std::vector<double>> ema;
  std::vector<double> rsi;
  std::vector<double> macd;
  std::vector<double> macd_signal;
  std::vector<double> macd_hist;
  std::uint64_t last_time = 0;
  std::uint64_t indicators_last_time = 0;
};

std::map<std::string, std::map<std::string, CandleDataCache>> cache;

struct Line {
  ImPlotPoint p1;
  ImPlotPoint p2;
};
struct Rect {
  ImPlotPoint p1;
  ImPlotPoint p2;
};
std::vector<Line> lines;
std::vector<Rect> rects;
enum class ShapeType { Line, Rect };
struct DrawnShape {
  ShapeType type;
  size_t index;
};
std::vector<DrawnShape> shape_order;
bool adding_line = false;
bool adding_rect = false;
bool measure_mode = false;
bool has_measure = false;
ImPlotPoint measure_start, measure_end;
bool line_anchor_set = false;
bool rect_anchor_set = false;
bool measure_anchor_set = false;

float DistancePointToSegment(const ImVec2 &p, const ImVec2 &v,
                             const ImVec2 &w) {
  const float l2 = (w.x - v.x) * (w.x - v.x) + (w.y - v.y) * (w.y - v.y);
  if (l2 == 0.0f)
    return std::sqrt((p.x - v.x) * (p.x - v.x) + (p.y - v.y) * (p.y - v.y));
  float t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
  t = std::max(0.0f, std::min(1.0f, t));
  ImVec2 proj{v.x + t * (w.x - v.x), v.y + t * (w.y - v.y)};
  return std::sqrt((p.x - proj.x) * (p.x - proj.x) +
                   (p.y - proj.y) * (p.y - proj.y));
}

} // namespace

void InvalidateCache(const std::string &pair, const std::string &interval) {
  auto &c = cache[pair][interval];
  c.times.clear();
  c.opens.clear();
  c.highs.clear();
  c.lows.clear();
  c.closes.clear();
  c.volumes.clear();
  c.indicator_times.clear();
  c.sma.clear();
  c.ema.clear();
  c.rsi.clear();
  c.macd.clear();
  c.macd_signal.clear();
  c.macd_hist.clear();
  c.last_time = 0;
  c.indicators_last_time = 0;
}

std::vector<Candle> DownsampleCandles(const std::vector<Candle> &candles,
                                      double t_min, double t_max,
                                      int pixel_width) {
  std::vector<Candle> result;
  if (candles.empty() || pixel_width <= 0 || t_max <= t_min)
    return result;

  double bucket = (t_max - t_min) / static_cast<double>(pixel_width);
  if (bucket <= 0)
    return result;

  int current_bin = -1;
  Candle agg{};

  for (const auto &c : candles) {
    double t = static_cast<double>(c.open_time) / 1000.0;
    if (t < t_min || t > t_max)
      continue;
    int bin = static_cast<int>((t - t_min) / bucket);
    if (bin != current_bin) {
      if (current_bin != -1)
        result.push_back(agg);
      current_bin = bin;
      agg = c;
    } else {
      agg.high = std::max(agg.high, c.high);
      agg.low = std::min(agg.low, c.low);
      agg.close = c.close;
      agg.volume += c.volume;
      agg.close_time = c.close_time;
    }
  }
  if (current_bin != -1)
    result.push_back(agg);
  return result;
}

void UpdateIndicatorsIfNeeded(const std::vector<Candle> &candles,
                              CandleDataCache &cached) {
  if (candles.empty())
    return;
  if (cached.indicators_last_time == candles.back().open_time)
    return;

  cached.indicator_times.resize(candles.size());
  for (std::size_t i = 0; i < candles.size(); ++i)
    cached.indicator_times[i] =
        static_cast<double>(candles[i].open_time) / 1000.0;

  const std::vector<int> sma_periods{7, 21, 50};
  for (int p : sma_periods) {
    auto &vec = cached.sma[p];
    vec.assign(candles.size(), std::numeric_limits<double>::quiet_NaN());
    if (candles.size() >= static_cast<std::size_t>(p)) {
      for (std::size_t i = p - 1; i < candles.size(); ++i)
        vec[i] = Signal::simple_moving_average(candles, i, p);
    }
  }

  const std::vector<int> ema_periods{21};
  for (int p : ema_periods) {
    auto &vec = cached.ema[p];
    vec.assign(candles.size(), std::numeric_limits<double>::quiet_NaN());
    if (candles.size() >= static_cast<std::size_t>(p)) {
      for (std::size_t i = p - 1; i < candles.size(); ++i)
        vec[i] = Signal::exponential_moving_average(candles, i, p);
    }
  }

  const int rsi_period = 14;
  cached.rsi.assign(candles.size(), std::numeric_limits<double>::quiet_NaN());
  if (candles.size() > static_cast<std::size_t>(rsi_period)) {
    for (std::size_t i = rsi_period; i < candles.size(); ++i)
      cached.rsi[i] = Signal::relative_strength_index(candles, i, rsi_period);
  }

  const int fast = 12, slow = 26, signal = 9;
  cached.macd.assign(candles.size(), std::numeric_limits<double>::quiet_NaN());
  cached.macd_signal.assign(candles.size(),
                            std::numeric_limits<double>::quiet_NaN());
  cached.macd_hist.assign(candles.size(),
                          std::numeric_limits<double>::quiet_NaN());
  if (candles.size() >= static_cast<std::size_t>(slow + signal)) {
    for (std::size_t i = slow + signal - 1; i < candles.size(); ++i) {
      auto m = Signal::macd(candles, i, fast, slow, signal);
      cached.macd[i] = m.macd;
      cached.macd_signal[i] = m.signal;
      cached.macd_hist[i] = m.histogram;
    }
  }

  cached.indicators_last_time = candles.back().open_time;
}

namespace {

void PlotRSI(const CandleDataCache &cached) {
  const int rsi_period = 14;
  const auto &vals = cached.rsi;
  const auto &t = cached.indicator_times;
  std::size_t offset = rsi_period;
  ImPlotRect limits = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
  ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
  if (vals.size() > offset)
    ImPlot::PlotLine("RSI", t.data() + offset, vals.data() + offset,
                     static_cast<int>(vals.size() - offset));
  double x[2] = {limits.X.Min, limits.X.Max};
  double y30[2] = {30.0, 30.0};
  double y70[2] = {70.0, 70.0};
  ImPlot::PlotLine("30", x, y30, 2);
  ImPlot::PlotLine("70", x, y70, 2);
  ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
}

void PlotMACD(const CandleDataCache &cached) {
  const int fast = 12, slow = 26, signal = 9;
  const auto &times_full = cached.indicator_times;
  const auto &macd_vals = cached.macd;
  const auto &signal_vals = cached.macd_signal;
  std::size_t start = slow + signal - 1;
  if (macd_vals.size() <= start || signal_vals.size() <= start)
    return;

  double minv = macd_vals[start];
  double maxv = macd_vals[start];
  for (std::size_t i = start; i < macd_vals.size(); ++i) {
    minv = std::min(minv, macd_vals[i]);
    maxv = std::max(maxv, macd_vals[i]);
  }
  for (std::size_t i = start; i < signal_vals.size(); ++i) {
    minv = std::min(minv, signal_vals[i]);
    maxv = std::max(maxv, signal_vals[i]);
  }
  if (minv == maxv) {
    minv -= 1.0;
    maxv += 1.0;
  }

  std::vector<double> macd_norm(macd_vals.size() - start);
  std::vector<double> signal_norm(signal_vals.size() - start);
  for (std::size_t i = start; i < macd_vals.size(); ++i)
    macd_norm[i - start] = (macd_vals[i] - minv) / (maxv - minv) * 100.0;
  for (std::size_t i = start; i < signal_vals.size(); ++i)
    signal_norm[i - start] = (signal_vals[i] - minv) / (maxv - minv) * 100.0;

  ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
  ImPlot::PlotLine("MACD", times_full.data() + start, macd_norm.data(),
                   static_cast<int>(macd_norm.size()));
  ImPlot::PlotLine("Signal", times_full.data() + start, signal_norm.data(),
                   static_cast<int>(signal_norm.size()));
  ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
}

} // namespace

void DrawChartWindow(
    const std::map<std::string, std::map<std::string, std::vector<Candle>>>
        &all_candles,
    std::string &active_pair, std::string &active_interval,
    const std::vector<std::string> &pair_list,
    const std::vector<std::string> &interval_list, bool show_on_chart,
    const std::vector<AppContext::TradeEvent> &trades,
    const Journal::Journal &journal, const Core::BacktestResult &last_result) {
  if (ImGui::Begin("Chart")) {
    auto start = std::chrono::steady_clock::now();

    static char pair_filter[64] = "";
    ImGui::InputText("##pair_filter", pair_filter, IM_ARRAYSIZE(pair_filter));
    std::string filter_str = pair_filter;
    std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(),
                   ::tolower);
    std::vector<std::string> filtered_pairs;
    for (const auto &p : pair_list) {
      std::string lower = p;
      std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
      if (filter_str.empty() || lower.find(filter_str) != std::string::npos)
        filtered_pairs.push_back(p);
    }

    if (ImGui::BeginCombo("Pair", active_pair.c_str())) {
      for (const auto &p : filtered_pairs) {
        bool sel = (p == active_pair);
        if (ImGui::Selectable(p.c_str(), sel))
          active_pair = p;
        if (sel)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::BeginCombo("Interval", active_interval.c_str())) {
      for (const auto &iv : interval_list) {
        bool sel = (iv == active_interval);
        if (ImGui::Selectable(iv.c_str(), sel))
          active_interval = iv;
        if (sel)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    const auto &candles = all_candles.at(active_pair).at(active_interval);
    auto &cached = cache[active_pair][active_interval];
    if (!candles.empty()) {
      if (cached.last_time != candles.back().open_time) {
        InvalidateCache(active_pair, active_interval);
        cached.last_time = candles.back().open_time;
      }
      UpdateIndicatorsIfNeeded(candles, cached);
    } else {
      InvalidateCache(active_pair, active_interval);
    }

    const auto &times = cached.times;
    const auto &opens = cached.opens;
    const auto &highs = cached.highs;
    const auto &lows = cached.lows;
    const auto &closes = cached.closes;
    const auto &volumes = cached.volumes;

    static bool show_sma7 = true;
    static bool show_sma21 = false;
    static bool show_sma50 = false;
    static bool show_ema21 = false;
    static bool show_external_indicator = false;
    static bool show_rsi = false;
    static bool show_macd = false;

    static float volume_height = 1.f;
    static float volume_bar_width = 0.5f;

    static ImPlotRect manual_limits;
    static bool apply_manual_limits = false;
    static float y_scroll_indicator_timer = 0.0f;
    static ImPlotRange volume_limits{0.0, 0.0};

    if (ImGui::Button("Reset")) {
      if (!candles.empty()) {
        manual_limits.X.Min =
            static_cast<double>(candles.front().open_time) / 1000.0;
        manual_limits.X.Max =
            static_cast<double>(candles.back().open_time) / 1000.0;
      }
      if (!lows.empty() && !highs.empty()) {
        manual_limits.Y.Min = *std::min_element(lows.begin(), lows.end());
        manual_limits.Y.Max = *std::max_element(highs.begin(), highs.end());
      }
      if (!candles.empty()) {
        double max_vol = 0.0;
        for (const auto &c : candles)
          max_vol = std::max(max_vol, c.volume);
        volume_limits.Min = 0.0;
        volume_limits.Max = max_vol * 1.1;
      }
      apply_manual_limits = true;
      InvalidateCache(active_pair, active_interval);
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
      ImPlot::SetNextAxesToFit();
      if (!candles.empty()) {
        double max_vol = 0.0;
        for (const auto &c : candles)
          max_vol = std::max(max_vol, c.volume);
        volume_limits.Min = 0.0;
        volume_limits.Max = max_vol * 1.1;
      }
      apply_manual_limits = false;
      InvalidateCache(active_pair, active_interval);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Line")) {
      adding_line = true;
      line_anchor_set = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Rect")) {
      adding_rect = true;
      rect_anchor_set = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Measure")) {
      measure_mode = true;
      has_measure = false;
      measure_anchor_set = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo")) {
      if (!shape_order.empty()) {
        auto last = shape_order.back();
        shape_order.pop_back();
        if (last.type == ShapeType::Line && last.index < lines.size()) {
          lines.erase(lines.begin() + last.index);
          for (auto &s : shape_order) {
            if (s.type == ShapeType::Line && s.index > last.index)
              --s.index;
          }
        } else if (last.type == ShapeType::Rect && last.index < rects.size()) {
          rects.erase(rects.begin() + last.index);
          for (auto &s : shape_order) {
            if (s.type == ShapeType::Rect && s.index > last.index)
              --s.index;
          }
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) {
      lines.clear();
      rects.clear();
      shape_order.clear();
      line_anchor_set = rect_anchor_set = measure_anchor_set = false;
      measure_mode = false;
      has_measure = false;
    }

    ImGui::Separator();
    ImGui::Checkbox("SMA7", &show_sma7);
    ImGui::SameLine();
    ImGui::Checkbox("SMA21", &show_sma21);
    ImGui::SameLine();
    ImGui::Checkbox("SMA50", &show_sma50);
    ImGui::SameLine();
    ImGui::Checkbox("EMA21", &show_ema21);
    ImGui::SameLine();
    ImGui::Checkbox("Ext EMA", &show_external_indicator);
    ImGui::SameLine();
    ImGui::Checkbox("RSI", &show_rsi);
    ImGui::SameLine();
    ImGui::Checkbox("MACD", &show_macd);

    if (adding_line)
      ImGui::Text("Line: click first point, then click again to finish");
    else if (adding_rect)
      ImGui::Text("Rect: click first corner, then click again to finish");
    else if (measure_mode)
      ImGui::Text("Measure: click start point and click again to end");
    else {
      ImGui::Text("Mouse wheel zooms. Hold Ctrl/Shift or hover Y axis for "
                  "vertical zoom.");
      ImGui::Text("Drag Y axis to pan vertically.");
    }

    ImPlotFlags plot_flags = ImPlotFlags_Crosshairs;
    ImPlotSubplotFlags subplot_flags = ImPlotSubplotFlags_LinkAllX;
    ImGui::SliderFloat("Volume height", &volume_height, 0.5f, 5.f);
    ImGui::SliderFloat("Volume bar width", &volume_bar_width, 0.1f, 2.f);
    std::vector<float> row_sizes = {3.f, volume_height};
    int subplot_rows = static_cast<int>(row_sizes.size());
    if (ImPlot::BeginSubplots("##price_volume", subplot_rows, 1,
                              ImGui::GetContentRegionAvail(), subplot_flags,
                              row_sizes.data())) {
      if (apply_manual_limits) {
        ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max,
                                  manual_limits.Y.Min, manual_limits.Y.Max,
                                  ImGuiCond_Always);
        apply_manual_limits = false;
      }

      if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(),
                            ImVec2(-1, -1), plot_flags)) {
        ImPlot::SetupAxes("Time", "Price");
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxisFormat(ImAxis_X1, "%H:%M:%S");
        ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Outside);
        if (show_rsi || show_macd) {
          ImPlot::SetupAxis(ImAxis_Y2, "Indicator", ImPlotAxisFlags_AuxDefault);
          ImPlot::SetupAxisLimits(ImAxis_Y2, 0.0, 100.0, ImPlotCond_Always);
        }
        ImPlotRect cur_limits;
        cur_limits = ImPlot::GetPlotLimits();
        int pixel_width = static_cast<int>(ImPlot::GetPlotSize().x);
        if (cached.times.empty()) {
          auto ds = DownsampleCandles(candles, cur_limits.X.Min,
                                      cur_limits.X.Max, pixel_width);
          cached.times.resize(ds.size());
          cached.opens.resize(ds.size());
          cached.highs.resize(ds.size());
          cached.lows.resize(ds.size());
          cached.closes.resize(ds.size());
          cached.volumes.resize(ds.size());
          for (std::size_t i = 0; i < ds.size(); ++i) {
            const auto &c = ds[i];
            cached.times[i] = static_cast<double>(c.open_time) / 1000.0;
            cached.opens[i] = c.open;
            cached.highs[i] = c.high;
            cached.lows[i] = c.low;
            cached.closes[i] = c.close;
            cached.volumes[i] = c.volume;
          }
        }

        Plot::PlotCandlestick("Candles", times.data(), opens.data(),
                              closes.data(), lows.data(), highs.data(),
                              (int)times.size(), true, 0.25f,
                              ImVec4(0.149f, 0.651f, 0.604f, 1.0f),
                              ImVec4(0.937f, 0.325f, 0.314f, 1.0f));

        auto plot_sma = [&](int period, const char *label,
                            const ImVec4 &color) {
          auto it = cached.sma.find(period);
          if (it == cached.sma.end())
            return;
          const auto &vals = it->second;
          const auto &t = cached.indicator_times;
          std::size_t offset = period - 1;
          if (vals.size() > offset) {
            ImPlot::SetNextLineStyle(color);
            ImPlot::PlotLine(label, t.data() + offset, vals.data() + offset,
                             static_cast<int>(vals.size() - offset));
          }
        };
        auto plot_ema = [&](int period, const char *label,
                            const ImVec4 &color) {
          auto it = cached.ema.find(period);
          if (it == cached.ema.end())
            return;
          const auto &vals = it->second;
          const auto &t = cached.indicator_times;
          std::size_t offset = period - 1;
          if (vals.size() > offset) {
            ImPlot::SetNextLineStyle(color);
            ImPlot::PlotLine(label, t.data() + offset, vals.data() + offset,
                             static_cast<int>(vals.size() - offset));
          }
        };
        if (show_sma7)
          plot_sma(7, "SMA7", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        if (show_sma21)
          plot_sma(21, "SMA21", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        if (show_sma50)
          plot_sma(50, "SMA50", ImVec4(0.5f, 0.0f, 1.0f, 1.0f));
        if (show_ema21)
          plot_ema(21, "EMA21", ImVec4(1.0f, 0.0f, 0.5f, 1.0f));
        if (show_external_indicator) {
          const int period = 21;
          auto it = cached.ema.find(period);
          if (it != cached.ema.end()) {
            const auto &vals = it->second;
            const auto &t = cached.indicator_times;
            std::size_t offset = period - 1;
            if (vals.size() > offset) {
              ImPlot::SetNextLineStyle(ImVec4(0.0f, 0.5f, 1.0f, 1.0f));
              ImPlot::PlotLine("EMA21 (TV)", t.data() + offset,
                               vals.data() + offset,
                               static_cast<int>(vals.size() - offset));
            }
          }
        }

        if (show_rsi)
          PlotRSI(cached);
        if (show_macd)
          PlotMACD(cached);

        cur_limits = ImPlot::GetPlotLimits();
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 plot_pos = ImPlot::GetPlotPos();
        ImVec2 plot_size = ImPlot::GetPlotSize();
        bool over_y_axis = mouse_pos.x < plot_pos.x &&
                           mouse_pos.y >= plot_pos.y &&
                           mouse_pos.y <= plot_pos.y + plot_size.y;

        if (ImPlot::IsPlotHovered() || over_y_axis) {
          ImGuiIO &io = ImGui::GetIO();
          ImPlotPoint mouse = ImPlot::GetPlotMousePos();
          if (io.MouseWheel != 0.0f && !adding_line && !adding_rect &&
              !measure_mode) {
            double zoom = io.MouseWheel > 0 ? 0.9 : 1.1;
            bool vertical_only = io.KeyCtrl || io.KeyShift || over_y_axis;
            if (vertical_only) {
              manual_limits.Y.Min =
                  mouse.y - (mouse.y - cur_limits.Y.Min) * zoom;
              manual_limits.Y.Max =
                  mouse.y + (cur_limits.Y.Max - mouse.y) * zoom;
              y_scroll_indicator_timer = 1.0f;
            } else {
              manual_limits.X.Min =
                  mouse.x - (mouse.x - cur_limits.X.Min) * zoom;
              manual_limits.X.Max =
                  mouse.x + (cur_limits.X.Max - mouse.x) * zoom;
              manual_limits.Y.Min =
                  mouse.y - (mouse.y - cur_limits.Y.Min) * zoom;
              manual_limits.Y.Max =
                  mouse.y + (cur_limits.Y.Max - mouse.y) * zoom;
            }
            apply_manual_limits = true;
            InvalidateCache(active_pair, active_interval);
          }
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !adding_line &&
              !adding_rect && !measure_mode) {
            ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            if (over_y_axis) {
              ImPlotPoint p0 = ImPlot::PixelsToPlot(ImVec2(0, 0));
              ImPlotPoint p1 = ImPlot::PixelsToPlot(ImVec2(0, drag.y));
              double dy = p0.y - p1.y;
              manual_limits.Y.Min += dy;
              manual_limits.Y.Max += dy;
              y_scroll_indicator_timer = 1.0f;
            } else {
              ImPlotPoint p0 = ImPlot::PixelsToPlot(ImVec2(0, 0));
              ImPlotPoint p1 = ImPlot::PixelsToPlot(drag);
              double dx = p0.x - p1.x;
              double dy = p0.y - p1.y;
              manual_limits.X.Min += dx;
              manual_limits.X.Max += dx;
              manual_limits.Y.Min += dy;
              manual_limits.Y.Max += dy;
            }
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            apply_manual_limits = true;
            InvalidateCache(active_pair, active_interval);
          }

          if (adding_line) {
            if (!line_anchor_set) {
              if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                lines.push_back({mouse, mouse});
                line_anchor_set = true;
              }
            } else {
              lines.back().p2 = mouse;
              if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                lines.back().p2 = mouse;
                shape_order.push_back({ShapeType::Line, lines.size() - 1});
                adding_line = false;
                line_anchor_set = false;
              }
            }
          }

          if (adding_rect) {
            if (!rect_anchor_set) {
              if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                rects.push_back({mouse, mouse});
                rect_anchor_set = true;
              }
            } else {
              rects.back().p2 = mouse;
              if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                rects.back().p2 = mouse;
                shape_order.push_back({ShapeType::Rect, rects.size() - 1});
                adding_rect = false;
                rect_anchor_set = false;
              }
            }
          }

          if (measure_mode) {
            if (!measure_anchor_set) {
              if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                measure_start = mouse;
                measure_end = mouse;
                has_measure = true;
                measure_anchor_set = true;
              }
            } else {
              measure_end = mouse;
              if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                measure_end = mouse;
                measure_mode = false;
                measure_anchor_set = false;
              }
            }
          }

          if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImVec2 cpix = ImPlot::PlotToPixels(mouse);
            bool removed = false;
            for (size_t i = 0; i < lines.size() && !removed; ++i) {
              ImVec2 p1 = ImPlot::PlotToPixels(lines[i].p1);
              ImVec2 p2 = ImPlot::PlotToPixels(lines[i].p2);
              if (DistancePointToSegment(cpix, p1, p2) < 5.0f) {
                lines.erase(lines.begin() + i);
                for (auto it = shape_order.begin(); it != shape_order.end();
                     ++it) {
                  if (it->type == ShapeType::Line && it->index == i) {
                    shape_order.erase(it);
                    break;
                  }
                }
                for (auto &s : shape_order) {
                  if (s.type == ShapeType::Line && s.index > i)
                    --s.index;
                }
                removed = true;
              }
            }
            if (!removed) {
              for (size_t i = 0; i < rects.size(); ++i) {
                double xmin = std::min(rects[i].p1.x, rects[i].p2.x);
                double xmax = std::max(rects[i].p1.x, rects[i].p2.x);
                double ymin = std::min(rects[i].p1.y, rects[i].p2.y);
                double ymax = std::max(rects[i].p1.y, rects[i].p2.y);
                if (mouse.x >= xmin && mouse.x <= xmax && mouse.y >= ymin &&
                    mouse.y <= ymax) {
                  rects.erase(rects.begin() + i);
                  for (auto it = shape_order.begin(); it != shape_order.end();
                       ++it) {
                    if (it->type == ShapeType::Rect && it->index == i) {
                      shape_order.erase(it);
                      break;
                    }
                  }
                  for (auto &s : shape_order) {
                    if (s.type == ShapeType::Rect && s.index > i)
                      --s.index;
                  }
                  break;
                }
              }
            }
          }
        }

        if (y_scroll_indicator_timer > 0.0f) {
          ImVec2 arrow_pos =
              ImVec2(plot_pos.x - 20, plot_pos.y + plot_size.y * 0.5f -
                                          ImGui::GetFontSize() * 0.5f);
          const char *arrow = reinterpret_cast<const char *>(u8"↕");
          ImGui::GetWindowDrawList()->AddText(
              arrow_pos, IM_COL32(255, 255, 0, 255), arrow);
          y_scroll_indicator_timer -= ImGui::GetIO().DeltaTime;
        }
        if (!apply_manual_limits) {
          manual_limits = cur_limits;
        }

        static double cursor_x = 0.0;
        static double cursor_y = 0.0;
        if (ImPlot::IsPlotHovered()) {
          ImPlotPoint mouse = ImPlot::GetPlotMousePos();
          cursor_x = mouse.x;
          cursor_y = mouse.y;
          std::time_t tt = static_cast<std::time_t>(cursor_x);
          char time_buf[32];
          if (std::tm *tm = std::localtime(&tt)) {
            std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm);
          } else {
            std::snprintf(time_buf, sizeof(time_buf), "%lld", (long long)tt);
          }
          ImGui::BeginTooltip();
          ImGui::Text("Time: %s\nPrice: %.2f", time_buf, cursor_y);
          ImGui::EndTooltip();
        }

        double vx[2] = {cursor_x, cursor_x};
        double vy[2] = {cur_limits.Y.Min, cur_limits.Y.Max};
        ImPlot::SetNextLineStyle(ImVec4(1, 1, 1, 0.5f));
        ImPlot::PlotLine("##vline", vx, vy, 2);

        double hx[2] = {cur_limits.X.Min, cur_limits.X.Max};
        double hy[2] = {cursor_y, cursor_y};
        ImPlot::SetNextLineStyle(ImVec4(1, 1, 1, 0.5f));
        ImPlot::PlotLine("##hline", hx, hy, 2);

        double price = closes.empty() ? 0.0 : closes.back();
        double px[2] = {cur_limits.X.Min, cur_limits.X.Max};
        double py[2] = {price, price};
        ImPlot::SetNextLineStyle(ImVec4(0, 1, 0, 1));
        ImPlot::PlotLine("##price", px, py, 2);
        if (show_on_chart && !trades.empty()) {
          std::vector<double> buy_times, buy_prices, sell_times, sell_prices;
          buy_times.reserve(trades.size());
          buy_prices.reserve(trades.size());
          sell_times.reserve(trades.size());
          sell_prices.reserve(trades.size());
          for (const auto &tr : trades) {
            if (tr.side == AppContext::TradeEvent::Side::Buy) {
              buy_times.push_back(tr.time);
              buy_prices.push_back(tr.price);
            } else {
              sell_times.push_back(tr.time);
              sell_prices.push_back(tr.price);
            }
          }
          if (!buy_times.empty()) {
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Up, 6, ImVec4(0, 1, 0, 1));
            ImPlot::PlotScatter("Buy", buy_times.data(), buy_prices.data(),
                                (int)buy_times.size());
          }
          if (!sell_times.empty()) {
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Down, 6,
                                       ImVec4(1, 0, 0, 1));
            ImPlot::PlotScatter("Sell", sell_times.data(), sell_prices.data(),
                                (int)sell_times.size());
          }
        }

        // Overlay journal entries
        std::vector<double> jb_times, jb_prices, js_times, js_prices;
        for (const auto &e : journal.entries()) {
          if (e.symbol == active_pair) {
            double t = (double)e.timestamp / 1000.0;
            if (e.side == Journal::Side::Buy) {
              jb_times.push_back(t);
              jb_prices.push_back(e.price);
            } else {
              js_times.push_back(t);
              js_prices.push_back(e.price);
            }
          }
        }
        if (!jb_times.empty()) {
          ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0, 1, 0, 1));
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4);
          ImPlot::PlotScatter("Journal Buy", jb_times.data(), jb_prices.data(),
                              jb_times.size());
          ImPlot::PopStyleColor();
        }
        if (!js_times.empty()) {
          ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1, 0, 0, 1));
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross, 4);
          ImPlot::PlotScatter("Journal Sell", js_times.data(), js_prices.data(),
                              js_times.size());
          ImPlot::PopStyleColor();
        }

        // Overlay backtest trades
        if (!last_result.trades.empty()) {
          std::vector<double> bt_entry_t, bt_entry_p, bt_exit_t, bt_exit_p;
          for (const auto &t : last_result.trades) {
            bt_entry_t.push_back((double)candles[t.entry_index].open_time /
                                 1000.0);
            bt_entry_p.push_back(candles[t.entry_index].close);
            bt_exit_t.push_back((double)candles[t.exit_index].open_time /
                                1000.0);
            bt_exit_p.push_back(candles[t.exit_index].close);
          }
          if (!bt_entry_t.empty()) {
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0, 0, 1, 1));
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 4);
            ImPlot::PlotScatter("BT Entry", bt_entry_t.data(),
                                bt_entry_p.data(), bt_entry_t.size());
            ImPlot::PopStyleColor();
          }
          if (!bt_exit_t.empty()) {
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1, 1, 0, 1));
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 4);
            ImPlot::PlotScatter("BT Exit", bt_exit_t.data(), bt_exit_p.data(),
                                bt_exit_t.size());
            ImPlot::PopStyleColor();
          }
        }

        auto *draw_list = ImPlot::GetPlotDrawList();
        for (const auto &l : lines) {
          ImVec2 p1 = ImPlot::PlotToPixels(l.p1);
          ImVec2 p2 = ImPlot::PlotToPixels(l.p2);
          draw_list->AddLine(p1, p2, IM_COL32(255, 255, 0, 255));
        }
        for (const auto &r : rects) {
          ImVec2 p1 = ImPlot::PlotToPixels(r.p1);
          ImVec2 p2 = ImPlot::PlotToPixels(r.p2);
          draw_list->AddRect(p1, p2, IM_COL32(0, 255, 255, 255));
        }
        if (has_measure) {
          ImVec2 p1 = ImPlot::PlotToPixels(measure_start);
          ImVec2 p2 = ImPlot::PlotToPixels(measure_end);
          draw_list->AddRect(p1, p2, IM_COL32(255, 0, 255, 255));
          char buf[64];
          std::snprintf(buf, sizeof(buf), "dT: %.2f dP: %.2f",
                        measure_end.x - measure_start.x,
                        measure_end.y - measure_start.y);
          draw_list->AddText(p2, IM_COL32(255, 255, 255, 255), buf);
        }

        ImPlot::EndPlot();
      }
      if (!times.empty() && !volumes.empty()) {
        double max_vol = *std::max_element(volumes.begin(), volumes.end());
        if (volume_limits.Max == 0.0 || volume_limits.Max < max_vol * 1.1) {
          volume_limits.Min = 0.0;
          volume_limits.Max = max_vol * 1.1;
        }
        ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max,
                                  volume_limits.Min, volume_limits.Max,
                                  ImGuiCond_Always);
        if (ImPlot::BeginPlot("Volume", ImVec2(-1, -1), ImPlotFlags_NoLegend)) {
          ImPlot::SetupAxes("Time", "Volume", ImPlotAxisFlags_Lock,
                            ImPlotAxisFlags_None);
          ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
          ImPlot::SetupAxisFormat(ImAxis_X1, "%H:%M:%S");
          double bar_width = times.size() > 1
                                 ? (times[1] - times[0]) * volume_bar_width
                                 : volume_bar_width;
          ImPlot::PlotBars("Volume", times.data(), volumes.data(),
                           static_cast<int>(volumes.size()), bar_width);
          ImPlotRect cur_limits = ImPlot::GetPlotLimits();
          volume_limits.Min = cur_limits.Y.Min;
          volume_limits.Max = cur_limits.Y.Max;
          ImPlot::EndPlot();
        }
      } else {
        ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max,
                                  volume_limits.Min, volume_limits.Max,
                                  ImGuiCond_Always);
        if (ImPlot::BeginPlot("Volume", ImVec2(-1, -1), ImPlotFlags_NoLegend)) {
          ImPlot::SetupAxes("Time", "Volume", ImPlotAxisFlags_Lock,
                            ImPlotAxisFlags_None);
          ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
          ImPlot::SetupAxisFormat(ImAxis_X1, "%H:%M:%S");
          ImPlotRect cur_limits = ImPlot::GetPlotLimits();
          volume_limits.Min = cur_limits.Y.Min;
          volume_limits.Max = cur_limits.Y.Max;
          ImPlot::EndPlot();
        }
      }
      ImPlot::EndSubplots();
    }
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                  .count();
    Core::Logger::instance().info("DrawChartWindow took " + std::to_string(ms) +
                                  " ms");
  }
  ImGui::End();
}
