#include "hyperliquid_data_provider.h"

#include "core/logger.h"
#include "core/interval_utils.h"
#include "core/candle_utils.h"
#include <nlohmann/json.hpp>

namespace Core {

namespace {
// Basic mapping: strip common quote suffixes to obtain Hyperliquid coin name.
std::string to_hyperliquid_coin(const std::string &symbol) {
  const std::string s = symbol;
  auto ends_with = [&](const char* suf) {
    size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size()-n, n, suf) == 0;
  };
  if (ends_with("USDT")) return s.substr(0, s.size()-4);
  if (ends_with("USD")) return s.substr(0, s.size()-3);
  return s;
}
}

HyperliquidDataProvider::HyperliquidDataProvider(std::shared_ptr<IHttpClient> http_client,
                                             std::shared_ptr<IRateLimiter> rate_limiter)
    : http_client_(std::move(http_client)),
      rate_limiter_(std::move(rate_limiter)) {}

KlinesResult HyperliquidDataProvider::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {

  if (!http_client_ || !rate_limiter_) {
    Logger::instance().error(
        "HyperliquidDataProvider not initialized with http_client or rate_limiter");
    return {FetchError::NetworkError, 0, "HyperliquidDataProvider not initialized", {}};
  }

  auto interval_ms = parse_interval(interval).count();
  if (interval_ms <= 0) {
    Logger::instance().error("Invalid interval: " + interval);
    return {FetchError::InvalidInterval, 0, "Invalid interval", {}};
  }

  long long end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  long long start_time = end_time - interval_ms * limit;

  nlohmann::json req_body = {
      {"type", "candleSnapshot"},
      {"req",
       {
           {"coin", to_hyperliquid_coin(symbol)},
           {"interval", interval},
           {"startTime", start_time},
           {"endTime", end_time},
       }},
  };

  const std::string url = "https://api.hyperliquid.xyz/info";
  std::vector<Candle> candles;
  int http_status = 0;

  for (int attempt = 0; attempt < max_retries; ++attempt) {
    rate_limiter_->acquire();
    HttpResponse r = http_client_->post(url, req_body.dump(), http_timeout_, {{"Content-Type", "application/json"}});

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
        for (const auto &kline : json_data) {
          candles.emplace_back(
              kline["t"].get<long long>(),
              std::stod(kline["o"].get<std::string>()),
              std::stod(kline["h"].get<std::string>()),
              std::stod(kline["l"].get<std::string>()),
              std::stod(kline["c"].get<std::string>()),
              std::stod(kline["v"].get<std::string>()),
              kline["T"].get<long long>(),
              0.0, // quote_asset_volume
              kline["n"].get<int>(),
              0.0, // taker_buy_base_asset_volume
              0.0, // taker_buy_quote_asset_volume
              0.0  // ignore
          );
        }
        return {FetchError::None, http_status, "", candles};
      } catch (const std::exception &e) {
        Logger::instance().error(
            std::string("Error processing Hyperliquid kline data: ") + e.what());
        return {FetchError::ParseError, http_status, e.what(), {}};
      }
    }

    Logger::instance().error("Hyperliquid HTTP Request failed with status code: " +
                             std::to_string(r.status_code));
    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(retry_delay);
    } else {
      return {FetchError::HttpError, r.status_code, r.error_message, {}};
    }
  }

  return {FetchError::HttpError, http_status, "Max retries exceeded", {}};
}

KlinesResult HyperliquidDataProvider::fetch_range(
    const std::string &symbol, const std::string &interval, long long start_ms,
    long long end_ms, int max_retries,
    std::chrono::milliseconds retry_delay) const {
  if (!http_client_ || !rate_limiter_) {
    Logger::instance().error(
        "HyperliquidDataProvider not initialized with http_client or rate_limiter");
    return {FetchError::NetworkError, 0, "HyperliquidDataProvider not initialized", {}};
  }

  auto interval_ms = parse_interval(interval).count();
  if (interval_ms <= 0) {
    Logger::instance().error("Invalid interval: " + interval);
    return {FetchError::InvalidInterval, 0, "Invalid interval", {}};
  }

  nlohmann::json req_body = {
      {"type", "candleSnapshot"},
      {"req",
       {
           {"coin", to_hyperliquid_coin(symbol)},
           {"interval", interval},
           {"startTime", start_ms},
           {"endTime", end_ms},
       }},
  };

  const std::string url = "https://api.hyperliquid.xyz/info";
  std::vector<Candle> candles;
  int http_status = 0;

  for (int attempt = 0; attempt < max_retries; ++attempt) {
    rate_limiter_->acquire();
    HttpResponse r =
        http_client_->post(url, req_body.dump(), http_timeout_, {{"Content-Type", "application/json"}});

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
        for (const auto &kline : json_data) {
          candles.emplace_back(
              kline["t"].get<long long>(),
              std::stod(kline["o"].get<std::string>()),
              std::stod(kline["h"].get<std::string>()),
              std::stod(kline["l"].get<std::string>()),
              std::stod(kline["c"].get<std::string>()),
              std::stod(kline["v"].get<std::string>()),
              kline["T"].get<long long>(),
              0.0,
              kline["n"].get<int>(),
              0.0,
              0.0,
              0.0);
        }
        return {FetchError::None, http_status, "", candles};
      } catch (const std::exception &e) {
        Logger::instance().error(
            std::string("Error processing Hyperliquid kline data: ") + e.what());
        return {FetchError::ParseError, http_status, e.what(), {}};
      }
    }

    Logger::instance().error("Hyperliquid HTTP Request failed with status code: " +
                             std::to_string(r.status_code));
    if (attempt < max_retries - 1) {
      std::this_thread::sleep_for(retry_delay);
    } else {
      return {FetchError::HttpError, r.status_code, r.error_message, {}};
    }
  }

  return {FetchError::HttpError, http_status, "Max retries exceeded", {}};
}

SymbolsResult HyperliquidDataProvider::fetch_all_symbols(int /*max_retries*/,
                                                        std::chrono::milliseconds /*retry_delay*/,
                                                        std::size_t top_n) const {
  // Hyperliquid "info" API does not expose a simple, stable public endpoint
  // for all symbols in the same way as centralized exchanges. Start with a
  // curated default list (top market cap/volume) expressed in common
  // USDT-quoted notation to keep UI consistent. Mapping to Hyperliquid coin
  // names is handled by to_hyperliquid_coin().
  static const std::vector<std::string> kDefaults = {
      "BTCUSDT", "ETHUSDT", "SOLUSDT", "BNBUSDT", "XRPUSDT",
      "DOGEUSDT", "TONUSDT", "ADAUSDT", "AVAXUSDT", "LINKUSDT",
      "TRXUSDT",  "DOTUSDT", "NEARUSDT", "MATICUSDT", "ATOMUSDT",
      "APTUSDT",  "ARBUSDT",  "PEPEUSDT", "OPUSDT",   "SUIUSDT"};

  std::vector<std::string> out;
  out.reserve(std::min(top_n, kDefaults.size()));
  for (size_t i = 0; i < kDefaults.size() && out.size() < top_n; ++i)
    out.push_back(kDefaults[i]);
  return {FetchError::None, 200, "", out};
}

IntervalsResult HyperliquidDataProvider::fetch_intervals(int /*max_retries*/,
                                                         std::chrono::milliseconds /*retry_delay*/) const {
  // Provide a conservative set known to be widely supported for spot/futures
  // style data. If Hyperliquid adds more granular intervals, they can be added
  // here and in interval parsing logic.
  static const std::vector<std::string> intervals = {
      "1m", "5m", "15m", "1h", "4h", "1d"};
  return {FetchError::None, 200, "", intervals};
}

} // namespace Core
