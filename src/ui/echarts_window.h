#pragma once

#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <webview.h>

// EChartsWindow embeds a webview to display a chart powered by ECharts and
// provides a simple JSON-based bridge between C++ and JavaScript.
class EChartsWindow {
 public:
  using JsonHandler =
      std::function<nlohmann::json(const nlohmann::json &request)>;

  explicit EChartsWindow(const std::string &html_path, bool debug = false);

  // Set handler that will be invoked when JavaScript sends JSON via the bridge.
  void SetHandler(JsonHandler handler);

  // Show the window and start the event loop.
  void Show();

  // Send JSON data to the JavaScript side. JS should implement
  // `window.receiveFromCpp` to receive it.
  void SendToJs(const nlohmann::json &data);

 private:
  std::string html_path_;
  bool debug_;
  JsonHandler handler_;
  std::unique_ptr<webview::webview> view_;
};

