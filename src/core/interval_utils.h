#pragma once

#include <chrono>
#include <string>

namespace Core {

std::chrono::milliseconds parse_interval(const std::string &interval);

} // namespace Core

