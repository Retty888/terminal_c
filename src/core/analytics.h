#pragma once

#include <vector>
#include <tuple>

namespace Core {
namespace Analytics {

std::vector<double> moving_average(const std::vector<double>& data, std::size_t period);
std::vector<double> rsi(const std::vector<double>& data, std::size_t period);
std::tuple<std::vector<double>, std::vector<double>, std::vector<double>> bollinger_bands(
    const std::vector<double>& data, std::size_t period, double num_stddev);

} // namespace Analytics
} // namespace Core

