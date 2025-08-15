#include "app.h"

#include "config.h"
#include "core/backtester.h"
#include "core/candle.h"
#include "imgui.h"
#include "implot.h"
#include "logger.h"
#include "plot/candlestick.h"
#include "signal.h"

#include <algorithm>
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

int App::run() {
  // Init GLFW
  auto level = Config::load_min_log_level("config.json");
  Logger::instance().set_min_level(level);
  Logger::instance().enable_console_output(true);
  Logger::instance().set_file("terminal.log");
  Logger::instance().info("Application started");

  if (!glfwInit()) {
    Logger::instance().error("Failed to initialize GLFW");
    return -1;
  }

  // Create window with OpenGL context
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "Trading Terminal", NULL, NULL);
  if (!window) {
    Logger::instance().error("Failed to create GLFW window");
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  // Load config
  std::vector<std::string> pair_names =
      data_service_.load_selected_pairs("config.json");
  if (pair_names.empty())
    pair_names.push_back("BTCUSDT");
  int candles_limit = Config::load_candles_limit("config.json");
  std::vector<PairItem> pairs;
  for (const auto &name : pair_names) {
    pairs.push_back({name, true});
  }

  auto save_pairs = [&]() {
    std::vector<std::string> names;
    for (const auto &p : pairs)
      names.push_back(p.name);
    data_service_.save_selected_pairs("config.json", names);
  };

  std::vector<std::string> selected_pairs = pair_names;
  std::string active_pair = selected_pairs[0];
  std::string active_interval = "1m";

  auto exchange_pairs_res = data_service_.fetch_all_symbols();
  std::vector<std::string> exchange_pairs =
      exchange_pairs_res.error == FetchError::None
          ? exchange_pairs_res.symbols
          : std::vector<std::string>{};

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
  std::map<std::string, std::future<Core::KlinesResult>> pending_fetches;
  journal_service_.load("journal.json");
  Core::BacktestResult last_result;

  struct FetchTask {
    std::string pair;
    std::string interval;
    std::future<Core::KlinesResult> future;
  };
  std::vector<FetchTask> initial_fetches;
  const int total_initial_fetches = selected_pairs.size() * intervals.size();
  int completed_initial_fetches = 0;

  for (const auto &pair : selected_pairs) {
    for (const auto &interval : intervals) {
      auto candles = data_service_.load_candles(pair, interval);
      if (candles.empty()) {
        all_candles[pair][interval] = {};
        initial_fetches.push_back({
            pair, interval,
            data_service_.fetch_klines_async(pair, interval, candles_limit)});
      } else {
        all_candles[pair][interval] = candles;
        ++completed_initial_fetches;
      }
    }
  }

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(ImGui::GetID("MainDock"),
                                 ImGui::GetMainViewport());

    static auto last_fetch = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - last_fetch >= std::chrono::minutes(1)) {
      for (const auto &item : pairs) {
        const auto &pair = item.name;
        if (pending_fetches.find(pair) == pending_fetches.end()) {
          pending_fetches[pair] =
              data_service_.fetch_klines_async(pair, "1m", 1);
        }
      }
      last_fetch = now;
    }
    for (auto it = pending_fetches.begin(); it != pending_fetches.end();) {
      if (it->second.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        auto latest = it->second.get();
        if (latest.error == FetchError::None && !latest.candles.empty()) {
          std::lock_guard<std::mutex> lock(candles_mutex);
          auto &vec = all_candles[it->first]["1m"];
          if (vec.empty() ||
              latest.candles.back().open_time > vec.back().open_time) {
            vec.push_back(latest.candles.back());
            data_service_.append_candles(it->first, "1m",
                                         {latest.candles.back()});
          }
        }
        it = pending_fetches.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = initial_fetches.begin(); it != initial_fetches.end();) {
      if (it->future.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        auto fetched = it->future.get();
        if (fetched.error == FetchError::None && !fetched.candles.empty()) {
          std::lock_guard<std::mutex> lock(candles_mutex);
          all_candles[it->pair][it->interval] = fetched.candles;
          data_service_.save_candles(it->pair, it->interval, fetched.candles);
        }
        ++completed_initial_fetches;
        it = initial_fetches.erase(it);
      } else {
        ++it;
      }
    }

    if (completed_initial_fetches < total_initial_fetches) {
      ImGui::Begin("Loading data", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
      float progress = static_cast<float>(completed_initial_fetches) /
                       static_cast<float>(total_initial_fetches);
      ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
      ImGui::Text("%d / %d", completed_initial_fetches, total_initial_fetches);
      ImGui::End();
    }

    DrawControlPanel(pairs, selected_pairs, active_pair, active_interval,
                     intervals, selected_interval, all_candles, save_pairs,
                     exchange_pairs);

    DrawSignalsWindow(short_period, long_period, show_on_chart, signal_entries,
                      buy_times, buy_prices, sell_times, sell_prices,
                      all_candles, active_pair, selected_interval);

    DrawAnalyticsWindow(all_candles, active_pair, selected_interval);

    DrawJournalWindow(journal_service_.journal());

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
      auto pair_it = all_candles.find(active_pair);
      if (pair_it != all_candles.end()) {
        auto interval_it = pair_it->second.find(active_interval);
        if (interval_it != pair_it->second.end()) {
          Core::Backtester bt(interval_it->second, strat);
          last_result = bt.run();
        }
      }
    }
    if (!last_result.equity_curve.empty()) {
      ImGui::Text("Total PnL: %.2f", last_result.total_pnl);
      ImGui::Text("Win rate: %.2f%%", last_result.win_rate * 100.0);
      ImGui::Text("Max Drawdown: %.2f", last_result.max_drawdown);
      ImGui::Text("Sharpe Ratio: %.2f", last_result.sharpe_ratio);
      ImGui::Text("Average Win: %.2f", last_result.avg_win);
      ImGui::Text("Average Loss: %.2f", last_result.avg_loss);
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

    DrawChartWindow(all_candles, active_pair, active_interval, selected_pairs,
                    intervals, show_on_chart, buy_times, buy_prices, sell_times,
                    sell_prices, journal_service_.journal(), last_result);

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
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

  Logger::instance().info("Application exiting");

  return 0;
}

