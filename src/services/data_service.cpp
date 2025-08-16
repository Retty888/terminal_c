#include "services/data_service.h"

#include "config.h"
#include "core/candle_manager.h"
#include "core/data_fetcher.h"

std::vector<std::string>
DataService::load_selected_pairs(const std::string &filename) const {
  return Config::load_selected_pairs(filename);
}

void DataService::save_selected_pairs(
    const std::string &filename, const std::vector<std::string> &pairs) const {
  Config::save_selected_pairs(filename, pairs);
}

Core::SymbolsResult DataService::fetch_all_symbols(
    int max_retries, std::chrono::milliseconds retry_delay,
    std::chrono::milliseconds request_pause, std::size_t top_n) const {
  return Core::DataFetcher::fetch_all_symbols(max_retries, retry_delay,
                                             request_pause, top_n);
}

Core::IntervalsResult DataService::fetch_intervals(
    int max_retries, std::chrono::milliseconds retry_delay,
    std::chrono::milliseconds request_pause) const {
  return Core::DataFetcher::fetch_all_intervals(max_retries, retry_delay,
                                               request_pause);
}

Core::KlinesResult DataService::fetch_klines(
    const std::string &symbol, const std::string &interval, int limit,
    int max_retries, std::chrono::milliseconds retry_delay,
    std::chrono::milliseconds request_pause) const {
  return Core::DataFetcher::fetch_klines(symbol, interval, limit, max_retries,
                                        retry_delay, request_pause);
}

std::future<Core::KlinesResult>
DataService::fetch_klines_async(const std::string &symbol,
                                const std::string &interval, int limit,
                                int max_retries,
                                std::chrono::milliseconds retry_delay,
                                std::chrono::milliseconds request_pause) const {
  return Core::DataFetcher::fetch_klines_async(symbol, interval, limit,
                                               max_retries, retry_delay,
                                               request_pause);
}

std::vector<Core::Candle>
DataService::load_candles(const std::string &pair,
                          const std::string &interval) const {
  return Core::CandleManager::load_candles(pair, interval);
}

void DataService::save_candles(
    const std::string &pair, const std::string &interval,
    const std::vector<Core::Candle> &candles) const {
  Core::CandleManager::save_candles(pair, interval, candles);
}

void DataService::append_candles(
    const std::string &pair, const std::string &interval,
    const std::vector<Core::Candle> &candles) const {
  Core::CandleManager::append_candles(pair, interval, candles);
}

