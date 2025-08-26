#include "ui/ui_manager.h"
#include "core/path_utils.h"
#include <gtest/gtest.h>
#include <imgui.h>
#include <implot.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <filesystem>

TEST(UiManager, MissingChartHtmlNotifies) {
    ImGui::CreateContext();
    ImPlot::CreateContext();

    {
        UiManager ui;
        std::string msg;
        ui.set_status_callback([&](const std::string &m) { msg = m; });

        // Ensure chart.html is absent next to the executable
        auto chart_path = Core::path_from_executable("chart.html");
        std::error_code ec;
        std::filesystem::remove(chart_path, ec);
        ASSERT_FALSE(std::filesystem::exists(chart_path));

        std::vector<std::string> pairs{"BTCUSDT"};
        std::vector<std::string> intervals{"1m"};

        auto &io = ImGui::GetIO();
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        io.DisplaySize = ImVec2(800, 600);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ui.set_pairs(pairs);
        ui.set_intervals(intervals);
        ui.draw_chart_panel();

        EXPECT_FALSE(msg.empty());
        EXPECT_NE(msg.find("chart"), std::string::npos);
    }

    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}
