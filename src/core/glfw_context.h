#pragma once

namespace Core {

class GlfwContext {
public:
  GlfwContext();
  ~GlfwContext();

  GlfwContext(const GlfwContext &) = delete;
  GlfwContext &operator=(const GlfwContext &) = delete;
  GlfwContext(GlfwContext &&) = delete;
  GlfwContext &operator=(GlfwContext &&) = delete;

  bool initialized() const { return initialized_; }

private:
  bool initialized_ = false;
};

} // namespace Core

