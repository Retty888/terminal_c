#include "data_fetcher.h"

#include "core/logger.h"
#include "interval_utils.h"
#include "candle_utils.h"
#include "core/exchange_utils.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <thread>

namespace {

std::string map_gate_interval(const std::string &interval) {
  static const std::unordered_map<std::string, std::string> mapping{{"5s", "10s"}};
  static const std::unordered_set<std::string> supported{
      "10s", "15s", "30s", "1m", "5m", "15m", "30m", "1h", "4h",
      "1d", "1w", "1M"};

  auto it = mapping.find(interval);
  if (it != mapping.end())
    return it->second;
  return supported.count(interval) ? interval : std::string{};
}

} // namespace

namespace Core {

DataFetcher::DataFetcher(std::shared_ptr<IHttpClient> http_client,
                         std::shared_ptr<IRateLimiter> rate_limiter)
    : http_client_(std::move(http_client)),
      rate_limiter_(std::move(rate_limiter)) {}

KlinesResult DataFetcher::fetch_klines_from_api(
    const std::string &prefix, const std::string &symbol,
    const std::string &interval, int limit, int max_retries,
    std::chrono::milliseconds retry_delay) const {
  const std::string base_url = prefix + symbol + "&interval=" + interval;
  std::vector<Candle> all_candles;
  all_candles.reserve(limit);
  auto interval_ms = parse_interval(interval).count();

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
      rate_limiter_->acquire();
      HttpResponse r = http_client_->get(url);
      if (r.network_error) {
        Logger::instance().error("Request error: " + r.error_message);
        if (attempt < max_retries - 1) {
          std::this_thread::sleep_for(retry_delay);
          continue;
        }
        return {FetchError::NetworkError, 0, r.error_message, {}};
      }
      http_status = r.status_code;
      if (r.status_code == 200) {
        try {
          auto json_data = nlohmann::json::parse(r.text);
          if (json_data.empty()) {
            std::reverse(all_candles.begin(), all_candles.end());
            fill_missing(all_candles, interval_ms);
            return {FetchError::None, http_status, "", all_candles};
          }
          for (auto it = json_data.rbegin(); it != json_data.rend(); ++it) {
            const auto &kline = *it;
            all_candles.push_back(Candle(
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
                std::stod(kline[11].get<std::string>())));
          }
          end_time = json_data.front()[0].get<long long>() - 1;
          success = true;
          break;
        } catch (const std::exception &e) {
          Logger::instance().error(
              std::string("Error processing kline data: ") + e.what());
          return {FetchError::ParseError, http_status, e.what(), {}};
        }
      }
      Logger::instance().error("HTTP Request failed with status code: " +
                               std::to_string(r.status_code));
      if (attempt < max_retries - 1) {
        std::this_thread::sleep_for(retry_delay);
      } else {
        return {FetchError::HttpError, r.status_code, r.error_message, {}};
      }
    }
    if (!success) {
      return {FetchError::HttpError, http_status, "Max retries exceeded", {}};
    }
  }
  std::reverse(all_candles.begin(), all_candles.end());
  fill_missing(all_candles, interval_ms);
  return {FetchError::None, http_status, "", all_candles};
}

KlinesResult DataFetcher::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
  if (interval == "5s" || interval == "15s") {
    return fetch_klines_alt(symbol, interval, limit, max_retries, retry_delay);
  }
  auto res = fetch_klines_from_api(
      "https://api.binance.com/api/v3/klines?symbol=", symbol, interval, limit,
      max_retries, retry_delay);
  if (res.error != FetchError::None) {
    return fetch_klines_alt(symbol, interval, limit, max_retries, retry_delay);
  }
  fill_missing(res.candles, parse_interval(interval).count());
  return res;
}

KlinesResult DataFetcher::fetch_klines_alt(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
  std::string mapped = map_gate_interval(interval);
  if (mapped.empty())
    return {FetchError::HttpError, 0, "Unsupported interval", {}};

  std::string pair = to_gate_symbol(symbol);
  std::vector<Candle> all_candles;
  auto interval_ms = parse_interval(mapped).count();
  long long end_ts =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  int http_status = 0;

  while (static_cast<int>(all_candles.size()) < limit) {
    int batch_limit = std::min(1000, limit - static_cast<int>(all_candles.size()));
    std::string url =
        "https://api.gateio.ws/api/v4/spot/candlesticks?currency_pair=" +
        pair + "&limit=" + std::to_string(batch_limit) + "&interval=" + mapped +
        "&to=" + std::to_string(end_ts);

    bool success = false;
    for (int attempt = 0; attempt < max_retries; ++attempt) {
      rate_limiter_->acquire();
      HttpResponse r = http_client_->get(url);
      if (r.network_error) {
        Logger::instance().error("Alt request error: " + r.error_message);
        if (attempt < max_retries - 1) {
          std::this_thread::sleep_for(retry_delay);
          continue;
        }
        return {FetchError::NetworkError, 0, r.error_message, {}};
      }
      http_status = r.status_code;
      if (r.status_code == 200) {
        try {
          std::vector<Candle> candles;
          auto json_data = nlohmann::json::parse(r.text);
          for (const auto &kline : json_data) {
            long long ts =
                static_cast<long long>(std::stoll(kline[0].get<std::string>())) *
                1000LL;
            double volume = std::stod(kline[1].get<std::string>());
            double close = std::stod(kline[2].get<std::string>());
            double high = std::stod(kline[3].get<std::string>());
            double low = std::stod(kline[4].get<std::string>());
            double open = std::stod(kline[5].get<std::string>());
            candles.emplace_back(ts, open, high, low, close, volume,
                                 ts + interval_ms - 1, 0.0, 0, 0.0, 0.0, 0.0);
          }
          if (candles.empty()) {
            fill_missing(all_candles, interval_ms);
            return {FetchError::None, http_status, "", all_candles};
          }
          all_candles.insert(all_candles.begin(), candles.begin(),
                             candles.end());
          end_ts = candles.front().open_time / 1000 -
                   (interval_ms / 1000);
          success = true;
          break;
        } catch (const std::exception &e) {
          Logger::instance().error(std::string("Alt kline parse error: ") +
                                   e.what() + " Body: " + r.text);
          return {FetchError::ParseError, r.status_code, e.what(), {}};
        }
      }
      Logger::instance().error("Alt HTTP Request failed. Status: " +
                               std::to_string(r.status_code) +
                               ", body: " + r.text);
      if (attempt < max_retries - 1)
        std::this_thread::sleep_for(retry_delay);
      else
        return {FetchError::HttpError, r.status_code, r.text, {}};
    }
    if (!success) {
      return {FetchError::HttpError, http_status, "Max retries exceeded", {}};
    }
  }
  fill_missing(all_candles, interval_ms);
  return {FetchError::None, http_status, "", all_candles};
}

std::future<KlinesResult> DataFetcher::fetch_klines_async(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
  return std::async(std::launch::async,
                    [this, symbol, interval, limit, max_retries, retry_delay]() {
                      return fetch_klines(symbol, interval, limit, max_retries,
                                          retry_delay);
                    });
}

SymbolsResult DataFetcher::fetch_all_symbols(
    int max_retries, std::chrono::milliseconds retry_delay,
    std::size_t top_n) const {
  const std::string url = "https://api.binance.com/api/v3/exchangeInfo";
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    rate_limiter_->acquire();
    HttpResponse r = http_client_->get(url);
    if (r.network_error) {
      Logger::instance().error("Request error: " + r.error_message);
      if (attempt < max_retries - 1) {
        std::this_thread::sleep_for(retry_delay);
        continue;
      }
      return {FetchError::NetworkError, 0, r.error_message, {}};
    }
    if (r.status_code == 200) {
      try {
        std::vector<std::string> symbols;
        auto json_data = nlohmann::json::parse(r.text);
        for (const auto &item : json_data["symbols"]) {
          symbols.push_back(item["symbol"].get<std::string>());
        }

        const std::string ticker_url =
            "https://api.binance.com/api/v3/ticker/24hr";
        rate_limiter_->acquire();
        HttpResponse t = http_client_->get(ticker_url);
        if (!t.network_error && t.status_code == 200) {
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
            Logger::instance().error(
                std::string("Error processing ticker data: ") + e.what());
          }
        } else {
          Logger::instance().error("Ticker request failed: " +
                                   t.error_message);
        }
        return {FetchError::None, r.status_code, "", symbols};
      } catch (const std::exception &e) {
        Logger::instance().error(
            std::string("Error processing symbol list: ") + e.what());
        return {FetchError::ParseError, r.status_code, e.what(), {}};
      }
    }
    Logger::instance().error("HTTP Request failed with status code: " +
                             std::to_string(r.status_code));
    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(retry_delay);
    } else {
      return {FetchError::HttpError, r.status_code, r.error_message, {}};
    }
  }
  return {FetchError::HttpError, 0, "Max retries exceeded", {}};
}

IntervalsResult DataFetcher::fetch_all_intervals(
    int max_retries, std::chrono::milliseconds retry_delay) const {
  const std::string url = "https://api.binance.com/api/v3/exchangeInfo";
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    rate_limiter_->acquire();
    HttpResponse r = http_client_->get(url);
    if (r.network_error) {
      Logger::instance().error("Request error: " + r.error_message);
      if (attempt < max_retries - 1) {
        std::this_thread::sleep_for(retry_delay);
        continue;
      }
      return {FetchError::NetworkError, 0, r.error_message, {}};
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
        Logger::instance().error(
            std::string("Error processing interval list: ") + e.what());
        return {FetchError::ParseError, r.status_code, e.what(), {}};
      }
    }
    Logger::instance().error("HTTP Request failed with status code: " +
                             std::to_string(r.status_code));
    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(retry_delay);
    } else {
      return {FetchError::HttpError, r.status_code, r.error_message, {}};
    }
  }
  return {FetchError::HttpError, 0, "Max retries exceeded", {}};
}

} // namespace Core

