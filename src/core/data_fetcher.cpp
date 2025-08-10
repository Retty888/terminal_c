#include "data_fetcher.h"
#include <chrono>
#include <cpr/cpr.h>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>
#include <vector>

namespace Core {

std::optional<std::vector<Candle>>
DataFetcher::fetch_klines(const std::string &symbol,
                          const std::string &interval, int limit) {
  const int max_retries = 3;
  const auto retry_delay = std::chrono::seconds(1);
  std::string url = "https://api.binance.com/api/v3/klines?symbol=" + symbol +
                    "&interval=" + interval + "&limit=" + std::to_string(limit);
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    cpr::Response r = cpr::Get(cpr::Url{url});
    if (r.status_code == 200) {
      try {
        std::vector<Candle> candles;
        auto json_data = nlohmann::json::parse(r.text);
        for (const auto &kline : json_data) {
          candles.emplace_back(
              kline[0].get<long long>(), std::stod(kline[1].get<std::string>()),
              std::stod(kline[2].get<std::string>()),
              std::stod(kline[3].get<std::string>()),
              std::stod(kline[4].get<std::string>()),
              std::stod(kline[5].get<std::string>()), kline[6].get<long long>(),
              std::stod(kline[7].get<std::string>()), kline[8].get<int>(),
              std::stod(kline[9].get<std::string>()),
              std::stod(kline[10].get<std::string>()),
              std::stod(kline[11].get<std::string>()));
        }
        return candles;
      } catch (const std::exception &e) {
        std::cerr << "Error processing kline data: " << e.what() << std::endl;
        return std::nullopt;
      }
    }
    std::cerr << "HTTP Request failed with status code: " << r.status_code
              << std::endl;
    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(retry_delay);
    }
  }
  return std::nullopt;
}

std::future<std::optional<std::vector<Candle>>>
DataFetcher::fetch_klines_async(const std::string &symbol,
                                const std::string &interval, int limit) {
  return std::async(std::launch::async, [symbol, interval, limit]() {
    return fetch_klines(symbol, interval, limit);
  });
}

std::optional<std::vector<std::string>> DataFetcher::fetch_all_symbols() {
  const std::string url =
      "https://api.binance.com/api/v3/exchangeInfo";
  cpr::Response r = cpr::Get(cpr::Url{url});
  if (r.status_code == 200) {
    try {
      std::vector<std::string> symbols;
      auto json_data = nlohmann::json::parse(r.text);
      for (const auto &item : json_data["symbols"]) {
        symbols.push_back(item["symbol"].get<std::string>());
      }
      return symbols;
    } catch (const std::exception &e) {
      std::cerr << "Error processing symbol list: " << e.what() << std::endl;
      return std::nullopt;
    }
  }
  std::cerr << "HTTP Request failed with status code: " << r.status_code
            << std::endl;
  return std::nullopt;
}

} // namespace Core
