#pragma once
struct GLFWwindow;
inline void glfwSwapBuffers(GLFWwindow*) {}
inline void glViewport(int, int, int, int) {}
inline void glClearColor(float, float, float, float) {}
inline void glClear(unsigned int) {}
constexpr unsigned int GL_COLOR_BUFFER_BIT = 0;
inline void glfwGetFramebufferSize(GLFWwindow*, int*, int*) {}
