#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <memory>

#include "candle.h"
#include "candle_manager.h"
#include "iwebsocket.h"

namespace Core {
class KlineStream {
public:
  using CandleCallback = std::function<void(const Candle&)>;
  using ErrorCallback = std::function<void()>;
  using SleepFunc = std::function<void(std::chrono::milliseconds)>;

  KlineStream(const std::string &symbol, const std::string &interval,
              CandleManager &manager,
              WebSocketFactory ws_factory = default_websocket_factory(),
              SleepFunc sleep_func = nullptr,
              std::chrono::milliseconds base_delay = std::chrono::milliseconds(1000));
  ~KlineStream();

  void start(CandleCallback cb, ErrorCallback err_cb = nullptr);
  void stop();
  bool running() const { return running_; }

private:
  void run(CandleCallback cb, ErrorCallback err_cb);

  std::string symbol_;
  std::string interval_;
  CandleManager &candle_manager_;
  WebSocketFactory ws_factory_;
  SleepFunc sleep_func_;
  std::chrono::milliseconds base_delay_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::mutex ws_mutex_;
  std::unique_ptr<IWebSocket> ws_;
};
} // namespace Core

