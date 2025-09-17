#include "services/data_service.h"

#include "config_manager.h"
#include "config_path.h"
#include "core/data_dir.h"
#include "core/candle_manager.h"
#include "core/candle_utils.h"
#include "core/exchange_utils.h"
#include "core/interval_utils.h"
#include "core/logger.h"
#include "core/candle_utils.h"
#include "core/net/binance_data_provider.h"
#include "core/net/hyperliquid_data_provider.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>

namespace {
constexpr const char *kDefaultProvider = "Hyperliquid";
}

DataService::DataService()
    : http_client_(std::make_shared<Core::CprHttpClient>()),
      rate_limiter_(std::make_shared<Core::TokenBucketRateLimiter>(
          1, std::chrono::milliseconds(1100))),
      candle_manager_(Core::resolve_data_dir()) {
  persist_allowed_after_ = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  // Binance disabled per pivot to Hyperliquid-only
  // register_provider("Binance", std::make_shared<Core::BinanceDataProvider>(http_client_, rate_limiter_));
  register_provider("Hyperliquid", std::make_shared<Core::HyperliquidDataProvider>(http_client_, rate_limiter_));
  if (!set_active_provider(kDefaultProvider)) {
    Core::Logger::instance().error("Default provider 'Hyperliquid' is not registered");
  }
  apply_configured_provider();
}

DataService::DataService(const std::filesystem::path &data_dir)
    : http_client_(std::make_shared<Core::CprHttpClient>()),
      rate_limiter_(std::make_shared<Core::TokenBucketRateLimiter>(
          1, std::chrono::milliseconds(1100))),
      candle_manager_(data_dir) {
  persist_allowed_after_ = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  // Binance disabled per pivot to Hyperliquid-only
  // register_provider("Binance", std::make_shared<Core::BinanceDataProvider>(http_client_, rate_limiter_));
  register_provider("Hyperliquid", std::make_shared<Core::HyperliquidDataProvider>(http_client_, rate_limiter_));
  if (!set_active_provider(kDefaultProvider)) {
    Core::Logger::instance().error("Default provider 'Hyperliquid' is not registered");
  }
  apply_configured_provider();
}

void DataService::register_provider(const std::string &name, std::shared_ptr<Core::IDataProvider> provider) {
  const auto key = normalize_provider_name(name);
  providers_[key] = ProviderRecord{name, std::move(provider)};
}

bool DataService::set_active_provider(const std::string &name) {
  const auto key = normalize_provider_name(name);
  auto it = providers_.find(key);
  if (it != providers_.end() && it->second.provider) {
    active_provider_key_ = key;
    return true;
  }
  return false;
}

std::vector<std::string> DataService::get_provider_names() const {
  std::vector<std::string> names;
  names.reserve(providers_.size());
  for (const auto &entry : providers_) {
    names.push_back(entry.second.display_name);
  }
  return names;
}

std::string DataService::get_active_provider_name() const {
  if (const auto *record = active_provider_record()) {
    return record->display_name;
  }
  return std::string();
}

Core::SymbolsResult
DataService::fetch_all_symbols(int max_retries,
                               std::chrono::milliseconds retry_delay,
                               std::size_t top_n) const {
  if (const auto *record = active_provider_record()) {
    return record->provider->fetch_all_symbols(max_retries, retry_delay, top_n);
  }
  return {Core::FetchError::NetworkError, 0, "No active provider", {}};
}

Core::IntervalsResult
DataService::fetch_intervals(int max_retries,
                             std::chrono::milliseconds retry_delay) const {
  if (const auto *record = active_provider_record()) {
    return record->provider->fetch_intervals(max_retries, retry_delay);
  }
  return {Core::FetchError::NetworkError, 0, "No active provider", {}};
}

const Config::ConfigData &DataService::config() const {
  if (!config_cache_) {
    config_cache_ =
        Config::ConfigManager::load(resolve_config_path().string())
            .value_or(Config::ConfigData{});
  }
  return *config_cache_;
}

Core::KlinesResult DataService::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
  if (const auto *record = active_provider_record()) {
    return record->provider->fetch_klines(symbol, interval, limit, max_retries, retry_delay);
  }
  return {Core::FetchError::NetworkError, 0, "No active provider", {}};
}

Core::KlinesResult DataService::fetch_range(
    const std::string &symbol, const std::string &interval, long long start_ms,
    long long end_ms, int max_retries,
    std::chrono::milliseconds retry_delay) const {
  if (const auto *record = active_provider_record()) {
    return record->provider
        ->fetch_range(symbol, interval, start_ms, end_ms, max_retries, retry_delay);
  }
  return {Core::FetchError::NetworkError, 0, "No active provider", {}};
}

std::future<Core::KlinesResult> DataService::fetch_klines_async(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay) const {
    return std::async(std::launch::async, [=]() {
        return fetch_klines(symbol, interval, limit, max_retries, retry_delay);
    });
}

const DataService::ProviderRecord *DataService::active_provider_record() const {
  if (!active_provider_key_) {
    return nullptr;
  }
  auto it = providers_.find(*active_provider_key_);
  if (it == providers_.end()) {
    return nullptr;
  }
  return &it->second;
}

std::string DataService::normalize_provider_name(const std::string &name) {
  std::string normalized;
  normalized.reserve(name.size());
  for (unsigned char c : name) {
    normalized.push_back(static_cast<char>(std::tolower(c)));
  }
  return normalized;
}

void DataService::apply_configured_provider() {
  const auto &cfg = config();
  const std::string requested = cfg.primary_provider;
  if (!requested.empty() && set_active_provider(requested)) {
    return;
  }

  if (!requested.empty()) {
    if (cfg.fallback_provider) {
      const std::string fallback = *cfg.fallback_provider;
      if (set_active_provider(fallback)) {
        Core::Logger::instance().warn(
            "Primary provider '" + requested + "' not available; switched to fallback '" + fallback + "'");
        return;
      }
      Core::Logger::instance().warn(
          "Primary provider '" + requested + "' not available; fallback provider '" + fallback +
          "' also unavailable; using default '" + std::string(kDefaultProvider) + "'");
    } else {
      Core::Logger::instance().warn(
          "Primary provider '" + requested + "' not available; fallback provider disabled; using default '" +
          std::string(kDefaultProvider) + "'");
    }
  }

  if (!set_active_provider(kDefaultProvider)) {
    Core::Logger::instance().error("Default provider '" + std::string(kDefaultProvider) + "' is not registered");
  }
}

std::vector<Core::Candle>
DataService::load_candles(const std::string &pair,
                          const std::string &interval) const {
  return candle_manager_.load_candles(pair, interval);
}

void DataService::append_candles(const std::string &pair,
                               const std::string &interval,
                               const std::vector<Core::Candle> &candles) const {
  candle_manager_.append_candles(pair, interval, candles);
}

void DataService::overwrite_candles(const std::string &pair, const std::string &interval, const std::vector<Core::Candle> &candles) const
{
    candle_manager_.save_candles(pair, interval, candles);
}

bool DataService::save_if_changed(const std::string &pair, const std::string &interval,
                                  const std::vector<Core::Candle> &candles) const {
  const std::string key = pair + '|' + interval;
  std::size_t n = candles.size();
  long long last = n ? candles.back().open_time : 0LL;
  auto now = std::chrono::steady_clock::now();
  // Global guard: allow persistence only after warm-up unless explicitly overridden by env
  if (now < persist_allowed_after_ && std::getenv("CANDLE_ALLOW_EARLY_SAVE") == nullptr) {
    last_saved_state_[key] = {n, last};
    last_saved_time_[key] = now;
    return false;
  }
  auto itS = last_saved_state_.find(key);
  auto itT = last_saved_time_.find(key);
  bool changed = itS == last_saved_state_.end() || itS->second.first != n || itS->second.second != last;
  bool debounced = itT == last_saved_time_.end() || (now - itT->second) >= save_debounce_;
  if (changed && debounced) {
    candle_manager_.save_candles(pair, interval, candles);
    last_saved_state_[key] = {n, last};
    last_saved_time_[key] = now;
    return true;
  }
  return false;
}

std::vector<std::string> DataService::list_stored_data() const {
  return candle_manager_.list_stored_data();
}

bool DataService::reload_candles(const std::string &pair, const std::string &interval) const {
  // Load fresh candles according to configured limit and overwrite storage.
  int limit = 5000;
  if (config_cache_) {
    limit = static_cast<int>(config_cache_->candles_limit);
  } else {
    auto cfg = Config::ConfigManager::load(resolve_config_path().string());
    if (cfg) limit = static_cast<int>(cfg->candles_limit);
  }
  auto res = fetch_klines(pair, interval, limit);
  if (res.error == Core::FetchError::None && !res.candles.empty()) {
    auto vec = res.candles;
    Core::normalize_candles(vec);
    overwrite_candles(pair, interval, vec);
    return true;
  }
  return false;
}

bool DataService::top_up_recent(const std::string &pair, const std::string &interval) const {
  auto existing = load_candles(pair, interval);
  auto interval_ms = Core::parse_interval(interval).count();
  if (existing.empty() || interval_ms <= 0) {
    return reload_candles(pair, interval);
  }
  long long last_ts = existing.back().open_time;
  long long start = last_ts + interval_ms;
  long long end = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  auto res = fetch_range(pair, interval, start, end);
  if (res.error == Core::FetchError::None && !res.candles.empty()) {
    const std::size_t before_n = existing.size();
    const long long before_last = before_n ? existing.back().open_time : 0LL;
    Core::merge_candles(existing, res.candles);
    const bool changed = existing.size() != before_n || (before_n && existing.back().open_time != before_last);
    if (changed) overwrite_candles(pair, interval, existing);
    return changed;
  }
  return false;
}

bool DataService::ensure_limit(const std::string &pair, const std::string &interval,
                               std::size_t target_count) const {
  auto existing = load_candles(pair, interval);
  auto interval_ms = Core::parse_interval(interval).count();
  if (existing.empty()) {
    return reload_candles(pair, interval);
  }
  if (existing.size() >= target_count || interval_ms <= 0) {
    return true;
  }
  // Backfill older
  std::size_t need = target_count - existing.size();
  long long min_ts = existing.front().open_time;
  long long start = min_ts - static_cast<long long>(need) * interval_ms;
  long long end = min_ts - interval_ms;
  if (end <= 0) end = min_ts - 1;
  auto res = fetch_range(pair, interval, start, end);
  if (res.error == Core::FetchError::None && !res.candles.empty()) {
    const std::size_t before_n = existing.size();
    const long long before_last = before_n ? existing.back().open_time : 0LL;
    Core::merge_candles(existing, res.candles);
    const bool changed = existing.size() != before_n || (before_n && existing.back().open_time != before_last);
    if (changed) overwrite_candles(pair, interval, existing);
    return changed;
  }
  return false;
}


