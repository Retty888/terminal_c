#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>

#include "candle.h"
#include "candle_manager.h"
#include "iwebsocket.h"

namespace Core {
class KlineStream : public std::enable_shared_from_this<KlineStream> {
public:
  using CandleCallback = std::function<void(const Candle &)>;
  using ErrorCallback = std::function<void()>;
  using UICallback = std::function<void(const std::string &)>;
  using SleepFunc = std::function<void(std::chrono::milliseconds)>;

  KlineStream(
      const std::string &symbol, const std::string &interval,
      CandleManager &manager,
      WebSocketFactory ws_factory = default_websocket_factory(),
      SleepFunc sleep_func = nullptr,
      std::chrono::milliseconds base_delay = std::chrono::milliseconds(1000),
      const std::string &provider = std::string("binance"));
  ~KlineStream();

  void start(CandleCallback cb, ErrorCallback err_cb = nullptr,
             UICallback ui_cb = nullptr);
  void stop();
  bool running() const { return running_; }

private:
  void run(CandleCallback cb, ErrorCallback err_cb, UICallback ui_cb);

  std::string symbol_;
  std::string interval_;
  std::string provider_;
  CandleManager &candle_manager_;
  WebSocketFactory ws_factory_;
  SleepFunc sleep_func_;
  std::chrono::milliseconds base_delay_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::mutex ws_mutex_;
  std::unique_ptr<IWebSocket> ws_;
  std::atomic<int> callbacks_inflight_{0};
  std::mutex cb_mutex_;
  std::condition_variable cb_cv_;
};
} // namespace Core
