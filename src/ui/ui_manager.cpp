#include "ui_manager.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <exception>
#include <nlohmann/json.hpp>
#include <thread>
#include <utility>

#include "core/candle_manager.h"
#include "core/logger.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui/echarts_window.h"

UiManager::~UiManager() = default;

bool UiManager::setup(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");
#if USE_WEBVIEW
  const std::filesystem::path html_path{"resources/chart.html"};
  const std::filesystem::path js_path{"../third_party/echarts/echarts.min.js"};

  resources_available_ =
      std::filesystem::exists(html_path) && std::filesystem::exists(js_path);
  if (!resources_available_) {
    Core::Logger::instance().error("Chart resources missing");
  } else {
    echarts_window_ = std::make_unique<EChartsWindow>(html_path.string());

    Core::CandleManager cm;
    auto data = cm.load_candles_json("BTCUSDT", "1m");
    echarts_window_->SetInitData(std::move(data));

    echarts_window_->SetHandler([this](const nlohmann::json &req) {
      if (req.contains("interval")) {
        if (on_interval_changed_) {
          on_interval_changed_(req.at("interval").get<std::string>());
        }
      }
      return nlohmann::json{};
    });
    echarts_thread_ = std::thread([this]() { echarts_window_->Show(); });
  }
#else
  Core::Logger::instance().warn(
      "ECharts disabled: install the webview dependency and rebuild with USE_WEBVIEW enabled");
#endif
  return true;
}

void UiManager::begin_frame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void UiManager::draw_echarts_panel(const std::string &selected_interval) {
  ImGui::Begin("Chart");
#if USE_WEBVIEW
  if (!resources_available_) {
    ImGui::Text("Chart resources missing");
  } else if (echarts_window_) {
    if (selected_interval != current_interval_) {
      echarts_window_->SendToJs(
          nlohmann::json{{"interval", selected_interval}});
      current_interval_ = selected_interval;
    }
    ImVec2 avail = ImGui::GetContentRegionAvail();
    echarts_window_->SetSize(static_cast<int>(avail.x),
                             static_cast<int>(avail.y));
    ImGui::BeginChild("EChartsView", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (void *handle = echarts_window_->GetNativeHandle()) {
      ImGui::Image(reinterpret_cast<ImTextureID>(handle), avail);
    } else {
      ImGui::Text("Loading chart...");
    }
    ImGui::EndChild();
  }
#else
  ImGui::Text(
      "ECharts disabled. Please install/enable the webview dependency (USE_WEBVIEW)");
#endif
  ImGui::End();
}

void UiManager::set_interval_callback(
    std::function<void(const std::string &)> cb) {
  on_interval_changed_ = std::move(cb);
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
  if (echarts_thread_.joinable()) {
    if (echarts_window_) {
      try {
        echarts_window_->Close();
      } catch (const std::exception &e) {
        Core::Logger::instance().error(
            std::string("Failed to close ECharts window: ") + e.what());
      }
    }
    echarts_thread_.join();
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
