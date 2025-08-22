#include "ui_manager.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <optional>

#include <webview.h>
#include "core/candle.h"
#include "core/candle_manager.h"

#include "config_manager.h"
#include "config_path.h"
#include "core/logger.h"
#include "core/path_utils.h"
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
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  if (auto cfg = Config::ConfigManager::load(resolve_config_path().string())) {
    chart_enabled_ = cfg->enable_chart;
    chart_html_path_ = cfg->chart_html_path;
  }
  return true;
}

void UiManager::begin_frame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void UiManager::draw_chart_panel(Core::CandleManager &candle_manager,
                                 const std::string &active_pair,
                                 const std::string &selected_interval) {
  ImGui::Begin("Chart");
  if (!chart_enabled_) {
    ImGui::Text("Chart disabled by configuration");
    ImGui::End();
    return;
  }

  if (!chart_view_) {
    chart_view_ = std::make_unique<webview::webview>(false, nullptr);
    chart_view_->set_title("Chart");
    auto chart_path = Core::path_from_executable(chart_html_path_);
    chart_view_->navigate(std::string("file://") + chart_path.string());
  }

  if (!active_pair.empty() &&
      (active_pair != current_pair_ || selected_interval != current_interval_)) {
    current_pair_ = active_pair;
    current_interval_ = selected_interval;
    auto data = candle_manager.load_candles_tradingview(active_pair,
                                                        selected_interval);
    chart_view_->eval(std::string("loadCandles(") + data.dump() + ");");
  }

  ImGui::Text("%s %s", active_pair.c_str(), selected_interval.c_str());
  ImGui::End();
}

void UiManager::set_markers(const std::string &markers_json) {
  if (chart_view_) {
    chart_view_->eval(std::string("window.chart && window.chart.setMarkers(") +
                      markers_json + ");");
  }
}

void UiManager::set_price_line(double price) {
  if (chart_view_) {
    std::ostringstream js;
    js << "window.chart && window.chart.setPriceLine(" << price << ");";
    chart_view_->eval(js.str());
  }
}

void UiManager::push_candle(const Core::Candle &candle) {
  cached_candle_ = candle;
  if (!chart_view_)
    return;
  auto now = std::chrono::steady_clock::now();
  if (now - last_push_time_ < throttle_interval_)
    return;
  const auto &c = *cached_candle_;
  std::ostringstream js;
  js << "window.chart && window.chart.addCandle({";
  js << "time:" << c.open_time / 1000 << ",";
  js << "open:" << c.open << ",";
  js << "high:" << c.high << ",";
  js << "low:" << c.low << ",";
  js << "close:" << c.close << ",";
  js << "volume:" << c.volume;
  js << "});";
  chart_view_->eval(js.str());
  last_push_time_ = now;
  cached_candle_.reset();
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
  if (shutdown_called_)
    return;
  shutdown_called_ = true;
  if (chart_view_) {
    chart_view_->terminate();
    chart_view_.reset();
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
