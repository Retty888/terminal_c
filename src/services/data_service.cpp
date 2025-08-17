#include "services/data_service.h"

#include "core/candle_manager.h"

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
  return fetcher_.fetch_klines_alt(symbol, interval, limit, max_retries,
                                   retry_delay);
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

std::vector<std::string> DataService::list_stored_data() const {
  return candle_manager_.list_stored_data();
}

