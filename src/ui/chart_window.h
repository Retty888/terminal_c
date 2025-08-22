#pragma once

#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#ifdef USE_WEBVIEW
#include <webview.h>
#endif

// ChartWindow embeds a webview to display a chart powered by
// TradingView Lightweight Charts and provides a simple JSON bridge
// between C++ and JavaScript.
class ChartWindow {
 public:
  using JsonHandler =
      std::function<nlohmann::json(const nlohmann::json &request)>;

  explicit ChartWindow(const std::string &html_path,
                       const std::string &js_path,
                       void *parent_window,
                       bool debug = false);

  void SetHandler(JsonHandler handler);
  void SetInitData(nlohmann::json data);
  void Show();
  void SetSize(int width, int height);
  void Close();
  void SendToJs(const nlohmann::json &data);
  void SetErrorCallback(std::function<void(const std::string &)> cb);

 private:
  std::string html_path_;
  std::string js_path_;
  void *parent_window_;
  bool debug_;
  JsonHandler handler_;
#ifdef USE_WEBVIEW
  std::unique_ptr<webview::webview> view_;
#endif
  std::function<void(const std::string &)> error_callback_;
  nlohmann::json init_data_{};
};

#ifndef USE_WEBVIEW
inline ChartWindow::ChartWindow(const std::string &, const std::string &, void *, bool) {}
inline void ChartWindow::SetHandler(JsonHandler) {}
inline void ChartWindow::SetInitData(nlohmann::json) {}
inline void ChartWindow::Show() {}
inline void ChartWindow::Close() {}
inline void ChartWindow::SendToJs(const nlohmann::json &) {}
inline void ChartWindow::SetSize(int, int) {}
inline void ChartWindow::SetErrorCallback(
    std::function<void(const std::string &)>) {}
#endif

