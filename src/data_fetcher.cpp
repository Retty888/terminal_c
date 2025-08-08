#include "data_fetcher.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <iostream>

namespace Core {

std::vector<Candle> DataFetcher::fetch_klines(const std::string& symbol, const std::string& interval, int limit) {
    std::vector<Candle> candles;

    std::string url = "https://api.binance.com/api/v3/klines?symbol=" + symbol +
                      "&interval=" + interval + "&limit=" + std::to_string(limit);

    auto response = cpr::Get(cpr::Url{url});
    if (response.status_code != 200) {
        std::cerr << "Failed to fetch data for " << symbol << " — status " << response.status_code << "\n";
        return candles;
    }

    try {
        auto json = nlohmann::json::parse(response.text);
        for (const auto& k : json) {
            candles.emplace_back(
                k[0].get<long long>(),
                std::stod(k[1].get<std::string>()),
                std::stod(k[2].get<std::string>()),
                std::stod(k[3].get<std::string>()),
                std::stod(k[4].get<std::string>()),
                std::stod(k[5].get<std::string>()),
                k[6].get<long long>(),
                std::stod(k[7].get<std::string>()),
                k[8].get<int>(),
                std::stod(k[9].get<std::string>()),
                std::stod(k[10].get<std::string>()),
                std::stod(k[11].get<std::string>())
            );
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing Binance response: " << e.what() << "\n";
    }

    return candles;
}

} // namespace Core
