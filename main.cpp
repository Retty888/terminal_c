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

        static auto last_fetch = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last_fetch >= std::chrono::minutes(1)) {
            for (const auto& pair : selected_pairs) {
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
