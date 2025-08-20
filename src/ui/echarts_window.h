#pragma once

#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#ifdef USE_WEBVIEW
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

  // Resize the underlying webview window.
  void SetSize(int width, int height);

  // Close the window and terminate the event loop so the hosting thread
  // can exit without user interaction.
  void Close();

  // Send JSON data to the JavaScript side. JS should implement
  // `window.receiveFromCpp` to receive it.
  void SendToJs(const nlohmann::json &data);

  // Set callback for reporting errors during initialization.
  void SetErrorCallback(std::function<void(const std::string &)> cb);

 private:
  std::string html_path_;
  bool debug_;
  JsonHandler handler_;
#ifdef USE_WEBVIEW
  std::unique_ptr<webview::webview> view_;
#endif
  std::function<void(const std::string &)> error_callback_;
  nlohmann::json init_data_{};
};

#ifndef USE_WEBVIEW
inline EChartsWindow::EChartsWindow(const std::string&, bool) {}
inline void EChartsWindow::SetHandler(JsonHandler) {}
inline void EChartsWindow::SetInitData(nlohmann::json) {}
inline void EChartsWindow::Show() {}
inline void EChartsWindow::Close() {}
inline void EChartsWindow::SendToJs(const nlohmann::json&) {}
inline void EChartsWindow::SetSize(int, int) {}
inline void EChartsWindow::SetErrorCallback(
    std::function<void(const std::string &)>) {}
#endif

