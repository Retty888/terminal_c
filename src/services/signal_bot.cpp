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
    } else if (cfg_.type == "ema") {
        std::size_t period = cfg_.short_period;
        if (period == 0) {
            auto it = cfg_.params.find("period");
            if (it != cfg_.params.end()) {
                period = static_cast<std::size_t>(it->second);
            }
        }
        return Signal::ema_signal(candles, index, period);
    } else if (cfg_.type == "rsi") {
        std::size_t period = cfg_.short_period;
        if (period == 0) {
            auto it = cfg_.params.find("period");
            if (it != cfg_.params.end()) {
                period = static_cast<std::size_t>(it->second);
            }
        }
        double oversold = 30.0;
        double overbought = 70.0;
        auto it_os = cfg_.params.find("oversold");
        if (it_os != cfg_.params.end()) {
            oversold = it_os->second;
        }
        auto it_ob = cfg_.params.find("overbought");
        if (it_ob != cfg_.params.end()) {
            overbought = it_ob->second;
        }
        return Signal::rsi_signal(candles, index, period, oversold, overbought);
    }
    return 0;
}

