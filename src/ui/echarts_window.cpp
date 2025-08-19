#include "ui/echarts_window.h"

#include <filesystem>
#include <utility>

EChartsWindow::EChartsWindow(const std::string &html_path, bool debug)
    : html_path_(html_path), debug_(debug),
      view_(std::make_unique<webview::webview>(debug, nullptr)) {}

void EChartsWindow::SetHandler(JsonHandler handler) { handler_ = std::move(handler); }

void EChartsWindow::SetInitData(nlohmann::json data) { init_data_ = std::move(data); }

void EChartsWindow::Show() {
  if (!view_) {
    view_ = std::make_unique<webview::webview>(debug_, nullptr);
  }

  view_->set_title("ECharts");
  view_->set_size(800, 600, WEBVIEW_HINT_NONE);
  native_handle_ = view_->window();

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
}

void EChartsWindow::SendToJs(const nlohmann::json &data) {
  if (view_) {
    std::string script = std::string("window.receiveFromCpp(") + data.dump() + ");";
    view_->eval(script);
  }
}

void EChartsWindow::Close() {
  if (view_) {
    // Terminate the webview event loop so the hosting thread can finish.
    view_->terminate();
  }
}

void *EChartsWindow::GetNativeHandle() const {
  return native_handle_.load();
}

void EChartsWindow::SetSize(int width, int height) {
  if (view_) {
    view_->set_size(width, height, WEBVIEW_HINT_NONE);
  }
}

