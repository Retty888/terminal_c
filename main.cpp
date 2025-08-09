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
    std::string active_interval = "1m";

    // Prepare candle storage by pair and interval
    const std::vector<std::string> intervals = {"1m", "5m", "15m", "1h", "4h", "1d"};
    std::map<std::string, std::map<std::string, std::vector<Candle>>> all_candles;
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
                if (all_candles[pair][active_interval].empty()) {
                    auto candles = CandleManager::load_candles(pair, active_interval);
                    if (candles.empty()) {
                        candles = DataFetcher::fetch_klines(pair, active_interval, 5000);
                        if (!candles.empty()) {
                            CandleManager::save_candles(pair, active_interval, candles);
                        }
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

        ImGui::End();

        ImGui::Begin("Chart");
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
