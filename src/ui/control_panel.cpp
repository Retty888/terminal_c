#include "ui/control_panel.h"

#include "config.h"
#include "core/candle_manager.h"
#include "core/data_fetcher.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>

using namespace Core;

void DrawControlPanel(
    std::vector<PairItem> &pairs, std::vector<std::string> &selected_pairs,
    std::string &active_pair, std::string &active_interval,
    const std::vector<std::string> &intervals, std::string &selected_interval,
    std::map<std::string, std::map<std::string, std::vector<Candle>>> &all_candles,
    const std::function<void()> &save_pairs,
    const std::vector<std::string> &exchange_pairs) {
  ImGui::Begin("Control Panel");

  ImGui::Text("Select pairs to load:");
  static char new_symbol[32] = "";
  static std::string load_error;

  auto try_add_symbol = [&](const std::string &input) {
    std::string symbol(input);
    symbol.erase(std::remove_if(symbol.begin(), symbol.end(),
                                [](unsigned char c) {
                                  return std::isspace(c) || c == '-';
                                }),
                 symbol.end());
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
    bool valid = !symbol.empty() &&
                 std::all_of(symbol.begin(), symbol.end(),
                             [](unsigned char c) { return std::isalnum(c); });
    if (valid &&
        std::none_of(pairs.begin(), pairs.end(),
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
        if (candles.empty()) {
          auto fetched = DataFetcher::fetch_klines(symbol, interval, 5000);
          if (fetched && !fetched->empty()) {
            candles = *fetched;
            CandleManager::save_candles(symbol, interval, candles);
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
  };

  ImGui::InputText("##new_symbol", new_symbol, IM_ARRAYSIZE(new_symbol));
  ImGui::SameLine();
  if (ImGui::Button("Load Symbol")) {
    try_add_symbol(new_symbol);
    new_symbol[0] = '\0';
  }

  ImGui::Separator();
  ImGui::Text("Load from exchange:");
  static int selected_idx = 0;
  std::string current =
      exchange_pairs.empty() ? std::string() : exchange_pairs[selected_idx];
  if (ImGui::BeginCombo("##exchange_combo", current.c_str())) {
    for (int i = 0; i < (int)exchange_pairs.size(); ++i) {
      bool is_selected = (selected_idx == i);
      if (ImGui::Selectable(exchange_pairs[i].c_str(), is_selected)) {
        selected_idx = i;
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Selected")) {
    if (!exchange_pairs.empty())
      try_add_symbol(exchange_pairs[selected_idx]);
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
  ImGui::Text("Active chart:");

  for (const auto &item : pairs) {
    if (!item.visible)
      continue;
    const auto &pair = item.name;
    std::string radio_id = pair + "##radiobtn_" + pair;
    if (ImGui::RadioButton(radio_id.c_str(), active_pair == pair)) {
      active_pair = pair;
      if (all_candles[pair][active_interval].empty()) {
        auto candles = CandleManager::load_candles(pair, active_interval);
        if (candles.empty()) {
          auto fetched = DataFetcher::fetch_klines(pair, active_interval, 5000);
          if (fetched && !fetched->empty()) {
            candles = *fetched;
            CandleManager::save_candles(pair, active_interval, candles);
          }
        }
        all_candles[pair][active_interval] = candles;
      }
    }
  }

  ImGui::Separator();
  ImGui::Text("Interval:");
  for (const auto &interval : intervals) {
    if (ImGui::RadioButton(interval.c_str(), active_interval == interval)) {
      active_interval = interval;
      if (all_candles[active_pair][interval].empty()) {
        auto candles = CandleManager::load_candles(active_pair, interval);
        if (candles.empty()) {
          auto fetched = DataFetcher::fetch_klines(active_pair, interval, 5000);
          if (fetched && !fetched->empty()) {
            candles = *fetched;
            CandleManager::save_candles(active_pair, interval, candles);
          }
        }
        all_candles[active_pair][interval] = candles;
      }
    }
  }

  ImGui::Separator();
  ImGui::Text("Timeframe:");
  if (ImGui::BeginCombo("##interval_combo", selected_interval.c_str())) {
    for (const auto &interval : intervals) {
      bool is_selected = (selected_interval == interval);
      if (ImGui::Selectable(interval.c_str(), is_selected)) {
        selected_interval = interval;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::End();
}
