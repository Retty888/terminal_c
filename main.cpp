#include "imgui.h"
#include "implot.h"
#include "core/candle.h"
#include "core/data_fetcher.h"
#include "config.h"
#include "core/candle_manager.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
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
    std::map<std::string, std::vector<Candle>> all_candles;
    for (const auto& pair : selected_pairs) {
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
                        if (interval == "1m") {
                            all_candles[pair] = candles;
                        }
                    }
                }
            }
        }

        ImGui::End();

        ImGui::Begin("Chart");
        if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), "Time", "Price")) {
            const auto& candles = all_candles[active_pair];
            std::vector<double> times, opens, highs, lows, closes;
            for (const auto& c : candles) {
                times.push_back((double)c.open_time / 1000.0);
                opens.push_back(c.open);
                highs.push_back(c.high);
                lows.push_back(c.low);
                closes.push_back(c.close);
            }

            ImPlot::PlotCandlestick(
                "Candles",
                times.data(), opens.data(), highs.data(), lows.data(), closes.data(),
                (int)candles.size(),
                ImPlotCandlestickFlags_None,
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
