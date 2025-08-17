#include "app.h"

#include "config_manager.h"
#include "core/backtester.h"
#include "core/candle.h"
#include "core/candle_manager.h"
#include "core/kline_stream.h"
#include "core/interval_utils.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "logger.h"
#include "plot/candlestick.h"
#include "signal.h"

#include <algorithm>
#include <chrono>
#include <atomic>
#include <future>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <functional>

#include <GLFW/glfw3.h>

#include "services/signal_bot.h"
#include "ui/analytics_window.h"
#include "ui/chart_window.h"
#include "ui/control_panel.h"
#include "ui/journal_window.h"
#include "ui/signals_window.h"

using namespace Core;

namespace {

struct AppContext {
  std::vector<PairItem> pairs;
  std::vector<std::string> selected_pairs;
  std::string active_pair;
  std::string active_interval;
  std::vector<std::string> intervals;
  std::vector<std::string> exchange_pairs;
  std::string selected_interval;
  std::string strategy = "sma_crossover";
  int short_period = 9;
  int long_period = 21;
  double oversold = 30.0;
  double overbought = 70.0;
  bool show_on_chart = false;
  std::vector<SignalEntry> signal_entries;
  std::vector<double> buy_times, buy_prices, sell_times, sell_prices;
  std::map<std::string, std::map<std::string, std::vector<Candle>>> all_candles;
  std::mutex candles_mutex;
  std::map<std::string, std::unique_ptr<KlineStream>> streams;
  std::atomic<bool> stream_failed{false};
  struct PendingFetch {
    std::string interval;
    std::future<Core::KlinesResult> future;
  };
  std::map<std::string, PendingFetch> pending_fetches;
  Core::BacktestResult last_result;
  Config::SignalConfig last_signal_cfg;
  struct FetchTask {
    std::string pair;
    std::string interval;
    std::future<Core::KlinesResult> future;
    std::chrono::steady_clock::time_point start;
  };
  std::deque<FetchTask> fetch_queue;
  std::size_t total_fetches = 0;
  std::size_t completed_fetches = 0;
  std::atomic<long long> next_fetch_time{0};
  int candles_limit = 0;
  bool streaming_enabled = false;
  std::function<void()> save_pairs;
  std::function<void(const std::string &)> cancel_pair;
  std::string last_active_pair;
  std::string last_active_interval;
  const std::chrono::seconds fetch_backoff{5};
  const std::chrono::seconds request_timeout{10};
};

AppContext ctx;
} // namespace

void App::add_status(const std::string &msg) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  status_.log.push_back(msg);
  if (status_.log.size() > 50)
    status_.log.erase(status_.log.begin());
}

bool App::init_window() {
  auto cfg = Config::ConfigManager::load("config.json");
  auto level = cfg ? cfg->log_level : LogLevel::Info;
  Logger::instance().set_min_level(level);
  Logger::instance().enable_console_output(true);
  Logger::instance().set_file("terminal.log");
  Logger::instance().info("Application started");
  status_ = AppStatus{};

  if (!glfwInit()) {
    Logger::instance().error("Failed to initialize GLFW");
    return false;
  }
  window_ = glfwCreateWindow(1280, 720, "Trading Terminal", NULL, NULL);
  if (!window_) {
    Logger::instance().error("Failed to create GLFW window");
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);
  return true;
}

void App::setup_imgui() { ui_manager_.setup(window_); }

void App::load_config() {
  auto cfg = Config::ConfigManager::load("config.json");
  std::vector<std::string> pair_names;
  if (cfg) {
    pair_names = cfg->pairs;
    ctx.candles_limit = static_cast<int>(cfg->candles_limit);
    ctx.streaming_enabled = cfg->enable_streaming;
  } else {
    Logger::instance().warn("Using default configuration");
    ctx.candles_limit = 5000;
    ctx.streaming_enabled = false;
  }
  ctx.intervals = {"1m", "3m", "5m",  "15m", "1h",
                   "4h", "1d", "15s", "5s"};
  auto exchange_interval_res = data_service_.fetch_intervals();
  if (exchange_interval_res.error == FetchError::None) {
    ctx.intervals.insert(ctx.intervals.end(),
                         exchange_interval_res.intervals.begin(),
                         exchange_interval_res.intervals.end());
  }
  if (pair_names.empty()) {
    auto stored = data_service_.list_stored_data();
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
    ctx.intervals.insert(ctx.intervals.end(), intervals_found.begin(),
                         intervals_found.end());
  }
  if (pair_names.empty())
    pair_names.push_back("BTCUSDT");
  std::sort(ctx.intervals.begin(), ctx.intervals.end(),
            [](const std::string &a, const std::string &b) {
              return parse_interval(a) < parse_interval(b);
            });
  ctx.intervals.erase(
      std::unique(ctx.intervals.begin(), ctx.intervals.end()),
      ctx.intervals.end());
  for (const auto &name : pair_names) {
    ctx.pairs.push_back({name, true});
  }
  ctx.save_pairs = [&]() {
    std::vector<std::string> names;
    for (const auto &p : ctx.pairs)
      names.push_back(p.name);
    Config::ConfigManager::save_selected_pairs("config.json", names);
  };
  ctx.selected_pairs = pair_names;
  ctx.active_pair = ctx.selected_pairs[0];
  ctx.active_interval = ctx.intervals.empty() ? "1m" : ctx.intervals[0];

  auto exchange_pairs_res = data_service_.fetch_all_symbols();
  ctx.exchange_pairs = exchange_pairs_res.error == FetchError::None
                            ? exchange_pairs_res.symbols
                            : std::vector<std::string>{};

  ctx.selected_interval = ctx.intervals.empty() ? "1m" : ctx.intervals[0];
  journal_service_.load("journal.json");

  for (const auto &pair : ctx.selected_pairs) {
    for (const auto &interval : ctx.intervals) {
      ctx.all_candles[pair][interval] =
          data_service_.load_candles(pair, interval);
    }
  }
  auto &initial = ctx.all_candles[ctx.active_pair][ctx.active_interval];
  int missing = ctx.candles_limit - static_cast<int>(initial.size());
  if (missing > 0) {
    ctx.fetch_queue.push_back({ctx.active_pair, ctx.active_interval,
                               data_service_.fetch_klines_async(ctx.active_pair,
                                                                ctx.active_interval,
                                                                missing),
                               std::chrono::steady_clock::now()});
    ctx.total_fetches = 1;
    add_status("Fetching " + ctx.active_pair + " " + ctx.active_interval);
  } else {
    status_.candle_progress = 1.0f;
  }
  ctx.next_fetch_time.store(0);
  if (ctx.streaming_enabled && ctx.active_interval != "5s" &&
      ctx.active_interval != "15s") {
    for (const auto &p : ctx.pairs) {
      std::string pair = p.name;
      auto stream =
          std::make_unique<KlineStream>(pair, ctx.active_interval, data_service_.candle_manager());
      stream->start(
          [pair](const Candle &c) {
            std::lock_guard<std::mutex> lock(ctx.candles_mutex);
            auto &vec = ctx.all_candles[pair][ctx.active_interval];
            if (vec.empty() || c.open_time > vec.back().open_time)
              vec.push_back(c);
          },
          [this, pair]() {
            ctx.stream_failed = true;
            ctx.next_fetch_time.store(0);
            add_status("Stream failed for " + pair + ", switching to HTTP");
          });
      ctx.streams[pair] = std::move(stream);
    }
  } else {
    ctx.streaming_enabled = false;
  }
  ctx.last_active_pair = ctx.active_pair;
  ctx.last_active_interval = ctx.active_interval;
  ctx.cancel_pair = [&](const std::string &pair) {
    ctx.pending_fetches.erase(pair);
    ctx.fetch_queue.erase(
        std::remove_if(ctx.fetch_queue.begin(), ctx.fetch_queue.end(),
                       [&](const AppContext::FetchTask &t) {
                         return t.pair == pair;
                       }),
        ctx.fetch_queue.end());
    auto it = ctx.streams.find(pair);
    if (it != ctx.streams.end()) {
      it->second->stop();
      ctx.streams.erase(it);
    }
  };
}

void App::process_events() {
  glfwPollEvents();
  auto period = parse_interval(ctx.active_interval);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  bool use_http = (!ctx.streaming_enabled || ctx.stream_failed.load());
  if (use_http && period.count() > 0) {
    if (ctx.next_fetch_time.load() == 0) {
      std::lock_guard<std::mutex> lock(ctx.candles_mutex);
      auto pair_it = ctx.all_candles.find(ctx.active_pair);
      if (pair_it != ctx.all_candles.end()) {
        auto interval_it = pair_it->second.find(ctx.active_interval);
        if (interval_it != pair_it->second.end() &&
            !interval_it->second.empty())
          ctx.next_fetch_time.store(
              interval_it->second.back().open_time + period.count());
      }
      if (ctx.next_fetch_time.load() == 0)
        ctx.next_fetch_time.store(now_ms + period.count());
    }
    if (now_ms >= ctx.next_fetch_time.load()) {
      for (const auto &item : ctx.pairs) {
        const auto &pair = item.name;
        if (ctx.pending_fetches.find(pair) == ctx.pending_fetches.end()) {
          ctx.pending_fetches[pair] = {ctx.active_interval,
                                       data_service_.fetch_klines_async(
                                           pair, ctx.active_interval, 1)};
          add_status("Updating " + pair);
        }
      }
      ctx.next_fetch_time.store(now_ms + period.count());
    }
  }
  if (use_http) {
    for (auto it = ctx.pending_fetches.begin(); it != ctx.pending_fetches.end();) {
      if (it->second.future.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        auto latest = it->second.future.get();
        auto result_now =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        if (latest.error == FetchError::None && !latest.candles.empty()) {
          std::lock_guard<std::mutex> lock(ctx.candles_mutex);
          auto &vec = ctx.all_candles[it->first][it->second.interval];
          if (vec.empty() ||
              latest.candles.back().open_time > vec.back().open_time) {
            vec.push_back(latest.candles.back());
            data_service_.append_candles(it->first, it->second.interval,
                                         {latest.candles.back()});
            auto p = parse_interval(it->second.interval);
            long long boundary = vec.back().open_time + p.count();
            auto nft = ctx.next_fetch_time.load();
            if (nft == 0 || boundary < nft)
              ctx.next_fetch_time.store(boundary);
          } else {
            long long retry =
                result_now +
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    ctx.fetch_backoff)
                    .count();
            auto nft2 = ctx.next_fetch_time.load();
            if (nft2 == 0 || retry < nft2)
              ctx.next_fetch_time.store(retry);
          }
          add_status("Updated " + it->first);
        } else {
          status_.error_message = "Update failed for " + it->first;
          add_status(status_.error_message);
          long long retry =
              result_now +
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  ctx.fetch_backoff)
                  .count();
          auto nft3 = ctx.next_fetch_time.load();
          if (nft3 == 0 || retry < nft3)
            ctx.next_fetch_time.store(retry);
        }
        it = ctx.pending_fetches.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (auto it = ctx.fetch_queue.begin(); it != ctx.fetch_queue.end();) {
    auto status = it->future.wait_for(std::chrono::seconds(0));
    if (status == std::future_status::ready) {
      auto fetched = it->future.get();
      if (fetched.error == FetchError::None && !fetched.candles.empty()) {
        std::lock_guard<std::mutex> lock(ctx.candles_mutex);
        auto &vec = ctx.all_candles[it->pair][it->interval];
        long long last_time = vec.empty() ? 0 : vec.back().open_time;
        std::vector<Candle> new_candles;
        for (const auto &c : fetched.candles) {
          if (c.open_time > last_time) {
            vec.push_back(c);
            new_candles.push_back(c);
          }
        }
        if (!new_candles.empty()) {
          if (last_time > 0)
            data_service_.append_candles(it->pair, it->interval,
                                         new_candles);
          else
            data_service_.save_candles(it->pair, it->interval, vec);
        }
        add_status("Loaded " + it->pair + " " + it->interval);
        ++ctx.completed_fetches;
        it = ctx.fetch_queue.erase(it);
      } else {
        status_.error_message = "Failed to fetch " + it->pair + " " +
                                it->interval + ", retrying";
        add_status(status_.error_message);
        int miss = ctx.candles_limit -
                   static_cast<int>(ctx.all_candles[it->pair][it->interval].size());
        if (miss <= 0)
          miss = ctx.candles_limit;
        it->future = data_service_.fetch_klines_async(it->pair, it->interval,
                                                      miss);
        it->start = std::chrono::steady_clock::now();
        ++it;
      }
    } else {
      if (std::chrono::steady_clock::now() - it->start > ctx.request_timeout) {
        status_.error_message = "Timeout fetching " + it->pair + " " +
                                it->interval + ", retrying";
        add_status(status_.error_message);
        int miss = ctx.candles_limit -
                   static_cast<int>(ctx.all_candles[it->pair][it->interval].size());
        if (miss <= 0)
          miss = ctx.candles_limit;
        it->future = data_service_.fetch_klines_async(it->pair, it->interval,
                                                      miss);
        it->start = std::chrono::steady_clock::now();
      }
      ++it;
    }
  }
  status_.candle_progress = ctx.total_fetches > 0
                               ? static_cast<float>(ctx.completed_fetches) /
                                     static_cast<float>(ctx.total_fetches)
                               : 1.0f;
}

void App::render_ui() {
  ui_manager_.begin_frame();

  ImGuiID dockspace_id = ImGui::GetID("MainDock");
  static bool dock_init = false;
  if (!dock_init) {
    dock_init = true;
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGuiID dock_main_top, dock_bottom;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f,
                                &dock_bottom, &dock_main_top);
    ImGuiID dock_left, dock_right;
    ImGui::DockBuilderSplitNode(dock_main_top, ImGuiDir_Left, 0.2f,
                                &dock_left, &dock_right);
    ImGui::DockBuilderDockWindow("Control Panel", dock_left);
    ImGui::DockBuilderDockWindow("Chart", dock_right);
    ImGui::DockBuilderDockWindow("Journal", dock_bottom);
    ImGui::DockBuilderDockWindow("Signals", dock_bottom);
    ImGui::DockBuilderDockWindow("Backtest", dock_bottom);
    ImGui::DockBuilderDockWindow("Analytics", dock_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
  }
  ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());

  if (ctx.completed_fetches < ctx.total_fetches) {
    ImGui::Begin("Status", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    float progress = ctx.total_fetches
                         ? static_cast<float>(ctx.completed_fetches) /
                               static_cast<float>(ctx.total_fetches)
                         : 1.0f;
    ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
    ImGui::Text("%zu / %zu", ctx.completed_fetches, ctx.total_fetches);
    ImGui::End();
  }

  DrawControlPanel(ctx.pairs, ctx.selected_pairs, ctx.active_pair,
                   ctx.intervals, ctx.selected_interval, ctx.all_candles,
                   ctx.save_pairs, ctx.exchange_pairs, status_,
                   data_service_, ctx.cancel_pair);

  DrawSignalsWindow(ctx.strategy, ctx.short_period, ctx.long_period,
                    ctx.oversold, ctx.overbought, ctx.show_on_chart,
                    ctx.signal_entries, ctx.buy_times, ctx.buy_prices,
                    ctx.sell_times, ctx.sell_prices, ctx.all_candles,
                    ctx.active_pair, ctx.selected_interval, status_);

  DrawAnalyticsWindow(ctx.all_candles, ctx.active_pair, ctx.selected_interval);
  DrawJournalWindow(journal_service_.journal());

  ImGui::Begin("Backtest");
  ImGui::Text("Strategy: %s", ctx.strategy.c_str());
  if (ctx.strategy == "sma_crossover") {
    ImGui::Text("Short SMA: %d", ctx.short_period);
    ImGui::Text("Long SMA: %d", ctx.long_period);
  } else if (ctx.strategy == "ema") {
    ImGui::Text("EMA Period: %d", ctx.short_period);
  } else if (ctx.strategy == "rsi") {
    ImGui::Text("RSI Period: %d", ctx.short_period);
    ImGui::Text("Oversold: %.2f", ctx.oversold);
    ImGui::Text("Overbought: %.2f", ctx.overbought);
  }
  if (ImGui::Button("Run Backtest")) {
    status_.analysis_message = "Running backtest";
    add_status("Backtest started");
    Config::SignalConfig cfg;
    cfg.type = ctx.strategy;
    cfg.short_period = static_cast<std::size_t>(ctx.short_period);
    cfg.long_period = static_cast<std::size_t>(ctx.long_period);
    if (ctx.strategy == "rsi") {
      cfg.params["oversold"] = ctx.oversold;
      cfg.params["overbought"] = ctx.overbought;
    }
    SignalBot bot(cfg);
    auto pair_it = ctx.all_candles.find(ctx.active_pair);
    if (pair_it != ctx.all_candles.end()) {
      auto interval_it = pair_it->second.find(ctx.active_interval);
      if (interval_it != pair_it->second.end()) {
        Core::Backtester bt(interval_it->second, bot);
        ctx.last_result = bt.run();
        ctx.last_signal_cfg = cfg;
      }
    }
    status_.analysis_message = "Backtest done";
    add_status("Backtest finished");
  }
  if (!ctx.last_result.equity_curve.empty()) {
    ImGui::Text("Strategy: %s", ctx.last_signal_cfg.type.c_str());
    if (ctx.last_signal_cfg.type == "sma_crossover") {
      ImGui::Text("Short SMA: %zu", ctx.last_signal_cfg.short_period);
      ImGui::Text("Long SMA: %zu", ctx.last_signal_cfg.long_period);
    } else if (ctx.last_signal_cfg.type == "ema") {
      ImGui::Text("EMA Period: %zu", ctx.last_signal_cfg.short_period);
    } else if (ctx.last_signal_cfg.type == "rsi") {
      ImGui::Text("RSI Period: %zu", ctx.last_signal_cfg.short_period);
      auto it_os = ctx.last_signal_cfg.params.find("oversold");
      if (it_os != ctx.last_signal_cfg.params.end())
        ImGui::Text("Oversold: %.2f", it_os->second);
      auto it_ob = ctx.last_signal_cfg.params.find("overbought");
      if (it_ob != ctx.last_signal_cfg.params.end())
        ImGui::Text("Overbought: %.2f", it_ob->second);
    }
    ImGui::Text("Total PnL: %.2f", ctx.last_result.total_pnl);
    ImGui::Text("Win rate: %.2f%%", ctx.last_result.win_rate * 100.0);
    ImGui::Text("Max Drawdown: %.2f", ctx.last_result.max_drawdown);
    ImGui::Text("Sharpe Ratio: %.2f", ctx.last_result.sharpe_ratio);
    ImGui::Text("Average Win: %.2f", ctx.last_result.avg_win);
    ImGui::Text("Average Loss: %.2f", ctx.last_result.avg_loss);
    if (ImPlot::BeginPlot("Equity")) {
      std::vector<double> x(ctx.last_result.equity_curve.size());
      for (size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<double>(i);
      ImPlot::PlotLine("Equity", x.data(),
                       ctx.last_result.equity_curve.data(), x.size());
      ImPlot::EndPlot();
    }
  }
  ImGui::End();

  DrawChartWindow(ctx.all_candles, ctx.active_pair, ctx.active_interval,
                  ctx.selected_pairs, ctx.intervals, ctx.show_on_chart,
                  ctx.buy_times, ctx.buy_prices, ctx.sell_times, ctx.sell_prices,
                  journal_service_.journal(), ctx.last_result);

  if (ctx.active_pair != ctx.last_active_pair ||
      ctx.active_interval != ctx.last_active_interval) {
    ctx.last_active_pair = ctx.active_pair;
    ctx.last_active_interval = ctx.active_interval;
    auto &candles = ctx.all_candles[ctx.active_pair][ctx.active_interval];
    if (candles.empty())
      candles = data_service_.load_candles(ctx.active_pair, ctx.active_interval);
    int miss = ctx.candles_limit - static_cast<int>(candles.size());
    bool exists = std::any_of(
        ctx.fetch_queue.begin(), ctx.fetch_queue.end(),
        [&](const AppContext::FetchTask &t) {
          return t.pair == ctx.active_pair && t.interval == ctx.active_interval;
        });
    if (miss > 0 && !exists) {
      ctx.fetch_queue.push_back({ctx.active_pair, ctx.active_interval,
                                 data_service_.fetch_klines_async(
                                     ctx.active_pair, ctx.active_interval, miss),
                                 std::chrono::steady_clock::now()});
      ++ctx.total_fetches;
      add_status("Fetching " + ctx.active_pair + " " + ctx.active_interval);
    }
  }

  ui_manager_.end_frame(window_);
}

void App::cleanup() {
  if (ctx.save_pairs)
    ctx.save_pairs();
  ui_manager_.shutdown();
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
  Logger::instance().info("Application exiting");
}

int App::run() {
  if (!init_window())
    return -1;
  setup_imgui();
  load_config();
  while (!glfwWindowShouldClose(window_)) {
    process_events();
    render_ui();
  }
  cleanup();
  return 0;
}
