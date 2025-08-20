#include "ui_manager.h"

#include <GLFW/glfw3.h>
#include <exception>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <thread>
#include <utility>
#include <chrono>

#include "core/candle_manager.h"
#include "core/logger.h"
#include "core/path_utils.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui/echarts_window.h"

UiManager::~UiManager() = default;

bool UiManager::setup(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  const auto ini_path = Core::path_from_executable("imgui.ini");
  std::filesystem::create_directories(ini_path.parent_path());
  static std::string ini_path_str = ini_path.string();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = ini_path_str.c_str();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");
#if USE_WEBVIEW
  const auto html_path = Core::path_from_executable("resources/chart.html");
  const auto js_path =
      Core::path_from_executable("third_party/echarts/echarts.min.js");

  Core::Logger::instance().info("Checking chart resources at " +
                                html_path.string() + " and " +
                                js_path.string());

  resources_available_ =
      std::filesystem::exists(html_path) && std::filesystem::exists(js_path);
  if (!resources_available_) {
    Core::Logger::instance().error("Chart resources missing. Checked '" +
                                   html_path.string() + "' and '" +
                                   js_path.string() + "'");
    Core::Logger::instance().warn(
        "Expected directory layout relative to the executable:\n"
        "  resources/chart.html\n  third_party/echarts/echarts.min.js");
  } else {
    echarts_window_ = std::make_unique<EChartsWindow>(html_path.string());

    candle_future_ = std::async(std::launch::async, []() {
      Core::CandleManager cm;
      return cm.load_candles_json("BTCUSDT", "1m");
    });

    echarts_window_->SetHandleCallback([this](void *handle) {
      echarts_native_handle_.store(handle);
    });

    echarts_window_->SetErrorCallback([this](const std::string &msg) {
      {
        std::lock_guard<std::mutex> lock(echarts_mutex_);
        echarts_error_ = msg;
      }
      if (status_callback_) {
        status_callback_(msg);
      }
    });

    echarts_window_->SetHandler([this](const nlohmann::json &req) {
      if (req.contains("interval")) {
        if (on_interval_changed_) {
          on_interval_changed_(req.at("interval").get<std::string>());
        }
      }
      if (req.contains("request") && req.at("request") == "init") {
        if (!current_interval_.empty()) {
          echarts_window_->SendToJs(
              nlohmann::json{{"interval", current_interval_}});
        }
      }
      return nlohmann::json{};
    });
    echarts_thread_ = std::thread([this]() {
      try {
        echarts_window_->Show();
      } catch (const std::exception &e) {
        const std::string msg =
            std::string("Failed to run ECharts window: ") + e.what();
        Core::Logger::instance().error(msg);
        {
          std::lock_guard<std::mutex> lock(echarts_mutex_);
          echarts_error_ = msg;
        }
        if (status_callback_) {
          status_callback_(msg);
        }
      } catch (...) {
        const std::string msg = "Failed to run ECharts window: unknown error";
        Core::Logger::instance().error(msg);
        {
          std::lock_guard<std::mutex> lock(echarts_mutex_);
          echarts_error_ = msg;
        }
        if (status_callback_) {
          status_callback_(msg);
        }
      }
    });
  }
#else
  Core::Logger::instance().warn(
      "ECharts disabled: install the webview dependency and rebuild with "
      "USE_WEBVIEW enabled");
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
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                       "Chart resources missing");
    ImGui::TextWrapped(
        "Expected layout relative to the executable:\n"
        "  resources/chart.html\n  third_party/echarts/echarts.min.js");
  } else {
    std::string err;
    {
      std::lock_guard<std::mutex> lock(echarts_mutex_);
      err = echarts_error_;
    }
    if (!candles_loaded_ && candle_future_.valid() &&
        echarts_native_handle_.load()) {
      if (candle_future_.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        try {
          auto data = candle_future_.get();
          echarts_window_->SendToJs(data);
        } catch (const std::exception &e) {
          Core::Logger::instance().error(e.what());
        }
        candles_loaded_ = true;
      }
    }
    if (!err.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                         "Failed to load chart");
      ImGui::TextWrapped("%s", err.c_str());
    } else if (!candles_loaded_) {
      ImGui::Text("Loading...");
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
                        ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoScrollWithMouse);
      if (void *handle = echarts_native_handle_.load()) {
        ImGui::Image(reinterpret_cast<ImTextureID>(handle), avail);
      } else {
        ImGui::Text("Loading chart...");
      }
      ImGui::EndChild();
    } else {
      ImGui::Text("Loading chart...");
    }
  }
#else
  ImGui::Text("ECharts disabled. Please install/enable the webview dependency "
              "(USE_WEBVIEW)");
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
#if USE_WEBVIEW
  if (echarts_window_) {
    echarts_window_->SendToJs(nlohmann::json{{"interval", interval}});
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
