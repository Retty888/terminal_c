#include "services/data_service.h"

#include "config_manager.h"
#include "config_path.h"
#include "core/candle_manager.h"
#include "core/candle_utils.h"
#include "core/exchange_utils.h"
#include "core/interval_utils.h"
#include "core/logger.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>

namespace {
const std::map<std::string, std::string> kDefaultHeaders{};
} // namespace

DataService::DataService()
    : http_client_(std::make_shared<Core::CprHttpClient>()),
      rate_limiter_(std::make_shared<Core::TokenBucketRateLimiter>(
          1, std::chrono::milliseconds(1100))),
      fetcher_(http_client_, rate_limiter_) {
  // Initialize fetcher timeout from config if available
  const auto &cfg = config();
  fetcher_.set_http_timeout(std::chrono::milliseconds(cfg.http_timeout_ms));
}

DataService::DataService(const std::filesystem::path &data_dir)
    : http_client_(std::make_shared<Core::CprHttpClient>()),
      rate_limiter_(std::make_shared<Core::TokenBucketRateLimiter>(
          1, std::chrono::milliseconds(1100))),
      fetcher_(http_client_, rate_limiter_), candle_manager_(data_dir) {
  const auto &cfg = config();
  fetcher_.set_http_timeout(std::chrono::milliseconds(cfg.http_timeout_ms));
}

const Config::ConfigData &DataService::config() const {
  if (!config_cache_) {
    config_cache_ =
        Config::ConfigManager::load(resolve_config_path().string())
            .value_or(Config::ConfigData{});
  }
  return *config_cache_;
}

std::string DataService::primary_provider() const {
  const auto &cfg = config();
  if (cfg.primary_provider == "binance" || cfg.primary_provider == "gateio")
    return cfg.primary_provider;
  Core::Logger::instance().warn("Unknown primary provider '" + cfg.primary_provider + "', defaulting to binance");
  return "binance";
}

Core::SymbolsResult
DataService::fetch_all_symbols(int max_retries,
                               std::chrono::milliseconds retry_delay,
                               std::size_t top_n) const {
  return fetcher_.fetch_all_symbols(max_retries, retry_delay, top_n);
}

Core::IntervalsResult
DataService::fetch_intervals(int max_retries,
                             std::chrono::milliseconds retry_delay) const {
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
  const Config::ConfigData &cfg = config();
  std::string fallback = cfg.fallback_provider;
  if (fallback != "binance" && fallback != "gateio") {
    Core::Logger::instance().warn("Unknown fallback provider '" + fallback +
                                  "', defaulting to binance");
    fallback = "binance";
  }
  std::chrono::milliseconds current_delay = retry_delay;
  Core::KlinesResult res;
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    if (fallback == "binance") {
      res = fetcher_.fetch_klines(symbol, interval, limit, 1, current_delay);
      if (res.error == Core::FetchError::None)
        return res;
      auto alt =
          fetcher_.fetch_klines_alt(symbol, interval, limit, 1, current_delay);
      if (alt.error == Core::FetchError::None)
        return alt;
      res = alt;
    } else {
      res = fetcher_.fetch_klines_alt(symbol, interval, limit, 1, current_delay);
      if (res.error == Core::FetchError::None)
        return res;
      auto alt =
          fetcher_.fetch_klines(symbol, interval, limit, 1, current_delay);
      if (alt.error == Core::FetchError::None)
        return alt;
      res = alt;
    }

    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(current_delay);
      current_delay *= 2;
    }
  }
  return res;
}

Core::KlinesResult DataService::FetchRangeImpl(
    const std::string &url,
    const std::function<std::vector<Core::Candle>(const std::string &)> &parser,
    int max_retries,
    std::chrono::milliseconds retry_delay) const {
  int http_status = 0;
  auto current_delay = retry_delay;
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    rate_limiter_->acquire();
    Core::HttpResponse r =
        http_client_->get(url, std::chrono::milliseconds(config().http_timeout_ms), kDefaultHeaders);
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
        auto candles = parser(r.text);
        return {Core::FetchError::None, http_status, "", std::move(candles)};
      } catch (const std::exception &e) {
        Core::Logger::instance().error(std::string("Range parse error: ") +
                                       e.what());
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
      return {Core::FetchError::HttpError, r.status_code, r.error_message, {}};
    }
  }
  return {Core::FetchError::HttpError, http_status, "Max retries exceeded", {}};
}

Core::KlinesResult
DataService::fetch_range(const std::string &symbol, const std::string &interval,
                         long long start_time, long long end_time,
                         int max_retries,
                         std::chrono::milliseconds retry_delay) const {
  std::vector<Core::Candle> result;
  long long interval_ms = Core::parse_interval(interval).count();
  if (interval_ms <= 0 || start_time > end_time)
    return {Core::FetchError::None, 0, "", result};

  int http_status = 0;
  if (interval == "5s" || interval == "15s") {
    std::string pair = to_gate_symbol(symbol);
    while (start_time <= end_time) {
      long long batch_end = std::min(end_time, start_time + interval_ms * 999);
      std::string url =
          "https://api.gateio.ws/api/v4/spot/candlesticks?currency_pair=" +
          pair + "&interval=" + interval +
          "&from=" + std::to_string(start_time / 1000) +
          "&to=" + std::to_string((batch_end + interval_ms) / 1000);

      auto parse = [interval_ms](const std::string &text) {
        std::vector<Core::Candle> candles;
        auto json_data = nlohmann::json::parse(text);
        for (const auto &kline : json_data) {
          long long ts = static_cast<long long>(
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
        return candles;
      };

      auto res = FetchRangeImpl(url, parse, max_retries, retry_delay);
      if (res.error != Core::FetchError::None)
        return res;
      if (res.candles.empty())
        return {Core::FetchError::None, res.http_status, "", result};
      result.insert(result.end(), res.candles.begin(), res.candles.end());
      http_status = res.http_status;
      start_time = result.back().open_time + interval_ms;
    }
  } else {
    const std::string base_url =
        "https://api.binance.com/api/v3/klines?symbol=" + symbol +
        "&interval=" + interval;
    while (start_time <= end_time) {
      long long batch_end = std::min(end_time, start_time + interval_ms * 999);
      std::string url = base_url + "&startTime=" + std::to_string(start_time) +
                        "&endTime=" + std::to_string(batch_end) + "&limit=1000";

      auto parse = [](const std::string &text) {
        std::vector<Core::Candle> candles;
        auto json_data = nlohmann::json::parse(text);
        for (const auto &kline : json_data) {
          candles.emplace_back(
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
        return candles;
      };

      auto res = FetchRangeImpl(url, parse, max_retries, retry_delay);
      if (res.error != Core::FetchError::None)
        return res;
      if (res.candles.empty())
        return {Core::FetchError::None, res.http_status, "", result};
      result.insert(result.end(), res.candles.begin(), res.candles.end());
      http_status = res.http_status;
      start_time = result.back().open_time + interval_ms;
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

std::vector<Core::Candle>
DataService::load_candles(const std::string &pair,
                          const std::string &interval) const {
  // Avoid noise and unnecessary work for unsupported sub-minute intervals
  if ((interval == "5s" || interval == "15s") && primary_provider() == "binance") {
    return candle_manager_.load_candles(pair, interval);
  }
  if (!candle_manager_.validate_candles(pair, interval)) {
    Core::Logger::instance().warn("Invalid candles detected for " + pair + " " +
                                  interval + ", reloading");
    candle_manager_.clear_interval(pair, interval);
    Core::Logger::instance().info("Cleared stored candles for " + pair + " " +
                                  interval);
    if (!reload_candles(pair, interval)) {
      Core::Logger::instance().warn("Reload failed for " + pair + " " +
                                    interval);
    }
  }
  return candle_manager_.load_candles(pair, interval);
}

void DataService::save_candles(const std::string &pair,
                               const std::string &interval,
                               const std::vector<Core::Candle> &candles) const {
  candle_manager_.save_candles(pair, interval, candles);
  if (!candles.empty() && !candle_manager_.validate_candles(pair, interval)) {
    Core::Logger::instance().warn("Data mismatch after save for " + pair + " " +
                                  interval);
  }
}

std::vector<Core::Candle>
DataService::load_candles_json(const std::string &pair,
                               const std::string &interval) const {
  return candle_manager_.load_candles_from_json(pair, interval);
}

void DataService::save_candles_json(
    const std::string &pair, const std::string &interval,
    const std::vector<Core::Candle> &candles) const {
  candle_manager_.save_candles_json(pair, interval, candles);
  if (!candles.empty()) {
    auto loaded = candle_manager_.load_candles_from_json(pair, interval);
    if (loaded.size() >= candles.size()) {
      const auto &orig = candles.back();
      const auto &read = loaded[candles.size() - 1];
      if (orig.open_time != read.open_time || orig.open != read.open ||
          orig.close != read.close || orig.volume != read.volume) {
        Core::Logger::instance().warn(
            "Data mismatch after JSON save/load for " + pair + " " + interval);
      }
    } else {
      Core::Logger::instance().warn(
          "Loaded fewer candles than saved (JSON) for " + pair + " " +
          interval);
    }
  }
}

void DataService::append_candles(
    const std::string &pair, const std::string &interval,
    const std::vector<Core::Candle> &candles) const {
  candle_manager_.append_candles(pair, interval, candles);
  if (!candles.empty() && !candle_manager_.validate_candles(pair, interval)) {
    Core::Logger::instance().warn("Data mismatch after append for " + pair +
                                  " " + interval);
  }
}

bool DataService::remove_candles(const std::string &pair) const {
  return candle_manager_.remove_candles(pair);
}

bool DataService::clear_interval(const std::string &pair,
                                 const std::string &interval) const {
  return candle_manager_.clear_interval(pair, interval);
}

bool DataService::reload_candles(const std::string &pair,
                                 const std::string &interval) const {
  // Skip unsupported sub-minute intervals when primary is Binance
  if ((interval == "5s" || interval == "15s") && primary_provider() == "binance") {
    Core::Logger::instance().info("Skipping reload for unsupported interval " + pair + " " + interval + " (primary=binance)");
    return false;
  }
  const Config::ConfigData &cfg = config();
  auto res = fetch_klines(pair, interval, static_cast<int>(cfg.candles_limit));
  if (res.error == Core::FetchError::None && !res.candles.empty()) {
    candle_manager_.clear_interval(pair, interval);
    candle_manager_.save_candles(pair, interval, res.candles);
    Core::Logger::instance().info("Reloaded " + pair + " " + interval);
    return true;
  }
  Core::Logger::instance().warn(
      "Reload failed for " + pair + " " + interval +
      (res.message.empty() ? "" : ": " + res.message));
  return false;
}

std::vector<std::string> DataService::list_stored_data() const {
  return candle_manager_.list_stored_data();
}

std::uintmax_t DataService::get_file_size(const std::string &pair,
                                          const std::string &interval) const {
  return candle_manager_.file_size(pair, interval);
}
