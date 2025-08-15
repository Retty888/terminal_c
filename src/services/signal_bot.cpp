#include "services/signal_bot.h"

SignalBot::SignalBot(const Config::SignalConfig& cfg) : cfg_(cfg) {}

void SignalBot::set_config(const Config::SignalConfig& cfg) {
    cfg_ = cfg;
}

const Config::SignalConfig& SignalBot::config() const noexcept {
    return cfg_;
}

int SignalBot::generate_signal(const std::vector<Core::Candle>& candles, size_t index) {
    if (cfg_.type == "sma_crossover") {
        return Signal::sma_crossover_signal(candles, index, cfg_.short_period, cfg_.long_period);
    }
    return 0;
}

