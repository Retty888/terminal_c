#include "core/candle_manager.h"
#include <cassert>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <fstream>

int main() {
    using namespace Core;
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "candle_manager_test";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);
    CandleManager::set_data_dir(test_dir);
    std::vector<Candle> candles;
    candles.emplace_back(1,1,1,1,1,1,1,1,1,1,1,0);
    bool saved = CandleManager::save_candles("TEST","1m",candles);
    assert(saved);
    auto loaded = CandleManager::load_candles("TEST","1m");
    assert(loaded.size() == 1);
    assert(loaded[0].close == 1);
    auto list = CandleManager::list_stored_data();
    assert(!list.empty());

    std::vector<Candle> more;
    more.emplace_back(2,2,2,2,2,2,2,2,2,2,2,0);
    bool appended = CandleManager::append_candles("TEST","1m",more);
    assert(appended);
    loaded = CandleManager::load_candles("TEST","1m");
    assert(loaded.size() == 2);
    assert(loaded[1].open_time == 2);
    bool appended_dup = CandleManager::append_candles("TEST","1m",more);
    assert(appended_dup);
    loaded = CandleManager::load_candles("TEST","1m");
    assert(loaded.size() == 2); // no duplicates

    std::filesystem::path idx_path = test_dir / "TEST_1m.idx";
    assert(std::filesystem::exists(idx_path));
    std::ifstream idx(idx_path);
    long long idx_time = -1;
    idx >> idx_time;
    assert(idx_time == 2);
    idx.close();

    std::filesystem::remove(idx_path);
    bool appended_dup_no_idx = CandleManager::append_candles("TEST","1m",more);
    assert(appended_dup_no_idx);
    loaded = CandleManager::load_candles("TEST","1m");
    assert(loaded.size() == 2);
    idx.open(idx_path);
    idx_time = -1;
    idx >> idx_time;
    assert(idx_time == 2);
    idx.close();

    std::vector<Candle> next;
    next.emplace_back(3,3,3,3,3,3,3,3,3,3,3,0);
    bool appended_next = CandleManager::append_candles("TEST","1m",next);
    assert(appended_next);
    loaded = CandleManager::load_candles("TEST","1m");
    assert(loaded.size() == 3);
    idx.open(idx_path);
    idx_time = -1;
    idx >> idx_time;
    assert(idx_time == 3);
    idx.close();

    std::filesystem::remove_all(test_dir);
    return 0;
}
