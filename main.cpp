#include "imgui.h"
#include "implot.h"
#include "plot/candlestick.h"
#include "core/candle.h"
#include "core/data_fetcher.h"
#include "config.h"
#include "core/candle_manager.h"
#include "signal.h"

#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

using namespace Core;

int main() {
    // Init GLFW
    if (!glfwInit()) return -1;

    // Create window with OpenGL context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Trading Terminal", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Load config
    std::vector<std::string> selected_pairs = Config::load_selected_pairs("config.json");
    if (selected_pairs.empty()) selected_pairs.push_back("BTCUSDT");
    std::string active_pair = selected_pairs[0];

    // Load candles for several intervals
    const std::vector<std::string> intervals = {"1m", "5m", "15m", "1h", "4h", "1d"};
    std::map<std::string, std::map<std::string, std::vector<Candle>>> all_candles;
    std::string selected_interval = "1m";

    struct SignalEntry {
        double time;
        double price;
        double short_sma;
        double long_sma;
        int type;
    };
    int short_period = 9;
    int long_period = 21;
    bool show_on_chart = false;
    std::vector<SignalEntry> signal_entries;
    std::vector<double> buy_times, buy_prices, sell_times, sell_prices;
    for (const auto& pair : selected_pairs) {
        for (const auto& interval : intervals) {
            auto candles = CandleManager::load_candles(pair, interval);
            if (candles.empty()) {
                candles = DataFetcher::fetch_klines(pair, interval, 5000);
                if (!candles.empty()) {
                    CandleManager::save_candles(pair, interval, candles);
                }
            }
            all_candles[pair][interval] = candles;
        }
    }

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static auto last_fetch = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last_fetch >= std::chrono::minutes(1)) {
            for (const auto& pair : selected_pairs) {
                auto latest = DataFetcher::fetch_klines(pair, "1m", 1);
                if (!latest.empty()) {
                    auto& vec = all_candles[pair]["1m"];
                    if (vec.empty() || latest.back().open_time > vec.back().open_time) {
                        vec.push_back(latest.back());
                        CandleManager::append_candles(pair, "1m", {latest.back()});
                    }
                }
            }
            last_fetch = now;
        }

        ImGui::Begin("Control Panel");

        ImGui::Text("Select pairs to load:");
        static char new_symbol[32] = "";
        ImGui::InputText("##new_symbol", new_symbol, IM_ARRAYSIZE(new_symbol));
        ImGui::SameLine();
        if (ImGui::Button("Load Symbol")) {
            std::string symbol(new_symbol);
            symbol.erase(
                std::remove_if(symbol.begin(), symbol.end(),
                               [](unsigned char c) { return std::isspace(c) || c == '-'; }),
                symbol.end());
            std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
            bool valid = !symbol.empty() &&
                         std::all_of(symbol.begin(), symbol.end(),
                                     [](unsigned char c) { return std::isalnum(c); });
            if (valid &&
                std::find(selected_pairs.begin(), selected_pairs.end(), symbol) == selected_pairs.end()) {
                selected_pairs.push_back(symbol);
                Config::save_selected_pairs("config.json", selected_pairs);
            }
            new_symbol[0] = '\0';
        }

        for (auto it = selected_pairs.begin(); it != selected_pairs.end();) {
            bool keep = true;
            if (ImGui::Checkbox(it->c_str(), &keep) && !keep) {
                std::string removed = *it;
                it = selected_pairs.erase(it);
                all_candles.erase(removed);
                if (active_pair == removed) {
                    if (!selected_pairs.empty()) {
                        active_pair = selected_pairs.front();
                    } else {
                        active_pair.clear();
                    }
                }
                Config::save_selected_pairs("config.json", selected_pairs);
            } else {
                ++it;
            }
        }

        ImGui::Separator();
        ImGui::Text("Active chart:");

        for (const auto& pair : selected_pairs) {
            if (ImGui::RadioButton(pair.c_str(), active_pair == pair)) {
                active_pair = pair;
                if (all_candles.find(pair) == all_candles.end()) {
                    for (const auto& interval : intervals) {
                        auto candles = CandleManager::load_candles(pair, interval);
                        if (candles.empty()) {
                            candles = DataFetcher::fetch_klines(pair, interval, 5000);
                            if (!candles.empty()) {
                                CandleManager::save_candles(pair, interval, candles);
                            }
                        }
                        all_candles[pair][interval] = candles;
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Timeframe:");
        if (ImGui::BeginCombo("##interval_combo", selected_interval.c_str())) {
            for (const auto& interval : intervals) {
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

        const auto& sig_candles = all_candles[active_pair][selected_interval];
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

        ImGui::Begin("Analytics");
        const auto& ana_candles = all_candles[active_pair][selected_interval];
        if (!ana_candles.empty()) {
            ImGui::Text("Data points: %d", (int)ana_candles.size());
            ImGui::Text("Last open: %.2f", ana_candles.back().open);
            ImGui::Text("Last close: %.2f", ana_candles.back().close);
        } else {
            ImGui::Text("No data");
        }
        ImGui::End();

        ImGui::Begin("Chart");
        if (ImPlot::BeginPlot(("Candles - " + active_pair + " (" + selected_interval + ")").c_str(), "Time", "Price")) {
            const auto& candles = all_candles[active_pair][selected_interval];
            std::vector<double> times, opens, highs, lows, closes;
            for (const auto& c : candles) {
                times.push_back((double)c.open_time / 1000.0);
                opens.push_back(c.open);
                highs.push_back(c.high);
                lows.push_back(c.low);
                closes.push_back(c.close);
            }

            Plot::PlotCandlestick(
                "Candles",
                times.data(), opens.data(), closes.data(), lows.data(), highs.data(),
                (int)candles.size(),
                true,
                0.25f
            );

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

            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
