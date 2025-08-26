// Caches previously computed signals to avoid recalculation when parameters
// have not changed.

#include "ui/signals_window.h"
#include "app.h"

#include "imgui.h"
#include "signal.h"

#include <algorithm>
#include <ctime>

void DrawSignalsWindow(
    std::string &strategy, int &short_period, int &long_period,
    double &oversold, double &overbought, bool &show_on_chart,
    std::vector<SignalEntry> &signal_entries,
    std::vector<AppContext::TradeEvent> &trades,
    const std::map<std::string,
                   std::map<std::string, std::vector<Core::Candle>>>
        &all_candles,
    const std::string &active_pair, const std::string &selected_interval,
    AppStatus &status) {
  auto vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_FirstUseEver);
  ImGui::Begin("Signals");
  static const char *strategies[] = {"sma_crossover", "ema", "rsi"};
  int strategy_idx = 0;
  if (strategy == "ema")
    strategy_idx = 1;
  else if (strategy == "rsi")
    strategy_idx = 2;
  ImGui::Combo("Strategy", &strategy_idx, strategies, IM_ARRAYSIZE(strategies));
  strategy = strategies[strategy_idx];

  if (strategy == "sma_crossover") {
    ImGui::InputInt("Short SMA", &short_period);
    ImGui::InputInt("Long SMA", &long_period);
    if (long_period <= short_period) {
      long_period = short_period + 1;
    }
  } else if (strategy == "ema") {
    ImGui::InputInt("EMA Period", &short_period);
  } else if (strategy == "rsi") {
    ImGui::InputInt("RSI Period", &short_period);
    ImGui::InputDouble("Oversold", &oversold);
    ImGui::InputDouble("Overbought", &overbought);
  }
  ImGui::Checkbox("Show on Chart", &show_on_chart);
  bool request = ImGui::Button("Request signals");

  struct SignalsCache {
    std::string strategy;
    int short_period = 0;
    int long_period = 0;
    double oversold = 30.0;
    double overbought = 70.0;
    std::string active_pair;
    std::string selected_interval;
    long long last_candle_time = 0;
    std::vector<SignalEntry> entries;
    std::vector<AppContext::TradeEvent> trades;
    bool initialized = false;
  };
  static SignalsCache cache;

  const auto &sig_candles = all_candles.at(active_pair).at(selected_interval);
  long long latest_time =
      sig_candles.empty() ? 0 : sig_candles.back().open_time;

  bool need_recalc =
      request || !cache.initialized || cache.short_period != short_period ||
      cache.long_period != long_period || cache.strategy != strategy ||
      cache.oversold != oversold || cache.overbought != overbought ||
      cache.active_pair != active_pair ||
      cache.selected_interval != selected_interval ||
      cache.last_candle_time != latest_time;

  if (need_recalc) {
    status.signal_message = "Computing signals";
    Core::Logger::instance().info("Computing signals for " + active_pair + " " +
                                  selected_interval);
    cache.strategy = strategy;
    cache.short_period = short_period;
    cache.long_period = long_period;
    cache.oversold = oversold;
    cache.overbought = overbought;
    cache.active_pair = active_pair;
    cache.selected_interval = selected_interval;
    cache.last_candle_time = latest_time;

    cache.entries.clear();
    cache.trades.clear();

    if (strategy == "sma_crossover") {
      for (std::size_t i = static_cast<std::size_t>(long_period);
           i < sig_candles.size(); ++i) {
        int sig = Signal::sma_crossover_signal(sig_candles, i, short_period,
                                               long_period);
        if (sig != 0) {
          double t = static_cast<double>(sig_candles[i].open_time) / 1000.0;
          double price = sig_candles[i].close;
          double short_sma =
              Signal::simple_moving_average(sig_candles, i, short_period);
          double long_sma =
              Signal::simple_moving_average(sig_candles, i, long_period);
          cache.entries.push_back({t, price, short_sma, long_sma, sig});
          cache.trades.push_back({t, price,
                                  sig > 0
                                      ? AppContext::TradeEvent::Side::Buy
                                      : AppContext::TradeEvent::Side::Sell});
        }
      }
    } else if (strategy == "ema") {
      for (std::size_t i = static_cast<std::size_t>(short_period);
           i < sig_candles.size(); ++i) {
        int sig = Signal::ema_signal(sig_candles, i,
                                     static_cast<std::size_t>(short_period));
        if (sig != 0) {
          double t = static_cast<double>(sig_candles[i].open_time) / 1000.0;
          double price = sig_candles[i].close;
          double ema = Signal::exponential_moving_average(
              sig_candles, i, static_cast<std::size_t>(short_period));
          cache.entries.push_back({t, price, ema, 0.0, sig});
          cache.trades.push_back({t, price,
                                  sig > 0
                                      ? AppContext::TradeEvent::Side::Buy
                                      : AppContext::TradeEvent::Side::Sell});
        }
      }
    } else if (strategy == "rsi") {
      for (std::size_t i = static_cast<std::size_t>(short_period);
           i < sig_candles.size(); ++i) {
        int sig = Signal::rsi_signal(sig_candles, i,
                                     static_cast<std::size_t>(short_period),
                                     oversold, overbought);
        if (sig != 0) {
          double t = static_cast<double>(sig_candles[i].open_time) / 1000.0;
          double price = sig_candles[i].close;
          double rsi = Signal::relative_strength_index(
              sig_candles, i, static_cast<std::size_t>(short_period));
          cache.entries.push_back({t, price, rsi, 0.0, sig});
          cache.trades.push_back({t, price,
                                  sig > 0
                                      ? AppContext::TradeEvent::Side::Buy
                                      : AppContext::TradeEvent::Side::Sell});
        }
      }
    }

    cache.initialized = true;
    status.signal_message = "Signals updated";
  }

  signal_entries = cache.entries;
  trades = cache.trades;

  if (ImGui::BeginTable("SignalsTable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Time");
    const char *col1 = "Value1";
    const char *col2 = "Value2";
    if (strategy == "sma_crossover") {
      col1 = "Short SMA";
      col2 = "Long SMA";
    } else if (strategy == "ema") {
      col1 = "EMA";
      col2 = "";
    } else if (strategy == "rsi") {
      col1 = "RSI";
      col2 = "";
    }
    ImGui::TableSetupColumn(col1);
    ImGui::TableSetupColumn(col2[0] ? col2 : " ");
    ImGui::TableSetupColumn("Signal");
    ImGui::TableHeadersRow();
    int rows = static_cast<int>(signal_entries.size());
    int start = std::max(0, rows - 10);
    char buf[20];
    for (int i = start; i < rows; ++i) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      std::time_t tt = static_cast<std::time_t>(signal_entries[i].time);
      std::tm *tm = std::localtime(&tt);
      if (tm && std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm)) {
        ImGui::TextUnformatted(buf);
      } else {
        ImGui::Text("%lld", static_cast<long long>(signal_entries[i].time));
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%.2f", signal_entries[i].value1);
      ImGui::TableSetColumnIndex(2);
      if (col2[0])
        ImGui::Text("%.2f", signal_entries[i].value2);
      else
        ImGui::Text("-");
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%s", signal_entries[i].type > 0 ? "Buy" : "Sell");
    }
    ImGui::EndTable();
  }
  ImGui::End();
}
