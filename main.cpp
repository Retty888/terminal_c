#include "config.h"
#include "core/backtester.h"
#include "core/candle.h"
#include "core/candle_manager.h"
#include "core/data_fetcher.h"
#include "imgui.h"
#include "implot.h"
#include "journal.h"
#include "plot/candlestick.h"
#include "signal.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "ui/analytics_window.h"
#include "ui/chart_window.h"
#include "ui/control_panel.h"
#include "ui/journal_window.h"
#include "ui/signals_window.h"

using namespace Core;

int main() {
  // Init GLFW
  if (!glfwInit())
    return -1;

  // Create window with OpenGL context
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "Trading Terminal", NULL, NULL);
  if (!window)
    return -1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  // Load config
  std::vector<std::string> pair_names =
      Config::load_selected_pairs("config.json");
  if (pair_names.empty())
    pair_names.push_back("BTCUSDT");
  std::vector<PairItem> pairs;
  for (const auto &name : pair_names) {
    pairs.push_back({name, true});
  }

  auto save_pairs = [&]() {
    std::vector<std::string> names;
    for (const auto &p : pairs)
      names.push_back(p.name);
    Config::save_selected_pairs("config.json", names);
  };

  std::vector<std::string> selected_pairs = pair_names;
  std::string active_pair = selected_pairs[0];
  std::string active_interval = "1m";

  // Prepare candle storage by pair and interval
  const std::vector<std::string> intervals = {"1m", "5m", "15m",
                                              "1h", "4h", "1d"};
  std::string selected_interval = "1m";
  int short_period = 9;
  int long_period = 21;
  bool show_on_chart = false;
  std::vector<SignalEntry> signal_entries;
  std::vector<double> buy_times, buy_prices, sell_times, sell_prices;

  std::map<std::string, std::map<std::string, std::vector<Candle>>> all_candles;
  std::mutex candles_mutex; // protects access to all_candles
  std::map<std::string, std::future<std::optional<std::vector<Candle>>>>
      pending_fetches;
  Journal::Journal journal;
  journal.load_json("journal.json");
  Core::BacktestResult last_result;

  for (const auto &pair : selected_pairs) {
    for (const auto &interval : intervals) {
      auto candles = CandleManager::load_candles(pair, interval);
      if (candles.empty()) {
        auto fetched = DataFetcher::fetch_klines(pair, interval, 5000);
        if (fetched && !fetched->empty()) {
          candles = *fetched;
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
      for (const auto &item : pairs) {
        const auto &pair = item.name;
        if (pending_fetches.find(pair) == pending_fetches.end()) {
          pending_fetches[pair] =
              DataFetcher::fetch_klines_async(pair, "1m", 1);
        }
      }
      last_fetch = now;
    }
    for (auto it = pending_fetches.begin(); it != pending_fetches.end();) {
      if (it->second.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        auto latest = it->second.get();
        if (latest && !latest->empty()) {
          std::lock_guard<std::mutex> lock(candles_mutex);
          auto &vec = all_candles[it->first]["1m"];
          if (vec.empty() || latest->back().open_time > vec.back().open_time) {
            vec.push_back(latest->back());
            CandleManager::append_candles(it->first, "1m", {latest->back()});
          }
        }
        it = pending_fetches.erase(it);
      } else {
        ++it;
      }
    }

        DrawControlPanel(pairs, selected_pairs, active_pair, active_interval, intervals, selected_interval, all_candles, save_pairs);

        DrawSignalsWindow(short_period, long_period, show_on_chart, signal_entries, buy_times, buy_prices, sell_times, sell_prices, all_candles, active_pair, selected_interval);

        DrawAnalyticsWindow(all_candles, active_pair, selected_interval);

        DrawJournalWindow(journal);

        // Backtest Window
        ImGui::Begin("Backtest");
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
                auto jt = it->second.find(active_interval);
                if (jt != it->second.end()) {
                    Core::Backtester bt(jt->second, strat);
                    last_result = bt.run();
                }
            }
    DrawControlPanel(pairs, selected_pairs, active_pair, active_interval,
                     intervals, selected_interval, all_candles, save_pairs);

    DrawSignalsWindow(short_period, long_period, show_on_chart, signal_entries,
                      buy_times, buy_prices, sell_times, sell_prices,
                      all_candles, active_pair, selected_interval);

    DrawAnalyticsWindow(all_candles, active_pair, selected_interval);

    DrawJournalWindow(journal);

    // Backtest Window
    ImGui::Begin("Backtest");
    static int short_p = 9;
    static int long_p = 21;
    ImGui::InputInt("Short SMA", &short_p);
    ImGui::InputInt("Long SMA", &long_p);
    if (ImGui::Button("Run Backtest")) {
      struct Strat : Core::IStrategy {
        int s, l;
        Strat(int sp, int lp) : s(sp), l(lp) {}
        int generate_signal(const std::vector<Candle> &candles,
                            size_t index) override {
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
        for (size_t i = 0; i < x.size(); ++i)
          x[i] = static_cast<double>(i);
        ImPlot::PlotLine("Equity", x.data(), last_result.equity_curve.data(),
                         x.size());
        ImPlot::EndPlot();
      }
    }
    ImGui::End();

    DrawChartWindow(all_candles, active_pair, active_interval, show_on_chart,
                    buy_times, buy_prices, sell_times, sell_prices, journal,
                    last_result);

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
