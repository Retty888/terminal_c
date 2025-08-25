#include "core/data_fetcher.h"
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <chrono>
#include <map>


class DummyLimiter : public Core::IRateLimiter {
public:
  void acquire() override {}
};

class FakeHttpClient : public Core::IHttpClient {
public:
  std::vector<Core::HttpResponse> responses;
  std::vector<std::string> urls;
  size_t index{0};
  Core::HttpResponse get(const std::string &url,
                         std::chrono::milliseconds /*timeout*/,
                         const std::map<std::string, std::string> & /*headers*/) override {
    urls.push_back(url);
    if (index < responses.size())
      return responses[index++];
    return {};
  }
};

static std::string make_gate_response(int start_ts, int count, int interval) {
  nlohmann::json arr = nlohmann::json::array();
  for (int i = 0; i < count; ++i) {
    int ts = start_ts + i * interval;
    arr.push_back({std::to_string(ts), "1", "2", "3", "4", "5"});
  }
  return arr.dump();
}

TEST(DataFetcherTest, FetchKlinesParsesCandles) {
  auto http = std::make_shared<FakeHttpClient>();
  http->responses.push_back({200,
                             "[[0,\"1\",\"2\",\"3\",\"4\",\"5\",0,\"7\",0,\"9\",\"10\",\"11\"]]",
                             "", false});
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_klines("BTCUSDT", "1m", 1);
  EXPECT_EQ(res.error, Core::FetchError::None);
  ASSERT_EQ(res.candles.size(), 1u);
  EXPECT_EQ(res.candles[0].open_time, 0);
}

TEST(DataFetcherTest, AltApiParsesCandles) {
  auto http = std::make_shared<FakeHttpClient>();
  http->responses.push_back({200, "[[\"1\",\"2\",\"3\",\"4\",\"5\",\"6\"]]", "", false});
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_klines_alt("BTCUSDT", "5s", 1);
  EXPECT_EQ(res.error, Core::FetchError::None);
  ASSERT_EQ(res.candles.size(), 1u);
  EXPECT_EQ(res.candles[0].close_time - res.candles[0].open_time + 1,
            10000);
}

TEST(DataFetcherTest, AsyncDelegatesToSync) {
  auto http = std::make_shared<FakeHttpClient>();
  http->responses.push_back({200,
                             "[[0,\"1\",\"2\",\"3\",\"4\",\"5\",0,\"7\",0,\"9\",\"10\",\"11\"]]",
                             "", false});
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto fut = fetcher.fetch_klines_async("BTCUSDT", "1m", 1);
  auto res = fut.get();
  EXPECT_EQ(res.error, Core::FetchError::None);
  ASSERT_EQ(res.candles.size(), 1u);
}

TEST(DataFetcherTest, AltApiBatchesRequestsOverLimit) {
  auto http = std::make_shared<FakeHttpClient>();
  http->responses.push_back({200, make_gate_response(10000, 1000, 10), "", false});
  http->responses.push_back({200, make_gate_response(5000, 500, 10), "", false});
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_klines_alt("BTCUSDT", "5s", 1500);
  EXPECT_EQ(res.error, Core::FetchError::None);
  EXPECT_EQ(res.candles.size(), 1500u);
  EXPECT_EQ(http->urls.size(), 2u);
}

TEST(DataFetcherTest, AltApiRejectsUnsupportedInterval) {
  auto http = std::make_shared<FakeHttpClient>();
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_klines_alt("BTCUSDT", "7s", 1);
  EXPECT_EQ(res.error, Core::FetchError::HttpError);
}

TEST(DataFetcherTest, TopSymbolsTickerFailureReturnsError) {
  auto http = std::make_shared<FakeHttpClient>();
  http->responses.push_back(
      {200, R"({"symbols":[{"symbol":"AAA"}]})", "", false});
  http->responses.push_back({0, "", "net fail", true});
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_all_symbols();
  EXPECT_EQ(res.error, Core::FetchError::NetworkError);
}

