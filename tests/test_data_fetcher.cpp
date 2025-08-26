#include "core/data_fetcher.h"
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <chrono>
#include <map>
#include <mutex>
#include <unordered_map>
#include <algorithm>


class DummyLimiter : public Core::IRateLimiter {
public:
  void acquire() override {}
};

class FakeHttpClient : public Core::IHttpClient {
public:
  std::vector<Core::HttpResponse> responses;
  std::unordered_map<std::string, Core::HttpResponse> url_responses;
  std::vector<std::string> urls;
  size_t index{0};
  std::mutex mtx;

  void set_response(const std::string &url, const Core::HttpResponse &resp) {
    url_responses[url] = resp;
  }

  Core::HttpResponse get(const std::string &url,
                         std::chrono::milliseconds /*timeout*/,
                         const std::map<std::string, std::string> & /*headers*/) override {
    std::lock_guard<std::mutex> lock(mtx);
    urls.push_back(url);
    auto it = url_responses.find(url);
    if (it != url_responses.end())
      return it->second;
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

TEST(DataFetcherTest, ReturnsNetworkErrorWhenUninitialized) {
  Core::DataFetcher fetcher(nullptr, nullptr);
  auto res1 = fetcher.fetch_klines("BTCUSDT", "1m", 1);
  EXPECT_EQ(res1.error, Core::FetchError::NetworkError);
  auto res2 = fetcher.fetch_klines_alt("BTCUSDT", "5s", 1);
  EXPECT_EQ(res2.error, Core::FetchError::NetworkError);
  auto sym = fetcher.fetch_all_symbols();
  EXPECT_EQ(sym.error, Core::FetchError::NetworkError);
  auto iv = fetcher.fetch_all_intervals();
  EXPECT_EQ(iv.error, Core::FetchError::NetworkError);
}

TEST(DataFetcherTest, NetworkErrorWhenHttpClientNull) {
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(nullptr, limiter);
  auto res = fetcher.fetch_klines("BTCUSDT", "1m", 1);
  EXPECT_EQ(res.error, Core::FetchError::NetworkError);
}

TEST(DataFetcherTest, NetworkErrorWhenRateLimiterNull) {
  auto http = std::make_shared<FakeHttpClient>();
  Core::DataFetcher fetcher(http, nullptr);
  auto res = fetcher.fetch_klines("BTCUSDT", "1m", 1);
  EXPECT_EQ(res.error, Core::FetchError::NetworkError);
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

TEST(DataFetcherTest, RejectsInvalidInterval) {
  auto http = std::make_shared<FakeHttpClient>();
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_klines("BTCUSDT", "", 1);
  EXPECT_EQ(res.error, Core::FetchError::InvalidInterval);
  EXPECT_TRUE(res.candles.empty());
  EXPECT_EQ(http->urls.size(), 0u);
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
  http->set_response(
      "https://api.binance.com/api/v3/exchangeInfo",
      {200, R"({"symbols":[{"symbol":"AAA"}]})", "", false});
  http->set_response("https://api.binance.com/api/v3/ticker/24hr",
                     {0, "", "net fail", true});
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_all_symbols();
  EXPECT_EQ(res.error, Core::FetchError::NetworkError);
}

TEST(DataFetcherTest, FetchAllSymbolsParallelSuccess) {
  auto http = std::make_shared<FakeHttpClient>();
  http->set_response(
      "https://api.binance.com/api/v3/exchangeInfo",
      {200, R"({"symbols":[{"symbol":"AAA"},{"symbol":"BBB"}]})", "", false});
  http->set_response(
      "https://api.binance.com/api/v3/ticker/24hr",
      {200,
       R"([{"symbol":"AAA","quoteVolume":"5"},{"symbol":"BBB","quoteVolume":"10"}])",
       "", false});
  auto limiter = std::make_shared<DummyLimiter>();
  Core::DataFetcher fetcher(http, limiter);
  auto res = fetcher.fetch_all_symbols();
  EXPECT_EQ(res.error, Core::FetchError::None);
  ASSERT_EQ(res.symbols.size(), 2u);
  EXPECT_EQ(res.symbols[0], "BBB");
  EXPECT_EQ(res.symbols[1], "AAA");
  // both endpoints should have been queried once
  EXPECT_EQ(http->urls.size(), 2u);
  EXPECT_TRUE(std::find(http->urls.begin(), http->urls.end(),
                        "https://api.binance.com/api/v3/exchangeInfo") !=
              http->urls.end());
  EXPECT_TRUE(std::find(http->urls.begin(), http->urls.end(),
                        "https://api.binance.com/api/v3/ticker/24hr") !=
              http->urls.end());
}

