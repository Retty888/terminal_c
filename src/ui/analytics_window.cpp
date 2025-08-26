#include "ui/analytics_window.h"

#include "imgui.h"

#include <algorithm>
#include <vector>

void DrawAnalyticsWindow(
    const std::map<std::string,
                   std::map<std::string, std::vector<Core::Candle>>>
        &all_candles,
    const std::string &active_pair, const std::string &selected_interval) {
  auto vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_FirstUseEver);
  ImGui::Begin("Analytics");
  auto pair_it = all_candles.find(active_pair);
  if (pair_it != all_candles.end()) {
    auto interval_it = pair_it->second.find(selected_interval);
    if (interval_it != pair_it->second.end()) {
      const auto &ana_candles = interval_it->second;
      if (!ana_candles.empty()) {
        double min_price = ana_candles.front().low;
        double max_price = ana_candles.front().high;
        double sum_volume = 0.0;
        double sum_close = 0.0;
        for (const auto &c : ana_candles) {
          min_price = std::min(min_price, c.low);
          max_price = std::max(max_price, c.high);
          sum_volume += c.volume;
          sum_close += c.close;
        }
        double avg_volume = sum_volume / ana_candles.size();
        double avg_close = sum_close / ana_candles.size();
        double change = ana_candles.back().close - ana_candles.front().close;
        double change_pct = ana_candles.front().close != 0.0
                                ? change / ana_candles.front().close * 100.0
                                : 0.0;

        if (ImGui::BeginTabBar("##analytics_tabs")) {
          if (ImGui::BeginTabItem("Price")) {
            ImGui::Text("Data points: %d", (int)ana_candles.size());
            ImGui::Text("Min price: %.2f", min_price);
            ImGui::Text("Max price: %.2f", max_price);
            ImGui::Text("Avg close: %.2f", avg_close);
            ImGui::Text("Change: %.2f (%.2f%%)", change, change_pct);
            ImGui::EndTabItem();
          }

          if (ImGui::BeginTabItem("Volume")) {
            ImGui::Text("Avg volume: %.2f", avg_volume);
            ImGui::EndTabItem();
          }
          ImGui::EndTabBar();
        }
      } else {
        ImGui::Text("No data");
      }
    } else {
      ImGui::Text("Information unavailable");
    }
  } else {
    ImGui::Text("Information unavailable");
  }
  ImGui::End();
}
