#include "ui/analytics_window.h"

#include "imgui.h"

using namespace Core;

void DrawAnalyticsWindow(
    const std::map<std::string, std::map<std::string, std::vector<Candle>>>& all_candles,
    const std::string& active_pair,
    const std::string& selected_interval) {
    ImGui::Begin("Analytics");
    const auto& ana_candles = all_candles.at(active_pair).at(selected_interval);
    if (!ana_candles.empty()) {
        ImGui::Text("Data points: %d", (int)ana_candles.size());
        ImGui::Text("Last open: %.2f", ana_candles.back().open);
        ImGui::Text("Last close: %.2f", ana_candles.back().close);
    } else {
        ImGui::Text("No data");
    }
    ImGui::End();
}

