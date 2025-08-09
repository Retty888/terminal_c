#include "ui/signals_window.h"

#include "imgui.h"
#include "signal.h"

using namespace Core;

void DrawSignalsWindow(
    int& short_period,
    int& long_period,
    bool& show_on_chart,
    std::vector<SignalEntry>& signal_entries,
    std::vector<double>& buy_times,
    std::vector<double>& buy_prices,
    std::vector<double>& sell_times,
    std::vector<double>& sell_prices,
    const std::map<std::string, std::map<std::string, std::vector<Candle>>>& all_candles,
    const std::string& active_pair,
    const std::string& selected_interval) {
    ImGui::Begin("Signals");
    ImGui::InputInt("Short SMA", &short_period);
    ImGui::InputInt("Long SMA", &long_period);
    if (long_period <= short_period) {
        long_period = short_period + 1;
    }
    ImGui::Checkbox("Show on Chart", &show_on_chart);

    signal_entries.clear();
    buy_times.clear();
    buy_prices.clear();
    sell_times.clear();
    sell_prices.clear();

    const auto& sig_candles = all_candles.at(active_pair).at(selected_interval);
    for (std::size_t i = static_cast<std::size_t>(long_period); i < sig_candles.size(); ++i) {
        int sig = Signal::sma_crossover_signal(sig_candles, i, short_period, long_period);
        if (sig != 0) {
            double t = static_cast<double>(sig_candles[i].open_time) / 1000.0;
            double price = sig_candles[i].close;
            double short_sma = Signal::simple_moving_average(sig_candles, i, short_period);
            double long_sma = Signal::simple_moving_average(sig_candles, i, long_period);
            signal_entries.push_back({t, price, short_sma, long_sma, sig});
            if (sig > 0) {
                buy_times.push_back(t);
                buy_prices.push_back(price);
            } else {
                sell_times.push_back(t);
                sell_prices.push_back(price);
            }
        }
    }

    if (ImGui::BeginTable("SignalsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Short SMA");
        ImGui::TableSetupColumn("Long SMA");
        ImGui::TableSetupColumn("Signal");
        ImGui::TableHeadersRow();
        int rows = static_cast<int>(signal_entries.size());
        int start = std::max(0, rows - 10);
        for (int i = start; i < rows; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%lld", (long long)signal_entries[i].time);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", signal_entries[i].short_sma);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", signal_entries[i].long_sma);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", signal_entries[i].type > 0 ? "Buy" : "Sell");
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

