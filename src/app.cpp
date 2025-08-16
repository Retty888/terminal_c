#include "app.h"

#include "config.h"
#include "core/backtester.h"
#include "core/candle.h"
#include "core/candle_manager.h"
#include "imgui.h"
#include "imgui_internal.h"
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
#include <set>
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
#include "ui/tradingview_style.h"
#include "services/signal_bot.h"

using namespace Core;

namespace {
std::chrono::milliseconds interval_to_duration(const std::string &interval) {
  if (interval.empty())
    return std::chrono::milliseconds(0);
  char unit = interval.back();
  long long value = 0;
  try {
    value = std::stoll(interval.substr(0, interval.size() - 1));
  } catch (...) {
    return std::chrono::milliseconds(0);
  }
  switch (unit) {
  case 's':
    return std::chrono::milliseconds(value * 1000LL);
  case 'm':
    return std::chrono::milliseconds(value * 60LL * 1000LL);
  case 'h':
    return std::chrono::milliseconds(value * 60LL * 60LL * 1000LL);
  case 'd':
    return std::chrono::milliseconds(value * 24LL * 60LL * 60LL * 1000LL);
  case 'w':
    return std::chrono::milliseconds(value * 7LL * 24LL * 60LL * 60LL * 1000LL);
  default:
    return std::chrono::milliseconds(0);
  }
}
} // namespace

void App::add_status(const std::string &msg) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  status_.log.push_back(msg);
  if (status_.log.size() > 50)
    status_.log.erase(status_.log.begin());
}

int App::run() {
  // Init GLFW
  auto level = Config::load_min_log_level("config.json");
  Logger::instance().set_min_level(level);
  Logger::instance().enable_console_output(true);
  Logger::instance().set_file("terminal.log");
  Logger::instance().info("Application started");
  status_ = AppStatus{};

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
  ApplyTradingViewStyle();

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  // Load config
  std::vector<std::string> pair_names =
      data_service_.load_selected_pairs("config.json");
  std::vector<std::string> intervals = {"1m", "3m", "5m", "15m", "1h", "4h", "1d", "15s", "5s"};
  auto exchange_interval_res = data_service_.fetch_intervals();
  if (exchange_interval_res.error == FetchError::None) {
    intervals.insert(intervals.end(),
                     exchange_interval_res.intervals.begin(),
                     exchange_interval_res.intervals.end());
  }
  if (pair_names.empty()) {
    auto stored = CandleManager::list_stored_data();
    std::set<std::string> pairs_found;
    std::set<std::string> intervals_found;
    for (const auto &entry : stored) {
      auto lp = entry.rfind(" (");
      auto rp = entry.rfind(')');
      if (lp != std::string::npos && rp != std::string::npos && lp < rp) {
        pairs_found.insert(entry.substr(0, lp));
        intervals_found.insert(entry.substr(lp + 2, rp - lp - 2));
      }
    }
    pair_names.assign(pairs_found.begin(), pairs_found.end());
    intervals.insert(intervals.end(), intervals_found.begin(),
                     intervals_found.end());
  }
  if (pair_names.empty())
    pair_names.push_back("BTCUSDT");
  std::sort(intervals.begin(), intervals.end(), [](const std::string &a,
                                                   const std::string &b) {
    return interval_to_duration(a) < interval_to_duration(b);
  });
  intervals.erase(std::unique(intervals.begin(), intervals.end()),
                  intervals.end());
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
  std::string active_interval = intervals.empty() ? "1m" : intervals[0];

  auto exchange_pairs_res = data_service_.fetch_all_symbols();
  std::vector<std::string> exchange_pairs =
      exchange_pairs_res.error == FetchError::None ? exchange_pairs_res.symbols
                                                   : std::vector<std::string>{};

  // Prepare candle storage by pair and interval
  std::string selected_interval = intervals.empty() ? "1m" : intervals[0];
  std::string strategy = "sma_crossover";
  int short_period = 9;
  int long_period = 21;
  double oversold = 30.0;
  double overbought = 70.0;
  bool show_on_chart = false;
  std::vector<SignalEntry> signal_entries;
  std::vector<double> buy_times, buy_prices, sell_times, sell_prices;

  std::map<std::string, std::map<std::string, std::vector<Candle>>> all_candles;
  std::mutex candles_mutex; // protects access to all_candles
  struct PendingFetch {
    std::string interval;
    std::future<Core::KlinesResult> future;
  };
  std::map<std::string, PendingFetch> pending_fetches;
  journal_service_.load("journal.json");
  Core::BacktestResult last_result;
  Config::SignalConfig last_signal_cfg;

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
      all_candles[pair][interval] = candles;
      int missing = candles_limit - static_cast<int>(candles.size());
      if (missing > 0) {
        initial_fetches.push_back({
            pair, interval,
            data_service_.fetch_klines_async(pair, interval, missing)});
        add_status("Fetching " + pair + " " + interval);
      } else {
        ++completed_initial_fetches;
        status_.candle_progress =
            static_cast<float>(completed_initial_fetches) /
            static_cast<float>(total_initial_fetches);
      }
    }
  }

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiID dockspace_id = ImGui::GetID("MainDock");
    static bool dock_init = false;
    if (!dock_init) {
      dock_init = true;
      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

      ImGuiID dock_top, dock_bottom;
      ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.3f, &dock_top,
                                  &dock_bottom);
      ImGuiID dock_top_left, dock_top_right;
      ImGui::DockBuilderSplitNode(dock_top, ImGuiDir_Left, 0.5f, &dock_top_left,
                                  &dock_top_right);

      ImGui::DockBuilderDockWindow("Chart", dock_bottom);
      ImGui::DockBuilderDockWindow("Control Panel", dock_top_left);
      ImGui::DockBuilderDockWindow("Backtest", dock_top_right);
      ImGui::DockBuilderDockWindow("Signals", dock_top_right);
      ImGui::DockBuilderDockWindow("Journal", dock_top_right);
      ImGui::DockBuilderDockWindow("Analytics", dock_top_right);

      ImGui::DockBuilderFinish(dockspace_id);
    }
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());

    static auto last_fetch = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto period = interval_to_duration(active_interval);
    if (period.count() > 0 && now - last_fetch >= period) {
      for (const auto &item : pairs) {
        const auto &pair = item.name;
        if (pending_fetches.find(pair) == pending_fetches.end()) {
          pending_fetches[pair] = {active_interval,
                                  data_service_.fetch_klines_async(pair, active_interval, 1)};
          add_status("Updating " + pair);
        }
      }
      last_fetch = now;
    }
    for (auto it = pending_fetches.begin(); it != pending_fetches.end();) {
      if (it->second.future.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        auto latest = it->second.future.get();
        if (latest.error == FetchError::None && !latest.candles.empty()) {
          std::lock_guard<std::mutex> lock(candles_mutex);
          auto &vec = all_candles[it->first][it->second.interval];
          if (vec.empty() ||
              latest.candles.back().open_time > vec.back().open_time) {
            vec.push_back(latest.candles.back());
            data_service_.append_candles(it->first, it->second.interval,
                                         {latest.candles.back()});
          }
          add_status("Updated " + it->first);
        } else {
          status_.error_message = "Update failed for " + it->first;
          add_status(status_.error_message);
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
          auto &vec = all_candles[it->pair][it->interval];
          bool had_candles = !vec.empty();
          long long last_time = had_candles ? vec.back().open_time : 0;
          std::vector<Candle> new_candles;
          for (const auto &c : fetched.candles) {
            if (c.open_time > last_time) {
              vec.push_back(c);
              new_candles.push_back(c);
            }
          }
          if (!new_candles.empty()) {
            if (had_candles)
              data_service_.append_candles(it->pair, it->interval, new_candles);
            else
              data_service_.save_candles(it->pair, it->interval, vec);
          }
          add_status("Loaded " + it->pair + " " + it->interval);
        } else {
          status_.error_message =
              "Failed to fetch " + it->pair + " " + it->interval;
          add_status(status_.error_message);
        }
        ++completed_initial_fetches;
        status_.candle_progress = static_cast<float>(completed_initial_fetches) /
                                  static_cast<float>(total_initial_fetches);
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

    DrawControlPanel(pairs, selected_pairs, active_pair, intervals,
                     selected_interval, all_candles, save_pairs,
                     exchange_pairs, status_);

    DrawSignalsWindow(strategy, short_period, long_period, oversold, overbought,
                      show_on_chart, signal_entries, buy_times, buy_prices,
                      sell_times, sell_prices, all_candles, active_pair,
                      selected_interval, status_);

    DrawAnalyticsWindow(all_candles, active_pair, selected_interval);

    DrawJournalWindow(journal_service_.journal());

    // Backtest Window
    ImGui::Begin("Backtest");
    ImGui::Text("Strategy: %s", strategy.c_str());
    if (strategy == "sma_crossover") {
      ImGui::Text("Short SMA: %d", short_period);
      ImGui::Text("Long SMA: %d", long_period);
    } else if (strategy == "ema") {
      ImGui::Text("EMA Period: %d", short_period);
    } else if (strategy == "rsi") {
      ImGui::Text("RSI Period: %d", short_period);
      ImGui::Text("Oversold: %.2f", oversold);
      ImGui::Text("Overbought: %.2f", overbought);
    }
    if (ImGui::Button("Run Backtest")) {
      status_.analysis_message = "Running backtest";
      add_status("Backtest started");
      Config::SignalConfig cfg;
      cfg.type = strategy;
      cfg.short_period = static_cast<std::size_t>(short_period);
      cfg.long_period = static_cast<std::size_t>(long_period);
      if (strategy == "rsi") {
        cfg.params["oversold"] = oversold;
        cfg.params["overbought"] = overbought;
      }
      SignalBot bot(cfg);
      auto pair_it = all_candles.find(active_pair);
      if (pair_it != all_candles.end()) {
        auto interval_it = pair_it->second.find(active_interval);
        if (interval_it != pair_it->second.end()) {
          Core::Backtester bt(interval_it->second, bot);
          last_result = bt.run();
          last_signal_cfg = cfg;
        }
      }
      status_.analysis_message = "Backtest done";
      add_status("Backtest finished");
    }
    if (!last_result.equity_curve.empty()) {
      ImGui::Text("Strategy: %s", last_signal_cfg.type.c_str());
      if (last_signal_cfg.type == "sma_crossover") {
        ImGui::Text("Short SMA: %zu", last_signal_cfg.short_period);
        ImGui::Text("Long SMA: %zu", last_signal_cfg.long_period);
      } else if (last_signal_cfg.type == "ema") {
        ImGui::Text("EMA Period: %zu", last_signal_cfg.short_period);
      } else if (last_signal_cfg.type == "rsi") {
        ImGui::Text("RSI Period: %zu", last_signal_cfg.short_period);
        auto it_os = last_signal_cfg.params.find("oversold");
        if (it_os != last_signal_cfg.params.end())
          ImGui::Text("Oversold: %.2f", it_os->second);
        auto it_ob = last_signal_cfg.params.find("overbought");
        if (it_ob != last_signal_cfg.params.end())
          ImGui::Text("Overbought: %.2f", it_ob->second);
      }
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
