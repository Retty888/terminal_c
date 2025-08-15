// Caches previously computed signals to avoid recalculation when parameters
// have not changed.

#include "ui/signals_window.h"

#include "imgui.h"
#include "signal.h"

#include <algorithm>
#include <ctime>

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
    const std::string& selected_interval,
    AppStatus& status) {
    ImGui::Begin("Signals");
    ImGui::InputInt("Short SMA", &short_period);
    ImGui::InputInt("Long SMA", &long_period);
    if (long_period <= short_period) {
        long_period = short_period + 1;
    }
    ImGui::Checkbox("Show on Chart", &show_on_chart);

    struct SignalsCache {
        int short_period = 0;
        int long_period = 0;
        std::string active_pair;
        std::string selected_interval;
        long long last_candle_time = 0;
        std::vector<SignalEntry> entries;
        std::vector<double> buy_times;
        std::vector<double> buy_prices;
        std::vector<double> sell_times;
        std::vector<double> sell_prices;
        bool initialized = false;
    };
    static SignalsCache cache;

    const auto& sig_candles = all_candles.at(active_pair).at(selected_interval);
    long long latest_time = sig_candles.empty() ? 0 : sig_candles.back().open_time;

    bool need_recalc = !cache.initialized || cache.short_period != short_period ||
                       cache.long_period != long_period ||
                       cache.active_pair != active_pair ||
                       cache.selected_interval != selected_interval ||
                       cache.last_candle_time != latest_time;

    if (need_recalc) {
        status.signal_message = "Computing signals";
        status.log.push_back("Computing signals for " + active_pair + " " + selected_interval);
        if (status.log.size() > 50) status.log.erase(status.log.begin());
        cache.short_period = short_period;
        cache.long_period = long_period;
        cache.active_pair = active_pair;
        cache.selected_interval = selected_interval;
        cache.last_candle_time = latest_time;

        cache.entries.clear();
        cache.buy_times.clear();
        cache.buy_prices.clear();
        cache.sell_times.clear();
        cache.sell_prices.clear();

        for (std::size_t i = static_cast<std::size_t>(long_period); i < sig_candles.size(); ++i) {
            int sig = Signal::sma_crossover_signal(sig_candles, i, short_period, long_period);
            if (sig != 0) {
                double t = static_cast<double>(sig_candles[i].open_time) / 1000.0;
                double price = sig_candles[i].close;
                double short_sma = Signal::simple_moving_average(sig_candles, i, short_period);
                double long_sma = Signal::simple_moving_average(sig_candles, i, long_period);
                cache.entries.push_back({t, price, short_sma, long_sma, sig});
                if (sig > 0) {
                    cache.buy_times.push_back(t);
                    cache.buy_prices.push_back(price);
                } else {
                    cache.sell_times.push_back(t);
                    cache.sell_prices.push_back(price);
                }
            }
        }

        cache.initialized = true;
        status.signal_message = "Signals updated";
    }

    signal_entries = cache.entries;
    buy_times = cache.buy_times;
    buy_prices = cache.buy_prices;
    sell_times = cache.sell_times;
    sell_prices = cache.sell_prices;

    if (ImGui::BeginTable("SignalsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Short SMA");
        ImGui::TableSetupColumn("Long SMA");
        ImGui::TableSetupColumn("Signal");
        ImGui::TableHeadersRow();
        int rows = static_cast<int>(signal_entries.size());
        int start = std::max(0, rows - 10);
        char buf[20];
        for (int i = start; i < rows; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::time_t tt = static_cast<std::time_t>(signal_entries[i].time);
            std::tm* tm = std::localtime(&tt);
            if (tm && std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm)) {
                ImGui::TextUnformatted(buf);
            } else {
                ImGui::Text("%lld", static_cast<long long>(signal_entries[i].time));
            }
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

