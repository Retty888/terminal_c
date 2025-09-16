# Architecture Overview

This project is a standalone C++ trading terminal using ImGui on top of GLFW.

Rendering and charting
- Windows: ImGui runs on a DirectX 11 backend. The primary chart is TradingView Lightweight Charts rendered inside an embedded WebView2 (Edge), hosted as a native child window inside the main GLFW window. A native ImPlot chart is available as a fallback when WebView cannot initialize.
- Linux/macOS: ImGui runs on an OpenGL backend and the fallback ImPlot chart is used.

Key components
- `src/app.cpp`: app lifecycle, windowing, configuration, data fetching, and UI orchestration.
- `src/core/dx11_context.*`: minimal DX11 swapchain, render target and helpers to integrate with ImGui on Windows.
- `src/ui/ui_manager.*`: ImGui setup, main panels, embedded WebView host management, and fallback ImPlot chart.
- `resources/`: `chart.html` and `lightweight-charts.standalone.production.js` used by the WebView chart.

Charting flow (WebView2)
- UI Manager creates an HWND child sized to the ImGui chart region and initializes WebView2 in it.
- `resources/chart.html` loads Lightweight Charts and binds a small API surface. When ready, it calls `appReady('ok')` back to native.
- Native code then pushes intervals, active pair/interval, candles, series selection, and optional markers/price line to JS.
- If the page fails to load or WebView doesn’t become ready within a grace period, the UI falls back to a native ImPlot chart.

Resources
- CMake copies `resources/` next to the executable (`<build>/Release/resources`) so `chart_html_path` can resolve relative to the executable directory.

Configuration highlights (`config.json`)
- `enable_chart`: set to `true` to enable the WebView chart on Windows.
- `chart_html_path`: relative or absolute path to `chart.html` (relative paths are resolved against the executable directory).
- `enable_streaming`: toggles Binance WS stream vs. HTTP fetch.

Build (Windows via vcpkg + CMakePresets)
- Configure: `cmake --preset default-vcpkg-Release`
- Build: `cmake --build --preset build-rel`
- Run: set `CANDLE_CONFIG_PATH` to your `config.json` or place it next to the executable.

