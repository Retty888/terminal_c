#include "ui/echarts_serializer.h"

nlohmann::json SerializeCandles(const std::vector<Core::Candle>& candles) {
    nlohmann::json data = nlohmann::json::array();
    for (const auto& c : candles) {
        data.push_back({c.open, c.close, c.low, c.high});
    }
    return data;
}

