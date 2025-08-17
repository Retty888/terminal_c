#include "kline_stream.h"

#include <algorithm>
#include <atomic>
#include <nlohmann/json.hpp>
#include <thread>

#include "logger.h"

namespace Core {

KlineStream::KlineStream(const std::string &symbol, const std::string &interval,
                         CandleManager &manager,
                         WebSocketFactory ws_factory,
                         SleepFunc sleep_func,
                         std::chrono::milliseconds base_delay)
    : symbol_(symbol), interval_(interval), candle_manager_(manager),
      ws_factory_(std::move(ws_factory)),
      sleep_func_(sleep_func ? std::move(sleep_func)
                             : [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); }),
      base_delay_(base_delay) {}

KlineStream::~KlineStream() { stop(); }

void KlineStream::start(CandleCallback cb, ErrorCallback err_cb) {
  if (running_) return;
  running_ = true;
  thread_ = std::thread(&KlineStream::run, this, cb, err_cb);
}

void KlineStream::stop() {
  running_ = false;
  if (thread_.joinable()) thread_.join();
}

void KlineStream::run(CandleCallback cb, ErrorCallback err_cb) {
  const std::string url = "wss://stream.binance.com:9443/ws/" + symbol_ + "@kline_" + interval_;
  std::size_t attempt = 0;

  while (running_) {
    auto ws = ws_factory_();
    if (!ws) {
      Logger::instance().warn("WebSocket support not available; Kline streaming disabled");
      if (err_cb) err_cb();
      running_ = false;
      break;
    }

    std::atomic<bool> error{false};
    ws->setUrl(url);
    ws->setOnMessage([this, cb](const std::string &msg) {
      try {
        auto j = nlohmann::json::parse(msg);
        if (j.contains("k")) {
          auto k = j["k"];
          bool closed = k.value("x", false);
          if (closed) {
            Candle c(
                k.value("t", 0LL),
                std::stod(k.value("o", std::string("0"))),
                std::stod(k.value("h", std::string("0"))),
                std::stod(k.value("l", std::string("0"))),
                std::stod(k.value("c", std::string("0"))),
                std::stod(k.value("v", std::string("0"))),
                k.value("T", 0LL),
                std::stod(k.value("q", std::string("0"))),
                k.value("n", 0),
                std::stod(k.value("V", std::string("0"))),
                std::stod(k.value("Q", std::string("0"))),
                0.0);
            candle_manager_.append_candles(symbol_, interval_, {c});
            if (cb) cb(c);
          }
        }
      } catch (const std::exception &e) {
        Logger::instance().error(std::string("Kline parse error: ") + e.what());
      }
    });
    ws->setOnError([&]() { error = true; });

    ws->start();
    while (running_ && !error) {
      sleep_func_(std::chrono::milliseconds(100));
    }
    ws->stop();

    if (!running_) break;

    if (err_cb) err_cb();

    ++attempt;
    auto delay = base_delay_ * (1 << std::min<std::size_t>(attempt - 1, 8));
    sleep_func_(delay);
  }
}

} // namespace Core

