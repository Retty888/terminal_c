#ifdef _WIN32

#include "dx11_context.h"
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <cstdio>
#include <string>

using Microsoft::WRL::ComPtr;

namespace Core {

Dx11Context& Dx11Context::instance() {
  static Dx11Context ctx;
  return ctx;
}

bool Dx11Context::initialize(HWND hwnd, int width, int height) {
  UINT flags = 0;
#if defined(_DEBUG)
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                  feature_levels, _countof(feature_levels), D3D11_SDK_VERSION,
                                  device_.ReleaseAndGetAddressOf(), &feature_level, context_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) return false;
  if (!create_swapchain(hwnd, width, height)) return false;
  if (!create_render_target()) return false;
  return true;
}

bool Dx11Context::create_swapchain(HWND hwnd, int width, int height) {
  ComPtr<IDXGIDevice> dxgiDevice;
  if (FAILED(device_.As(&dxgiDevice))) return false;
  ComPtr<IDXGIAdapter> adapter;
  if (FAILED(dxgiDevice->GetAdapter(adapter.ReleaseAndGetAddressOf()))) return false;
  ComPtr<IDXGIFactory> factory;
  if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(factory.ReleaseAndGetAddressOf())))) return false;

  // Try modern flip-model swapchain first
  ComPtr<IDXGIFactory2> factory2;
  if (SUCCEEDED(factory.As(&factory2)) && factory2) {
    DXGI_SWAP_CHAIN_DESC1 sd1 = {};
    sd1.BufferCount = 2;
    sd1.Width = static_cast<UINT>(width > 0 ? width : 1);
    sd1.Height = static_cast<UINT>(height > 0 ? height : 1);
    sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.SampleDesc.Count = 1;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd1.Scaling = DXGI_SCALING_STRETCH;
    sd1.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    Core::Logger::instance().info("Attempting to create modern swapchain");
    ComPtr<IDXGISwapChain1> sc1;
    HRESULT hr = factory2->CreateSwapChainForHwnd(device_.Get(), hwnd, &sd1, nullptr, nullptr, sc1.GetAddressOf());
    if (SUCCEEDED(hr)) {
      sc1.As(&swapchain_);
    } else {
      std::stringstream ss;
      ss << "CreateSwapChainForHwnd failed with HRESULT: 0x" << std::hex << hr;
      Core::Logger::instance().warn(ss.str());
    }
  }
  // Fallback to legacy swapchain if flip model creation failed
  if (!swapchain_) {
    Core::Logger::instance().info("Attempting to create legacy swapchain");
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = width > 0 ? width : 1;
    sd.BufferDesc.Height = height > 0 ? height : 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    HRESULT hr = factory->CreateSwapChain(device_.Get(), &sd, swapchain_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
      std::stringstream ss;
      ss << "CreateSwapChain failed with HRESULT: 0x" << std::hex << hr;
      Core::Logger::instance().error(ss.str());
      MessageBoxA(NULL, ss.str().c_str(), "Error", MB_OK);
      return false;
    }
  }
  return true;
}

bool Dx11Context::create_render_target() {
  ComPtr<ID3D11Texture2D> backbuffer;
  if (FAILED(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backbuffer.ReleaseAndGetAddressOf()))))
    return false;
  if (FAILED(device_->CreateRenderTargetView(backbuffer.Get(), nullptr, rtv_.ReleaseAndGetAddressOf())))
    return false;
  return true;
}

void Dx11Context::cleanup_render_target() {
  rtv_.Reset();
}

void Dx11Context::resize(int width, int height) {
  if (!swapchain_) return;
  cleanup_render_target();
  // Resize with explicit format for consistency
  swapchain_->ResizeBuffers(2, width > 0 ? width : 1, height > 0 ? height : 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
  create_render_target();
}

void Dx11Context::begin_frame(float r, float g, float b, float a) {
  ID3D11RenderTargetView* rt = rtv_.Get();
  context_->OMSetRenderTargets(1, &rt, nullptr);
  // Ensure viewport matches current backbuffer size
  if (swapchain_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> bb;
    if (SUCCEEDED(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(bb.ReleaseAndGetAddressOf())))) {
      D3D11_TEXTURE2D_DESC desc{}; bb->GetDesc(&desc);
      D3D11_VIEWPORT vp{}; vp.TopLeftX = 0; vp.TopLeftY = 0; vp.MinDepth = 0; vp.MaxDepth = 1;
      vp.Width = static_cast<FLOAT>(desc.Width); vp.Height = static_cast<FLOAT>(desc.Height);
      context_->RSSetViewports(1, &vp);
    }
  }
  const float clear_col[4] = { r, g, b, a };
  context_->ClearRenderTargetView(rtv_.Get(), clear_col);
  // Occasional viewport log for diagnostics
  static int s_log = 0;
  if ((++s_log % 180) == 1) {
    D3D11_VIEWPORT vp{}; UINT n=1; context_->RSGetViewports(&n, &vp);
    char buf[128];
    snprintf(buf, sizeof(buf), "DX11 VP=%.0fx%.0f at %.0f,%.0f", vp.Width, vp.Height, vp.TopLeftX, vp.TopLeftY);
    Core::Logger::instance().info(std::string(buf));
  }
}

void Dx11Context::end_frame() {
  // Optional one-shot dump controlled by env
  static int s_should_dump = [](){ const char* e = std::getenv("CANDLE_DUMP_FRAME"); return (e && e[0]=='1') ? 1 : 0; }();
  if (s_should_dump) {
    save_backbuffer_png(L"frame_dump.png");
    s_should_dump = 0;
  }
  if (swapchain_) swapchain_->Present(1, 0);
}

namespace {
struct DebugVert { float x, y; };
struct DebugColor { float rgba[4]; };
}

void Dx11Context::debug_draw_fullscreen_rect(float r, float g, float b, float a) {
  if (!device_ || !context_) return;
  HRESULT hr;
  if (!dbg_vs_ || !dbg_ps_ || !dbg_layout_) {
    static const char* vs_src =
      "struct VS_IN { float2 pos : POSITION; };\n"
      "struct VS_OUT { float4 pos : SV_POSITION; };\n"
      "VS_OUT main(VS_IN vin){ VS_OUT o; o.pos=float4(vin.pos,0,1); return o; }";
    static const char* ps_src =
      "cbuffer CB : register(b0) { float4 color; };\n"
      "float4 main() : SV_Target { return color; }";
    Microsoft::WRL::ComPtr<ID3DBlob> vsb, psb, err;
    hr = D3DCompile(vs_src, strlen(vs_src), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, vsb.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) return;
    hr = device_->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, dbg_vs_.GetAddressOf());
    if (FAILED(hr)) return;
    D3D11_INPUT_ELEMENT_DESC il = { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    hr = device_->CreateInputLayout(&il, 1, vsb->GetBufferPointer(), vsb->GetBufferSize(), dbg_layout_.GetAddressOf());
    if (FAILED(hr)) return;
    hr = D3DCompile(ps_src, strlen(ps_src), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, psb.GetAddressOf(), err.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return;
    hr = device_->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, dbg_ps_.GetAddressOf());
    if (FAILED(hr)) return;
    D3D11_BUFFER_DESC cbd = {}; cbd.ByteWidth = sizeof(DebugColor); cbd.Usage = D3D11_USAGE_DYNAMIC; cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device_->CreateBuffer(&cbd, nullptr, dbg_cb_.GetAddressOf());
    if (FAILED(hr)) return;
  }
  if (!dbg_vb_) {
    DebugVert verts[6] = {
      {-1.f, -1.f}, { 1.f, -1.f}, { 1.f,  1.f},
      {-1.f, -1.f}, { 1.f,  1.f}, {-1.f,  1.f}
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_IMMUTABLE; bd.ByteWidth = sizeof(verts); bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd = { verts, 0, 0 };
    hr = device_->CreateBuffer(&bd, &srd, dbg_vb_.GetAddressOf());
    if (FAILED(hr)) return;
  }
  // Upload color
  D3D11_MAPPED_SUBRESOURCE map;
  if (SUCCEEDED(context_->Map(dbg_cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
    auto* cb = reinterpret_cast<DebugColor*>(map.pData);
    cb->rgba[0] = r; cb->rgba[1] = g; cb->rgba[2] = b; cb->rgba[3] = a;
    context_->Unmap(dbg_cb_.Get(), 0);
  }
  UINT stride = sizeof(DebugVert), offset = 0;
  ID3D11Buffer* vb = dbg_vb_.Get();
  context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
  context_->IASetInputLayout(dbg_layout_.Get());
  context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  if (dbg_rs_noscissor_) context_->RSSetState(dbg_rs_noscissor_.Get());
  // Ensure full-RT viewport
  if (swapchain_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> bb;
    if (SUCCEEDED(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(bb.GetAddressOf())))) {
      D3D11_TEXTURE2D_DESC desc{}; bb->GetDesc(&desc);
      D3D11_VIEWPORT vp{}; vp.TopLeftX = 0; vp.TopLeftY = 0; vp.MinDepth = 0; vp.MaxDepth = 1;
      vp.Width = static_cast<FLOAT>(desc.Width); vp.Height = static_cast<FLOAT>(desc.Height);
      context_->RSSetViewports(1, &vp);
    }
  }
  context_->VSSetShader(dbg_vs_.Get(), nullptr, 0);
  context_->PSSetShader(dbg_ps_.Get(), nullptr, 0);
  if (dbg_cb_) {
    ID3D11Buffer* cb = dbg_cb_.Get();
    context_->PSSetConstantBuffers(0, 1, &cb);
  }
  ID3D11RenderTargetView* rt = rtv_.Get();
  context_->OMSetRenderTargets(1, &rt, nullptr);
  context_->Draw(6, 0);
}

void Dx11Context::debug_draw_corner_marker(float r, float g, float b, float a) {
  if (!device_ || !context_) return;
  HRESULT hr;
  if (!dbg_vs_ || !dbg_ps_ || !dbg_layout_ || !dbg_cb_) {
    // Ensure pipeline exists by calling the other helper which lazily builds it
    debug_draw_fullscreen_rect(r, g, b, a);
  }
  // Create a rasterizer state with scissor disabled so our marker isn't clipped
  if (!dbg_rs_noscissor_) {
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.ScissorEnable = FALSE;
    rs.DepthClipEnable = TRUE;
    rs.MultisampleEnable = FALSE;
    rs.AntialiasedLineEnable = FALSE;
    device_->CreateRasterizerState(&rs, dbg_rs_noscissor_.GetAddressOf());
  }
  if (!dbg_marker_vb_) {
    struct V { float x, y; };
    // Small triangle near top-left in clip space
    V v[3] = { {-0.98f, 0.98f}, {-0.80f, 0.98f}, {-0.98f, 0.80f} };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_IMMUTABLE; bd.ByteWidth = sizeof(v); bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd = { v, 0, 0 };
    hr = device_->CreateBuffer(&bd, &srd, dbg_marker_vb_.GetAddressOf());
    if (FAILED(hr)) return;
  }
  // Upload color
  if (dbg_cb_) {
    D3D11_MAPPED_SUBRESOURCE map;
    if (SUCCEEDED(context_->Map(dbg_cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
      struct DebugColor { float rgba[4]; };
      auto* cb = reinterpret_cast<DebugColor*>(map.pData);
      cb->rgba[0] = r; cb->rgba[1] = g; cb->rgba[2] = b; cb->rgba[3] = a;
      context_->Unmap(dbg_cb_.Get(), 0);
    }
  }
  UINT stride = sizeof(float) * 2, offset = 0;
  ID3D11Buffer* vb = dbg_vb_.Get();
  context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
  context_->IASetInputLayout(dbg_layout_.Get());
  context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  if (dbg_rs_noscissor_) context_->RSSetState(dbg_rs_noscissor_.Get());
  // Ensure full-RT viewport
  if (swapchain_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> bb;
    if (SUCCEEDED(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(bb.GetAddressOf())))) {
      D3D11_TEXTURE2D_DESC desc{}; bb->GetDesc(&desc);
      D3D11_VIEWPORT vp{}; vp.TopLeftX = 0; vp.TopLeftY = 0; vp.MinDepth = 0; vp.MaxDepth = 1;
      vp.Width = static_cast<FLOAT>(desc.Width); vp.Height = static_cast<FLOAT>(desc.Height);
      context_->RSSetViewports(1, &vp);
    }
  }
  context_->VSSetShader(dbg_vs_.Get(), nullptr, 0);
  context_->PSSetShader(dbg_ps_.Get(), nullptr, 0);
  if (dbg_cb_) {
    ID3D11Buffer* cb = dbg_cb_.Get();
    context_->PSSetConstantBuffers(0, 1, &cb);
  }
  ID3D11RenderTargetView* rt = rtv_.Get();
  context_->OMSetRenderTargets(1, &rt, nullptr);
  context_->Draw(6, 0);
}

void Dx11Context::force_no_scissor() {
  if (!device_ || !context_) return;
  if (!dbg_rs_noscissor_) {
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.ScissorEnable = FALSE;
    rs.DepthClipEnable = TRUE;
    rs.MultisampleEnable = FALSE;
    rs.AntialiasedLineEnable = FALSE;
    device_->CreateRasterizerState(&rs, dbg_rs_noscissor_.GetAddressOf());
  }
  if (dbg_rs_noscissor_) context_->RSSetState(dbg_rs_noscissor_.Get());
}

void Dx11Context::reset_state() {
  if (!context_) return;
  context_->ClearState();
  // Rebind our render target and viewport after clearing state
  if (rtv_) {
    ID3D11RenderTargetView* rt = rtv_.Get();
    context_->OMSetRenderTargets(1, &rt, nullptr);
  }
  if (swapchain_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> bb;
    if (SUCCEEDED(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(bb.GetAddressOf())))) {
      D3D11_TEXTURE2D_DESC desc{}; bb->GetDesc(&desc);
      D3D11_VIEWPORT vp{}; vp.TopLeftX = 0; vp.TopLeftY = 0; vp.MinDepth = 0; vp.MaxDepth = 1;
      vp.Width = static_cast<FLOAT>(desc.Width); vp.Height = static_cast<FLOAT>(desc.Height);
      context_->RSSetViewports(1, &vp);
    }
  }
}

bool Dx11Context::save_backbuffer_png(const wchar_t* path) {
  if (!device_ || !context_ || !swapchain_) return false;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
  if (FAILED(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backbuffer.GetAddressOf()))))
    return false;
  D3D11_TEXTURE2D_DESC desc{}; backbuffer->GetDesc(&desc);
  D3D11_TEXTURE2D_DESC sd = desc; sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
  if (FAILED(device_->CreateTexture2D(&sd, nullptr, staging.GetAddressOf()))) return false;
  context_->CopyResource(staging.Get(), backbuffer.Get());
  D3D11_MAPPED_SUBRESOURCE map{};
  if (FAILED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map))) return false;

  bool ok = false;
  IWICImagingFactory* factory = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  IStream* stream = nullptr;
  do {
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) break;
    if (FAILED(SHCreateStreamOnFileW(path, STGM_CREATE|STGM_WRITE, &stream))) break;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))) break;
    if (FAILED(encoder->Initialize(stream, WICBitmapEncoderNoCache))) break;
    if (FAILED(encoder->CreateNewFrame(&frame, nullptr))) break;
    if (FAILED(frame->Initialize(nullptr))) break;
    if (FAILED(frame->SetSize(desc.Width, desc.Height))) break;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA; // matches DXGI_FORMAT_R8G8B8A8_UNORM BGRA order for ImGui
    if (FAILED(frame->SetPixelFormat(&format))) break;
    // Write line by line to handle row pitch
    for (UINT y = 0; y < desc.Height; ++y) {
      BYTE* src = static_cast<BYTE*>(map.pData) + y * map.RowPitch;
      if (FAILED(frame->WritePixels(1, desc.Width * 4, desc.Width * 4, src))) { ok=false; break; }
      ok = true;
    }
    if (ok) {
      frame->Commit(); encoder->Commit();
    }
  } while(false);
  if (frame) frame->Release();
  if (encoder) encoder->Release();
  if (stream) stream->Release();
  if (factory) factory->Release();
  context_->Unmap(staging.Get(), 0);
  return ok;
}

} // namespace Core

#endif // _WIN32
