#include "data_fetcher.h"
#include <chrono>
#include <cpr/cpr.h>
#include <future>
#include "logger.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <mutex>

namespace {
std::mutex request_mutex;
std::chrono::steady_clock::time_point last_request =
    std::chrono::steady_clock::now();

void throttle(std::chrono::milliseconds pause) {
  std::unique_lock<std::mutex> lock(request_mutex);
  auto now = std::chrono::steady_clock::now();
  auto next = last_request + pause;
  if (now < next) {
    std::this_thread::sleep_for(next - now);
  }
  last_request = std::chrono::steady_clock::now();
}
} // namespace

namespace Core {

KlinesResult DataFetcher::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay,
    std::chrono::milliseconds request_pause) {
  std::string url = "https://api.binance.com/api/v3/klines?symbol=" + symbol +
                    "&interval=" + interval + "&limit=" +
                    std::to_string(limit);
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    throttle(request_pause);
    cpr::Response r = cpr::Get(cpr::Url{url});
    if (r.error.code != cpr::ErrorCode::OK) {
      Logger::instance().error("Request error: " + r.error.message);
      if (attempt < max_retries - 1) {
        std::this_thread::sleep_for(retry_delay);
        continue;
      }
      return {FetchError::NetworkError, 0, r.error.message, {}};
    }
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
        return {FetchError::None, r.status_code, "", candles};
      } catch (const std::exception &e) {
        Logger::instance().error(std::string("Error processing kline data: ") +
                                e.what());
        return {FetchError::ParseError, r.status_code, e.what(), {}};
      }
    }
    Logger::instance().error("HTTP Request failed with status code: " +
                             std::to_string(r.status_code));
    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(retry_delay);
    } else {
      return {FetchError::HttpError, r.status_code, r.error.message, {}};
    }
  }
  return {FetchError::HttpError, 0, "Max retries exceeded", {}};
}

std::future<KlinesResult> DataFetcher::fetch_klines_async(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay,
    std::chrono::milliseconds request_pause) {
  return std::async(std::launch::async, [symbol, interval, limit, max_retries,
                                        retry_delay, request_pause]() {
    return fetch_klines(symbol, interval, limit, max_retries, retry_delay,
                        request_pause);
  });
}

SymbolsResult DataFetcher::fetch_all_symbols(
    int max_retries, std::chrono::milliseconds retry_delay,
    std::chrono::milliseconds request_pause) {
  const std::string url = "https://api.binance.com/api/v3/exchangeInfo";
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    throttle(request_pause);
    cpr::Response r = cpr::Get(cpr::Url{url});
    if (r.error.code != cpr::ErrorCode::OK) {
      Logger::instance().error("Request error: " + r.error.message);
      if (attempt < max_retries - 1) {
        std::this_thread::sleep_for(retry_delay);
        continue;
      }
      return {FetchError::NetworkError, 0, r.error.message, {}};
    }
    if (r.status_code == 200) {
      try {
        std::vector<std::string> symbols;
        auto json_data = nlohmann::json::parse(r.text);
        for (const auto &item : json_data["symbols"]) {
          symbols.push_back(item["symbol"].get<std::string>());
        }
        return {FetchError::None, r.status_code, "", symbols};
      } catch (const std::exception &e) {
        Logger::instance().error(std::string("Error processing symbol list: ") +
                                e.what());
        return {FetchError::ParseError, r.status_code, e.what(), {}};
      }
    }
    Logger::instance().error("HTTP Request failed with status code: " +
                             std::to_string(r.status_code));
    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(retry_delay);
    } else {
      return {FetchError::HttpError, r.status_code, r.error.message, {}};
    }
  }
  return {FetchError::HttpError, 0, "Max retries exceeded", {}};
}

} // namespace Core
