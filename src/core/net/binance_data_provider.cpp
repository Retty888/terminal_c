#include "binance_data_provider.h"

#include "core/logger.h"
#include "core/interval_utils.h"
#include "core/candle_utils.h"
#include <future>
#include <set>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace Core {

BinanceDataProvider::BinanceDataProvider(std::shared_ptr<IHttpClient> http_client,
                                       std::shared_ptr<IRateLimiter> rate_limiter)
    : http_client_(std::move(http_client)),
      rate_limiter_(std::move(rate_limiter)) {}

KlinesResult BinanceDataProvider::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {

  if (!http_client_ || !rate_limiter_) {
    Logger::instance().error(
        "BinanceDataProvider not initialized with http_client or rate_limiter");
    return {FetchError::NetworkError, 0, "BinanceDataProvider not initialized", {}};
  }

  const std::string base_url = "https://api.binance.com/api/v3/klines?symbol=" + symbol + "&interval=" + interval;
  std::vector<Candle> all_candles;
  all_candles.reserve(limit);
  auto interval_ms = parse_interval(interval).count();
  if (interval_ms <= 0) {
    Logger::instance().error("Invalid interval: " + interval);
    return {FetchError::InvalidInterval, 0, "Invalid interval", {}};
  }

  long long end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
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
      HttpResponse r = http_client_->get(url, http_timeout_, {});
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
          auto get_ll = [](const nlohmann::json &v) -> long long {
            if (v.is_number_integer() || v.is_number_unsigned())
              return v.get<long long>();
            if (v.is_string())
              return std::stoll(v.get<std::string>());
            if (v.is_number_float())
              return static_cast<long long>(v.get<double>());
            return 0LL;
          };
          auto get_d = [](const nlohmann::json &v) -> double {
            if (v.is_number())
              return v.get<double>();
            if (v.is_string())
              return std::stod(v.get<std::string>());
            return 0.0;
          };
          for (auto it = json_data.rbegin(); it != json_data.rend(); ++it) {
            const auto &kline = *it;
            all_candles.push_back(Candle(
                get_ll(kline[0]),
                get_d(kline[1]),
                get_d(kline[2]),
                get_d(kline[3]),
                get_d(kline[4]),
                get_d(kline[5]),
                get_ll(kline[6]),
                get_d(kline[7]),
                kline[8].is_number() ? kline[8].get<int>()
                                      : std::stoi(kline[8].get<std::string>()),
                get_d(kline[9]),
                get_d(kline[10]),
                get_d(kline[11])));
          }
          end_time = get_ll(json_data.front()[0]) - 1;
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

KlinesResult BinanceDataProvider::fetch_range(
    const std::string &symbol, const std::string &interval, long long start_ms,
    long long end_ms, int max_retries,
    std::chrono::milliseconds retry_delay) const {
  if (!http_client_ || !rate_limiter_) {
    Logger::instance().error(
        "BinanceDataProvider not initialized with http_client or rate_limiter");
    return {FetchError::NetworkError, 0, "BinanceDataProvider not initialized", {}};
  }

  auto interval_ms = parse_interval(interval).count();
  if (interval_ms <= 0) {
    Logger::instance().error("Invalid interval: " + interval);
    return {FetchError::InvalidInterval, 0, "Invalid interval", {}};
  }

  const std::string base_url =
      "https://api.binance.com/api/v3/klines?symbol=" + symbol +
      "&interval=" + interval;

  std::vector<Candle> all_candles;
  int http_status = 0;
  long long cur_end = end_ms;

  while (cur_end >= start_ms) {
    long long span = 1000LL * interval_ms; // placeholder; not used for limit
    (void)span;
    long long window = 1000LL * interval_ms; // placeholder; not used
    (void)window;
    long long approx_start = cur_end - (interval_ms * 1000) + 1; // not exact
    (void)approx_start;
    long long cur_start = std::max(start_ms, cur_end - interval_ms * 1000 + 1);
    std::string url = base_url + "&startTime=" + std::to_string(cur_start) +
                      "&endTime=" + std::to_string(cur_end) + "&limit=1000";

    bool success = false;
    for (int attempt = 0; attempt < max_retries; ++attempt) {
      rate_limiter_->acquire();
      HttpResponse r = http_client_->get(url, http_timeout_, {});
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
            // nothing more in this window, stop
            cur_end = cur_start - 1;
            success = true;
            break;
          }
          auto get_ll = [](const nlohmann::json &v) -> long long {
            if (v.is_number_integer() || v.is_number_unsigned())
              return v.get<long long>();
            if (v.is_string())
              return std::stoll(v.get<std::string>());
            if (v.is_number_float())
              return static_cast<long long>(v.get<double>());
            return 0LL;
          };
          auto get_d = [](const nlohmann::json &v) -> double {
            if (v.is_number())
              return v.get<double>();
            if (v.is_string())
              return std::stod(v.get<std::string>());
            return 0.0;
          };

          long long earliest = LLONG_MAX;
          for (auto it = json_data.rbegin(); it != json_data.rend(); ++it) {
            const auto &kline = *it;
            all_candles.push_back(Candle(
                get_ll(kline[0]), get_d(kline[1]), get_d(kline[2]),
                get_d(kline[3]), get_d(kline[4]), get_d(kline[5]),
                get_ll(kline[6]), get_d(kline[7]),
                kline[8].is_number() ? kline[8].get<int>()
                                      : std::stoi(kline[8].get<std::string>()),
                get_d(kline[9]), get_d(kline[10]), get_d(kline[11])));
            earliest = std::min(earliest, get_ll(kline[0]));
          }
          if (earliest == LLONG_MAX || earliest <= start_ms) {
            success = true;
            cur_end = start_ms - 1; // exit
          } else {
            cur_end = earliest - 1;
            success = true;
          }
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

SymbolsResult BinanceDataProvider::fetch_all_symbols(
    int max_retries, std::chrono::milliseconds retry_delay,
    std::size_t top_n) const {
  if (!http_client_ || !rate_limiter_) {
    Logger::instance().error(
        "BinanceDataProvider not initialized with http_client or rate_limiter");
    return {FetchError::NetworkError, 0, "BinanceDataProvider not initialized", {}};
  }
  const std::string info_url = "https://api.binance.com/api/v3/exchangeInfo";
  const std::string ticker_url = "https://api.binance.com/api/v3/ticker/24hr";

  for (int attempt = 0; attempt < max_retries; ++attempt) {
    auto ticker_future = std::async(std::launch::async, [this, &ticker_url]() {
      rate_limiter_->acquire();
      return http_client_->get(ticker_url, http_timeout_, {});
    });

    rate_limiter_->acquire();
    HttpResponse info_resp =
        http_client_->get(info_url, http_timeout_, {});
    HttpResponse ticker_resp = ticker_future.get();

    if (info_resp.network_error) {
      Logger::instance().error("Request error: " + info_resp.error_message);
      if (attempt < max_retries - 1) {
        std::this_thread::sleep_for(retry_delay);
        continue;
      }
      return {FetchError::NetworkError, 0, info_resp.error_message, {}};
    }
    if (info_resp.status_code != 200) {
      Logger::instance().error("HTTP Request failed with status code: " +
                               std::to_string(info_resp.status_code));
      if (attempt < max_retries - 1) {
        std::this_thread::sleep_for(retry_delay);
        continue;
      }
      return {FetchError::HttpError, info_resp.status_code,
              info_resp.error_message, {}};
    }

    std::vector<std::string> symbols;
    try {
      auto json_data = nlohmann::json::parse(info_resp.text);
      for (const auto &item : json_data["symbols"]) {
        symbols.push_back(item["symbol"].get<std::string>());
      }
    } catch (const std::exception &e) {
      Logger::instance().error(
          std::string("Error processing symbol list: ") + e.what());
      return {FetchError::ParseError, info_resp.status_code, e.what(), {}};
    }

    if (ticker_resp.network_error) {
      Logger::instance().error("Ticker request failed: " +
                               ticker_resp.error_message);
      return {FetchError::NetworkError, 0, ticker_resp.error_message, symbols};
    }
    if (ticker_resp.status_code != 200) {
      Logger::instance().error(
          "Ticker request failed with status code: " +
          std::to_string(ticker_resp.status_code));
      return {FetchError::HttpError, ticker_resp.status_code,
              ticker_resp.error_message, symbols};
    }

    try {
      auto tickers = nlohmann::json::parse(ticker_resp.text);
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
                [](const auto &a, const auto &b) { return a.second > b.second; });
      std::vector<std::string> top_symbols;
      for (size_t i = 0; i < std::min(top_n, vols.size()); ++i) {
        top_symbols.push_back(vols[i].first);
      }
      return {FetchError::None, info_resp.status_code, "", top_symbols};
    } catch (const std::exception &e) {
      Logger::instance().error(
          std::string("Error processing ticker data: ") + e.what());
      return {FetchError::ParseError, info_resp.status_code,
              "Ticker parse error", {}};
    }
  }

  return {FetchError::HttpError, 0, "Max retries exceeded", {}};
}

IntervalsResult BinanceDataProvider::fetch_intervals(
    int max_retries, std::chrono::milliseconds retry_delay) const {
  if (!http_client_ || !rate_limiter_) {
    Logger::instance().error(
        "BinanceDataProvider not initialized with http_client or rate_limiter");
    return {FetchError::NetworkError, 0, "BinanceDataProvider not initialized", {}};
  }
  const std::string url = "https://api.binance.com/api/v3/exchangeInfo";
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    rate_limiter_->acquire();
    HttpResponse r = http_client_->get(url, http_timeout_, {});
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
