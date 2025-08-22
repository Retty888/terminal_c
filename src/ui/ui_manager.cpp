#include "ui_manager.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <Windows.h>
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif
#include <cassert>
#include <exception>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <thread>
#include <typeinfo>
#include <utility>

#include "config_manager.h"
#include "config_path.h"
#include "config_schema.h"
#include "core/candle_manager.h"
#include "core/logger.h"
#include "core/path_utils.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui/chart_window.h"

UiManager::~UiManager() {
  if (chart_thread_.joinable()) {
    shutdown();
  }
  assert(!chart_thread_.joinable());
}

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
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  if (auto cfg = Config::ConfigManager::load(resolve_config_path().string())) {
    chart_enabled_ = cfg->enable_chart;
    chart_html_path_ = cfg->chart_html_path;
    chart_js_path_ = cfg->chart_js_path;
  } else {
    chart_html_path_ = Config::kDefaultChartHtmlPath;
    chart_js_path_ = Config::kDefaultChartJsPath;
  }
#ifdef USE_WEBVIEW
  if (chart_enabled_) {
    const auto html_path = std::filesystem::path(chart_html_path_);
    const auto js_path = std::filesystem::path(chart_js_path_);

    Core::Logger::instance().info("Checking chart resources at " +
                                  html_path.string() + " and " +
                                  js_path.string());

    resources_available_ =
        std::filesystem::exists(html_path) && std::filesystem::exists(js_path);
    if (!resources_available_) {
      Core::Logger::instance().error("Chart resources missing. Checked '" +
                                     html_path.string() + "' and '" +
                                     js_path.string() + "'");
      Core::Logger::instance().warn("Expected files:\n  " + chart_html_path_ +
                                    "\n  " + chart_js_path_);
    } else {
      void *native_handle = nullptr;
#if defined(_WIN32)
      native_handle = glfwGetWin32Window(window);
#elif defined(__APPLE__)
      native_handle = glfwGetCocoaWindow(window);
#elif defined(__linux__)
      native_handle = reinterpret_cast<void *>(glfwGetX11Window(window));
#endif
      chart_window_ = std::make_unique<ChartWindow>(
          html_path.string(), js_path.string(), native_handle);

      try {
        Core::CandleManager cm;
        auto data = cm.load_candles_json("BTCUSDT", "1m");
        chart_window_->SetInitData(data);
      } catch (const std::exception &e) {
        Core::Logger::instance().error(e.what());
      }

      chart_window_->SetErrorCallback([this](const std::string &msg) {
        {
          std::lock_guard<std::mutex> lock(chart_mutex_);
          chart_error_ = msg;
        }
        if (status_callback_) {
          status_callback_(msg);
        }
      });

      chart_window_->SetHandler([this](const nlohmann::json &req) {
        if (req.contains("interval")) {
          if (on_interval_changed_) {
            on_interval_changed_(req.at("interval").get<std::string>());
          }
        }
        if (req.contains("request") && req.at("request") == "init") {
          if (!current_interval_.empty()) {
            chart_window_->SendToJs(
                nlohmann::json{{"interval", current_interval_}});
          }
        }
        return nlohmann::json{};
      });
      chart_thread_ = std::thread([this]() {
#if defined(_WIN32)
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif
        try {
          chart_window_->Show();
        } catch (const std::exception &e) {
          std::string err = e.what();
          const std::string type_name = typeid(e).name();
          if (err.empty()) {
            err = type_name;
          }

          std::string msg;
          if (type_name.find("webview::exception") != std::string::npos) {
            msg =
                "WebView2 runtime not found. Install Microsoft Edge WebView2.";
          } else {
            msg = std::string("Failed to run chart window: ") + err;
          }

          Core::Logger::instance().error(msg);
          {
            std::lock_guard<std::mutex> lock(chart_mutex_);
            chart_error_ = msg;
          }
          if (status_callback_) {
            status_callback_(msg);
          }
        } catch (...) {
          const std::string msg = "Failed to run chart window: unknown error";
          Core::Logger::instance().error(msg);
          {
            std::lock_guard<std::mutex> lock(chart_mutex_);
            chart_error_ = msg;
          }
          if (status_callback_) {
            status_callback_(msg);
          }
        }
#if defined(_WIN32)
        CoUninitialize();
#endif
      });
    }
  } else {
    Core::Logger::instance().info("Chart disabled by configuration");
  }
#else
  Core::Logger::instance().warn(
      "Chart disabled: install the webview dependency and rebuild with "
      "USE_WEBVIEW enabled");
#endif
  return true;
}

void UiManager::begin_frame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void UiManager::draw_chart_panel(const std::string &selected_interval) {
  ImGui::Begin("Chart");
#ifdef USE_WEBVIEW
  if (!chart_enabled_) {
    ImGui::Text("Chart disabled by configuration");
  } else if (!resources_available_) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                       "Chart resources missing");
    ImGui::TextWrapped("Expected files:\n  %s\n  %s", chart_html_path_.c_str(),
                       chart_js_path_.c_str());
  } else {
    std::string err;
    {
      std::lock_guard<std::mutex> lock(chart_mutex_);
      err = chart_error_;
    }
    if (!err.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                         "Failed to load chart");
      ImGui::TextWrapped("%s", err.c_str());
    } else if (chart_window_) {
      if (selected_interval != current_interval_) {
        chart_window_->SendToJs(
            nlohmann::json{{"interval", selected_interval}});
        current_interval_ = selected_interval;
      }
      ImVec2 avail = ImGui::GetContentRegionAvail();
      chart_window_->SetSize(static_cast<int>(avail.x),
                             static_cast<int>(avail.y));
      ImGui::Text("Chart is displayed in a separate window.");
    } else {
      ImGui::Text("Loading chart...");
    }
  }
#else
  if (!chart_enabled_) {
    ImGui::Text("Chart disabled by configuration");
  } else {
    ImGui::Text(
        "Chart disabled. Please install/enable the webview dependency "
        "(USE_WEBVIEW)");
  }
#endif
  ImGui::End();
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
#ifdef USE_WEBVIEW
  if (chart_window_) {
    chart_window_->SendToJs(nlohmann::json{{"interval", interval}});
  }
#endif
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
  if (shutdown_called_) {
    return;
  }
  shutdown_called_ = true;
  if (chart_thread_.joinable()) {
    if (chart_window_) {
      try {
        chart_window_->Close();
      } catch (const std::exception &e) {
        Core::Logger::instance().error(
            std::string("Failed to close chart window: ") + e.what());
      }
    }
    chart_thread_.join();
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
