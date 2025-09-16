#pragma once

#include "idata_provider.h"
#include "ihttp_client.h"
#include "irate_limiter.h"
#include <memory>

namespace Core {

class BinanceDataProvider : public IDataProvider {
public:
  BinanceDataProvider(std::shared_ptr<IHttpClient> http_client,
                      std::shared_ptr<IRateLimiter> rate_limiter);

  KlinesResult fetch_klines(const std::string &symbol, const std::string &interval,
                            int limit = 500, int max_retries = 3,
                            std::chrono::milliseconds retry_delay =
                                std::chrono::milliseconds(1000)) const override;

  KlinesResult fetch_range(const std::string &symbol, const std::string &interval,
                           long long start_ms, long long end_ms,
                           int max_retries = 3,
                           std::chrono::milliseconds retry_delay =
                               std::chrono::milliseconds(1000)) const override;

  SymbolsResult fetch_all_symbols(int max_retries = 3,
                                std::chrono::milliseconds retry_delay =
                                    std::chrono::milliseconds(1000),
                                std::size_t top_n = 100) const override;

  IntervalsResult fetch_intervals(int max_retries = 3,
                                  std::chrono::milliseconds retry_delay =
                                      std::chrono::milliseconds(1000)) const override;

private:
  std::shared_ptr<IHttpClient> http_client_;
  std::shared_ptr<IRateLimiter> rate_limiter_;
  std::chrono::milliseconds http_timeout_{std::chrono::milliseconds(15000)};
};

} // namespace Core
