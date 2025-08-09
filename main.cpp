#include "imgui.h"
#include "implot.h"
#include "plot/candlestick.h"
#include "core/candle.h"
#include "core/data_fetcher.h"
#include "config.h"
#include "core/candle_manager.h"

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

    // Load candles for several intervals
    const std::vector<std::string> intervals = {"1m", "5m", "15m", "1h", "4h", "1d"};
    std::map<std::string, std::vector<Candle>> all_candles;
    for (const auto& item : pairs) {
        const auto& pair = item.name;
        for (const auto& interval : intervals) {
            auto candles = CandleManager::load_candles(pair, interval);
            if (candles.empty()) {
                candles = DataFetcher::fetch_klines(pair, interval, 5000);
                if (!candles.empty()) {
                    CandleManager::save_candles(pair, interval, candles);
                }
            }
            // keep 1m candles in memory for charting
            if (interval == "1m") {
                all_candles[pair] = candles;
            }
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
            for (const auto& item : pairs) {
                const auto& pair = item.name;
                auto latest = DataFetcher::fetch_klines(pair, "1m", 1);
                if (!latest.empty()) {
                    auto& vec = all_candles[pair];
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
                std::none_of(pairs.begin(), pairs.end(),
                             [&](const PairItem& p) { return p.name == symbol; })) {
                pairs.push_back({symbol, true});
                save_pairs();
            }
            new_symbol[0] = '\0';
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
                if (all_candles.find(pair) == all_candles.end()) {
                    for (const auto& interval : intervals) {
                        auto candles = CandleManager::load_candles(pair, interval);
                        if (candles.empty()) {
                            candles = DataFetcher::fetch_klines(pair, interval, 5000);
                            if (!candles.empty()) {
                                CandleManager::save_candles(pair, interval, candles);
                            }
                        }
                        if (interval == "1m") {
                            all_candles[pair] = candles;
                        }
                    }
                }
            }
        }

        ImGui::End();

        ImGui::Begin("Chart");
        if (!active_pair.empty() && ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), "Time", "Price")) {
            const auto& candles = all_candles[active_pair];
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
