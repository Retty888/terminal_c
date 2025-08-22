#include "ui/control_panel.h"
#include "app.h"

#include "config_manager.h"
#include "config_path.h"
#include "core/data_fetcher.h"
#include "core/interval_utils.h"
#include "imgui.h"
#include "core/logger.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <ctime>
#include <numeric>
#include <cstdint>
#include <chrono>
#include <string>


namespace {
const size_t EXPECTED_CANDLES = [] {
  auto cfg = Config::ConfigManager::load(resolve_config_path().string());
  return cfg ? cfg->candles_limit : 5000u;
}();
constexpr size_t THRESHOLD_LOW = 100;
constexpr size_t THRESHOLD_MED = 1000;

const char *EMOJI_LOW = "\xF0\x9F\x98\x9F";  // 😟
const char *EMOJI_MED = "\xF0\x9F\x98\x90";  // 😐
const char *EMOJI_HIGH = "\xF0\x9F\x98\x83"; // 😃

const ImVec4 COLOR_LOW = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
const ImVec4 COLOR_MED = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
const ImVec4 COLOR_HIGH = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);


// Format milliseconds since epoch into a dd.mm string or "-".
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

struct TooltipStat {
  std::string interval;
  size_t count;
  double volume;
  std::string start;
  std::string end;
  std::uintmax_t size_bytes;
};

// Load candle history for a symbol across multiple intervals.
bool LoadInitialCandles(
    DataService &data_service, const std::string &symbol,
    const std::vector<std::string> &intervals,
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>> &all_candles,
    std::string &load_error) {
  bool failed = false;
  for (const auto &interval : intervals) {
    auto candles = data_service.load_candles(symbol, interval);
    long long last_time = candles.empty() ? 0 : candles.back().open_time;
    if (candles.size() < EXPECTED_CANDLES) {
      int missing = EXPECTED_CANDLES - static_cast<int>(candles.size());
      auto fetched = data_service.fetch_klines(symbol, interval, missing);
      if (fetched.error == Core::FetchError::None && !fetched.candles.empty()) {
        auto interval_ms = Core::parse_interval(interval).count();
        std::vector<Core::Candle> to_append;
        long long expected = last_time + interval_ms;
        for (const auto &c : fetched.candles) {
          if (c.open_time > expected) {
            long long gap_end = c.open_time - interval_ms;
            auto gap_res = data_service.fetch_range(symbol, interval, expected,
                                                    gap_end);
            if (gap_res.error == Core::FetchError::None &&
                !gap_res.candles.empty()) {
              to_append.insert(to_append.end(), gap_res.candles.begin(),
                               gap_res.candles.end());
              load_error.clear();
            } else {
              failed = true;
              load_error = "Gap load failed for " + symbol + " " + interval +
                           ": " + gap_res.message;
            }
            expected = gap_end + interval_ms;
          }
          if (c.open_time >= expected) {
            to_append.push_back(c);
            expected = c.open_time + interval_ms;
          }
        }
        if (!to_append.empty()) {
          data_service.append_candles(symbol, interval, to_append);
          for (const auto &c : to_append) {
            if (c.open_time > last_time) {
              candles.push_back(c);
              last_time = c.open_time;
            }
          }
        }
      } else if (fetched.error != Core::FetchError::None) {
        failed = true;
        load_error =
            "Load failed for " + symbol + " " + interval + ": " + fetched.message;
      }
    }
    if (candles.empty()) {
      failed = true;
    } else {
      all_candles[symbol][interval] = candles;
    }
  }
  if (failed && load_error.empty())
    load_error = "Failed to load " + symbol;
  return !failed;
}

// Render a single pair row with stats and controls. Returns true if the pair
// should be removed from the list.
bool RenderPairRow(
    std::vector<PairItem> &pairs, PairItem &item,
    std::vector<std::string> &selected_pairs, std::string &active_pair,
    const std::vector<std::string> &intervals, std::string &selected_interval,
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>> &all_candles,
    const std::function<void()> &save_pairs, DataService &data_service,
    AppStatus &status,
    const std::function<void(const std::string &)> &cancel_pair) {
  bool missing_data = false;
  std::vector<TooltipStat> stats;
  size_t sel_count = 0;
  std::string sel_start = "-";
  std::string sel_end = "-";
  for (const auto &interval : intervals) {
    const auto &candles = all_candles[item.name][interval];
    size_t count = candles.size();
    double volume =
        std::accumulate(candles.begin(), candles.end(), 0.0,
                        [](double sum, const Core::Candle &c) { return sum + c.volume; });
    long long min_t = 0;
    long long max_t = 0;
    if (!candles.empty()) {
      auto [min_it, max_it] = std::minmax_element(
          candles.begin(), candles.end(),
          [](const Core::Candle &a, const Core::Candle &b) { return a.open_time < b.open_time; });
      min_t = min_it->open_time;
      max_t = max_it->open_time;
    } else {
      missing_data = true;
    }
    std::string start = format_date(min_t);
    std::string end = format_date(max_t);
    auto size_bytes = data_service.get_file_size(item.name, interval);
    stats.push_back({interval, count, volume, start, end, size_bytes});
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

  // Column 1: visibility checkbox and pair name
  ImGui::PushID(item.name.c_str());
  ImGui::TableNextColumn();
  if (ImGui::Checkbox(item.name.c_str(), &item.visible)) {
    if (!item.visible && active_pair == item.name) {
      auto new_active =
          std::find_if(pairs.begin(), pairs.end(),
                       [](const PairItem &p) { return p.visible; });
      active_pair = new_active != pairs.end() ? new_active->name : std::string();
    } else if (item.visible && active_pair.empty()) {
      active_pair = item.name;
    }
  }

  // Column 2: candle stats and progress bar
  ImGui::TableNextColumn();
  ImGui::Text("%s %s", emoji, label.c_str());
  float progress = EXPECTED_CANDLES
                       ? static_cast<float>(sel_count) / EXPECTED_CANDLES
                       : 0.0f;
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
  ImGui::ProgressBar(progress, ImVec2(100.0f, 0.0f));
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    for (const auto &s : stats) {
      double sz = static_cast<double>(s.size_bytes);
      const char *unit = "B";
      if (sz > 1024) {
        sz /= 1024;
        unit = "KB";
      }
      if (sz > 1024) {
        sz /= 1024;
        unit = "MB";
      }
      ImGui::Text("%s: %zu candles, vol %.2f, %s-%s, %.2f %s",
                  s.interval.c_str(), s.count, s.volume, s.start.c_str(),
                  s.end.c_str(), sz, unit);
    }
    ImGui::EndTooltip();
  }
  if (missing_data) {
    ImGui::SameLine();
    ImGui::TextColored(COLOR_LOW, "!");
  }

  // Column 3: action buttons
  ImGui::TableNextColumn();
  bool removed = false;
  if (ImGui::SmallButton("X")) {
    all_candles.erase(item.name);
    if (active_pair == item.name) {
      auto new_active =
          std::find_if(pairs.begin(), pairs.end(),
                       [](const PairItem &p) { return p.visible; });
      active_pair = new_active != pairs.end() ? new_active->name : std::string();
    }
    selected_pairs.erase(std::remove(selected_pairs.begin(), selected_pairs.end(),
                                     item.name),
                         selected_pairs.end());
    Config::ConfigManager::save_selected_pairs(resolve_config_path().string(), selected_pairs);
    data_service.remove_candles(item.name);
    if (cancel_pair)
      cancel_pair(item.name);
    save_pairs();
    removed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload")) {
    ImGui::OpenPopup("ReloadPopup");
  }
  if (ImGui::BeginPopup("ReloadPopup")) {
    for (const auto &interval : intervals) {
      if (ImGui::Selectable(interval.c_str())) {
        bool ok = data_service.reload_candles(item.name, interval);
        if (ok) {
          all_candles[item.name][interval] =
              data_service.load_candles(item.name, interval);
        }
      }
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Delete")) {
    ImGui::OpenPopup("DeletePopup");
  }
  if (ImGui::BeginPopup("DeletePopup")) {
    for (const auto &interval : intervals) {
      if (ImGui::Selectable(interval.c_str())) {
        data_service.clear_interval(item.name, interval);
        all_candles[item.name][interval].clear();
        Core::Logger::instance().info("Deleted " + item.name + " " + interval);
      }
    }
    ImGui::EndPopup();
  }

  ImGui::PopID();
  return removed;
}
} // namespace

// Render controls to load new pairs from the exchange.
static void RenderLoadControls(
    std::vector<PairItem> &pairs, std::vector<std::string> &selected_pairs,
    const std::vector<std::string> &intervals,
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>> &all_candles,
    const std::function<void()> &save_pairs,
    const std::vector<std::string> &exchange_pairs, DataService &data_service) {
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
  if (ImGui::Button("Load Selected") && !sorted_pairs.empty()) {
    std::string symbol = sorted_pairs[selected_idx];
    if (std::none_of(pairs.begin(), pairs.end(),
                     [&](const PairItem &p) { return p.name == symbol; })) {
      pairs.push_back({symbol, true});
      save_pairs();
      if (std::find(selected_pairs.begin(), selected_pairs.end(), symbol) ==
          selected_pairs.end()) {
        selected_pairs.push_back(symbol);
        Config::ConfigManager::save_selected_pairs(resolve_config_path().string(), selected_pairs);
      }
      if (!LoadInitialCandles(data_service, symbol, intervals, all_candles,
                              load_error)) {
        // load_error already set inside helper
      }
    }
  }

  if (!load_error.empty()) {
    ImGui::Text("%s", load_error.c_str());
  }
}

// Render the list of loaded pairs with statistics and controls.
static void RenderPairSelector(
    std::vector<PairItem> &pairs, std::vector<std::string> &selected_pairs,
    std::string &active_pair, const std::vector<std::string> &intervals,
    std::string &selected_interval,
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>> &all_candles,
    const std::function<void()> &save_pairs, DataService &data_service,
    AppStatus &status,
    const std::function<void(const std::string &)> &cancel_pair) {
  if (ImGui::BeginTable("pairs_table", 3, ImGuiTableFlags_SizingStretchProp)) {
    for (auto it = pairs.begin(); it != pairs.end();) {
      ImGui::TableNextRow();
      if (RenderPairRow(pairs, *it, selected_pairs, active_pair, intervals,
                        selected_interval, all_candles, save_pairs, data_service,
                        status, cancel_pair)) {
        it = pairs.erase(it);
      } else {
        ++it;
      }
    }
    ImGui::EndTable();
  }
}

// Render application status information and recent log messages.
static void RenderStatusPane(AppStatus &status) {
  ImGui::Separator();
  ImGui::Text("Status");
  ImGui::Text("Candles: %.0f%%", status.candle_progress * 100.0f);
  ImGui::Text("Analysis: %s", status.analysis_message.c_str());
  ImGui::Text("Signals: %s", status.signal_message.c_str());
  if (!status.error_message.empty())
    ImGui::TextColored(COLOR_LOW, "%s", status.error_message.c_str());
  if (ImGui::BeginListBox("##status_log", ImVec2(-FLT_MIN, 100))) {
    for (const auto &entry : status.log) {
      std::tm tm;
      auto t = std::chrono::system_clock::to_time_t(entry.time);
#if defined(_WIN32)
      localtime_s(&tm, &t);
#else
      localtime_r(&t, &tm);
#endif
      char buf[9];
      std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
      const char *lvl = "INFO";
      ImVec4 col = ImGui::GetStyle().Colors[ImGuiCol_Text];
      if (entry.level == Core::LogLevel::Warning) {
        lvl = "WARN";
        col = COLOR_MED;
      } else if (entry.level == Core::LogLevel::Error) {
        lvl = "ERROR";
        col = COLOR_LOW;
      }
      if (entry.level != Core::LogLevel::Info)
        ImGui::PushStyleColor(ImGuiCol_Text, col);
      std::string line = std::string(buf) + " [" + lvl + "] " + entry.message;
      ImGui::Selectable(line.c_str(), false, ImGuiSelectableFlags_Disabled);
      if (entry.level != Core::LogLevel::Info)
        ImGui::PopStyleColor();
    }
    ImGui::EndListBox();
  }
}

void DrawControlPanel(
    std::vector<PairItem> &pairs, std::vector<std::string> &selected_pairs,
    std::string &active_pair, const std::vector<std::string> &intervals,
    std::string &selected_interval,
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>> &all_candles,
    const std::function<void()> &save_pairs,
    const std::vector<std::string> &exchange_pairs, AppStatus &status,
    DataService &data_service,
    const std::function<void(const std::string &)> &cancel_pair) {
  ImGui::Begin("Control Panel");

  RenderLoadControls(pairs, selected_pairs, intervals, all_candles, save_pairs,
                     exchange_pairs, data_service);
  RenderPairSelector(pairs, selected_pairs, active_pair, intervals,
                     selected_interval, all_candles, save_pairs, data_service,
                     status, cancel_pair);
  RenderStatusPane(status);

  ImGui::End();
}
