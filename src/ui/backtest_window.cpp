#include "ui/backtest_window.h"

#include "config_manager.h"
#include "config_path.h"
#include "imgui.h"
#include "implot.h"
#include "services/signal_bot.h"

void DrawBacktestWindow(
    const std::map<std::string,
                   std::map<std::string, std::vector<Core::Candle>>>
        &all_candles,
    const std::string &active_pair, const std::string &selected_interval) {
  ImGui::Begin("Backtest");

  static Core::BacktestResult result;
  static bool ran = false;

  ImGui::Text("Pair: %s", active_pair.c_str());
  ImGui::SameLine();
  ImGui::Text("Interval: %s", selected_interval.c_str());

  if (ImGui::Button("Run Backtest") && !active_pair.empty() &&
      !selected_interval.empty()) {
    auto pair_it = all_candles.find(active_pair);
    if (pair_it != all_candles.end()) {
      auto interval_it = pair_it->second.find(selected_interval);
      if (interval_it != pair_it->second.end()) {
        auto cfg = Config::ConfigManager::load(resolve_config_path().string());
        Config::SignalConfig scfg;
        if (cfg)
          scfg = cfg->signal;
        SignalBot bot(scfg);
        Core::Backtester bt(interval_it->second, bot);
        result = bt.run();
        ran = true;
      }
    }
  }

  if (ran) {
    ImGui::Text("Total PnL: %.2f", result.total_pnl);
    ImGui::Text("Win Rate: %.2f%%", result.win_rate * 100.0);
    if (!result.equity_curve.empty()) {
      if (ImPlot::BeginPlot("Equity Curve")) {
        std::vector<double> xs(result.equity_curve.size());
        for (size_t i = 0; i < xs.size(); ++i)
          xs[i] = static_cast<double>(i);
        ImPlot::PlotLine("Equity", xs.data(), result.equity_curve.data(),
                         static_cast<int>(result.equity_curve.size()));
        ImPlot::EndPlot();
      }
    }
  }

  ImGui::End();
}
