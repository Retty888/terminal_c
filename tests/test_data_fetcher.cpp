#include "core/data_fetcher.h"
#include "core/net/i_http_client.h"
#include "core/net/i_rate_limiter.h"
#include <gtest/gtest.h>
#include <chrono>
#include <memory>

using namespace Core;

class FakeHttpClient : public Net::IHttpClient {
public:
  Net::HttpResponse response;
  Net::HttpResponse get(const std::string &url) override { return response; }
};

class NoopRateLimiter : public Net::IRateLimiter {
public:
  void acquire() override {}
};

static const char *kKlineJson =
    R"([[0,"1","1","1","1","1",1,"0",0,"0","0","0"]])";

TEST(DataFetcherTest, ParsesKlines) {
  auto client = std::make_shared<FakeHttpClient>();
  client->response.status_code = 200;
  client->response.text = kKlineJson;
  auto limiter = std::make_shared<NoopRateLimiter>();
  DataFetcher fetcher(client, limiter);
  auto res =
      fetcher.fetch_klines("BTCUSDT", "1m", 1, 1, std::chrono::milliseconds(0));
  EXPECT_EQ(res.error, FetchError::None);
  ASSERT_EQ(res.candles.size(), 1u);
  EXPECT_EQ(res.candles[0].open_time, 0);
  EXPECT_DOUBLE_EQ(res.candles[0].open, 1.0);
}

TEST(DataFetcherTest, AltFetchWorks) {
  auto client = std::make_shared<FakeHttpClient>();
  client->response.status_code = 200;
  client->response.text = kKlineJson;
  auto limiter = std::make_shared<NoopRateLimiter>();
  DataFetcher fetcher(client, limiter);
  auto res =
      fetcher.fetch_klines_alt("BTCUSDT", "1m", 1, 1, std::chrono::milliseconds(0));
  EXPECT_EQ(res.error, FetchError::None);
  ASSERT_EQ(res.candles.size(), 1u);
}

TEST(DataFetcherTest, AsyncFetch) {
  auto client = std::make_shared<FakeHttpClient>();
  client->response.status_code = 200;
  client->response.text = kKlineJson;
  auto limiter = std::make_shared<NoopRateLimiter>();
  DataFetcher fetcher(client, limiter);
  auto fut = fetcher.fetch_klines_async("BTCUSDT", "1m", 1, 1,
                                        std::chrono::milliseconds(0));
  auto res = fut.get();
  EXPECT_EQ(res.error, FetchError::None);
  ASSERT_EQ(res.candles.size(), 1u);
}
