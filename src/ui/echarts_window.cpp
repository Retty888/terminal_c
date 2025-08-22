#include "ui/echarts_window.h"

#include <filesystem>
#include <utility>

#include "core/logger.h"

EChartsWindow::EChartsWindow(const std::string &html_path,
                             const std::string &js_path,
                             void *parent_window, bool debug)
    : html_path_(html_path), js_path_(js_path), parent_window_(parent_window),
      debug_(debug) {}

void EChartsWindow::SetHandler(JsonHandler handler) {
  handler_ = std::move(handler);
}

void EChartsWindow::SetInitData(nlohmann::json data) {
  init_data_ = std::move(data);
}

void EChartsWindow::SetErrorCallback(
    std::function<void(const std::string &)> cb) {
  error_callback_ = std::move(cb);
}

void EChartsWindow::Show() {
  // Validate that required resources are present before launching the view.
  if (!std::filesystem::exists(html_path_)) {
    const std::string msg = "HTML file not found: " + html_path_;
    if (error_callback_) {
      error_callback_(msg);
    } else {
      Core::Logger::instance().error(msg);
    }
    return;
  }

  const std::filesystem::path js_path(js_path_);
  if (!std::filesystem::exists(js_path)) {
    const std::string msg = "ECharts script not found: " + js_path.string();
    if (error_callback_) {
      error_callback_(msg);
    } else {
      Core::Logger::instance().error(msg);
    }
    return;
  }

  if (!view_) {
    view_ = std::make_unique<webview::webview>(debug_, parent_window_);
  }

  view_->set_title("ECharts");
  // Avoid forcing a fixed initial size.  Let the webview inherit the
  // current dimensions from its parent window so users can freely resize.

  view_->bind("bridge", [this](std::string req) -> std::string {
    nlohmann::json json;
    try {
      json = nlohmann::json::parse(req);
    } catch (const nlohmann::json::parse_error &) {
      return nlohmann::json{{"error", "invalid json"}}.dump();
    }
    if (json.contains("request") && json["request"] == "init") {
      if (!init_data_.is_null()) {
        SendToJs(init_data_);
      }
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
  // After the event loop exits, free webview resources on this thread.
  view_.reset();
}

void EChartsWindow::SendToJs(const nlohmann::json &data) {
  if (view_) {
    std::string script =
        std::string("window.receiveFromCpp(") + data.dump() + ");";
    view_->dispatch([this, s = std::move(script)]() { view_->eval(s); });
  }
}

void EChartsWindow::Close() {
  if (view_) {
    // Terminate the webview event loop so the hosting thread can finish.
    view_->dispatch([this]() { view_->terminate(); });
  }
}

void EChartsWindow::SetSize(int width, int height) {
  if (view_) {
    view_->dispatch([this, width, height]() {
      view_->set_size(width, height, WEBVIEW_HINT_NONE);
    });
  }
}
