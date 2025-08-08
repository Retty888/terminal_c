#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>

// Add required headers for our app
#include "implot.h"
#include "core/candle_manager.h"
#include "core/data_fetcher.h"
#include "core/analytics.h"

// Data
static ID3D11Device*            g_pd3d11Device = nullptr;
static ID3D11DeviceContext*     g_pd3d11DeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global application state (or static in main if preferred)
std::map<std::string, std::map<std::string, std::vector<Core::Candle>>> all_candles;
std::vector<std::string> tracked_symbols;
std::string current_symbol = "BTCUSDT";
std::string current_interval = "1h";

const std::vector<std::string> available_timeframes = {"1m", "5m", "15m", "30m", "1h", "4h", "1d"};

// Helper function to fetch data for a symbol across all timeframes
void FetchAndStoreDataForSymbol(const std::string& symbol, std::stringstream& log_buffer) {
    log_buffer << "Fetching data for symbol: " << symbol << "...\n";
    for (const auto& interval : available_timeframes) {
        log_buffer << "  Fetching " << interval << "... ";
        try {
            std::vector<Core::Candle> fetched_candles = Core::DataFetcher::fetch_klines(symbol, interval, 5000); // Fetch 5000 candles
            if (fetched_candles.empty()) {
                log_buffer << "Warning: Returned 0 candles.\n";
            } else {
                all_candles[symbol][interval] = fetched_candles;
                log_buffer << "Successfully fetched " << fetched_candles.size() << " candles.\n";
            }
        } catch (const std::exception& e) {
            log_buffer << "ERROR: Exception: " << e.what() << "\n";
        } catch (...) {
            log_buffer << "ERROR: Unknown exception.\n";
        }
    }
}

// Main code
int main(int, char**)
{
    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("ImGui Example"), nullptr };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Trading Terminal C++"), WS_OVERLAPPEDWINDOW, 100, 100, 1600, 900, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3d11Device, g_pd3d11DeviceContext);

    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);

    // Local log buffer
    static std::stringstream log_buffer; 

    // Initial setup
    tracked_symbols.push_back("BTCUSDT");
    FetchAndStoreDataForSymbol("BTCUSDT", log_buffer);

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // --- Main Menu Bar ---
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit")) { done = true; }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMainMenuBar();

        // --- Control Panel Window ---
        ImGui::Begin("Control Panel");
        
        // Symbol Management
        ImGui::Text("Tracked Symbols:");
        ImGui::SameLine();
        static char new_symbol_buffer[128] = "";
        ImGui::InputText("##NewSymbol", new_symbol_buffer, IM_ARRAYSIZE(new_symbol_buffer));
        ImGui::SameLine();
        if (ImGui::Button("Add Symbol")) {
            std::string new_sym = new_symbol_buffer;
            if (!new_sym.empty() && std::find(tracked_symbols.begin(), tracked_symbols.end(), new_sym) == tracked_symbols.end()) {
                tracked_symbols.push_back(new_sym);
                FetchAndStoreDataForSymbol(new_sym, log_buffer);
                current_symbol = new_sym; // Switch to newly added symbol
            }
            new_symbol_buffer[0] = '\0'; // Clear input
        }

        ImGui::Separator();

        // Display and manage current symbols
        for (const auto& sym : tracked_symbols) {
            if (ImGui::Button(sym.c_str())) {
                current_symbol = sym;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("Remove ") + sym).c_str())) {
                // Remove symbol logic
                tracked_symbols.erase(std::remove(tracked_symbols.begin(), tracked_symbols.end(), sym), tracked_symbols.end());
                all_candles.erase(sym);
                if (current_symbol == sym && !tracked_symbols.empty()) {
                    current_symbol = tracked_symbols[0];
                }
                break; // Break to avoid iterator invalidation
            }
            ImGui::SameLine();
        }

        ImGui::Separator();

        // Timeframe selection
        ImGui::Text("Timeframe: ");
        ImGui::SameLine();
        for (const auto& tf : available_timeframes) {
            if (ImGui::Button(tf.c_str())) {
                current_interval = tf;
            }
            ImGui::SameLine();
        }

        ImGui::End();

        // --- Indicators Window ---
        ImGui::Begin("Indicators");
        static bool show_ma = false;
        static int ma_period = 20;
        ImGui::Checkbox("Moving Average", &show_ma);
        if (show_ma) {
            ImGui::InputInt("MA Period", &ma_period);
        }
        static bool show_rsi = false;
        static int rsi_period = 14;
        ImGui::Checkbox("RSI", &show_rsi);
        if (show_rsi) {
            ImGui::InputInt("RSI Period", &rsi_period);
        }
        static bool show_bb = false;
        static int bb_period = 20;
        static double bb_stddev = 2.0;
        ImGui::Checkbox("Bollinger Bands", &show_bb);
        if (show_bb) {
            ImGui::InputInt("BB Period", &bb_period);
            ImGui::InputDouble("BB StdDev", &bb_stddev);
        }
        ImGui::End();

        // --- Signals Table Window ---
        ImGui::Begin("Trading Signals");
        // ... (Table content remains the same for now)
        ImGui::End();

        // --- Chart Window ---
        ImGui::Begin("Price Chart");
        if (ImPlot::BeginPlot("Candlestick Chart", ImVec2(-1, -1))) {
            if (all_candles.count(current_symbol) && all_candles[current_symbol].count(current_interval)) {
                const auto& candles_to_plot = all_candles[current_symbol][current_interval];
                if (!candles_to_plot.empty()) {
                    std::vector<double> timestamps, closes;
                    for (const auto& candle : candles_to_plot) {
                        timestamps.push_back(static_cast<double>(candle.open_time / 1000));
                        closes.push_back(candle.close);
                    }

                    ImPlot::SetupAxes("Time", "Price");
                    ImPlot::PlotLine("Close Price", timestamps.data(), closes.data(), closes.size());
                    if (show_ma && ma_period > 0 && closes.size() >= static_cast<size_t>(ma_period)) {
                        auto ma = Core::Analytics::moving_average(closes, static_cast<std::size_t>(ma_period));
                        ImPlot::PlotLine("MA", timestamps.data() + (ma_period - 1), ma.data(), ma.size());
                    }
                    if (show_bb && bb_period > 0 && closes.size() >= static_cast<size_t>(bb_period)) {
                        auto bands = Core::Analytics::bollinger_bands(closes, static_cast<std::size_t>(bb_period), bb_stddev);
                        const auto& upper = std::get<0>(bands);
                        const auto& middle = std::get<1>(bands);
                        const auto& lower = std::get<2>(bands);
                        ImPlot::PlotLine("BB Upper", timestamps.data() + (bb_period - 1), upper.data(), upper.size());
                        ImPlot::PlotLine("BB Middle", timestamps.data() + (bb_period - 1), middle.data(), middle.size());
                        ImPlot::PlotLine("BB Lower", timestamps.data() + (bb_period - 1), lower.data(), lower.size());
                    }
                }
            }
            ImPlot::EndPlot();
        }
        ImGui::End();

        // --- RSI Window ---
        if (show_rsi) {
            ImGui::Begin("RSI");
            if (all_candles.count(current_symbol) && all_candles[current_symbol].count(current_interval)) {
                const auto& candles_to_plot = all_candles[current_symbol][current_interval];
                if (candles_to_plot.size() > static_cast<size_t>(rsi_period)) {
                    std::vector<double> rsi_timestamps, rsi_closes;
                    for (const auto& candle : candles_to_plot) {
                        rsi_timestamps.push_back(static_cast<double>(candle.open_time / 1000));
                        rsi_closes.push_back(candle.close);
                    }
                    auto rsi_vals = Core::Analytics::rsi(rsi_closes, static_cast<std::size_t>(rsi_period));
                    if (ImPlot::BeginPlot("RSI", ImVec2(-1, -1))) {
                        ImPlot::PlotLine("RSI", rsi_timestamps.data() + rsi_period, rsi_vals.data(), rsi_vals.size());
                        ImPlot::EndPlot();
                    }
                }
            }
            ImGui::End();
        }

        // --- Log/Info Window ---
        ImGui::Begin("System Information");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Separator();
        ImGui::TextUnformatted(log_buffer.str().c_str());
        ImGui::End();

        // --- Signal Details Window ---
        ImGui::Begin("Signal Details & Management");
        // ... (Details content remains the same)
        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3d11DeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3d11DeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
    }

    // Cleanup
    ImPlot::DestroyContext();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions (the same as before)
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3d11Device, &featureLevel, &g_pd3d11DeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3d11Device, &featureLevel, &g_pd3d11DeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3d11DeviceContext) { g_pd3d11DeviceContext->Release(); g_pd3d11DeviceContext = nullptr; }
    if (g_pd3d11Device) { g_pd3d11Device->Release(); g_pd3d11Device = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3d11Device->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_Proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_Proc(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3d11Device != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(LPARAM), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}