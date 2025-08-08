#include "analytics.h"
#include <numeric>
#include <cmath>

namespace Core {
namespace Analytics {

std::vector<double> moving_average(const std::vector<double>& data, std::size_t period) {
    std::vector<double> result;
    if (period == 0 || data.size() < period) return result;
    double sum = std::accumulate(data.begin(), data.begin() + period, 0.0);
    result.push_back(sum / period);
    for (std::size_t i = period; i < data.size(); ++i) {
        sum += data[i] - data[i - period];
        result.push_back(sum / period);
    }
    return result;
}

std::vector<double> rsi(const std::vector<double>& data, std::size_t period) {
    std::vector<double> result;
    if (period == 0 || data.size() <= period) return result;
    double gain = 0.0, loss = 0.0;
    for (std::size_t i = 1; i <= period; ++i) {
        double change = data[i] - data[i - 1];
        if (change >= 0) gain += change; else loss -= change;
    }
    gain /= period;
    loss /= period;
    double rs = loss == 0 ? 0 : gain / loss;
    result.push_back(loss == 0 ? 100.0 : 100.0 - (100.0 / (1.0 + rs)));
    for (std::size_t i = period + 1; i < data.size(); ++i) {
        double change = data[i] - data[i - 1];
        double cur_gain = change > 0 ? change : 0;
        double cur_loss = change < 0 ? -change : 0;
        gain = (gain * (period - 1) + cur_gain) / period;
        loss = (loss * (period - 1) + cur_loss) / period;
        rs = loss == 0 ? 0 : gain / loss;
        result.push_back(loss == 0 ? 100.0 : 100.0 - (100.0 / (1.0 + rs)));
    }
    return result;
}

std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>
bollinger_bands(const std::vector<double>& data, std::size_t period, double num_stddev) {
    std::vector<double> upper, middle, lower;
    if (period == 0 || data.size() < period) return {upper, middle, lower};
    for (std::size_t i = 0; i + period <= data.size(); ++i) {
        double sum = std::accumulate(data.begin() + i, data.begin() + i + period, 0.0);
        double mean = sum / period;
        double sq_sum = 0.0;
        for (std::size_t j = 0; j < period; ++j) {
            double diff = data[i + j] - mean;
            sq_sum += diff * diff;
        }
        double stddev = std::sqrt(sq_sum / period);
        middle.push_back(mean);
        upper.push_back(mean + num_stddev * stddev);
        lower.push_back(mean - num_stddev * stddev);
    }
    return {upper, middle, lower};
}

} // namespace Analytics
} // namespace Core

