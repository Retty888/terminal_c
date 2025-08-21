#include "exchange_utils.h"

std::string to_gate_symbol(const std::string &symbol) {
  if (symbol.size() < 6)
    return symbol;
  std::string base = symbol.substr(0, symbol.size() - 4);
  std::string quote = symbol.substr(symbol.size() - 4);
  return base + "_" + quote;
}

