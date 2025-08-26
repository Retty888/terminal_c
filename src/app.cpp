#include "app.h"

#include "config_manager.h"
#include "config_path.h"
#include "core/candle.h"
#include "core/candle_manager.h"
#include "core/candle_utils.h"
#include "core/interval_utils.h"
#include "core/kline_stream.h"
#include "core/logger.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>

#include "ui/analytics_window.h"
#include "ui/backtest_window.h"
#include "ui/control_panel.h"
#include "ui/journal_window.h"

static void OnFramebufferResize(GLFWwindow * /*w*/, int width, int height) {
  glViewport(0, 0, width, height);
}

void App::WindowDeleter::operator()(GLFWwindow *window) const {
  if (window)
    glfwDestroyWindow(window);
}

App::App() : ctx_(std::make_unique<AppContext>()) {}
App::~App() { cleanup(); }

void App::add_status(const std::string &msg, Core::LogLevel level,
                     std::chrono::system_clock::time_point time) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  std::tm tm;
  auto t = std::chrono::system_clock::to_time_t(time);
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[9];
  std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
  const char *lvl = "INFO";
  if (level == Core::LogLevel::Warning)
    lvl = "WARN";
  else if (level == Core::LogLevel::Error)
    lvl = "ERROR";
  status_.log.emplace_back(std::string(buf) + " [" + lvl + "] " + msg);
  if (status_.log.size() > AppStatus::kMaxLogEntries)
    status_.log.pop_front();
}

void App::clear_failed_fetches() {
  std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
  this->ctx_->failed_fetches.clear();
  add_status("Cleared failed fetches");
}

void App::set_error_message(const std::string &msg) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  status_.error_message = msg;
}

AppStatus App::get_status_snapshot() const {
  std::lock_guard<std::mutex> lock(status_mutex_);
  return status_;
}

bool App::init_window() {
  auto cfg = Config::ConfigManager::load(resolve_config_path().string());
  auto level = cfg ? cfg->log_level : Core::LogLevel::Info;
  Core::Logger::instance().set_min_level(level);
  bool console = cfg ? cfg->log_to_console : true;
  bool file = cfg ? cfg->log_to_file : true;
  Core::Logger::instance().enable_console_output(console);
  if (file)
    Core::Logger::instance().set_file(cfg ? cfg->log_file : "terminal.log");
  else
    Core::Logger::instance().set_file("");
  Core::Logger::instance().info("Application started");
  {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_ = AppStatus{};
  }

  glfw_context_ = std::make_unique<Core::GlfwContext>();
  if (!glfw_context_->initialized()) {
    Core::Logger::instance().error("Failed to initialize GLFW");
    glfw_context_.reset();
    return false;
  }
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  window_.reset(glfwCreateWindow(1280, 720, "Trading Terminal", NULL, NULL));
  if (!window_) {
    Core::Logger::instance().error("Failed to create GLFW window");
    glfw_context_.reset();
    return false;
  }
  glfwMakeContextCurrent(window_.get());
  glfwMaximizeWindow(window_.get());
  glfwSwapInterval(1);
  glfwSetFramebufferSizeCallback(window_.get(), OnFramebufferResize);
  int w, h;
  glfwGetFramebufferSize(window_.get(), &w, &h);
  OnFramebufferResize(window_.get(), w, h);
  return true;
}

void App::setup_imgui() {
  ui_manager_.setup(window_.get());
  ui_manager_.set_interval_callback([this](const std::string &iv) {
    this->ctx_->selected_interval = iv;
    this->ctx_->active_interval = iv;
  });
  ui_manager_.set_pair_callback([this](const std::string &p) {
    this->ctx_->active_pair = p;
    update_available_intervals();
  });
  ui_manager_.set_status_callback(
      [this](const std::string &msg) { this->add_status(msg); });
  Core::Logger::instance().set_sink(
      [this](Core::LogLevel level, std::chrono::system_clock::time_point time,
             const std::string &msg) { this->add_status(msg, level, time); });
}

void App::load_config() {
  auto cfg = Config::ConfigManager::load(resolve_config_path().string());
  std::vector<std::string> pair_names;
  if (cfg) {
    pair_names = cfg->pairs;
    this->ctx_->candles_limit = static_cast<int>(cfg->candles_limit);
    this->ctx_->streaming_enabled = cfg->enable_streaming;
    this->ctx_->save_journal_csv = cfg->save_journal_csv;
  } else {
    Core::Logger::instance().warn("Using default configuration");
    this->ctx_->candles_limit = 5000;
    this->ctx_->streaming_enabled = false;
    this->ctx_->save_journal_csv = true;
  }
  this->ctx_->intervals = {"1m", "3m", "5m", "15m", "1h", "4h", "1d"};
  auto exchange_interval_res = data_service_.fetch_intervals();
  if (exchange_interval_res.error == Core::FetchError::None) {
    this->ctx_->intervals.insert(this->ctx_->intervals.end(),
                                 exchange_interval_res.intervals.begin(),
                                 exchange_interval_res.intervals.end());
  }
  load_pairs(pair_names);
  update_available_intervals();
  load_existing_candles();
  start_initial_fetch_and_streams();
}

void App::load_pairs(std::vector<std::string> &pair_names) {
  if (pair_names.empty()) {
    auto stored = data_service_.list_stored_data();
    std::set<std::string> pairs_found;
    std::set<std::string> intervals_found;
    for (const auto &entry : stored) {
      auto lp = entry.rfind(" (");
      auto rp = entry.rfind(')');
      if (lp != std::string::npos && rp != std::string::npos && lp < rp) {
        auto interval = entry.substr(lp + 2, rp - lp - 2);
        if (Core::parse_interval(interval).count() == 0)
          continue;
        pairs_found.insert(entry.substr(0, lp));
        intervals_found.insert(interval);
      }
    }
    pair_names.assign(pairs_found.begin(), pairs_found.end());
    this->ctx_->intervals.insert(this->ctx_->intervals.end(),
                                 intervals_found.begin(),
                                 intervals_found.end());
  }
  if (pair_names.empty())
    pair_names.push_back("BTCUSDT");
  std::sort(this->ctx_->intervals.begin(), this->ctx_->intervals.end(),
            [](const std::string &a, const std::string &b) {
              return Core::parse_interval(a) < Core::parse_interval(b);
            });
  this->ctx_->intervals.erase(
      std::unique(this->ctx_->intervals.begin(), this->ctx_->intervals.end()),
      this->ctx_->intervals.end());
  for (const auto &name : pair_names)
    this->ctx_->pairs.push_back({name, true});
  this->ctx_->save_pairs = [&]() {
    std::vector<std::string> names;
    for (const auto &p : this->ctx_->pairs)
      names.push_back(p.name);
    Config::ConfigManager::save_selected_pairs(resolve_config_path().string(),
                                               names);
  };
  this->ctx_->selected_pairs = pair_names;
  this->ctx_->active_pair = this->ctx_->selected_pairs[0];
  this->ctx_->active_interval =
      this->ctx_->intervals.empty() ? "1m" : this->ctx_->intervals[0];

  auto exchange_pairs_res = data_service_.fetch_all_symbols();
  this->ctx_->exchange_pairs =
      exchange_pairs_res.error == Core::FetchError::None
          ? exchange_pairs_res.symbols
          : std::vector<std::string>{};

  this->ctx_->selected_interval =
      this->ctx_->intervals.empty() ? "1m" : this->ctx_->intervals[0];
  ui_manager_.set_initial_interval(this->ctx_->selected_interval);
  ui_manager_.set_initial_pair(this->ctx_->active_pair);
}

void App::update_available_intervals() {
  this->ctx_->available_intervals.clear();
  auto stored = data_service_.list_stored_data();
  for (const auto &entry : stored) {
    auto lp = entry.rfind(" (");
    auto rp = entry.rfind(')');
    if (lp != std::string::npos && rp != std::string::npos && lp < rp) {
      auto symbol = entry.substr(0, lp);
      auto interval = entry.substr(lp + 2, rp - lp - 2);
      if (symbol == this->ctx_->active_pair &&
          Core::parse_interval(interval).count() > 0) {
        this->ctx_->available_intervals.push_back(interval);
      }
    }
  }
  std::sort(this->ctx_->available_intervals.begin(),
            this->ctx_->available_intervals.end(),
            [](const std::string &a, const std::string &b) {
              return Core::parse_interval(a) < Core::parse_interval(b);
            });
  this->ctx_->available_intervals.erase(
      std::unique(this->ctx_->available_intervals.begin(),
                  this->ctx_->available_intervals.end()),
      this->ctx_->available_intervals.end());
  if (!this->ctx_->available_intervals.empty()) {
    if (std::find(this->ctx_->available_intervals.begin(),
                  this->ctx_->available_intervals.end(),
                  this->ctx_->active_interval) ==
        this->ctx_->available_intervals.end()) {
      this->ctx_->active_interval = this->ctx_->available_intervals.front();
      this->ctx_->selected_interval = this->ctx_->active_interval;
    }
  }
  ui_manager_.set_initial_interval(this->ctx_->active_interval);
}

void App::load_existing_candles() {
  struct LoadTask {
    std::string pair;
    std::string interval;
    std::future<std::vector<Core::Candle>> future;
  };

  std::vector<LoadTask> tasks;
  for (const auto &pair : this->ctx_->selected_pairs) {
    for (const auto &interval : this->ctx_->intervals) {
      tasks.push_back({
          pair, interval,
          std::async(std::launch::async, [this, pair, interval]() {
            auto candles = data_service_.load_candles(pair, interval);
            long long interval_ms = Core::parse_interval(interval).count();
            if (interval_ms > 0 && candles.size() > 1) {
              bool fixed = false;
              for (std::size_t i = 0; i + 1 < candles.size(); ++i) {
                const auto &cur = candles[i];
                const auto &next = candles[i + 1];
                long long expected = cur.open_time + interval_ms;
                if (next.open_time - cur.open_time > interval_ms) {
                  auto res = data_service_.fetch_range(
                      pair, interval, expected, next.open_time - interval_ms);
                  if (res.error == Core::FetchError::None &&
                      !res.candles.empty()) {
                    candles.insert(candles.begin() + i + 1, res.candles.begin(),
                                   res.candles.end());
                    data_service_.append_candles(pair, interval, res.candles);
                    i += res.candles.size();
                    fixed = true;
                  }
                }
              }
              if (fixed)
                Core::fill_missing(candles, interval_ms);
            }
            return candles;
          })});
    }
  }

  {
    std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
    this->ctx_->total_fetches = tasks.size();
    this->ctx_->completed_fetches = 0;
  }

  while (!tasks.empty()) {
    for (auto it = tasks.begin(); it != tasks.end();) {
      if (it->future.wait_for(std::chrono::milliseconds(0)) ==
          std::future_status::ready) {
        auto pair = it->pair;
        auto interval = it->interval;
        auto candles = it->future.get();
        {
          std::lock_guard<std::shared_mutex> lock(this->ctx_->candles_mutex);
          this->ctx_->all_candles[pair][interval] = candles;
        }
        if (pair == this->ctx_->active_pair &&
            interval == this->ctx_->active_interval) {
          ui_manager_.set_candles(candles);
        }
        {
          std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
          ++this->ctx_->completed_fetches;
        }
        update_candle_progress();
        it = tasks.erase(it);
        add_status("Loaded " + pair + " " + interval);
      } else {
        ++it;
      }
    }

    glfwPollEvents();
    ui_manager_.begin_frame();
    render_status_window();
    ui_manager_.end_frame(window_.get());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  {
    std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
    this->ctx_->total_fetches = 0;
    this->ctx_->completed_fetches = 0;
  }
  update_candle_progress();
}

void App::start_initial_fetch_and_streams() {
  auto &initial =
      this->ctx_
          ->all_candles[this->ctx_->active_pair][this->ctx_->active_interval];
  int missing = this->ctx_->candles_limit - static_cast<int>(initial.size());
  if (missing > 0) {
    {
      std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
      this->ctx_->fetch_queue.push_back(
          {this->ctx_->active_pair, this->ctx_->active_interval,
           data_service_.fetch_klines_async(
               this->ctx_->active_pair, this->ctx_->active_interval, missing,
               this->ctx_->max_retries, this->ctx_->retry_delay),
           std::chrono::steady_clock::now()});
      this->ctx_->total_fetches = 1;
    }
    this->ctx_->fetch_cv.notify_one();
    add_status("Fetching " + this->ctx_->active_pair + " " +
               this->ctx_->active_interval);
  } else {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      status_.candle_progress = 1.0f;
    }
  }
  this->ctx_->next_fetch_time.store(0);
  if (this->ctx_->streaming_enabled && this->ctx_->active_interval != "5s" &&
      this->ctx_->active_interval != "15s") {
    for (const auto &p : this->ctx_->pairs) {
      std::string pair = p.name;
      auto stream = std::make_unique<Core::KlineStream>(
          pair, this->ctx_->active_interval, data_service_.candle_manager());
      stream->start(
          [this, pair](const Core::Candle &c) {
            std::lock_guard<std::shared_mutex> lock(this->ctx_->candles_mutex);
            auto &vec =
                this->ctx_->all_candles[pair][this->ctx_->active_interval];
            if (vec.empty() || c.open_time > vec.back().open_time)
              vec.push_back(c);
          },
          [this, pair]() {
            this->ctx_->stream_failed = true;
            this->ctx_->next_fetch_time.store(0);
            add_status("Stream failed for " + pair + ", switching to HTTP");
          },
          ui_manager_.candle_callback());
      this->ctx_->streams[pair] = std::move(stream);
    }
  } else {
    this->ctx_->streaming_enabled = false;
  }
  this->ctx_->last_active_pair = this->ctx_->active_pair;
  this->ctx_->last_active_interval = this->ctx_->active_interval;
  this->ctx_->cancel_pair = [&](const std::string &pair) {
    this->ctx_->pending_fetches.erase(pair);
    {
      std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
      this->ctx_->fetch_queue.erase(
          std::remove_if(
              this->ctx_->fetch_queue.begin(), this->ctx_->fetch_queue.end(),
              [&](const AppContext::FetchTask &t) { return t.pair == pair; }),
          this->ctx_->fetch_queue.end());
    }
    auto it = this->ctx_->streams.find(pair);
    if (it != this->ctx_->streams.end()) {
      it->second->stop();
      this->ctx_->streams.erase(it);
    }
  };
}

void App::start_fetch_thread() {
  fetch_thread_ = std::jthread([this](std::stop_token stoken) {
    std::unique_lock<std::mutex> lock(this->ctx_->fetch_mutex);
    while (!stoken.stop_requested()) {
      if (this->ctx_->fetch_queue.empty()) {
        this->ctx_->fetch_cv.wait(lock, [&]() {
          return stoken.stop_requested() || !this->ctx_->fetch_queue.empty();
        });
        if (stoken.stop_requested())
          break;
      }
      for (auto it = this->ctx_->fetch_queue.begin();
           it != this->ctx_->fetch_queue.end();) {
        if (this->ctx_->failed_fetches.count({it->pair, it->interval})) {
          ++this->ctx_->completed_fetches;
          it = this->ctx_->fetch_queue.erase(it);
          continue;
        }
        auto status = it->future.wait_for(std::chrono::seconds(0));
        if (status == std::future_status::ready) {
          auto fetched = it->future.get();
          if (fetched.error == Core::FetchError::None &&
              !fetched.candles.empty()) {
            {
              std::lock_guard<std::shared_mutex> lock_candles(
                  this->ctx_->candles_mutex);
              auto &vec = this->ctx_->all_candles[it->pair][it->interval];
              long long last_time = vec.empty() ? 0 : vec.back().open_time;
              std::vector<Core::Candle> new_candles;
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
            }
            add_status("Loaded " + it->pair + " " + it->interval);
            ++this->ctx_->completed_fetches;
            it = this->ctx_->fetch_queue.erase(it);
          } else {
            int miss =
                this->ctx_->candles_limit -
                static_cast<int>(
                    this->ctx_->all_candles[it->pair][it->interval].size());
            if (miss <= 0)
              miss = this->ctx_->candles_limit;
            ++it->retries;
            if (it->retries > this->ctx_->max_retries) {
              this->ctx_->failed_fetches.insert({it->pair, it->interval});
              auto msg = "Failed to fetch " + it->pair + " " + it->interval +
                         " after " + std::to_string(this->ctx_->max_retries) +
                         " retries";
              set_error_message(msg);
              Core::Logger::instance().error(msg);
              add_status("Failed to fetch " + it->pair + " " + it->interval);
              ++this->ctx_->completed_fetches;
              it = this->ctx_->fetch_queue.erase(it);
            } else {
              auto delay = this->ctx_->retry_delay;
              if (this->ctx_->exponential_backoff)
                delay *= (1 << (it->retries - 1));
              auto msg = "Failed to fetch " + it->pair + " " + it->interval +
                         ", retrying";
              set_error_message(msg);
              Core::Logger::instance().error(msg);
              add_status(msg);
              it->future = data_service_.fetch_klines_async(
                  it->pair, it->interval, miss, this->ctx_->max_retries, delay);
              it->start = std::chrono::steady_clock::now();
              ++it;
            }
          }
        } else {
          if (std::chrono::steady_clock::now() - it->start >
              this->ctx_->request_timeout) {
            int miss =
                this->ctx_->candles_limit -
                static_cast<int>(
                    this->ctx_->all_candles[it->pair][it->interval].size());
            if (miss <= 0)
              miss = this->ctx_->candles_limit;
            ++it->retries;
            if (it->retries > this->ctx_->max_retries) {
              this->ctx_->failed_fetches.insert({it->pair, it->interval});
              auto msg = "Timeout fetching " + it->pair + " " + it->interval +
                         " after " + std::to_string(this->ctx_->max_retries) +
                         " retries";
              set_error_message(msg);
              Core::Logger::instance().error(msg);
              add_status("Failed to fetch " + it->pair + " " + it->interval);
              ++this->ctx_->completed_fetches;
              it = this->ctx_->fetch_queue.erase(it);
            } else {
              auto delay = this->ctx_->retry_delay;
              if (this->ctx_->exponential_backoff)
                delay *= (1 << (it->retries - 1));
              auto msg = "Timeout fetching " + it->pair + " " + it->interval +
                         ", retrying";
              set_error_message(msg);
              Core::Logger::instance().error(msg);
              add_status(msg);
              it->future = data_service_.fetch_klines_async(
                  it->pair, it->interval, miss, this->ctx_->max_retries, delay);
              it->start = std::chrono::steady_clock::now();
              ++it;
            }
          } else {
            ++it;
          }
        }
      }
      if (this->ctx_->fetch_queue.empty())
        continue;
      this->ctx_->fetch_cv.wait(lock, [&]() {
        return stoken.stop_requested() || !this->ctx_->fetch_queue.empty();
      });
    }
  });
}

void App::stop_fetch_thread() {
  fetch_thread_.request_stop();
  this->ctx_->fetch_cv.notify_all();
  fetch_thread_ = std::jthread();
}

void App::process_events() {
  glfwPollEvents();
  auto period = Core::parse_interval(this->ctx_->active_interval);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  bool use_http =
      (!this->ctx_->streaming_enabled || this->ctx_->stream_failed.load());
  if (use_http && period.count() > 0)
    schedule_http_updates(period, now_ms);
  if (use_http)
    handle_http_updates();
  update_candle_progress();
}

void App::schedule_http_updates(std::chrono::milliseconds period,
                                long long now_ms) {
  if (this->ctx_->next_fetch_time.load() == 0) {
    std::shared_lock<std::shared_mutex> lock(this->ctx_->candles_mutex);
    auto pair_it = this->ctx_->all_candles.find(this->ctx_->active_pair);
    if (pair_it != this->ctx_->all_candles.end()) {
      auto interval_it = pair_it->second.find(this->ctx_->active_interval);
      if (interval_it != pair_it->second.end() && !interval_it->second.empty())
        update_next_fetch_time(interval_it->second.back().open_time +
                               period.count());
    }
    if (this->ctx_->next_fetch_time.load() == 0)
      update_next_fetch_time(now_ms + period.count());
  }
  if (now_ms >= this->ctx_->next_fetch_time.load()) {
    for (const auto &item : this->ctx_->pairs) {
      const auto &pair = item.name;
      bool skip = false;
      {
        std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
        skip = this->ctx_->failed_fetches.count(
                   {pair, this->ctx_->active_interval}) > 0;
      }
      if (skip)
        continue;
      if (this->ctx_->pending_fetches.find(pair) ==
          this->ctx_->pending_fetches.end()) {
        this->ctx_->pending_fetches[pair] = {
            this->ctx_->active_interval,
            data_service_.fetch_klines_async(pair, this->ctx_->active_interval,
                                             1, this->ctx_->max_retries,
                                             this->ctx_->retry_delay)};
        add_status("Updating " + pair);
      }
    }
    update_next_fetch_time(now_ms + period.count());
  }
}

void App::handle_http_updates() {
  for (auto it = this->ctx_->pending_fetches.begin();
       it != this->ctx_->pending_fetches.end();) {
    if (it->second.future.wait_for(std::chrono::seconds(0)) ==
        std::future_status::ready) {
      auto latest = it->second.future.get();
      auto result_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
      if (latest.error == Core::FetchError::None && !latest.candles.empty()) {
        std::lock_guard<std::shared_mutex> lock(this->ctx_->candles_mutex);
        auto &vec = this->ctx_->all_candles[it->first][it->second.interval];
        bool was_empty = vec.empty();
        bool appended = false;
        for (const auto &c : latest.candles) {
          if (vec.empty() || c.open_time > vec.back().open_time) {
            vec.push_back(c);
            data_service_.append_candles(it->first, it->second.interval, {c});
            if (it->first == this->ctx_->active_pair &&
                it->second.interval == this->ctx_->active_interval) {
              ui_manager_.push_candle(c);
            }
            appended = true;
          }
        }
        if (was_empty && appended && it->first == this->ctx_->active_pair &&
            it->second.interval == this->ctx_->active_interval) {
          ui_manager_.set_candles(vec);
        }
        if (appended) {
          auto p = Core::parse_interval(it->second.interval);
          long long boundary = vec.back().open_time + p.count();
          update_next_fetch_time(boundary);
        } else {
          schedule_retry(result_now, this->ctx_->retry_delay);
        }
        add_status("Updated " + it->first);
      } else {
        schedule_retry(result_now, this->ctx_->retry_delay,
                       "Update failed for " + it->first);
      }
      it = this->ctx_->pending_fetches.erase(it);
    } else {
      ++it;
    }
  }
}

void App::update_candle_progress() {
  std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
  {
    std::lock_guard<std::mutex> status_lock(status_mutex_);
    status_.candle_progress =
        this->ctx_->total_fetches > 0
            ? static_cast<float>(this->ctx_->completed_fetches) /
                  static_cast<float>(this->ctx_->total_fetches)
            : 1.0f;
  }
}

void App::schedule_retry(long long now_ms, std::chrono::milliseconds delay,
                         const std::string &msg) {
  if (!msg.empty()) {
    set_error_message(msg);
    Core::Logger::instance().error(msg);
    add_status(msg);
  }
  long long retry = now_ms + delay.count();
  update_next_fetch_time(retry);
}

void App::update_next_fetch_time(long long candidate) {
  auto nft = this->ctx_->next_fetch_time.load();
  if (nft == 0 || candidate < nft)
    this->ctx_->next_fetch_time.store(candidate);
}

void App::render_ui() {
  ui_manager_.begin_frame();
  render_status_window();
  render_main_windows();
  handle_active_pair_change();
  ui_manager_.end_frame(window_.get());
}

void App::render_status_window() {
  std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
  if (this->ctx_->completed_fetches < this->ctx_->total_fetches) {
    auto vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_FirstUseEver);
    ImGui::Begin("Status", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    float progress = this->ctx_->total_fetches
                         ? static_cast<float>(this->ctx_->completed_fetches) /
                               static_cast<float>(this->ctx_->total_fetches)
                         : 1.0f;
    ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
    ImGui::Text("%zu / %zu", this->ctx_->completed_fetches,
                this->ctx_->total_fetches);
    ImGui::End();
  }
}

void App::render_main_windows() {
  {
    std::unique_lock<std::shared_mutex> lock(this->ctx_->candles_mutex);
    DrawControlPanel(
        this->ctx_->pairs, this->ctx_->selected_pairs, this->ctx_->active_pair,
        this->ctx_->intervals, this->ctx_->selected_interval,
        this->ctx_->all_candles, this->ctx_->save_pairs,
        this->ctx_->exchange_pairs, status_, status_mutex_, data_service_,
        this->ctx_->cancel_pair, this->ctx_->show_analytics_window,
        this->ctx_->show_journal_window, this->ctx_->show_backtest_window);
  }
  {
    std::shared_lock<std::shared_mutex> lock(this->ctx_->candles_mutex);
    ui_manager_.set_pairs(this->ctx_->selected_pairs);
    ui_manager_.set_intervals(this->ctx_->available_intervals);
    ui_manager_.draw_chart_panel();
    if (this->ctx_->show_analytics_window) {
      DrawAnalyticsWindow(this->ctx_->all_candles, this->ctx_->active_pair,
                          this->ctx_->selected_interval);
    }
    if (this->ctx_->show_journal_window) {
      DrawJournalWindow(journal_service_, this->ctx_->save_journal_csv);
    }
    if (this->ctx_->show_backtest_window) {
      DrawBacktestWindow(this->ctx_->all_candles, this->ctx_->active_pair,
                         this->ctx_->selected_interval);
    }
  }
}

void App::handle_active_pair_change() {
  if (this->ctx_->active_pair != this->ctx_->last_active_pair ||
      this->ctx_->active_interval != this->ctx_->last_active_interval) {
    this->ctx_->last_active_pair = this->ctx_->active_pair;
    this->ctx_->last_active_interval = this->ctx_->active_interval;
    int miss;
    {
      std::lock_guard<std::shared_mutex> lock(this->ctx_->candles_mutex);
      auto &candles = this->ctx_->all_candles[this->ctx_->active_pair]
                                             [this->ctx_->active_interval];
      if (candles.empty())
        candles = data_service_.load_candles(this->ctx_->active_pair,
                                             this->ctx_->active_interval);
      ui_manager_.set_candles(candles);
      miss = this->ctx_->candles_limit - static_cast<int>(candles.size());
    }
    bool exists;
    bool failed;
    {
      std::lock_guard<std::mutex> lock(this->ctx_->fetch_mutex);
      exists = std::any_of(this->ctx_->fetch_queue.begin(),
                           this->ctx_->fetch_queue.end(),
                           [&](const AppContext::FetchTask &t) {
                             return t.pair == this->ctx_->active_pair &&
                                    t.interval == this->ctx_->active_interval;
                           });
      failed = this->ctx_->failed_fetches.count(
                   {this->ctx_->active_pair, this->ctx_->active_interval}) > 0;
      if (miss > 0 && !exists && !failed) {
        this->ctx_->fetch_queue.push_back(
            {this->ctx_->active_pair, this->ctx_->active_interval,
             data_service_.fetch_klines_async(
                 this->ctx_->active_pair, this->ctx_->active_interval, miss),
             std::chrono::steady_clock::now()});
        ++this->ctx_->total_fetches;
      }
    }
    if (miss > 0 && !exists && !failed)
      this->ctx_->fetch_cv.notify_one();
    if (miss > 0 && !exists && !failed) {
      add_status("Fetching " + this->ctx_->active_pair + " " +
                 this->ctx_->active_interval);
    }
  }
}

void App::cleanup() {
  stop_fetch_thread();
  if (this->ctx_->save_pairs)
    this->ctx_->save_pairs();
  if (!journal_service_.save("journal.json")) {
    add_status("Failed to save journal.json", Core::LogLevel::Error);
  }
  if (this->ctx_->save_journal_csv) {
    auto csv_path = (journal_service_.base_dir() / "journal.csv").string();
    if (!journal_service_.journal().save_csv(csv_path)) {
      add_status("Failed to save journal.csv", Core::LogLevel::Error);
      Core::Logger::instance().error("Failed to save journal.csv");
    } else {
      Core::Logger::instance().info("Saved journal.csv");
      add_status("Saved journal.csv");
    }
  }
  ui_manager_.shutdown();
  window_.reset();
  glfw_context_.reset();
  Core::Logger::instance().info("Application exiting");
}

int App::run() {
  if (!init_window())
    return -1;
  setup_imgui();
  load_config();
  if (!journal_service_.load("journal.json")) {
    add_status("Failed to load journal.json", Core::LogLevel::Error);
  }
  start_fetch_thread();
  while (!glfwWindowShouldClose(window_.get())) {
    process_events();
    render_ui();
  }
  return 0;
}
