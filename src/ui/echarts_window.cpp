#include "ui/echarts_window.h"

#include <filesystem>
#include <utility>
#include "core/candle_manager.h"

EChartsWindow::EChartsWindow(const std::string &html_path, bool debug)
    : html_path_(html_path), debug_(debug),
      view_(std::make_unique<webview::webview>(debug, nullptr)) {}

void EChartsWindow::SetHandler(JsonHandler handler) { handler_ = std::move(handler); }

void EChartsWindow::Show() {
  if (!view_) {
    view_ = std::make_unique<webview::webview>(debug_, nullptr);
  }

  view_->set_title("ECharts");
  view_->set_size(800, 600, WEBVIEW_HINT_NONE);

  view_->bind("bridge", [this](std::string req) -> std::string {
    auto json = nlohmann::json::parse(req, nullptr, false);
    if (json.contains("request") && json["request"] == "init") {
      Core::CandleManager cm;
      auto data = cm.load_candles_json("BTCUSDT", "1m");
      SendToJs(data);
    }
    if (handler_) {
      auto resp = handler_(json);
      return resp.dump();
    }
    return "{}";
  });

  std::string url = html_path_;
  if (url.rfind("file://", 0) != 0) {
    url = std::string("file://") + std::filesystem::absolute(url).string();
  }
  view_->navigate(url);
  view_->run();
}

void EChartsWindow::SendToJs(const nlohmann::json &data) {
  if (view_) {
    std::string script = std::string("window.receiveFromCpp(") + data.dump() + ");";
    view_->eval(script);
  }
}

