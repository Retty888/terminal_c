#include "services/data_service.h"

#include "core/candle_manager.h"
#include "core/interval_utils.h"
#include "core/logger.h"
#include "core/candle_utils.h"
#include "config_manager.h"
#include "config_path.h"
#include "core/exchange_utils.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <thread>

DataService::DataService()
    : http_client_(std::make_shared<Core::CprHttpClient>()),
      rate_limiter_(std::make_shared<Core::TokenBucketRateLimiter>(
          1, std::chrono::milliseconds(1100))),
      fetcher_(http_client_, rate_limiter_) {}

DataService::DataService(const std::filesystem::path &data_dir)
    : http_client_(std::make_shared<Core::CprHttpClient>()),
      rate_limiter_(std::make_shared<Core::TokenBucketRateLimiter>(
          1, std::chrono::milliseconds(1100))),
      fetcher_(http_client_, rate_limiter_),
      candle_manager_(data_dir) {}

Core::SymbolsResult DataService::fetch_all_symbols(
    int max_retries, std::chrono::milliseconds retry_delay,
    std::size_t top_n) const {
  return fetcher_.fetch_all_symbols(max_retries, retry_delay, top_n);
}

Core::IntervalsResult DataService::fetch_intervals(
    int max_retries, std::chrono::milliseconds retry_delay) const {
  return fetcher_.fetch_all_intervals(max_retries, retry_delay);
}

Core::KlinesResult DataService::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
  return fetcher_.fetch_klines(symbol, interval, limit, max_retries,
                               retry_delay);
}

Core::KlinesResult DataService::fetch_klines_alt(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
  auto cfg = Config::ConfigManager::load(resolve_config_path().string());
  std::string fallback = cfg ? cfg->fallback_provider : "";
  std::chrono::milliseconds current_delay = retry_delay;
  Core::KlinesResult res;
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    res = fetcher_.fetch_klines_alt(symbol, interval, limit, 1, current_delay);
    if (res.error == Core::FetchError::None)
      return res;

    if (!fallback.empty()) {
      auto fb = fetcher_.fetch_klines(symbol, interval, limit, 1, current_delay);
      if (fb.error == Core::FetchError::None)
        return fb;
      res = fb;
    }

    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(current_delay);
      current_delay *= 2;
    }
  }
  return res;
}

Core::KlinesResult DataService::fetch_range(
    const std::string &symbol, const std::string &interval,
    long long start_time, long long end_time, int max_retries,
    std::chrono::milliseconds retry_delay) const {
  std::vector<Core::Candle> result;
  long long interval_ms = Core::parse_interval(interval).count();
  if (interval_ms <= 0 || start_time > end_time)
    return {Core::FetchError::None, 0, "", result};

  int http_status = 0;
  if (interval == "5s" || interval == "15s") {
    std::string pair = to_gate_symbol(symbol);
    while (start_time <= end_time) {
      long long batch_end =
          std::min(end_time, start_time + interval_ms * 999);
      std::string url =
          "https://api.gateio.ws/api/v4/spot/candlesticks?currency_pair=" +
          pair + "&interval=" + interval + "&from=" +
          std::to_string(start_time / 1000) + "&to=" +
          std::to_string((batch_end + interval_ms) / 1000);
      bool success = false;
      auto current_delay = retry_delay;
      for (int attempt = 0; attempt < max_retries; ++attempt) {
        rate_limiter_->acquire();
        Core::HttpResponse r = http_client_->get(url);
        if (r.network_error) {
          Core::Logger::instance().error("Alt range request error: " +
                                         r.error_message);
          if (attempt < max_retries - 1) {
            std::this_thread::sleep_for(current_delay);
            current_delay *= 2;
            continue;
          }
          return {Core::FetchError::NetworkError, 0, r.error_message, {}};
        }
        http_status = r.status_code;
        if (r.status_code == 200) {
          try {
            std::vector<Core::Candle> candles;
            auto json_data = nlohmann::json::parse(r.text);
            for (const auto &kline : json_data) {
              long long ts =
                  static_cast<long long>(
                      std::stoll(kline[0].get<std::string>())) *
                  1000LL;
              double volume = std::stod(kline[1].get<std::string>());
              double close = std::stod(kline[2].get<std::string>());
              double high = std::stod(kline[3].get<std::string>());
              double low = std::stod(kline[4].get<std::string>());
              double open = std::stod(kline[5].get<std::string>());
              candles.emplace_back(ts, open, high, low, close, volume,
                                   ts + interval_ms - 1, 0.0, 0, 0.0, 0.0, 0.0);
            }
            std::reverse(candles.begin(), candles.end());
            if (candles.empty())
              return {Core::FetchError::None, http_status, "", result};
            result.insert(result.end(), candles.begin(), candles.end());
            start_time = result.back().open_time + interval_ms;
            success = true;
            break;
          } catch (const std::exception &e) {
            Core::Logger::instance().error(
                std::string("Alt range parse error: ") + e.what());
            return {Core::FetchError::ParseError, http_status, e.what(), {}};
          }
        }
        Core::Logger::instance().error(
            "Alt range HTTP Request failed with status code: " +
            std::to_string(r.status_code));
        if (attempt < max_retries - 1) {
          std::this_thread::sleep_for(current_delay);
          current_delay *= 2;
        } else {
          return {Core::FetchError::HttpError, r.status_code, r.error_message,
                  {}};
        }
      }
      if (!success) {
        return {Core::FetchError::HttpError, http_status,
                "Max retries exceeded", {}};
      }
    }
  } else {
    const std::string base_url =
        "https://api.binance.com/api/v3/klines?symbol=" + symbol +
        "&interval=" + interval;
    while (start_time <= end_time) {
      long long batch_end =
          std::min(end_time, start_time + interval_ms * 999);
      std::string url = base_url + "&startTime=" + std::to_string(start_time) +
                        "&endTime=" + std::to_string(batch_end) +
                        "&limit=1000";
      bool success = false;
      auto current_delay = retry_delay;
      for (int attempt = 0; attempt < max_retries; ++attempt) {
        rate_limiter_->acquire();
        Core::HttpResponse r = http_client_->get(url);
        if (r.network_error) {
          Core::Logger::instance().error("Range request error: " +
                                         r.error_message);
          if (attempt < max_retries - 1) {
            std::this_thread::sleep_for(current_delay);
            current_delay *= 2;
            continue;
          }
          return {Core::FetchError::NetworkError, 0, r.error_message, {}};
        }
        http_status = r.status_code;
        if (r.status_code == 200) {
          try {
            auto json_data = nlohmann::json::parse(r.text);
            if (json_data.empty())
              return {Core::FetchError::None, http_status, "", result};
            for (const auto &kline : json_data) {
              result.emplace_back(
                  kline[0].get<long long>(),
                  std::stod(kline[1].get<std::string>()),
                  std::stod(kline[2].get<std::string>()),
                  std::stod(kline[3].get<std::string>()),
                  std::stod(kline[4].get<std::string>()),
                  std::stod(kline[5].get<std::string>()),
                  kline[6].get<long long>(),
                  std::stod(kline[7].get<std::string>()),
                  kline[8].get<int>(),
                  std::stod(kline[9].get<std::string>()),
                  std::stod(kline[10].get<std::string>()),
                  std::stod(kline[11].get<std::string>()));
            }
            if (result.empty())
              return {Core::FetchError::None, http_status, "", result};
            start_time = result.back().open_time + interval_ms;
            success = true;
            break;
          } catch (const std::exception &e) {
            Core::Logger::instance().error(
                std::string("Range parse error: ") + e.what());
            return {Core::FetchError::ParseError, http_status, e.what(), {}};
          }
        }
        Core::Logger::instance().error(
            "Range HTTP Request failed with status code: " +
            std::to_string(r.status_code));
        if (attempt < max_retries - 1) {
          std::this_thread::sleep_for(current_delay);
          current_delay *= 2;
        } else {
          return {Core::FetchError::HttpError, r.status_code, r.error_message,
                  {}};
        }
      }
      if (!success) {
        return {Core::FetchError::HttpError, http_status,
                "Max retries exceeded", {}};
      }
    }
  }

  Core::fill_missing(result, interval_ms);
  return {Core::FetchError::None, http_status, "", result};
}

std::future<Core::KlinesResult> DataService::fetch_klines_async(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
  return fetcher_.fetch_klines_async(symbol, interval, limit, max_retries,
                                     retry_delay);
}

std::vector<Core::Candle> DataService::load_candles(
    const std::string &pair, const std::string &interval) const {
  return candle_manager_.load_candles(pair, interval);
}

void DataService::save_candles(const std::string &pair,
                               const std::string &interval,
                               const std::vector<Core::Candle> &candles) const {
  candle_manager_.save_candles(pair, interval, candles);
}

void DataService::append_candles(
    const std::string &pair, const std::string &interval,
    const std::vector<Core::Candle> &candles) const {
  candle_manager_.append_candles(pair, interval, candles);
}

bool DataService::remove_candles(const std::string &pair) const {
  return candle_manager_.remove_candles(pair);
}

bool DataService::reload_candles(const std::string &pair, const std::string &interval) const {
  candle_manager_.clear_interval(pair, interval);
  auto cfg = Config::ConfigManager::load(resolve_config_path().string());
  int limit = cfg ? static_cast<int>(cfg->candles_limit) : 1000;
  auto res = fetch_klines(pair, interval, limit);
  if (res.error == Core::FetchError::None && !res.candles.empty()) {
    candle_manager_.save_candles(pair, interval, res.candles);
    return true;
  }
  return false;
}

std::vector<std::string> DataService::list_stored_data() const {
  return candle_manager_.list_stored_data();
}

