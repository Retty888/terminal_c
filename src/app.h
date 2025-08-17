#pragma once

#include "services/data_service.h"
#include "services/journal_service.h"
#include "ui/ui_manager.h"
#include "ui/control_panel.h"
#include "ui/signal_entry.h"
#include "core/candle.h"
#include "core/kline_stream.h"
#include "core/backtester.h"
#include "config_manager.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct AppStatus {
  float candle_progress = 0.0f;
  std::string analysis_message = "Idle";
  std::string signal_message = "Idle";
  std::string error_message;
  std::vector<std::string> log;
};

// The App class owns the services and drives the main event loop.
class App {
public:
  App();
  ~App();
  // Runs the application. Returns the exit code.
  int run();
  const AppStatus &status() const { return status_; }
  void add_status(const std::string &msg);

private:
  bool init_window();
  void setup_imgui();
  void load_config();
  void process_events();
  void render_ui();
  void cleanup();

  struct AppContext {
    struct TradeEvent {
      double time;
      double price;
      enum class Side { Buy, Sell } side;
    };
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
    std::vector<TradeEvent> trades;
    std::map<std::string, std::map<std::string, std::vector<Core::Candle>>> all_candles;
    std::mutex candles_mutex;
    std::map<std::string, std::unique_ptr<Core::KlineStream>> streams;
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

  std::unique_ptr<AppContext> ctx_;
  DataService data_service_;
  JournalService journal_service_;
  AppStatus status_;
  mutable std::mutex status_mutex_;
  GLFWwindow *window_ = nullptr;
  UiManager ui_manager_;
};

