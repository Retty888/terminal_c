#pragma once

#include <GLFW/glfw3.h>

// Manages ImGui/ImPlot initialization and per-frame rendering.
class UiManager {
public:
  bool setup(GLFWwindow *window);
  void begin_frame();
  void end_frame(GLFWwindow *window);
  void shutdown();
};
