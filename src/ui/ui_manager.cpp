#include "ui_manager.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>

#include "config_manager.h"
#include "config_path.h"
#include "core/logger.h"
#include "core/path_utils.h"
#ifndef _WIN32
#include <sys/resource.h>
#endif
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

UiManager::~UiManager() { shutdown(); }

bool UiManager::setup(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::LoadIniSettingsFromMemory("");
  const auto ini_path = Core::path_from_executable("imgui.ini");
  std::filesystem::create_directories(ini_path.parent_path());
  static std::string ini_path_str = ini_path.string();
  bool load_ini = true;
  if (std::filesystem::exists(ini_path)) {
    std::ifstream file(ini_path);
    std::string line;
    while (std::getline(file, line)) {
      if (line.rfind("Size=", 0) == 0) {
        int w = 0;
        int h = 0;
        if (std::sscanf(line.c_str(), "Size=%d,%d", &w, &h) == 2) {
          if (w < 100 || h < 100) {
            std::error_code ec;
            std::filesystem::remove(ini_path, ec);
            load_ini = false;
          }
        }
        break;
      }
    }
  }
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = ini_path_str.c_str();
  if (load_ini) {
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
  }
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");
  chart_enabled_ = false;
  auto cfg = Config::ConfigManager::load(resolve_config_path().string());
  if (!cfg) {
    Core::Logger::instance().warn(
        "WebView initialization skipped: config not loaded");
  } else if (!cfg->enable_chart) {
    Core::Logger::instance().info(
        "WebView initialization disabled by configuration");
  } else {
    auto chart_path = Core::path_from_executable(cfg->chart_html_path).string();
    Core::Logger::instance().info("Initializing WebView with " + chart_path);
#ifdef HAVE_WEBVIEW
    if (std::filesystem::exists(chart_path)) {
      chart_enabled_ = true;
      void *native_handle = nullptr;
#if defined(_WIN32)
      native_handle = glfwGetWin32Window(window);
#elif defined(__linux__)
      native_handle = reinterpret_cast<void *>(glfwGetX11Window(window));
#endif
      webview_ = std::make_unique<webview::webview>(false, native_handle);
      webview_->set_title("Chart");
      webview_->navigate("file://" + chart_path);
      Core::Logger::instance().info("WebView initialized successfully");
    } else {
      Core::Logger::instance().error(
          "WebView initialization failed: file not found - " + chart_path);
    }
#else
    Core::Logger::instance().warn(
        "WebView library not available; chart disabled");
#endif
  }
  return true;
}

void UiManager::begin_frame() {
#ifdef HAVE_WEBVIEW
  {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    if (cached_candle_ && chart_enabled_ && webview_) {
      auto now = std::chrono::steady_clock::now();
      if (now - last_push_time_ >= throttle_interval_) {
        last_push_time_ = now;
        nlohmann::json j{{"time", cached_candle_->open_time / 1000},
                         {"open", cached_candle_->open},
                         {"high", cached_candle_->high},
                         {"low", cached_candle_->low},
                         {"close", cached_candle_->close},
                         {"volume", cached_candle_->volume}};
        auto json = j.dump();
        cached_candle_.reset();
        webview_->eval("updateCandle(" + json + ");");
      }
    }
  }
#else
  // WebView library not available; skip chart updates
#endif
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void UiManager::draw_chart_panel(
    [[maybe_unused]] const std::string &selected_interval) {
  ImGui::Begin("Chart");
  if (!chart_enabled_) {
    ImGui::Text("Chart disabled (missing file or disabled by configuration)");
  } else if (webview_) {
#ifdef HAVE_WEBVIEW
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x > 0 && size.y > 0) {
#if defined(_WIN32)
      auto hwnd = static_cast<HWND>(
          webview_get_native_handle(static_cast<webview_t>(webview_.get()),
                                    WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
      if (hwnd) {
        SetWindowPos(hwnd, nullptr, static_cast<int>(pos.x),
                     static_cast<int>(pos.y), static_cast<int>(size.x),
                     static_cast<int>(size.y),
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
#elif defined(__linux__)
      auto widget = static_cast<Window>(reinterpret_cast<uintptr_t>(
          webview_get_native_handle(static_cast<webview_t>(webview_.get()),
                                    WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET)));
      if (widget) {
        Display *dpy = glfwGetX11Display();
        XMoveResizeWindow(dpy, widget, static_cast<int>(pos.x),
                          static_cast<int>(pos.y),
                          static_cast<unsigned int>(size.x),
                          static_cast<unsigned int>(size.y));
        XFlush(dpy);
      }
#endif
      webview_->set_size(static_cast<int>(size.x), static_cast<int>(size.y),
                         WEBVIEW_HINT_NONE);
      ImGui::Dummy(size);
    }
#else
    ImGui::Text("WebView library not available");
#endif
  }
  ImGui::End();
}

void UiManager::set_markers(const std::string &markers_json) {
#ifdef HAVE_WEBVIEW
  std::lock_guard<std::mutex> lock(ui_mutex_);
  if (!chart_enabled_ || !webview_)
    return;
  webview_->eval("chart.setMarkers(" + markers_json + ");");
#else
  (void)markers_json;
#endif
}

void UiManager::set_price_line(double price) {
#ifdef HAVE_WEBVIEW
  std::lock_guard<std::mutex> lock(ui_mutex_);
  if (!chart_enabled_ || !webview_)
    return;
  webview_->eval("chart.setPriceLine(" + std::to_string(price) + ");");
#else
  (void)price;
#endif
}

std::function<void(const std::string &)> UiManager::candle_callback() {
#ifdef HAVE_WEBVIEW
  return [this](const std::string &json) {
    std::lock_guard<std::mutex> lock(ui_mutex_);
    if (!chart_enabled_ || !webview_)
      return;
    webview_->eval("updateCandle(" + json + ");");
  };
#else
  return [](const std::string &) {};
#endif
}

void UiManager::push_candle(const Core::Candle &candle) {
#ifdef HAVE_WEBVIEW
  std::lock_guard<std::mutex> lock(ui_mutex_);
  if (!chart_enabled_ || !webview_)
    return;
  auto now = std::chrono::steady_clock::now();
  if (now - last_push_time_ < throttle_interval_) {
    cached_candle_ = candle;
    return;
  }
  last_push_time_ = now;
  nlohmann::json j{{"time", candle.open_time / 1000},
                   {"open", candle.open},
                   {"high", candle.high},
                   {"low", candle.low},
                   {"close", candle.close},
                   {"volume", candle.volume}};
  auto json = j.dump();
  webview_->eval("updateCandle(" + json + ");");
#else
  (void)candle;
#endif
}

void UiManager::set_interval_callback(
    std::function<void(const std::string &)> cb) {
  on_interval_changed_ = std::move(cb);
}

void UiManager::set_status_callback(
    std::function<void(const std::string &)> cb) {
  status_callback_ = std::move(cb);
}

void UiManager::set_initial_interval(const std::string &interval) {
  current_interval_ = interval;
}

void UiManager::end_frame(GLFWwindow *window) {
  ImGui::Render();
  int display_w, display_h;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window);
}

void UiManager::shutdown() {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  if (shutdown_called_)
    return;
  shutdown_called_ = true;
#ifdef HAVE_WEBVIEW
  if (webview_) {
    webview_->terminate();
    webview_.reset();
  }
#else
  // WebView library not available; nothing to shut down
#endif
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
