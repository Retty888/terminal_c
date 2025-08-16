#include "ui/chart_window.h"

#include "imgui.h"
#include "implot.h"
#include "plot/candlestick.h"
#include "signal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
  std::uint64_t last_time = 0;
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

void DrawExternalIndicator(const std::vector<Candle> &candles,
                           const std::vector<double> &times,
                           const ImPlotRect &limits) {
  const int period = 21;
  std::vector<double> ema_times, ema_vals;
  if (candles.size() >= (size_t)period) {
    for (size_t i = period - 1; i < candles.size(); ++i) {
      ema_times.push_back(times[i]);
      ema_vals.push_back(
          Signal::exponential_moving_average(candles, i, period));
    }
  }
  double ymin = 0.0, ymax = 0.0;
  if (!ema_vals.empty()) {
    auto [min_it, max_it] =
        std::minmax_element(ema_vals.begin(), ema_vals.end());
    ymin = *min_it;
    ymax = *max_it;
  }
  ImGui::Begin("External Indicator");
  ImPlot::SetNextAxesLimits(limits.X.Min, limits.X.Max, ymin, ymax,
                            ImGuiCond_Always);
  if (ImPlot::BeginPlot("EMA21 (TV)", ImVec2(-1, -1),
                        ImPlotFlags_NoLegend | ImPlotFlags_NoInputs)) {
    ImPlot::SetupAxes("Time", "Value");
    if (!ema_vals.empty())
      ImPlot::PlotLine("EMA21 (TV)", ema_times.data(), ema_vals.data(),
                       static_cast<int>(ema_vals.size()));
    ImPlot::EndPlot();
  }
  ImGui::End();
}

} // namespace

void DrawChartWindow(
    const std::map<std::string, std::map<std::string, std::vector<Candle>>>
        &all_candles,
    std::string &active_pair, std::string &active_interval,
    const std::vector<std::string> &pair_list,
    const std::vector<std::string> &interval_list, bool show_on_chart,
    const std::vector<double> &buy_times, const std::vector<double> &buy_prices,
    const std::vector<double> &sell_times,
    const std::vector<double> &sell_prices, const Journal::Journal &journal,
    const Core::BacktestResult &last_result) {
  ImGui::Begin("Chart");

  if (ImGui::BeginCombo("Pair", active_pair.c_str())) {
    for (const auto &p : pair_list) {
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
  if (!candles.empty() && (cached.times.size() != candles.size() ||
                           cached.last_time != candles.back().open_time)) {
    cached.times.resize(candles.size());
    cached.opens.resize(candles.size());
    cached.highs.resize(candles.size());
    cached.lows.resize(candles.size());
    cached.closes.resize(candles.size());
    cached.volumes.resize(candles.size());
    for (size_t i = 0; i < candles.size(); ++i) {
      const auto &c = candles[i];
      cached.times[i] = static_cast<double>(c.open_time) / 1000.0;
      cached.opens[i] = c.open;
      cached.highs[i] = c.high;
      cached.lows[i] = c.low;
      cached.closes[i] = c.close;
      cached.volumes[i] = c.volume;
    }
    cached.last_time = candles.back().open_time;
  } else if (candles.empty()) {
    cached.times.clear();
    cached.opens.clear();
    cached.highs.clear();
    cached.lows.clear();
    cached.closes.clear();
    cached.volumes.clear();
    cached.last_time = 0;
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

  static ImPlotRect manual_limits;
  static bool apply_manual_limits = false;

  if (ImGui::Button("Reset")) {
    if (!times.empty()) {
      manual_limits.X.Min = times.front();
      manual_limits.X.Max = times.back();
    }
    if (!lows.empty() && !highs.empty()) {
      manual_limits.Y.Min = *std::min_element(lows.begin(), lows.end());
      manual_limits.Y.Max = *std::max_element(highs.begin(), highs.end());
    }
    apply_manual_limits = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Fit")) {
    ImPlot::SetNextAxesToFit();
    apply_manual_limits = false;
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

  ImPlotFlags plot_flags = ImPlotFlags_Crosshairs;
  ImPlotSubplotFlags subplot_flags = ImPlotSubplotFlags_LinkAllX;
  int rows = 2 + (show_rsi ? 1 : 0) + (show_macd ? 1 : 0);
  if (ImPlot::BeginSubplots("##price_volume", rows, 1,
                            ImGui::GetContentRegionAvail(), subplot_flags)) {
    if (apply_manual_limits) {
      ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max,
                                manual_limits.Y.Min, manual_limits.Y.Max,
                                ImGuiCond_Always);
      apply_manual_limits = false;
    }

    if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), ImVec2(-1, -1),
                          plot_flags)) {
      ImPlot::SetupAxes("Time", "Price");
      ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Outside);
      Plot::PlotCandlestick("Candles", times.data(), opens.data(),
                            closes.data(), lows.data(), highs.data(),
                            (int)candles.size(), true, 0.25f,
                            ImVec4(0.149f, 0.651f, 0.604f, 1.0f),
                            ImVec4(0.937f, 0.325f, 0.314f, 1.0f));

    auto plot_sma = [&](int period, const char *label, const ImVec4 &color) {
      if (candles.size() >= (size_t)period) {
        std::vector<double> ma_times, ma_vals;
        for (size_t i = period - 1; i < candles.size(); ++i) {
          ma_times.push_back(times[i]);
          ma_vals.push_back(
              Signal::simple_moving_average(candles, i, period));
        }
        ImPlot::SetNextLineStyle(color);
        ImPlot::PlotLine(label, ma_times.data(), ma_vals.data(), ma_vals.size());
      }
    };
    auto plot_ema = [&](int period, const char *label, const ImVec4 &color) {
      if (candles.size() >= (size_t)period) {
        std::vector<double> ma_times, ma_vals;
        for (size_t i = period - 1; i < candles.size(); ++i) {
          ma_times.push_back(times[i]);
          ma_vals.push_back(
              Signal::exponential_moving_average(candles, i, period));
        }
        ImPlot::SetNextLineStyle(color);
        ImPlot::PlotLine(label, ma_times.data(), ma_vals.data(), ma_vals.size());
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

    ImPlotRect cur_limits = ImPlot::GetPlotLimits();

    if (ImPlot::IsPlotHovered()) {
      ImGuiIO &io = ImGui::GetIO();
      ImPlotPoint mouse = ImPlot::GetPlotMousePos();
      if (io.MouseWheel != 0.0f && !adding_line && !adding_rect &&
          !measure_mode) {
        double zoom = io.MouseWheel > 0 ? 0.9 : 1.1;
        manual_limits.X.Min = mouse.x - (mouse.x - cur_limits.X.Min) * zoom;
        manual_limits.X.Max = mouse.x + (cur_limits.X.Max - mouse.x) * zoom;
        manual_limits.Y.Min = mouse.y - (mouse.y - cur_limits.Y.Min) * zoom;
        manual_limits.Y.Max = mouse.y + (cur_limits.Y.Max - mouse.y) * zoom;
        apply_manual_limits = true;
      }
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !adding_line &&
          !adding_rect && !measure_mode) {
        ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImPlotPoint p0 = ImPlot::PixelsToPlot(ImVec2(0, 0));
        ImPlotPoint p1 = ImPlot::PixelsToPlot(drag);
        double dx = p0.x - p1.x;
        double dy = p0.y - p1.y;
        manual_limits.X.Min += dx;
        manual_limits.X.Max += dx;
        manual_limits.Y.Min += dy;
        manual_limits.Y.Max += dy;
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        apply_manual_limits = true;
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
            for (auto it = shape_order.begin(); it != shape_order.end(); ++it) {
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

    if (!apply_manual_limits) {
      manual_limits = cur_limits;
    }

    static double cursor_x = 0.0;
    static double cursor_y = 0.0;
    if (ImPlot::IsPlotHovered()) {
      ImPlotPoint mouse = ImPlot::GetPlotMousePos();
      cursor_x = mouse.x;
      cursor_y = mouse.y;
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
    if (show_on_chart) {
      if (!buy_times.empty()) {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Up, 6, ImVec4(0, 1, 0, 1));
        ImPlot::PlotScatter("Buy", buy_times.data(), buy_prices.data(),
                            (int)buy_times.size());
      }
      if (!sell_times.empty()) {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Down, 6, ImVec4(1, 0, 0, 1));
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
        bt_entry_t.push_back((double)candles[t.entry_index].open_time / 1000.0);
        bt_entry_p.push_back(candles[t.entry_index].close);
        bt_exit_t.push_back((double)candles[t.exit_index].open_time / 1000.0);
        bt_exit_p.push_back(candles[t.exit_index].close);
      }
      if (!bt_entry_t.empty()) {
        ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0, 0, 1, 1));
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 4);
        ImPlot::PlotScatter("BT Entry", bt_entry_t.data(), bt_entry_p.data(),
                            bt_entry_t.size());
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
    ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max, 0.0,
                              max_vol * 1.1, ImGuiCond_Always);
    if (ImPlot::BeginPlot("Volume", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoInputs)) {
      ImPlot::SetupAxes("Time", "Volume");
      double bar_width = times.size() > 1 ? (times[1] - times[0]) * 0.5 : 0.5;
      ImPlot::PlotBars("Volume", times.data(), volumes.data(),
                       static_cast<int>(volumes.size()), bar_width);
      ImPlot::EndPlot();
    }
  } else {
    if (ImPlot::BeginPlot("Volume", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoInputs)) {
      ImPlot::SetupAxes("Time", "Volume");
      ImPlot::EndPlot();
    }
  }

  if (show_rsi) {
    const int rsi_period = 14;
    std::vector<double> rsi_times, rsi_vals;
    if (candles.size() >= (size_t)rsi_period) {
      for (size_t i = rsi_period; i < candles.size(); ++i) {
        rsi_times.push_back(times[i]);
        rsi_vals.push_back(
            Signal::relative_strength_index(candles, i, rsi_period));
      }
    }
    ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max, 0.0,
                              100.0, ImGuiCond_Always);
    if (ImPlot::BeginPlot("RSI", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoInputs)) {
      ImPlot::SetupAxes("Time", "RSI");
      if (!rsi_vals.empty())
        ImPlot::PlotLine("RSI", rsi_times.data(), rsi_vals.data(),
                         rsi_vals.size());
      ImPlot::EndPlot();
    }
  }

  if (show_macd) {
    const int fast_period = 12;
    const int slow_period = 26;
    const int signal_period = 9;
    std::vector<double> macd_t, macd_vals, signal_vals, hist_vals;
    size_t start = slow_period + signal_period - 2;
    if (candles.size() >= start + 1) {
      for (size_t i = start; i < candles.size(); ++i) {
        double macd_line =
            Signal::macd(candles, i, fast_period, slow_period);
        double signal_line = Signal::macd_signal(
            candles, i, fast_period, slow_period, signal_period);
        macd_t.push_back(times[i]);
        macd_vals.push_back(macd_line);
        signal_vals.push_back(signal_line);
        hist_vals.push_back(macd_line - signal_line);
      }
    }
    double ymin = -1.0, ymax = 1.0;
    if (!macd_vals.empty()) {
      auto [min_it, max_it] =
          std::minmax_element(macd_vals.begin(), macd_vals.end());
      ymin = *min_it;
      ymax = *max_it;
      if (!signal_vals.empty()) {
        auto [smin, smax] =
            std::minmax_element(signal_vals.begin(), signal_vals.end());
        ymin = std::min(ymin, *smin);
        ymax = std::max(ymax, *smax);
      }
      if (!hist_vals.empty()) {
        auto [hmin, hmax] =
            std::minmax_element(hist_vals.begin(), hist_vals.end());
        ymin = std::min(ymin, *hmin);
        ymax = std::max(ymax, *hmax);
      }
    }
    ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max, ymin,
                              ymax, ImGuiCond_Always);
    if (ImPlot::BeginPlot("MACD", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoInputs)) {
      ImPlot::SetupAxes("Time", "MACD");
      if (!macd_vals.empty())
        ImPlot::PlotLine("MACD", macd_t.data(), macd_vals.data(),
                         macd_vals.size());
      if (!signal_vals.empty())
        ImPlot::PlotLine("Signal", macd_t.data(), signal_vals.data(),
                         signal_vals.size());
      if (!hist_vals.empty()) {
        double bar_width =
            macd_t.size() > 1 ? (macd_t[1] - macd_t[0]) * 0.5 : 0.5;
        ImPlot::PlotBars("Histogram", macd_t.data(), hist_vals.data(),
                         hist_vals.size(), bar_width);
      }
      ImPlot::EndPlot();
    }
  }

  ImPlot::EndSubplots();
  }
  ImGui::End();
  if (show_external_indicator)
    DrawExternalIndicator(candles, times, manual_limits);
}
