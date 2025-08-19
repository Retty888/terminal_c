#pragma once
#include <functional>
#include <string>

namespace webview {

enum hint {
  WEBVIEW_HINT_NONE = 0,
  WEBVIEW_HINT_MIN = 1,
  WEBVIEW_HINT_MAX = 2,
  WEBVIEW_HINT_FIXED = 3
};

class webview {
 public:
  webview(bool = false, void* = nullptr) {}
  void set_title(const std::string&) {}
  void set_size(int, int, int) {}
  void navigate(const std::string&) {}
  void run() {}
  void eval(const std::string&) {}
  void bind(const std::string&, std::function<std::string(std::string)>) {}
  void terminate() {}
  void *window() { return nullptr; }
};

} // namespace webview
