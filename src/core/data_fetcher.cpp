#include "data_fetcher.h"
#include <chrono>
#include <cpr/cpr.h>
#include <future>
#include "logger.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <set>

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

long long interval_to_ms(const std::string &interval) {
  if (interval.empty())
    return 0;
  char unit = interval.back();
  long long value = 0;
  try {
    value = std::stoll(interval.substr(0, interval.size() - 1));
  } catch (...) {
    return 0;
  }
  switch (unit) {
  case 's':
    return value * 1000LL;
  case 'm':
    return value * 60LL * 1000LL;
  case 'h':
    return value * 60LL * 60LL * 1000LL;
  case 'd':
    return value * 24LL * 60LL * 60LL * 1000LL;
  case 'w':
    return value * 7LL * 24LL * 60LL * 60LL * 1000LL;
  default:
    return 0;
  }
}
} // namespace

namespace Core {

KlinesResult DataFetcher::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay,
    std::chrono::milliseconds request_pause) {
  const std::string base_url =
      "https://api.binance.com/api/v3/klines?symbol=" + symbol +
      "&interval=" + interval;
  std::vector<Candle> all_candles;
  long long interval_ms = interval_to_ms(interval);
  auto now = std::chrono::system_clock::now();
  long long current_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
          .count();
  long long end_time = current_ms / interval_ms * interval_ms - 1;
  int http_status = 0;

  while (static_cast<int>(all_candles.size()) < limit) {
    int batch_limit =
        std::min(1000, limit - static_cast<int>(all_candles.size()));
    long long start_time = end_time - interval_ms * batch_limit + 1;
    std::string url = base_url + "&startTime=" + std::to_string(start_time) +
                      "&endTime=" + std::to_string(end_time) + "&limit=" +
                      std::to_string(batch_limit);

    bool success = false;
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
      http_status = r.status_code;
      if (r.status_code == 200) {
        try {
          std::vector<Candle> candles;
          auto json_data = nlohmann::json::parse(r.text);
          for (const auto &kline : json_data) {
            candles.emplace_back(
                kline[0].get<long long>(),
                std::stod(kline[1].get<std::string>()),
                std::stod(kline[2].get<std::string>()),
                std::stod(kline[3].get<std::string>()),
                std::stod(kline[4].get<std::string>()),
                std::stod(kline[5].get<std::string>()),
                kline[6].get<long long>(),
                std::stod(kline[7].get<std::string>()), kline[8].get<int>(),
                std::stod(kline[9].get<std::string>()),
                std::stod(kline[10].get<std::string>()),
                std::stod(kline[11].get<std::string>()));
          }
          if (candles.empty()) {
            return {FetchError::None, http_status, "", all_candles};
          }
          all_candles.insert(all_candles.begin(), candles.begin(),
                             candles.end());
          end_time = candles.front().open_time - 1;
          success = true;
          break;
        } catch (const std::exception &e) {
          Logger::instance().error(std::string("Error processing kline data: ") +
                                   e.what());
          return {FetchError::ParseError, http_status, e.what(), {}};
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
    if (!success) {
      return {FetchError::HttpError, http_status, "Max retries exceeded", {}};
    }
  }
  return {FetchError::None, http_status, "", all_candles};
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
    std::chrono::milliseconds request_pause, std::size_t top_n) {
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

        // Fetch 24h ticker statistics to sort by volume
        const std::string ticker_url =
            "https://api.binance.com/api/v3/ticker/24hr";
        throttle(request_pause);
        cpr::Response t = cpr::Get(cpr::Url{ticker_url});
        if (t.error.code == cpr::ErrorCode::OK && t.status_code == 200) {
          try {
            auto tickers = nlohmann::json::parse(t.text);
            std::vector<std::pair<std::string, double>> vols;
            vols.reserve(tickers.size());
            for (const auto &tk : tickers) {
              if (!tk.contains("symbol") || !tk.contains("quoteVolume"))
                continue;
              double vol = 0.0;
              try {
                vol = std::stod(tk["quoteVolume"].get<std::string>());
              } catch (...) {
                continue;
              }
              vols.emplace_back(tk["symbol"].get<std::string>(), vol);
            }
            std::sort(vols.begin(), vols.end(),
                      [](const auto &a, const auto &b) {
                        return a.second > b.second;
                      });
            std::vector<std::string> top_symbols;
            for (size_t i = 0; i < std::min(top_n, vols.size()); ++i) {
              top_symbols.push_back(vols[i].first);
            }
            return {FetchError::None, r.status_code, "", top_symbols};
          } catch (const std::exception &e) {
            Logger::instance().error(std::string("Error processing ticker data: ") +
                                    e.what());
            // fall through to return unsorted symbols
          }
        } else {
          Logger::instance().error("Ticker request failed: " +
                                   t.error.message);
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

IntervalsResult DataFetcher::fetch_all_intervals(
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
        std::set<std::string> intervals;
        auto json_data = nlohmann::json::parse(r.text);
        if (json_data.contains("symbols")) {
          for (const auto &item : json_data["symbols"]) {
            if (item.contains("klineIntervals")) {
              for (const auto &iv : item["klineIntervals"]) {
                intervals.insert(iv.get<std::string>());
              }
            }
          }
        }
        return {FetchError::None, r.status_code, "",
                std::vector<std::string>(intervals.begin(), intervals.end())};
      } catch (const std::exception &e) {
        Logger::instance().error(std::string("Error processing interval list: ") +
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
