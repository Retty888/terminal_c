#pragma once

#include "candle.h"
#include <vector>

namespace Core {

class SignalManager {
public:
    static std::vector<double> calculate_sma(const std::vector<Candle>& candles, int period);
    static std::vector<double> calculate_rsi(const std::vector<Candle>& candles, int period);
    static std::vector<bool> generate_buy_signal(const std::vector<Candle>& candles,
                                                 const std::vector<double>& sma,
                                                 const std::vector<double>& rsi);
    static std::vector<bool> generate_sell_signal(const std::vector<Candle>& candles,
                                                  const std::vector<double>& sma,
                                                  const std::vector<double>& rsi);
};

} // namespace Core

