#include "kline_stream.h"

#include <chrono>
#include <nlohmann/json.hpp>

#include "candle_manager.h"
#include "logger.h"

#if __has_include(<ixwebsocket/IXWebSocket.h>)
#define HAS_IXWEBSOCKET 1
#include <ixwebsocket/IXWebSocket.h>
#endif

namespace Core {

KlineStream::KlineStream(const std::string &symbol, const std::string &interval)
    : symbol_(symbol), interval_(interval) {}

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
#ifdef HAS_IXWEBSOCKET
  ix::WebSocket ws;
  std::string url = "wss://stream.binance.com:9443/ws/" + symbol_ + "@kline_" + interval_;
  ws.setUrl(url);
  ws.setOnMessage([this, cb, err_cb](const ix::WebSocketMessagePtr &msg) {
    if (msg->type == ix::WebSocketMessageType::Message) {
      try {
        auto j = nlohmann::json::parse(msg->str);
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
            CandleManager::append_candles(symbol_, interval_, {c});
            if (cb) cb(c);
          }
        }
      } catch (const std::exception &e) {
        Logger::instance().error(std::string("Kline parse error: ") + e.what());
      }
    } else if (msg->type == ix::WebSocketMessageType::Error) {
      Logger::instance().error("Kline stream error: " + msg->errorInfo.reason);
      running_ = false;
      if (err_cb) err_cb();
    }
  });
  ws.start();
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  ws.stop();
#else
  Logger::instance().warn("WebSocket support not available; Kline streaming disabled");
  running_ = false;
  if (err_cb) err_cb();
#endif
}

} // namespace Core

