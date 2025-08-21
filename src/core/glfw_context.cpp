#include "glfw_context.h"

#include <GLFW/glfw3.h>

namespace Core {

GlfwContext::GlfwContext() { initialized_ = glfwInit(); }

GlfwContext::~GlfwContext() {
  if (initialized_)
    glfwTerminate();
}

} // namespace Core

