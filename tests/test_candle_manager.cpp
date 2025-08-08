#include "core/candle_manager.h"
#include <cassert>
#include <vector>

int main() {
    using namespace Core;
    std::vector<Candle> candles;
    candles.emplace_back(1,1,1,1,1,1,1,1,1,1,1,0);
    bool saved = CandleManager::save_candles("TEST","1m",candles);
    assert(saved);
    auto loaded = CandleManager::load_candles("TEST","1m");
    assert(loaded.size() == 1);
    assert(loaded[0].close == 1);
    auto list = CandleManager::list_stored_data();
    assert(!list.empty());
    return 0;
}
