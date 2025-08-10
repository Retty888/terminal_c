// Реализация методов для структуры Candle
#include "core/candle.h"

namespace Core {

bool Candle::is_bullish() const {
    return close > open;
}

} // namespace Core
