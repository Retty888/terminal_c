#include "ui_manager.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
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
  return true;
}

void UiManager::begin_frame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void UiManager::draw_chart_panel(
    [[maybe_unused]] const std::string &selected_interval) {
  ImGui::Begin("Chart");
  if (!chart_enabled_) {
    ImGui::Text("Chart disabled (missing file or disabled by configuration)");
  } else {
    ImGui::Text("Chart opened in a separate window");
  }
  ImGui::End();
}

void UiManager::set_markers(const std::string &markers_json) {
  (void)markers_json;
}

void UiManager::set_price_line(double price) {
  (void)price;
}

std::function<void(const std::string &)> UiManager::candle_callback() {
  return [](const std::string &) {};
}

void UiManager::push_candle(const Core::Candle &candle) {
  (void)candle;
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
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
