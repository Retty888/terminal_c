#pragma once

#include "core/candle.h"
#include <string>
#include <vector>

namespace Core {

enum class FetchError {
  None = 0,
  HttpError = 1,
  ParseError = 2,
  NetworkError = 3,
  InvalidInterval = 4
};

struct KlinesResult {
  FetchError error{FetchError::None};
  int http_status{0};
  std::string message;
  std::vector<Candle> candles;
};

struct SymbolsResult {
  FetchError error{FetchError::None};
  int http_status{0};
  std::string message;
  std::vector<std::string> symbols;
};

struct IntervalsResult {
  FetchError error{FetchError::None};
  int http_status{0};
  std::string message;
  std::vector<std::string> intervals;
};

} // namespace Core
