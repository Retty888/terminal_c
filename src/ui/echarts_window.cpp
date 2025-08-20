#include "ui/echarts_window.h"

#include <filesystem>
#include <utility>
#include <cstdint>

// clang-format off
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Cocoa/Cocoa.h>
#else
#include <X11/Xlib.h>
#endif
// clang-format on

#include "core/logger.h"

EChartsWindow::EChartsWindow(const std::string &html_path, void* parent_window,
                             bool debug)
    : html_path_(html_path), debug_(debug), parent_window_(parent_window),
      view_(std::make_unique<webview::webview>(debug, parent_window)) {}

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
  if (!view_) {
    view_ = std::make_unique<webview::webview>(debug_, parent_window_);
  }

  view_->set_title("ECharts");
  view_->set_size(800, 600, WEBVIEW_HINT_NONE);

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

void EChartsWindow::SetBounds(int x, int y, int width, int height) {
  if (!view_) {
    return;
  }
  view_->dispatch([this, x, y, width, height]() {
    view_->set_size(width, height, WEBVIEW_HINT_NONE);
    void* handle = view_->window();
#if defined(_WIN32)
    HWND hwnd = static_cast<HWND>(handle);
    if (hwnd) {
      ::SetWindowPos(hwnd, nullptr, x, y, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
#elif defined(__APPLE__)
    id nsview = (id)handle;
    if (nsview) {
      CGRect frame = CGRectMake(x, y, width, height);
      SEL sel = sel_registerName("setFrame:");
      using SetFrameFn = void (*)(id, SEL, CGRect);
      auto fn = reinterpret_cast<SetFrameFn>(objc_msgSend);
      fn(nsview, sel, frame);
    }
#else
    Display* d = XOpenDisplay(nullptr);
    if (d && handle) {
      ::XMoveResizeWindow(d, static_cast<Window>(reinterpret_cast<uintptr_t>(handle)),
                         x, y, static_cast<unsigned int>(width),
                         static_cast<unsigned int>(height));
      XCloseDisplay(d);
    }
#endif
  });
}
