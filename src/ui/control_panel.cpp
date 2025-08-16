#include "ui/control_panel.h"

#include "config.h"
#include "core/candle_manager.h"
#include "core/data_fetcher.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <ctime>
#include <numeric>

using namespace Core;

namespace {
const size_t EXPECTED_CANDLES = Config::load_candles_limit("config.json");
constexpr size_t THRESHOLD_LOW = 100;
constexpr size_t THRESHOLD_MED = 1000;

const char *EMOJI_LOW = "\xF0\x9F\x98\x9F";  // 😟
const char *EMOJI_MED = "\xF0\x9F\x98\x90";  // 😐
const char *EMOJI_HIGH = "\xF0\x9F\x98\x83"; // 😃

const ImVec4 COLOR_LOW = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
const ImVec4 COLOR_MED = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
const ImVec4 COLOR_HIGH = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

std::string format_date(long long ms) {
  if (ms == 0)
    return "-";
  std::time_t t = ms / 1000;
  std::tm *tm = std::gmtime(&t);
  char buf[6];
  if (std::strftime(buf, sizeof(buf), "%d.%m", tm))
    return std::string(buf);
  return "-";
}
} // namespace

void DrawControlPanel(
    std::vector<PairItem> &pairs, std::vector<std::string> &selected_pairs,
    std::string &active_pair, const std::vector<std::string> &intervals,
    std::string &selected_interval,
    std::map<std::string, std::map<std::string, std::vector<Candle>>>
        &all_candles,
    const std::function<void()> &save_pairs,
    const std::vector<std::string> &exchange_pairs, const AppStatus &status) {
  ImGui::Begin("Control Panel");

  ImGui::Text("Select pairs to load:");
  static std::string load_error;

  ImGui::Separator();
  ImGui::Text("Load from exchange:");
  static int selected_idx = 0;
  std::vector<std::string> sorted_pairs = exchange_pairs;
  std::sort(sorted_pairs.begin(), sorted_pairs.end());
  if (selected_idx >= static_cast<int>(sorted_pairs.size()))
    selected_idx = 0;
  static char pair_filter[64] = "";
  ImGui::InputText("##pair_filter", pair_filter, IM_ARRAYSIZE(pair_filter));
  std::string filter_str = pair_filter;
  std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(),
                 ::tolower);
  std::string current =
      sorted_pairs.empty() ? std::string() : sorted_pairs[selected_idx];
  if (ImGui::BeginCombo("##exchange_combo", current.c_str())) {
    for (int i = 0; i < (int)sorted_pairs.size(); ++i) {
      std::string item = sorted_pairs[i];
      std::transform(item.begin(), item.end(), item.begin(), ::tolower);
      if (!filter_str.empty() && item.find(filter_str) == std::string::npos)
        continue;
      bool is_selected = (selected_idx == i);
      if (ImGui::Selectable(sorted_pairs[i].c_str(), is_selected)) {
        selected_idx = i;
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Selected")) {
    if (!sorted_pairs.empty()) {
      std::string symbol = sorted_pairs[selected_idx];
      if (std::none_of(pairs.begin(), pairs.end(),
                       [&](const PairItem &p) { return p.name == symbol; })) {
        pairs.push_back({symbol, true});
        save_pairs();
        if (std::find(selected_pairs.begin(), selected_pairs.end(), symbol) ==
            selected_pairs.end()) {
          selected_pairs.push_back(symbol);
          Config::save_selected_pairs("config.json", selected_pairs);
        }
        bool failed = false;
        for (const auto &interval : intervals) {
          auto candles = CandleManager::load_candles(symbol, interval);
          if (candles.size() < EXPECTED_CANDLES) {
            int missing = EXPECTED_CANDLES - static_cast<int>(candles.size());
            auto fetched = DataFetcher::fetch_klines(symbol, interval, missing);
            if (fetched.error == FetchError::None && !fetched.candles.empty()) {
              CandleManager::append_candles(symbol, interval, fetched.candles);
              long long last_time =
                  candles.empty() ? 0 : candles.back().open_time;
              for (const auto &c : fetched.candles) {
                if (c.open_time > last_time)
                  candles.push_back(c);
              }
            }
          }
          if (candles.empty()) {
            failed = true;
          } else {
            all_candles[symbol][interval] = candles;
          }
        }
        load_error = failed ? "Failed to load " + symbol : "";
      }
    }
  }

  if (!load_error.empty()) {
    ImGui::Text("%s", load_error.c_str());
  }

  for (auto it = pairs.begin(); it != pairs.end();) {
    std::string checkbox_id = it->name + "##checkbox_" + it->name;
    if (ImGui::Checkbox(checkbox_id.c_str(), &it->visible)) {
      if (!it->visible && active_pair == it->name) {
        auto new_active =
            std::find_if(pairs.begin(), pairs.end(),
                         [](const PairItem &p) { return p.visible; });
        active_pair =
            new_active != pairs.end() ? new_active->name : std::string();
      } else if (it->visible && active_pair.empty()) {
        active_pair = it->name;
      }
    }

    bool missing_data = false;
    struct TooltipStat {
      std::string interval;
      size_t count;
      double volume;
      std::string start;
      std::string end;
    };
    std::vector<TooltipStat> stats;
    size_t sel_count = 0;
    std::string sel_start = "-";
    std::string sel_end = "-";
    for (const auto &interval : intervals) {
      const auto &candles = all_candles[it->name][interval];
      size_t count = candles.size();
      double volume = std::accumulate(
          candles.begin(), candles.end(), 0.0,
          [](double sum, const Candle &c) { return sum + c.volume; });
      long long min_t = 0;
      long long max_t = 0;
      if (!candles.empty()) {
        auto [min_it, max_it] =
            std::minmax_element(candles.begin(), candles.end(),
                                [](const Candle &a, const Candle &b) {
                                  return a.open_time < b.open_time;
                                });
        min_t = min_it->open_time;
        max_t = max_it->open_time;
      } else {
        missing_data = true;
      }
      std::string start = format_date(min_t);
      std::string end = format_date(max_t);
      stats.push_back({interval, count, volume, start, end});
      if (interval == selected_interval) {
        sel_count = count;
        sel_start = start;
        sel_end = end;
      }
    }

    const char *emoji = EMOJI_LOW;
    ImVec4 color = COLOR_LOW;
    if (sel_count >= THRESHOLD_MED) {
      emoji = EMOJI_HIGH;
      color = COLOR_HIGH;
    } else if (sel_count >= THRESHOLD_LOW) {
      emoji = EMOJI_MED;
      color = COLOR_MED;
    }
    std::string label =
        sel_start + "–" + sel_end + " (" + std::to_string(sel_count) + ")";
    ImGui::SameLine();
    ImGui::Text("%s %s", emoji, label.c_str());
    ImGui::SameLine();
    float progress = EXPECTED_CANDLES
                         ? static_cast<float>(sel_count) / EXPECTED_CANDLES
                         : 0.0f;
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(progress, ImVec2(100.0f, 0.0f));
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      for (const auto &s : stats) {
        ImGui::Text("%s: %zu candles, vol %.2f, %s-%s", s.interval.c_str(),
                    s.count, s.volume, s.start.c_str(), s.end.c_str());
      }
      ImGui::EndTooltip();
    }
    if (missing_data) {
      ImGui::SameLine();
      ImGui::TextColored(COLOR_LOW, "!");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton((std::string("X##remove_") + it->name).c_str())) {
      std::string removed = it->name;
      it = pairs.erase(it);
      all_candles.erase(removed);
      if (active_pair == removed) {
        auto new_active =
            std::find_if(pairs.begin(), pairs.end(),
                         [](const PairItem &p) { return p.visible; });
        active_pair =
            new_active != pairs.end() ? new_active->name : std::string();
      }
      save_pairs();
    } else {
      ++it;
    }
  }

  ImGui::Separator();
  ImGui::Text("Status");
  ImGui::Text("Candles: %.0f%%", status.candle_progress * 100.0f);
  ImGui::Text("Analysis: %s", status.analysis_message.c_str());
  ImGui::Text("Signals: %s", status.signal_message.c_str());
  if (!status.error_message.empty())
    ImGui::TextColored(COLOR_LOW, "%s", status.error_message.c_str());
  if (ImGui::BeginListBox("##status_log", ImVec2(-FLT_MIN, 100))) {
    for (const auto &msg : status.log) {
      ImGui::Selectable(msg.c_str(), false, ImGuiSelectableFlags_Disabled);
    }
    ImGui::EndListBox();
  }

  ImGui::End();
}
