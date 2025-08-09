#include "imgui.h"
#include "implot.h"
#include "core/candle.h"
#include "core/data_fetcher.h"
#include "config.h"
#include "candle_manager.h"

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <atomic>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

using namespace Core;

std::vector<std::string> available_pairs = {
    "BTCUSDT", "ETHUSDT", "SOLUSDT", "AVAXUSDT", "TONUSDT"
};

constexpr int FETCH_INTERVAL_SECONDS = 60;

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
    std::mutex data_mutex;
    std::atomic<bool> fetch_running{true};

    // Load candles
    std::map<std::string, std::vector<Candle>> all_candles;
    for (const auto& pair : selected_pairs) {
        auto candles = CandleManager::load_candles(pair, "1m");
        if (candles.empty()) {
            candles = DataFetcher::fetch_klines(pair, "1m", 200);
            if (!candles.empty()) {
                CandleManager::save_candles(pair, "1m", candles);
            }
        }
        all_candles[pair] = candles;
    }

    std::thread fetch_thread([&]() {
        while (fetch_running) {
            std::this_thread::sleep_for(std::chrono::seconds(FETCH_INTERVAL_SECONDS));
            std::vector<std::string> pairs;
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                pairs = selected_pairs;
            }
            for (const auto& pair : pairs) {
                auto new_data = DataFetcher::fetch_klines(pair, "1m", 1);
                if (!new_data.empty()) {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    auto& vec = all_candles[pair];
                    long long last_time = vec.empty() ? 0 : vec.back().open_time;
                    const Candle& nc = new_data.back();
                    if (nc.open_time > last_time) {
                        vec.push_back(nc);
                        CandleManager::save_candles(pair, "1m", vec);
                        glfwPostEmptyEvent();
                    }
                }
            }
        }
    });

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Control Panel");

        std::vector<std::string> selected_copy;
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            selected_copy = selected_pairs;
        }

        ImGui::Text("Select pairs to load:");
        for (const auto& pair : available_pairs) {
            bool selected = std::find(selected_copy.begin(), selected_copy.end(), pair) != selected_copy.end();
            if (ImGui::Checkbox(pair.c_str(), &selected)) {
                std::lock_guard<std::mutex> lock(data_mutex);
                if (selected)
                    selected_pairs.push_back(pair);
                else
                    selected_pairs.erase(std::remove(selected_pairs.begin(), selected_pairs.end(), pair), selected_pairs.end());
                Config::save_selected_pairs("config.json", selected_pairs);
            }
        }

        ImGui::Separator();
        ImGui::Text("Active chart:");

        for (const auto& pair : selected_copy) {
            if (ImGui::RadioButton(pair.c_str(), active_pair == pair)) {
                active_pair = pair;
                bool need_load = false;
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    need_load = all_candles.find(pair) == all_candles.end();
                }
                if (need_load) {
                    auto candles = CandleManager::load_candles(pair, "1m");
                    if (candles.empty()) {
                        candles = DataFetcher::fetch_klines(pair, "1m", 200);
                        if (!candles.empty()) {
                            CandleManager::save_candles(pair, "1m", candles);
                        }
                    }
                    std::lock_guard<std::mutex> lock(data_mutex);
                    all_candles[pair] = candles;
                }
            }
        }

        ImGui::End();

        ImGui::Begin("Chart");
        if (ImPlot::BeginPlot(("Candles - " + active_pair).c_str(), "Time", "Price")) {
            std::vector<Candle> candles;
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                candles = all_candles[active_pair];
            }
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

    fetch_running = false;
    fetch_thread.join();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
