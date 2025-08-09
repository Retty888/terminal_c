#include "imgui.h"
#include "implot.h"
#include "plot/candlestick.h"
#include "core/candle.h"
#include "core/data_fetcher.h"
#include "config.h"
#include "core/candle_manager.h"
#include "journal.h"
#include "core/backtester.h"
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

struct PairItem {
    std::string name;
    bool visible;
};

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
    std::vector<std::string> pair_names = Config::load_selected_pairs("config.json");
    if (pair_names.empty()) pair_names.push_back("BTCUSDT");
    std::vector<PairItem> pairs;
    for (const auto& name : pair_names) {
        pairs.push_back({name, true});
    }
    std::string active_pair = pairs.front().name;

    auto save_pairs = [&]() {
        std::vector<std::string> names;
        for (const auto& p : pairs) names.push_back(p.name);
        Config::save_selected_pairs("config.json", names);
    };
    std::vector<std::string> selected_pairs = Config::load_selected_pairs("config.json");
    if (selected_pairs.empty()) selected_pairs.push_back("BTCUSDT");
    std::string active_pair = selected_pairs[0];
    std::string active_interval = "1m";

    // Prepare candle storage by pair and interval
    const std::vector<std::string> intervals = {"1m", "5m", "15m", "1h", "4h", "1d"};
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
    std::map<std::string, std::vector<Candle>> all_candles;
    for (const auto& item : pairs) {
        const auto& pair = item.name;
    Journal::Journal journal;
    journal.load_json("journal.json");
    Core::BacktestResult last_result;

    std::map<std::string, std::map<std::string, std::vector<Candle>>> all_candles;
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
    std::map<std::string, std::vector<Candle>> all_candles;
    Journal::Journal journal;
    journal.load_json("journal.json");
    Core::BacktestResult last_result;
    for (const auto& pair : selected_pairs) {
        auto candles = CandleManager::load_candles(pair, active_interval);
        if (candles.empty()) {
            candles = DataFetcher::fetch_klines(pair, active_interval, 5000);
            if (!candles.empty()) {
                CandleManager::save_candles(pair, active_interval, candles);
            }
        }
        all_candles[pair][active_interval] = candles;
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
            for (const auto& item : pairs) {
                const auto& pair = item.name;
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
        static std::string load_error;
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
                std::none_of(pairs.begin(), pairs.end(),
                             [&](const PairItem& p) { return p.name == symbol; })) {
                pairs.push_back({symbol, true});
                save_pairs();
                std::find(selected_pairs.begin(), selected_pairs.end(), symbol) == selected_pairs.end()) {
                selected_pairs.push_back(symbol);
                Config::save_selected_pairs("config.json", selected_pairs);
                bool failed = false;
                for (const auto& interval : intervals) {
                    auto candles = CandleManager::load_candles(symbol, interval);
                    if (candles.empty()) {
                        candles = DataFetcher::fetch_klines(symbol, interval, 5000);
                        if (!candles.empty()) {
                            CandleManager::save_candles(symbol, interval, candles);
                        }
                    }
                    if (candles.empty()) {
                        failed = true;
                    } else {
                        all_candles[symbol][interval] = candles;
                    }
                }
                if (failed) {
                    load_error = "Failed to load " + symbol;
                } else {
                    load_error.clear();
                }
            }
            new_symbol[0] = '\0';
        }
        if (!load_error.empty()) {
            ImGui::Text("%s", load_error.c_str());
        }

        for (auto it = pairs.begin(); it != pairs.end();) {
            if (ImGui::Checkbox(it->name.c_str(), &it->visible)) {
                if (!it->visible && active_pair == it->name) {
                    auto new_active = std::find_if(
                        pairs.begin(), pairs.end(),
                        [](const PairItem& p) { return p.visible; });
                    active_pair = new_active != pairs.end() ? new_active->name : std::string();
                } else if (it->visible && active_pair.empty()) {
                    active_pair = it->name;
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("X##") + it->name).c_str())) {
                std::string removed = it->name;
                it = pairs.erase(it);
                all_candles.erase(removed);
                if (active_pair == removed) {
                    auto new_active = std::find_if(
                        pairs.begin(), pairs.end(),
                        [](const PairItem& p) { return p.visible; });
                    active_pair = new_active != pairs.end() ? new_active->name : std::string();
                }
                save_pairs();
            } else {
                ++it;
            }
        }

        ImGui::Separator();
        ImGui::Text("Active chart:");

        for (const auto& item : pairs) {
            if (!item.visible) continue;
            const auto& pair = item.name;
            if (ImGui::RadioButton(pair.c_str(), active_pair == pair)) {
                active_pair = pair;
                if (all_candles[pair][active_interval].empty()) {
                    auto candles = CandleManager::load_candles(pair, active_interval);
                    if (candles.empty()) {
                        candles = DataFetcher::fetch_klines(pair, active_interval, 5000);
                        if (!candles.empty()) {
                            CandleManager::save_candles(pair, active_interval, candles);
                        }
                        all_candles[pair][interval] = candles;
                    }
                    all_candles[pair][active_interval] = candles;
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Interval:");
        for (const auto& interval : intervals) {
            if (ImGui::RadioButton(interval.c_str(), active_interval == interval)) {
                active_interval = interval;
                if (all_candles[active_pair][interval].empty()) {
                    auto candles = CandleManager::load_candles(active_pair, interval);
                    if (candles.empty()) {
                        candles = DataFetcher::fetch_klines(active_pair, interval, 5000);
                        if (!candles.empty()) {
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

        // Journal Window
        ImGui::Begin("Journal");
        static char j_symbol[32] = "";
        static int j_side = 0;
        static double j_price = 0.0;
        static double j_qty = 0.0;
        ImGui::InputText("Symbol", j_symbol, IM_ARRAYSIZE(j_symbol));
        ImGui::Combo("Side", &j_side, "BUY\0SELL\0");
        ImGui::InputDouble("Price", &j_price, 0, 0, "%.2f");
        ImGui::InputDouble("Quantity", &j_qty, 0, 0, "%.4f");
        if (ImGui::Button("Add Trade")) {
            Journal::Entry e;
            e.symbol = j_symbol;
            e.side = j_side == 0 ? "BUY" : "SELL";
            e.price = j_price;
            e.quantity = j_qty;
            e.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            journal.add_entry(e);
            j_symbol[0] = '\0';
            j_price = 0.0;
            j_qty = 0.0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            journal.save_json("journal.json");
            journal.save_csv("journal.csv");
        }
        if (ImGui::BeginTable("JournalTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("Side");
            ImGui::TableSetupColumn("Price");
            ImGui::TableSetupColumn("Qty");
            ImGui::TableSetupColumn("Time");
            ImGui::TableHeadersRow();
            for (const auto& e : journal.entries()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", e.symbol.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", e.side.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", e.price);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", e.quantity);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%lld", (long long)e.timestamp);
            }
            ImGui::EndTable();
        }
        ImGui::End();

        // Analytics Window
        ImGui::Begin("Analytics");
        static int short_p = 9;
        static int long_p = 21;
        ImGui::InputInt("Short SMA", &short_p);
        ImGui::InputInt("Long SMA", &long_p);
        if (ImGui::Button("Run Backtest")) {
            struct Strat : Core::IStrategy {
                int s, l;
                Strat(int sp, int lp) : s(sp), l(lp) {}
                int generate_signal(const std::vector<Candle>& candles, size_t index) override {
                    return Signal::sma_crossover_signal(candles, index, s, l);
                }
            } strat(short_p, long_p);
            auto it = all_candles.find(active_pair);
            if (it != all_candles.end()) {
                Core::Backtester bt(it->second, strat);
                last_result = bt.run();
            }
        }
        if (!last_result.equity_curve.empty()) {
            ImGui::Text("Total PnL: %.2f", last_result.total_pnl);
            ImGui::Text("Win rate: %.2f%%", last_result.win_rate * 100.0);
            if (ImPlot::BeginPlot("Equity")) {
                std::vector<double> x(last_result.equity_curve.size());
                for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<double>(i);
                ImPlot::PlotLine("Equity", x.data(), last_result.equity_curve.data(), x.size());
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        ImGui::Begin("Chart");

        const auto& candles = all_candles[active_pair];
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
        if (ImPlot::BeginPlot(("Candles - " + active_pair + " (" + selected_interval + ")").c_str(), "Time", "Price")) {
            const auto& candles = all_candles[active_pair][selected_interval];
        if (!active_pair.empty() && ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), "Time", "Price")) {
            const auto& candles = all_candles[active_pair];
        if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), "Time", "Price")) {
            const auto& candles = all_candles[active_pair]["1m"];
        if (ImPlot::BeginPlot(("Candles - " + active_pair + " [" + active_interval + "]").c_str(), "Time", "Price")) {
            const auto& candles = all_candles[active_pair][active_interval];
            std::vector<double> times, opens, highs, lows, closes;
            for (const auto& c : candles) {
                times.push_back((double)c.open_time / 1000.0);
                opens.push_back(c.open);
                highs.push_back(c.high);
                lows.push_back(c.low);
                closes.push_back(c.close);
            }
            use_manual_limits = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Fit")) {
            ImPlot::FitNextPlotAxes();
            use_manual_limits = false;
        }

        if (use_manual_limits) {
            ImPlot::SetNextPlotLimits(manual_limits.X.Min, manual_limits.X.Max,
                                     manual_limits.Y.Min, manual_limits.Y.Max,
                                     ImGuiCond_Always);
        }

        ImPlotFlags plot_flags = ImPlotFlags_ContextMenu | ImPlotFlags_Crosshairs;
        if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), "Time", "Price", plot_flags)) {
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
            // Overlay journal entries
            std::vector<double> jb_times, jb_prices, js_times, js_prices;
            for (const auto& e : journal.entries()) {
                if (e.symbol == active_pair) {
                    double t = (double)e.timestamp / 1000.0;
                    if (e.side == "BUY") { jb_times.push_back(t); jb_prices.push_back(e.price); }
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
            ImPlot::ShowAltLegend();
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
