#include "data_fetcher.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <future>

namespace Core {

std::vector<Candle> DataFetcher::fetch_klines(const std::string& symbol, const std::string& interval, int limit) {
    std::vector<Candle> candles;
    std::string url = "https://api.binance.com/api/v3/klines?symbol=" + symbol +
                       "&interval=" + interval + "&limit=" + std::to_string(limit);
    cpr::Response r = cpr::Get(cpr::Url{url});

    if (r.status_code == 200) {
        try {
            auto json_data = nlohmann::json::parse(r.text);
            for (const auto& kline : json_data) {
                // Binance kline data format:
                // [0] Open time
                // [1] Open
                // [2] High
                // [3] Low
                // [4] Close
                // [5] Volume
                // [6] Close time
                // [7] Quote asset volume
                // [8] Number of trades
                // [9] Taker buy base asset volume
                // [10] Taker buy quote asset volume
                // [11] Ignore
                candles.emplace_back(
                    kline[0].get<long long>(), // Open time
                    std::stod(kline[1].get<std::string>()), // Open
                    std::stod(kline[2].get<std::string>()), // High
                    std::stod(kline[3].get<std::string>()), // Low
                    std::stod(kline[4].get<std::string>()), // Close
                    std::stod(kline[5].get<std::string>()), // Volume
                    kline[6].get<long long>(), // Close time
                    std::stod(kline[7].get<std::string>()), // Quote asset volume
                    kline[8].get<int>(), // Number of trades
                    std::stod(kline[9].get<std::string>()), // Taker buy base asset volume
                    std::stod(kline[10].get<std::string>()), // Taker buy quote asset volume
                    std::stod(kline[11].get<std::string>())  // Ignore
                );
            }
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error processing kline data: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "HTTP Request failed with status code: " << r.status_code << std::endl;
        std::cerr << "Response text: " << r.text << std::endl;
    }

    return candles;
}

std::future<std::vector<Candle>> DataFetcher::fetch_klines_async(const std::string& symbol, const std::string& interval, int limit) {
    return std::async(std::launch::async, [symbol, interval, limit]() {
        return fetch_klines(symbol, interval, limit);
    });
}

} // namespace Core
