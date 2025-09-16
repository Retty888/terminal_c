# Build and Run on Windows

## Build (vcpkg + CMakePresets)

- Configure: `cmake --preset default-vcpkg-Release`
- Build: `cmake --build --preset build-rel --parallel 8`

Debug configuration is also available via `default-vcpkg-Debug` and `build-dbg`.

## Run

From repo root:

```
set CANDLE_VIS_DEBUG=1
set CANDLE_RESET_LAYOUT=1
set CANDLE_FORCE_CENTER=1
build_vs_rel\Release\TradingTerminal.exe
```

Optional:

- `set CANDLE_NO_MAXIMIZE=1` — не разворачивать окно на старте.
- `set CANDLE_DISABLE_WEBVIEW=1` — принудительно отключить WebView (сборка DX11 и так отключает WebView по умолчанию).

To pick a specific config file:

```
set CANDLE_CONFIG_PATH=%CD%\config.json
build_vs_rel\Release\TradingTerminal.exe
```

## OpenGL fallback (optional)

If you need to test the OpenGL backend on Windows:

```
cmake -S . -B build_vs_rel -G "Visual Studio 17 2022" -A x64 -D USE_OPENGL_BACKEND=ON -D CMAKE_TOOLCHAIN_FILE="%CD%/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build_vs_rel --config Release --parallel 8
build_vs_rel\Release\TradingTerminal.exe
```

