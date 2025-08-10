#include "ui/chart_window.h"

#include "imgui.h"
#include "implot.h"
#include "plot/candlestick.h"

#include <algorithm>

using namespace Core;

void DrawChartWindow(
    const std::map<std::string, std::map<std::string, std::vector<Candle>>>& all_candles,
    const std::string& active_pair,
    const std::string& active_interval,
    bool show_on_chart,
    const std::vector<double>& buy_times,
    const std::vector<double>& buy_prices,
    const std::vector<double>& sell_times,
    const std::vector<double>& sell_prices,
    const Journal::Journal& journal,
    const Core::BacktestResult& last_result) {
    ImGui::Begin("Chart");

    const auto& candles = all_candles.at(active_pair).at(active_interval);
    std::vector<double> times, opens, highs, lows, closes;
    for (const auto& c : candles) {
        times.push_back((double)c.open_time / 1000.0);
        opens.push_back(c.open);
        highs.push_back(c.high);
        lows.push_back(c.low);
        closes.push_back(c.close);
    }

    static ImPlotRect manual_limits;
    static bool use_manual_limits = false;

    if (ImGui::Button("Zoom In")) {
        double xc = (manual_limits.X.Min + manual_limits.X.Max) * 0.5;
        double yc = (manual_limits.Y.Min + manual_limits.Y.Max) * 0.5;
        double xr = (manual_limits.X.Max - manual_limits.X.Min) * 0.5;
        double yr = (manual_limits.Y.Max - manual_limits.Y.Min) * 0.5;
        manual_limits.X.Min = xc - xr * 0.5;
        manual_limits.X.Max = xc + xr * 0.5;
        manual_limits.Y.Min = yc - yr * 0.5;
        manual_limits.Y.Max = yc + yr * 0.5;
        use_manual_limits = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out")) {
        double xc = (manual_limits.X.Min + manual_limits.X.Max) * 0.5;
        double yc = (manual_limits.Y.Min + manual_limits.Y.Max) * 0.5;
        double xr = (manual_limits.X.Max - manual_limits.X.Min);
        double yr = (manual_limits.Y.Max - manual_limits.Y.Min);
        manual_limits.X.Min = xc - xr;
        manual_limits.X.Max = xc + xr;
        manual_limits.Y.Min = yc - yr;
        manual_limits.Y.Max = yc + yr;
        use_manual_limits = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        if (!times.empty()) {
            manual_limits.X.Min = times.front();
            manual_limits.X.Max = times.back();
        }
        if (!lows.empty() && !highs.empty()) {
            manual_limits.Y.Min = *std::min_element(lows.begin(), lows.end());
            manual_limits.Y.Max = *std::max_element(highs.begin(), highs.end());
        }
        use_manual_limits = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
        ImPlot::SetNextAxesToFit();
        use_manual_limits = false;
    }

    if (use_manual_limits) {
        ImPlot::SetNextAxesLimits(manual_limits.X.Min, manual_limits.X.Max,
                                  manual_limits.Y.Min, manual_limits.Y.Max,
                                  ImPlotCond_Always);
    }

    ImPlotFlags plot_flags = ImPlotFlags_Crosshairs;
    if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), ImVec2(-1,0), plot_flags)) {
        ImPlot::SetupAxes("Time", "Price");
        Plot::PlotCandlestick(
            "Candles",
            times.data(), opens.data(), closes.data(), lows.data(), highs.data(),
            (int)candles.size(),
            true,
            0.25f
        );

        ImPlotRect cur_limits = ImPlot::GetPlotLimits();
        if (!use_manual_limits) {
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
        ImPlot::SetNextLineStyle(ImVec4(1,1,1,0.5f));
        ImPlot::PlotLine("##vline", vx, vy, 2);

        double hx[2] = {cur_limits.X.Min, cur_limits.X.Max};
        double hy[2] = {cursor_y, cursor_y};
        ImPlot::SetNextLineStyle(ImVec4(1,1,1,0.5f));
        ImPlot::PlotLine("##hline", hx, hy, 2);

        double price = closes.empty() ? 0.0 : closes.back();
        double px[2] = {cur_limits.X.Min, cur_limits.X.Max};
        double py[2] = {price, price};
        ImPlot::SetNextLineStyle(ImVec4(0,1,0,1));
        ImPlot::PlotLine("##price", px, py, 2);
        if (show_on_chart) {
            if (!buy_times.empty()) {
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Up, 6, ImVec4(0,1,0,1));
                ImPlot::PlotScatter("Buy", buy_times.data(), buy_prices.data(), (int)buy_times.size());
            }
            if (!sell_times.empty()) {
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Down, 6, ImVec4(1,0,0,1));
                ImPlot::PlotScatter("Sell", sell_times.data(), sell_prices.data(), (int)sell_times.size());
            }
        }

        // Overlay journal entries
        std::vector<double> jb_times, jb_prices, js_times, js_prices;
        for (const auto& e : journal.entries()) {
            if (e.symbol == active_pair) {
                double t = (double)e.timestamp / 1000.0;
                if (e.side == Journal::Side::Buy) { jb_times.push_back(t); jb_prices.push_back(e.price); }
                else { js_times.push_back(t); js_prices.push_back(e.price); }
            }
        }
        if (!jb_times.empty()) {
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0,1,0,1));
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4);
            ImPlot::PlotScatter("Journal Buy", jb_times.data(), jb_prices.data(), jb_times.size());
            ImPlot::PopStyleColor();
        }
        if (!js_times.empty()) {
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1,0,0,1));
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross, 4);
            ImPlot::PlotScatter("Journal Sell", js_times.data(), js_prices.data(), js_times.size());
            ImPlot::PopStyleColor();
        }

        // Overlay backtest trades
        if (!last_result.trades.empty()) {
            std::vector<double> bt_entry_t, bt_entry_p, bt_exit_t, bt_exit_p;
            for (const auto& t : last_result.trades) {
                bt_entry_t.push_back((double)candles[t.entry_index].open_time / 1000.0);
                bt_entry_p.push_back(candles[t.entry_index].close);
                bt_exit_t.push_back((double)candles[t.exit_index].open_time / 1000.0);
                bt_exit_p.push_back(candles[t.exit_index].close);
            }
            if (!bt_entry_t.empty()) {
                ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0,0,1,1));
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 4);
                ImPlot::PlotScatter("BT Entry", bt_entry_t.data(), bt_entry_p.data(), bt_entry_t.size());
                ImPlot::PopStyleColor();
            }
            if (!bt_exit_t.empty()) {
                ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1,1,0,1));
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 4);
                ImPlot::PlotScatter("BT Exit", bt_exit_t.data(), bt_exit_p.data(), bt_exit_t.size());
                ImPlot::PopStyleColor();
            }
        }

        ImPlot::EndPlot();
    }
    ImGui::End();
}

