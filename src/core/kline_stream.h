#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include "candle.h"
#include "candle_manager.h"

namespace Core {
class KlineStream {
public:
  using CandleCallback = std::function<void(const Candle&)>;
  using ErrorCallback = std::function<void()>;

  KlineStream(const std::string &symbol, const std::string &interval,
              CandleManager &manager);
  ~KlineStream();

  void start(CandleCallback cb, ErrorCallback err_cb = nullptr);
  void stop();
  bool running() const { return running_; }

private:
  void run(CandleCallback cb, ErrorCallback err_cb);

  std::string symbol_;
  std::string interval_;
  CandleManager &candle_manager_;
  std::thread thread_;
  std::atomic<bool> running_{false};
};
} // namespace Core

