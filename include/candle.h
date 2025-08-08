#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace Core {

struct Candle {
    long long open_time; // Unix timestamp in milliseconds
    double open;
    double high;
    double low;
    double close;
    double volume;
    long long close_time; // Unix timestamp in milliseconds
    double quote_asset_volume;
    int number_of_trades;
    double taker_buy_base_asset_volume;
    double taker_buy_quote_asset_volume;
    double ignore; // Unused field, often present in kline data

    // Constructor
    Candle(long long ot = 0, double o = 0.0, double h = 0.0, double l = 0.0, double c = 0.0, double v = 0.0,
           long long ct = 0, double qav = 0.0, int notr = 0, double tbbav = 0.0, double tbqav = 0.0, double ign = 0.0)
        : open_time(ot), open(o), high(h), low(l), close(c), volume(v),
          close_time(ct), quote_asset_volume(qav), number_of_trades(notr),
          taker_buy_base_asset_volume(tbbav), taker_buy_quote_asset_volume(tbqav), ignore(ign) {}
};

} // namespace Core
