#include "ui/tradingview_style.h"

#include "imgui.h"
#include "implot.h"

void ApplyTradingViewStyle() {
  // ImGui style
  ImGuiStyle &style = ImGui::GetStyle();
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;
  style.WindowRounding = 0.0f;
  style.FrameRounding = 2.0f;

  ImVec4 bg = ImVec4(0.07f, 0.09f, 0.13f, 1.0f); // TradingView dark background
  style.Colors[ImGuiCol_WindowBg] = bg;
  style.Colors[ImGuiCol_ChildBg] = bg;
  style.Colors[ImGuiCol_MenuBarBg] = bg;

  // ImPlot style
  ImPlotStyle &ps = ImPlot::GetStyle();
  ps.AntiAliasedLines = true;
  ps.AntiAliasedFill = true;
  ps.PlotBorderSize = 0.0f;
  ps.Colors[ImPlotCol_PlotBg] = bg;
  ps.Colors[ImPlotCol_XAxis] = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
  ps.Colors[ImPlotCol_YAxis] = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
  ps.Colors[ImPlotCol_XAxisGrid] = ImVec4(0.18f, 0.2f, 0.25f, 1.0f);
  ps.Colors[ImPlotCol_YAxisGrid] = ImVec4(0.18f, 0.2f, 0.25f, 1.0f);
  ps.Colors[ImPlotCol_Crosshairs] = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
}
