#include "ui/echarts_serializer.h"

nlohmann::json SerializeCandles(const std::vector<Core::Candle>& candles) {
    nlohmann::json x = nlohmann::json::array();
    nlohmann::json y = nlohmann::json::array();
    for (const auto& c : candles) {
        x.push_back(c.open_time);
        y.push_back({c.open, c.close, c.low, c.high});
    }
    return nlohmann::json{{"x", x}, {"y", y}};
}

