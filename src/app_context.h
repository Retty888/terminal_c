#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "core/candle.h"
#include "core/data_fetcher.h"
#include "core/kline_stream.h"
#include "ui/control_panel.h"

struct AppContext {
  std::vector<PairItem> pairs;
  std::vector<std::string> selected_pairs;
  std::string active_pair;
  std::string active_interval;
  std::vector<std::string> intervals;
  std::vector<std::string> available_intervals;
  std::vector<std::string> exchange_pairs;
  std::string selected_interval;
  std::map<std::string, std::map<std::string, std::vector<Core::Candle>>>
      all_candles;
  std::shared_mutex candles_mutex;
  std::map<std::string, std::unique_ptr<Core::KlineStream>> streams;
  std::atomic<bool> stream_failed{false};
  struct PendingFetch {
    std::string interval;
    std::future<Core::KlinesResult> future;
  };
  std::map<std::string, PendingFetch> pending_fetches;
  struct FetchTask {
    std::string pair;
    std::string interval;
    std::future<Core::KlinesResult> future;
    std::chrono::steady_clock::time_point start;
    int retries = 0;
  };
  std::deque<FetchTask> fetch_queue;
  std::mutex fetch_mutex;
  std::condition_variable fetch_cv;
  std::set<std::pair<std::string, std::string>> failed_fetches;
  std::size_t total_fetches = 0;
  std::size_t completed_fetches = 0;
  std::atomic<long long> next_fetch_time{0};
  int candles_limit = 0;
  bool streaming_enabled = false;
  bool save_journal_csv = true;
  std::function<void()> save_pairs;
  std::function<void(const std::string &)> cancel_pair;
  std::string last_active_pair;
  std::string last_active_interval;
  bool show_analytics_window = false;
  bool show_journal_window = false;
  bool show_backtest_window = false;
  std::chrono::milliseconds retry_delay{5000};
  int max_retries = 3;
  bool exponential_backoff = true;
  const std::chrono::seconds request_timeout{10};
};
