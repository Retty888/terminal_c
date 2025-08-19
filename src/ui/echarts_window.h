#pragma once

#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#if USE_WEBVIEW
#include <webview.h>
#endif

// EChartsWindow embeds a webview to display a chart powered by ECharts and
// provides a simple JSON-based bridge between C++ and JavaScript.
class EChartsWindow {
 public:
  using JsonHandler =
      std::function<nlohmann::json(const nlohmann::json &request)>;

  explicit EChartsWindow(const std::string &html_path, bool debug = false);

  // Set handler that will be invoked when JavaScript sends JSON via the bridge.
  void SetHandler(JsonHandler handler);

  // Provide preformatted JSON that will be sent to JS upon initialization.
  void SetInitData(nlohmann::json data);

  // Show the window and start the event loop.
  void Show();

  // Close the window and terminate the event loop so the hosting thread
  // can exit without user interaction.
  void Close();

  // Send JSON data to the JavaScript side. JS should implement
  // `window.receiveFromCpp` to receive it.
  void SendToJs(const nlohmann::json &data);

 private:
  std::string html_path_;
  bool debug_;
  JsonHandler handler_;
#if USE_WEBVIEW
  std::unique_ptr<webview::webview> view_;
#endif
  nlohmann::json init_data_{};
};

#if !USE_WEBVIEW
inline EChartsWindow::EChartsWindow(const std::string&, bool) {}
inline void EChartsWindow::SetHandler(JsonHandler) {}
inline void EChartsWindow::SetInitData(nlohmann::json) {}
inline void EChartsWindow::Show() {}
inline void EChartsWindow::Close() {}
inline void EChartsWindow::SendToJs(const nlohmann::json&) {}
#endif

