#pragma once

#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace Core {

class Dx11Context {
public:
  static Dx11Context& instance();

  bool initialize(HWND hwnd, int width, int height);
  void resize(int width, int height);
  void begin_frame(float r = 0.15f, float g = 0.15f, float b = 0.15f, float a = 1.0f);
  void end_frame();
  // Save current backbuffer to a PNG file (debug only).
  bool save_backbuffer_png(const wchar_t* path);
  // Reset device context state to a clean baseline (unbind shaders, buffers, states).
  void reset_state();
  // Debug: draw a solid fullscreen rectangle to validate rendering path.
  void debug_draw_fullscreen_rect(float r, float g, float b, float a = 1.0f);
  // Debug: draw a small marker triangle in the top-left corner.
  void debug_draw_corner_marker(float r = 1.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
  // Force a rasterizer state with scissor disabled for subsequent draws.
  void force_no_scissor();

  ID3D11Device* device() const { return device_.Get(); }
  ID3D11DeviceContext* context() const { return context_.Get(); }

private:
  bool create_swapchain(HWND hwnd, int width, int height);
  bool create_render_target();
  void cleanup_render_target();

  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain_;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;

  // Debug pipeline resources (lazily created)
  Microsoft::WRL::ComPtr<ID3D11Buffer> dbg_vb_;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> dbg_vs_;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> dbg_ps_;
  Microsoft::WRL::ComPtr<ID3D11InputLayout> dbg_layout_;
  Microsoft::WRL::ComPtr<ID3D11Buffer> dbg_cb_;
  Microsoft::WRL::ComPtr<ID3D11RasterizerState> dbg_rs_noscissor_;
  Microsoft::WRL::ComPtr<ID3D11Buffer> dbg_marker_vb_;
};

} // namespace Core
#endif // _WIN32
