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

App::App() : ctx_(std::make_unique<AppContext>()) {}
App::~App() = default;

void App::add_status(const std::string &msg) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  status_.log.push_back(msg);
  if (status_.log.size() > 50)
    status_.log.pop_front();
}

bool App::init_window() {
  auto cfg = Config::ConfigManager::load("config.json");
  auto level = cfg ? cfg->log_level : LogLevel::Info;
  Logger::instance().set_min_level(level);
  bool console = cfg ? cfg->log_to_console : true;
  bool file = cfg ? cfg->log_to_file : true;
  Logger::instance().enable_console_output(console);
  if (file)
    Logger::instance().set_file(cfg ? cfg->log_file : "terminal.log");
  else
    Logger::instance().set_file("");
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
    this->ctx_->candles_limit = static_cast<int>(cfg->candles_limit);
    this->ctx_->streaming_enabled = cfg->enable_streaming;
  } else {
    Logger::instance().warn("Using default configuration");
    this->ctx_->candles_limit = 5000;
    this->ctx_->streaming_enabled = false;
  }
  this->ctx_->intervals = {"1m", "3m", "5m",  "15m", "1h",
                   "4h", "1d", "15s", "5s"};
  auto exchange_interval_res = data_service_.fetch_intervals();
  if (exchange_interval_res.error == FetchError::None) {
    this->ctx_->intervals.insert(this->ctx_->intervals.end(),
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
    this->ctx_->intervals.insert(this->ctx_->intervals.end(), intervals_found.begin(),
                         intervals_found.end());
  }
  if (pair_names.empty())
    pair_names.push_back("BTCUSDT");
  std::sort(this->ctx_->intervals.begin(), this->ctx_->intervals.end(),
            [](const std::string &a, const std::string &b) {
              return parse_interval(a) < parse_interval(b);
            });
  this->ctx_->intervals.erase(
      std::unique(this->ctx_->intervals.begin(), this->ctx_->intervals.end()),
      this->ctx_->intervals.end());
  for (const auto &name : pair_names) {
    this->ctx_->pairs.push_back({name, true});
  }
  this->ctx_->save_pairs = [&]() {
    std::vector<std::string> names;
    for (const auto &p : this->ctx_->pairs)
      names.push_back(p.name);
    Config::ConfigManager::save_selected_pairs("config.json", names);
  };
  this->ctx_->selected_pairs = pair_names;
  this->ctx_->active_pair = this->ctx_->selected_pairs[0];
  this->ctx_->active_interval = this->ctx_->intervals.empty() ? "1m" : this->ctx_->intervals[0];

  auto exchange_pairs_res = data_service_.fetch_all_symbols();
  this->ctx_->exchange_pairs = exchange_pairs_res.error == FetchError::None
                            ? exchange_pairs_res.symbols
                            : std::vector<std::string>{};

  this->ctx_->selected_interval = this->ctx_->intervals.empty() ? "1m" : this->ctx_->intervals[0];
  journal_service_.load("journal.json");

  for (const auto &pair : this->ctx_->selected_pairs) {
    for (const auto &interval : this->ctx_->intervals) {
      this->ctx_->all_candles[pair][interval] =
          data_service_.load_candles(pair, interval);
    }
  }
  auto &initial = this->ctx_->all_candles[this->ctx_->active_pair][this->ctx_->active_interval];
  int missing = this->ctx_->candles_limit - static_cast<int>(initial.size());
  if (missing > 0) {
    this->ctx_->fetch_queue.push_back({this->ctx_->active_pair, this->ctx_->active_interval,
                               data_service_.fetch_klines_async(this->ctx_->active_pair,
                                                                this->ctx_->active_interval,
                                                                missing),
                               std::chrono::steady_clock::now()});
    this->ctx_->total_fetches = 1;
    add_status("Fetching " + this->ctx_->active_pair + " " + this->ctx_->active_interval);
  } else {
    status_.candle_progress = 1.0f;
  }
  this->ctx_->next_fetch_time.store(0);
  if (this->ctx_->streaming_enabled && this->ctx_->active_interval != "5s" &&
      this->ctx_->active_interval != "15s") {
    for (const auto &p : this->ctx_->pairs) {
      std::string pair = p.name;
      auto stream =
          std::make_unique<KlineStream>(pair, this->ctx_->active_interval, data_service_.candle_manager());
      stream->start(
          [this, pair](const Candle &c) {
            std::lock_guard<std::mutex> lock(this->ctx_->candles_mutex);
            auto &vec = this->ctx_->all_candles[pair][this->ctx_->active_interval];
            if (vec.empty() || c.open_time > vec.back().open_time)
              vec.push_back(c);
          },
          [this, pair]() {
            this->ctx_->stream_failed = true;
            this->ctx_->next_fetch_time.store(0);
            add_status("Stream failed for " + pair + ", switching to HTTP");
          });
      this->ctx_->streams[pair] = std::move(stream);
    }
  } else {
    this->ctx_->streaming_enabled = false;
  }
  this->ctx_->last_active_pair = this->ctx_->active_pair;
  this->ctx_->last_active_interval = this->ctx_->active_interval;
  this->ctx_->cancel_pair = [&](const std::string &pair) {
    this->ctx_->pending_fetches.erase(pair);
    this->ctx_->fetch_queue.erase(
        std::remove_if(this->ctx_->fetch_queue.begin(), this->ctx_->fetch_queue.end(),
                       [&](const AppContext::FetchTask &t) {
                         return t.pair == pair;
                       }),
        this->ctx_->fetch_queue.end());
    auto it = this->ctx_->streams.find(pair);
    if (it != this->ctx_->streams.end()) {
      it->second->stop();
      this->ctx_->streams.erase(it);
    }
  };
}

void App::process_events() {
  glfwPollEvents();
  auto period = parse_interval(this->ctx_->active_interval);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  bool use_http = (!this->ctx_->streaming_enabled || this->ctx_->stream_failed.load());
  if (use_http && period.count() > 0) {
    if (this->ctx_->next_fetch_time.load() == 0) {
      std::lock_guard<std::mutex> lock(this->ctx_->candles_mutex);
      auto pair_it = this->ctx_->all_candles.find(this->ctx_->active_pair);
      if (pair_it != this->ctx_->all_candles.end()) {
        auto interval_it = pair_it->second.find(this->ctx_->active_interval);
        if (interval_it != pair_it->second.end() &&
            !interval_it->second.empty())
          this->ctx_->next_fetch_time.store(
              interval_it->second.back().open_time + period.count());
      }
      if (this->ctx_->next_fetch_time.load() == 0)
        this->ctx_->next_fetch_time.store(now_ms + period.count());
    }
    if (now_ms >= this->ctx_->next_fetch_time.load()) {
      for (const auto &item : this->ctx_->pairs) {
        const auto &pair = item.name;
        if (this->ctx_->pending_fetches.find(pair) == this->ctx_->pending_fetches.end()) {
          this->ctx_->pending_fetches[pair] = {this->ctx_->active_interval,
                                       data_service_.fetch_klines_async(
                                           pair, this->ctx_->active_interval, 1)};
          add_status("Updating " + pair);
        }
      }
      this->ctx_->next_fetch_time.store(now_ms + period.count());
    }
  }
  if (use_http) {
    for (auto it = this->ctx_->pending_fetches.begin(); it != this->ctx_->pending_fetches.end();) {
      if (it->second.future.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        auto latest = it->second.future.get();
        auto result_now =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        if (latest.error == FetchError::None && !latest.candles.empty()) {
          std::lock_guard<std::mutex> lock(this->ctx_->candles_mutex);
          auto &vec = this->ctx_->all_candles[it->first][it->second.interval];
          if (vec.empty() ||
              latest.candles.back().open_time > vec.back().open_time) {
            vec.push_back(latest.candles.back());
            data_service_.append_candles(it->first, it->second.interval,
                                         {latest.candles.back()});
            auto p = parse_interval(it->second.interval);
            long long boundary = vec.back().open_time + p.count();
            auto nft = this->ctx_->next_fetch_time.load();
            if (nft == 0 || boundary < nft)
              this->ctx_->next_fetch_time.store(boundary);
          } else {
            long long retry =
                result_now +
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    this->ctx_->fetch_backoff)
                    .count();
            auto nft2 = this->ctx_->next_fetch_time.load();
            if (nft2 == 0 || retry < nft2)
              this->ctx_->next_fetch_time.store(retry);
          }
          add_status("Updated " + it->first);
        } else {
          status_.error_message = "Update failed for " + it->first;
          add_status(status_.error_message);
          long long retry =
              result_now +
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  this->ctx_->fetch_backoff)
                  .count();
          auto nft3 = this->ctx_->next_fetch_time.load();
          if (nft3 == 0 || retry < nft3)
            this->ctx_->next_fetch_time.store(retry);
        }
        it = this->ctx_->pending_fetches.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (auto it = this->ctx_->fetch_queue.begin(); it != this->ctx_->fetch_queue.end();) {
    auto status = it->future.wait_for(std::chrono::seconds(0));
    if (status == std::future_status::ready) {
      auto fetched = it->future.get();
      if (fetched.error == FetchError::None && !fetched.candles.empty()) {
        std::lock_guard<std::mutex> lock(this->ctx_->candles_mutex);
        auto &vec = this->ctx_->all_candles[it->pair][it->interval];
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
        ++this->ctx_->completed_fetches;
        it = this->ctx_->fetch_queue.erase(it);
      } else {
        status_.error_message = "Failed to fetch " + it->pair + " " +
                                it->interval + ", retrying";
        add_status(status_.error_message);
        int miss = this->ctx_->candles_limit -
                   static_cast<int>(this->ctx_->all_candles[it->pair][it->interval].size());
        if (miss <= 0)
          miss = this->ctx_->candles_limit;
        it->future = data_service_.fetch_klines_async(it->pair, it->interval,
                                                      miss);
        it->start = std::chrono::steady_clock::now();
        ++it;
      }
    } else {
      if (std::chrono::steady_clock::now() - it->start > this->ctx_->request_timeout) {
        status_.error_message = "Timeout fetching " + it->pair + " " +
                                it->interval + ", retrying";
        add_status(status_.error_message);
        int miss = this->ctx_->candles_limit -
                   static_cast<int>(this->ctx_->all_candles[it->pair][it->interval].size());
        if (miss <= 0)
          miss = this->ctx_->candles_limit;
        it->future = data_service_.fetch_klines_async(it->pair, it->interval,
                                                      miss);
        it->start = std::chrono::steady_clock::now();
      }
      ++it;
    }
  }
  status_.candle_progress = this->ctx_->total_fetches > 0
                               ? static_cast<float>(this->ctx_->completed_fetches) /
                                     static_cast<float>(this->ctx_->total_fetches)
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

  if (this->ctx_->completed_fetches < this->ctx_->total_fetches) {
    ImGui::Begin("Status", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    float progress = this->ctx_->total_fetches
                         ? static_cast<float>(this->ctx_->completed_fetches) /
                               static_cast<float>(this->ctx_->total_fetches)
                         : 1.0f;
    ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
    ImGui::Text("%zu / %zu", this->ctx_->completed_fetches, this->ctx_->total_fetches);
    ImGui::End();
  }

  DrawControlPanel(this->ctx_->pairs, this->ctx_->selected_pairs, this->ctx_->active_pair,
                   this->ctx_->intervals, this->ctx_->selected_interval, this->ctx_->all_candles,
                   this->ctx_->save_pairs, this->ctx_->exchange_pairs, status_,
                   data_service_, this->ctx_->cancel_pair);

  DrawSignalsWindow(this->ctx_->strategy, this->ctx_->short_period, this->ctx_->long_period,
                    this->ctx_->oversold, this->ctx_->overbought, this->ctx_->show_on_chart,
                    this->ctx_->signal_entries, this->ctx_->trades, this->ctx_->all_candles,
                    this->ctx_->active_pair, this->ctx_->selected_interval, status_);

  DrawAnalyticsWindow(this->ctx_->all_candles, this->ctx_->active_pair, this->ctx_->selected_interval);
  DrawJournalWindow(journal_service_.journal());

  ImGui::Begin("Backtest");
  ImGui::Text("Strategy: %s", this->ctx_->strategy.c_str());
  if (this->ctx_->strategy == "sma_crossover") {
    ImGui::Text("Short SMA: %d", this->ctx_->short_period);
    ImGui::Text("Long SMA: %d", this->ctx_->long_period);
  } else if (this->ctx_->strategy == "ema") {
    ImGui::Text("EMA Period: %d", this->ctx_->short_period);
  } else if (this->ctx_->strategy == "rsi") {
    ImGui::Text("RSI Period: %d", this->ctx_->short_period);
    ImGui::Text("Oversold: %.2f", this->ctx_->oversold);
    ImGui::Text("Overbought: %.2f", this->ctx_->overbought);
  }
  if (ImGui::Button("Run Backtest")) {
    status_.analysis_message = "Running backtest";
    add_status("Backtest started");
    Config::SignalConfig cfg;
    cfg.type = this->ctx_->strategy;
    cfg.short_period = static_cast<std::size_t>(this->ctx_->short_period);
    cfg.long_period = static_cast<std::size_t>(this->ctx_->long_period);
    if (this->ctx_->strategy == "rsi") {
      cfg.params["oversold"] = this->ctx_->oversold;
      cfg.params["overbought"] = this->ctx_->overbought;
    }
    SignalBot bot(cfg);
    auto pair_it = this->ctx_->all_candles.find(this->ctx_->active_pair);
    if (pair_it != this->ctx_->all_candles.end()) {
      auto interval_it = pair_it->second.find(this->ctx_->active_interval);
      if (interval_it != pair_it->second.end()) {
        Core::Backtester bt(interval_it->second, bot);
        this->ctx_->last_result = bt.run();
        this->ctx_->last_signal_cfg = cfg;
      }
    }
    status_.analysis_message = "Backtest done";
    add_status("Backtest finished");
  }
  if (!this->ctx_->last_result.equity_curve.empty()) {
    ImGui::Text("Strategy: %s", this->ctx_->last_signal_cfg.type.c_str());
    if (this->ctx_->last_signal_cfg.type == "sma_crossover") {
      ImGui::Text("Short SMA: %zu", this->ctx_->last_signal_cfg.short_period);
      ImGui::Text("Long SMA: %zu", this->ctx_->last_signal_cfg.long_period);
    } else if (this->ctx_->last_signal_cfg.type == "ema") {
      ImGui::Text("EMA Period: %zu", this->ctx_->last_signal_cfg.short_period);
    } else if (this->ctx_->last_signal_cfg.type == "rsi") {
      ImGui::Text("RSI Period: %zu", this->ctx_->last_signal_cfg.short_period);
      auto it_os = this->ctx_->last_signal_cfg.params.find("oversold");
      if (it_os != this->ctx_->last_signal_cfg.params.end())
        ImGui::Text("Oversold: %.2f", it_os->second);
      auto it_ob = this->ctx_->last_signal_cfg.params.find("overbought");
      if (it_ob != this->ctx_->last_signal_cfg.params.end())
        ImGui::Text("Overbought: %.2f", it_ob->second);
    }
    ImGui::Text("Total PnL: %.2f", this->ctx_->last_result.total_pnl);
    ImGui::Text("Win rate: %.2f%%", this->ctx_->last_result.win_rate * 100.0);
    ImGui::Text("Max Drawdown: %.2f", this->ctx_->last_result.max_drawdown);
    ImGui::Text("Sharpe Ratio: %.2f", this->ctx_->last_result.sharpe_ratio);
    ImGui::Text("Average Win: %.2f", this->ctx_->last_result.avg_win);
    ImGui::Text("Average Loss: %.2f", this->ctx_->last_result.avg_loss);
    if (ImPlot::BeginPlot("Equity")) {
      std::vector<double> x(this->ctx_->last_result.equity_curve.size());
      for (size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<double>(i);
      ImPlot::PlotLine("Equity", x.data(),
                       this->ctx_->last_result.equity_curve.data(), x.size());
      ImPlot::EndPlot();
    }
  }
  ImGui::End();

  DrawChartWindow(this->ctx_->all_candles, this->ctx_->active_pair, this->ctx_->active_interval,
                  this->ctx_->selected_pairs, this->ctx_->intervals, this->ctx_->show_on_chart,
                  this->ctx_->trades, journal_service_.journal(), this->ctx_->last_result);

  if (this->ctx_->active_pair != this->ctx_->last_active_pair ||
      this->ctx_->active_interval != this->ctx_->last_active_interval) {
    this->ctx_->last_active_pair = this->ctx_->active_pair;
    this->ctx_->last_active_interval = this->ctx_->active_interval;
    auto &candles = this->ctx_->all_candles[this->ctx_->active_pair][this->ctx_->active_interval];
    if (candles.empty())
      candles = data_service_.load_candles(this->ctx_->active_pair, this->ctx_->active_interval);
    int miss = this->ctx_->candles_limit - static_cast<int>(candles.size());
    bool exists = std::any_of(
        this->ctx_->fetch_queue.begin(), this->ctx_->fetch_queue.end(),
        [&](const AppContext::FetchTask &t) {
          return t.pair == this->ctx_->active_pair && t.interval == this->ctx_->active_interval;
        });
    if (miss > 0 && !exists) {
      this->ctx_->fetch_queue.push_back({this->ctx_->active_pair, this->ctx_->active_interval,
                                 data_service_.fetch_klines_async(
                                     this->ctx_->active_pair, this->ctx_->active_interval, miss),
                                 std::chrono::steady_clock::now()});
      ++this->ctx_->total_fetches;
      add_status("Fetching " + this->ctx_->active_pair + " " + this->ctx_->active_interval);
    }
  }

  ui_manager_.end_frame(window_);
}

void App::cleanup() {
  if (this->ctx_->save_pairs)
    this->ctx_->save_pairs();
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
