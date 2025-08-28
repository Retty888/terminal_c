#include "kline_stream.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

#include "core/logger.h"

namespace Core {

KlineStream::KlineStream(const std::string &symbol, const std::string &interval,
                         CandleManager &manager, WebSocketFactory ws_factory,
                         SleepFunc sleep_func,
                         std::chrono::milliseconds base_delay)
    : symbol_(symbol), interval_(interval), candle_manager_(manager),
      ws_factory_(std::move(ws_factory)),
      sleep_func_(sleep_func ? std::move(sleep_func)
                             : [](std::chrono::milliseconds
                                      d) { std::this_thread::sleep_for(d); }),
      base_delay_(base_delay) {}

KlineStream::~KlineStream() { stop(); }

void KlineStream::start(CandleCallback cb, ErrorCallback err_cb,
                        UICallback ui_cb) {
  if (running_)
    return;
  running_ = true;
  auto self = shared_from_this();
  thread_ = std::thread([self, cb, err_cb, ui_cb]() {
    self->run(cb, err_cb, ui_cb);
  });
}

void KlineStream::stop() {
  running_ = false;
  {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    if (ws_)
      ws_->stop();
  }
  if (thread_.joinable())
    thread_.join();
  std::unique_lock<std::mutex> lk(cb_mutex_);
  cb_cv_.wait(lk, [this] { return callbacks_inflight_.load() == 0; });
}

void KlineStream::run(CandleCallback cb, ErrorCallback err_cb,
                      UICallback ui_cb) {
  const std::string url =
      "wss://stream.binance.com:9443/ws/" + symbol_ + "@kline_" + interval_;
  std::size_t attempt = 0;

  while (running_) {
    {
      std::lock_guard<std::mutex> lock(ws_mutex_);
      ws_ = ws_factory_();
    }
    if (!ws_) {
      Logger::instance().warn(
          "WebSocket support not available; Kline streaming disabled");
      if (err_cb)
        err_cb();
      running_ = false;
      break;
    }

    auto error = std::make_shared<std::atomic<bool>>(false);
    auto m = std::make_shared<std::mutex>();
    auto cv = std::make_shared<std::condition_variable>();
    auto closed = std::make_shared<std::atomic<bool>>(false);

    ws_->setUrl(url);
    ws_->setOnMessage([this, cb, err_cb, ui_cb](const std::string &msg) {
      try {
        auto j = nlohmann::json::parse(msg);
        if (j.contains("k")) {
          auto k = j["k"];
          bool closed = k.value("x", false);
          if (closed) {
            Candle c(
                k.value("t", 0LL), k.value("o", 0.0),
                k.value("h", 0.0),
                k.value("l", 0.0),
                k.value("c", 0.0),
                k.value("v", 0.0), k.value("T", 0LL),
                k.value("q", 0.0), k.value("n", 0),
                k.value("V", 0.0),
                k.value("Q", 0.0), 0.0);
            candle_manager_.append_candles(symbol_, interval_, {c});
            if (cb)
              cb(c);
            if (ui_cb) {
              nlohmann::json out{{"time", c.open_time / 1000},
                                 {"open", c.open},
                                 {"high", c.high},
                                 {"low", c.low},
                                 {"close", c.close},
                                 {"volume", c.volume}};
              ui_cb(out.dump());
            }
          }
        }
      } catch (const std::exception &e) {
        Logger::instance().error(std::string("Kline parse error: ") + e.what());
        if (err_cb)
          err_cb();
      }
    });
    auto weak_self = std::weak_ptr<KlineStream>(shared_from_this());
    ws_->setOnError([weak_self, error, m, cv, closed]() {
      if (auto self = weak_self.lock()) {
        self->callbacks_inflight_++;
        error->store(true);
        {
          std::lock_guard<std::mutex> lock(self->ws_mutex_);
          if (self->ws_)
            self->ws_->stop();
        }
        // Ensure the run loop doesn't block waiting for a close event
        // if the underlying implementation fails to deliver it promptly.
        {
          std::lock_guard<std::mutex> lk(*m);
          closed->store(true);
        }
        cv->notify_one();
        self->callbacks_inflight_--;
        self->cb_cv_.notify_all();
      }
    });
    ws_->setOnClose([weak_self, m, cv, closed]() {
      if (auto self = weak_self.lock()) {
        self->callbacks_inflight_++;
        {
          std::lock_guard<std::mutex> lk(*m);
          closed->store(true);
        }
        cv->notify_one();
        self->callbacks_inflight_--;
        self->cb_cv_.notify_all();
      }
    });

    ws_->start();

    {
      std::unique_lock<std::mutex> lk(*m);
      cv->wait(lk, [closed] { return closed->load(); });
    }

    {
      std::lock_guard<std::mutex> lock(ws_mutex_);
      if (ws_) {
        ws_->stop();
        ws_.reset();
      }
    }

    if (!running_)
      break;

    if (error->load() && err_cb)
      err_cb();

    if (error->load()) {
      ++attempt;
      auto delay = base_delay_ * (1 << std::min<std::size_t>(attempt - 1, 8));
      sleep_func_(delay);
    } else {
      attempt = 0;
    }
  }
}

} // namespace Core
