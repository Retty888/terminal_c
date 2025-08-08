#include "signal.h"
#include <cassert>
#include <vector>

int main() {
    std::vector<Core::Candle> candles;
    double closes[] = {5,4,3,2,3,4};
    for (int i = 0; i < 6; ++i) {
        candles.emplace_back(i,0,0,0,closes[i],0,0,0,0,0,0,0);
    }
    int sig = Signal::sma_crossover_signal(candles,5,2,3);
    assert(sig == 1);
    candles.emplace_back(6,0,0,0,3,0,0,0,0,0,0,0);
    candles.emplace_back(7,0,0,0,2,0,0,0,0,0,0,0);
    sig = Signal::sma_crossover_signal(candles,7,2,3);
    assert(sig == -1);
    double sma = Signal::simple_moving_average(candles,7,3);
    assert(static_cast<int>(sma) == 3); // average of 3,2,4? Wait index 7: candles[5]=4, candles[6]=3, candles[7]=2 => 3
    return 0;
}
