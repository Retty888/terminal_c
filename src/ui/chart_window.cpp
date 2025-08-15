#include "ui/chart_window.h"

#include "imgui.h"
#include "implot.h"
#include "plot/candlestick.h"

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
    for (size_t i = 0; i < candles.size(); ++i) {
      const auto &c = candles[i];
      cached.times[i] = static_cast<double>(c.open_time) / 1000.0;
      cached.opens[i] = c.open;
      cached.highs[i] = c.high;
      cached.lows[i] = c.low;
      cached.closes[i] = c.close;
    }
    cached.last_time = candles.back().open_time;
  } else if (candles.empty()) {
    cached.times.clear();
    cached.opens.clear();
    cached.highs.clear();
    cached.lows.clear();
    cached.closes.clear();
    cached.last_time = 0;
  }

  const auto &times = cached.times;
  const auto &opens = cached.opens;
  const auto &highs = cached.highs;
  const auto &lows = cached.lows;
  const auto &closes = cached.closes;

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
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Rect")) {
    adding_rect = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Measure")) {
    measure_mode = true;
    has_measure = false;
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
  }

  if (apply_manual_limits) {
    ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max,
                              manual_limits.Y.Min, manual_limits.Y.Max,
                              ImGuiCond_Always);
    apply_manual_limits = false;
  }

  ImPlotFlags plot_flags = ImPlotFlags_Crosshairs;
  if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(),
                        ImGui::GetContentRegionAvail(), plot_flags)) {
    ImPlot::SetupAxes("Time", "Price");
    ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Outside);
    Plot::PlotCandlestick("Candles", times.data(), opens.data(), closes.data(),
                          lows.data(), highs.data(), (int)candles.size(), true,
                          0.25f, ImVec4(1, 1, 1, 1),
                          ImVec4(0.3f, 0.3f, 0.3f, 1));

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
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          lines.push_back({mouse, mouse});
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                   !lines.empty()) {
          lines.back().p2 = mouse;
        } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                   !lines.empty()) {
          lines.back().p2 = mouse;
          shape_order.push_back({ShapeType::Line, lines.size() - 1});
          adding_line = false;
        }
      }

      if (adding_rect) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          rects.push_back({mouse, mouse});
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                   !rects.empty()) {
          rects.back().p2 = mouse;
        } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                   !rects.empty()) {
          rects.back().p2 = mouse;
          shape_order.push_back({ShapeType::Rect, rects.size() - 1});
          adding_rect = false;
        }
      }

      if (measure_mode) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          measure_start = mouse;
          measure_end = mouse;
          has_measure = true;
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
          measure_end = mouse;
        } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          measure_end = mouse;
          measure_mode = false;
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
  ImGui::End();
}
