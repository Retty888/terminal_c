#include "core/kline_stream.h"
#include "core/candle_manager.h"
#include <gtest/gtest.h>
#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>
#include <chrono>

using namespace Core;

class MockWebSocket : public IWebSocket {
public:
  explicit MockWebSocket(int &starts) : starts_(starts) {}
  void setUrl(const std::string &) override {}
  void setOnMessage(MessageCallback) override {}
  void setOnError(ErrorCallback cb) override { err_cb_ = std::move(cb); }
  void start() override {
    starts_++;
    if (err_cb_) {
      std::thread([cb = err_cb_]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cb();
      }).detach();
    }
  }
  void stop() override {}

private:
  int &starts_;
  ErrorCallback err_cb_;
};

TEST(KlineStreamTest, ReconnectsWithBackoff) {
  std::filesystem::path tmp = std::filesystem::temp_directory_path() / "kline_stream_test";
  CandleManager mgr(tmp);
  int starts = 0;
  auto factory = [&]() { return std::make_unique<MockWebSocket>(starts); };
  std::vector<int> delays;
  auto sleep_fn = [&](std::chrono::milliseconds d) {
    if (d.count() < 50)
      delays.push_back(static_cast<int>(d.count()));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  KlineStream ks("btcusdt", "1m", mgr, factory, sleep_fn, std::chrono::milliseconds(1));
  std::atomic<int> errors{0};
  ks.start(nullptr, [&]() { errors++; });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  ks.stop();

  EXPECT_GE(starts, 2);
  EXPECT_GE(errors.load(), 1);
  if (delays.size() >= 2) {
    EXPECT_EQ(delays[0], 1);
    EXPECT_EQ(delays[1], 2);
  }
}

