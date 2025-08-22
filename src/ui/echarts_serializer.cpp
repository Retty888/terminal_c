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

// Serialize candle data for TradingView Lightweight Charts. Returns an array of
// objects with Unix timestamps in seconds and OHLC fields.
nlohmann::json SerializeCandlesTV(const std::vector<Core::Candle>& candles) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& c : candles) {
        arr.push_back({
            {"time", c.open_time / 1000LL}, // convert ms to seconds
            {"open", c.open},
            {"high", c.high},
            {"low", c.low},
            {"close", c.close}
        });
    }
    return arr;
}

