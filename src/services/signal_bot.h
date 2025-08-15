#pragma once

#include <vector>

#include "config.h"
#include "core/backtester.h"
#include "signal.h"

// SignalBot wraps signal generation based on configurable settings.
// It implements Core::IStrategy so it can be used with the backtester
// or real-time trading modules.
class SignalBot : public Core::IStrategy {
public:
    explicit SignalBot(const Config::SignalConfig& cfg);

    void set_config(const Config::SignalConfig& cfg);
    [[nodiscard]] const Config::SignalConfig& config() const noexcept;

    int generate_signal(const std::vector<Core::Candle>& candles, size_t index) override;

private:
    Config::SignalConfig cfg_;
};

