#include "kline_stream.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

#include "core/logger.h"
#include "exchange_utils.h"

namespace Core {

KlineStream::KlineStream(const std::string &symbol, const std::string &interval,
                         CandleManager &manager, WebSocketFactory ws_factory,
                         SleepFunc sleep_func,
                         std::chrono::milliseconds base_delay,
                         const std::string &provider)
    : symbol_(symbol), interval_(interval), provider_(provider), candle_manager_(manager),
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
  std::string url;
  const bool is_binance = (provider_ == "binance");
  const bool is_gateio = (provider_ == "gateio");
  if (is_binance) {
    std::string sym = symbol_;
    std::transform(sym.begin(), sym.end(), sym.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    url = "wss://stream.binance.com:9443/ws/" + sym + "@kline_" + interval_;
  } else if (is_gateio) {
    url = "wss://api.gateio.ws/ws/v4/";
  } else {
    Logger::instance().warn("Streaming provider '" + provider_ + "' not supported; Kline streaming disabled");
    if (err_cb) err_cb();
    running_ = false;
    return;
  }
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
    auto weak_self = std::weak_ptr<KlineStream>(shared_from_this());
    // Log open for any provider; Gate.io also sends subscription
    ws_->setOnOpen([weak_self]() {
      if (auto self = weak_self.lock()) {
        Logger::instance().info(
            std::string("WS open (") + self->provider_ + ") for " + self->symbol_ +
            " " + self->interval_);
      }
    });
    if (is_gateio) {
      // Subscribe upon open
      ws_->setOnOpen([weak_self]() {
        if (auto self = weak_self.lock()) {
          try {
            nlohmann::json sub = {
              {"time", std::time(nullptr)},
              {"channel", "spot.candlesticks"},
              {"event", "subscribe"},
              {"payload", nlohmann::json::array({ to_gate_symbol(self->symbol_), self->interval_ })}
            };
            Logger::instance().info("WS open (gateio), subscribing to " + to_gate_symbol(self->symbol_) + " " + self->interval_);
            self->ws_->sendText(sub.dump());
          } catch (...) {
          }
        }
      });
    }
    ws_->setOnMessage([this, cb, err_cb, ui_cb, is_binance, is_gateio](const std::string &msg) {
      try {
        auto j = nlohmann::json::parse(msg);
        if (is_binance) {
          if (j.contains("k")) {
            auto k = j["k"];
            bool closed = k.value("x", false);
            if (closed) {
              auto as_double = [](const nlohmann::json &v) -> double {
                try {
                  if (v.is_string()) return std::stod(v.get<std::string>());
                  if (v.is_number_float()) return v.get<double>();
                  if (v.is_number_integer()) return static_cast<double>(v.get<long long>());
                } catch (...) {
                }
                return 0.0;
              };
              long long t = k.value("t", 0LL);
              long long T = k.value("T", 0LL);
              double o = as_double(k["o"]);
              double h = as_double(k["h"]);
              double l = as_double(k["l"]);
              double cpx = as_double(k["c"]);
              double v = as_double(k["v"]);
              double q = as_double(k["q"]);
              double V = as_double(k["V"]);
              double Q = as_double(k["Q"]);
              Candle c(t, o, h, l, cpx, v, T, q, k.value("n", 0), V, Q, 0.0);
              candle_manager_.append_candles(symbol_, interval_, {c});
              if (cb) cb(c);
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
        } else if (is_gateio) {
          if (j.value("channel", std::string()) == std::string("spot.candlesticks") &&
              j.value("event", std::string()) == std::string("update")) {
            auto res = j["result"];
            auto process_entry = [&](const nlohmann::json &e) {
              long long t = 0;
              double o=0,h=0,l=0,c=0,v=0;
              std::string pair;
              if (e.is_object()) {
                t = static_cast<long long>(std::stoll(e.value("t", std::string("0")))) * 1000LL;
                o = std::stod(e.value("o", std::string("0")));
                h = std::stod(e.value("h", std::string("0")));
                l = std::stod(e.value("l", std::string("0")));
                c = std::stod(e.value("c", std::string("0")));
                v = std::stod(e.value("v", std::string("0")));
              } else if (e.is_array()) {
                // Gate WS format: typically [t, o, h, l, c, v] or [t, v, c, h, l, o]
                // We'll first try REST-like [t, v, c, h, l, o]
                try {
                  t = std::stoll(e.at(0).get<std::string>()) * 1000LL;
                  v = std::stod(e.at(1).get<std::string>());
                  c = std::stod(e.at(2).get<std::string>());
                  h = std::stod(e.at(3).get<std::string>());
                  l = std::stod(e.at(4).get<std::string>());
                  o = std::stod(e.at(5).get<std::string>());
                } catch (...) {
                  // Fallback: [t, o, h, l, c, v]
                  try {
                    t = std::stoll(e.at(0).get<std::string>()) * 1000LL;
                    o = std::stod(e.at(1).get<std::string>());
                    h = std::stod(e.at(2).get<std::string>());
                    l = std::stod(e.at(3).get<std::string>());
                    c = std::stod(e.at(4).get<std::string>());
                    v = std::stod(e.at(5).get<std::string>());
                  } catch (...) {
                    t = 0; // parsing failed
                  }
                }
              }
              if (t > 0) {
                Candle cd(t, o, h, l, c, v, t, 0.0, 0, 0.0, 0.0, 0.0);
                candle_manager_.append_candles(symbol_, interval_, {cd});
                if (cb) cb(cd);
                if (ui_cb) {
                  nlohmann::json out{{"time", cd.open_time / 1000},
                                     {"open", cd.open},
                                     {"high", cd.high},
                                     {"low", cd.low},
                                     {"close", cd.close},
                                     {"volume", cd.volume}};
                  ui_cb(out.dump());
                }
              }
            };
            if (res.is_array()) {
              if (!res.empty() && res[0].is_array()) {
                for (const auto &e : res) process_entry(e);
              } else {
                process_entry(res);
              }
            } else if (res.is_object()) {
              process_entry(res);
            }
          }
        }
      } catch (const std::exception &e) {
        Logger::instance().error(std::string("Kline parse error: ") + e.what());
        if (err_cb)
          err_cb();
      }
    });
    ws_->setOnError([weak_self, error, m, cv, closed]() {
      if (auto self = weak_self.lock()) {
        self->callbacks_inflight_++;
        Logger::instance().warn(std::string("WS error (") + self->provider_ + ") for " + self->symbol_ + " " + self->interval_);
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
        Logger::instance().info(std::string("WS close (") + self->provider_ + ") for " + self->symbol_ + " " + self->interval_);
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

    if (error->load() && err_cb)
      err_cb();

    if (error->load()) {
      ++attempt;
      auto delay = base_delay_ * (1 << std::min<std::size_t>(attempt - 1, 8));
      sleep_func_(delay);
    } else {
      attempt = 0;
    }

    // Check stop condition after handling error/backoff to allow at least
    // one reconnection attempt to be scheduled when an error occurs,
    // improving robustness under races with a concurrent stop().
    if (!running_)
      break;
  }
}

} // namespace Core
