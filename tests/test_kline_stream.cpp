#include "core/kline_stream.h"
#include "core/candle_manager.h"
#include <gtest/gtest.h>
#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>
#include <chrono>


class MockWebSocket : public Core::IWebSocket {
public:
  explicit MockWebSocket(int &starts) : starts_(starts) {}
  void setUrl(const std::string &) override {}
  void setOnMessage(MessageCallback) override {}
  void setOnError(ErrorCallback cb) override { err_cb_ = std::move(cb); }
  void setOnClose(CloseCallback cb) override { close_cb_ = std::move(cb); }
  void start() override {
    starts_++;
    std::thread([ec = err_cb_, cc = close_cb_]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (ec) ec();
      if (cc) cc();
    }).detach();
  }
  void stop() override {
    if (close_cb_) close_cb_();
  }

private:
  int &starts_;
  ErrorCallback err_cb_;
  CloseCallback close_cb_;
};

TEST(KlineStreamTest, ReconnectsWithBackoff) {
  std::filesystem::path tmp = std::filesystem::temp_directory_path() / "kline_stream_test";
  Core::CandleManager mgr(tmp);
  int starts = 0;
  auto factory = [&]() { return std::make_unique<MockWebSocket>(starts); };
  std::vector<int> delays;
  auto sleep_fn = [&](std::chrono::milliseconds d) {
    if (d.count() < 50)
      delays.push_back(static_cast<int>(d.count()));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  Core::KlineStream ks("btcusdt", "1m", mgr, factory, sleep_fn, std::chrono::milliseconds(1));
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

class BlockingWebSocket : public Core::IWebSocket {
public:
  void setUrl(const std::string &) override {}
  void setOnMessage(MessageCallback) override {}
  void setOnError(ErrorCallback) override {}
  void setOnClose(CloseCallback cb) override { close_cb_ = std::move(cb); }
  void start() override {}
  void stop() override {
    if (close_cb_) close_cb_();
  }

private:
  CloseCallback close_cb_;
};

TEST(KlineStreamTest, StopsPromptly) {
  std::filesystem::path tmp = std::filesystem::temp_directory_path() / "kline_stream_test2";
  Core::CandleManager mgr(tmp);
  auto factory = []() { return std::make_unique<BlockingWebSocket>(); };
  std::atomic<int> sleeps{0};
  auto sleep_fn = [&](std::chrono::milliseconds d) {
    ++sleeps;
    std::this_thread::sleep_for(d);
  };

  Core::KlineStream ks("btcusdt", "1m", mgr, factory, sleep_fn, std::chrono::milliseconds(1));
  ks.start(nullptr, nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  auto t0 = std::chrono::steady_clock::now();
  ks.stop();
  auto elapsed = std::chrono::steady_clock::now() - t0;
  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 20);
  EXPECT_LE(sleeps.load(), 1);
}

