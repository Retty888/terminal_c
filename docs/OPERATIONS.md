# Running & Troubleshooting

## Normal Run (Windows)

- Configure: `cmake --preset default-vcpkg-Release`
- Build: `cmake --build --preset build-rel`
- Run: set `CANDLE_CONFIG_PATH` to `config.json` and launch `build_vs_rel/Release/TradingTerminal.exe`.
- Resources (`resources/`) are copied next to the executable.

## Environment Flags

- `CANDLE_WEBVIEW_NO_CHILD=1`: Host WebView2 in the main window (default). Use `=0` to force child-host.
- `CANDLE_RESET_LAYOUT=1`: Delete ImGui ini and reset window layout if panels are off-screen.
- `CANDLE_DISABLE_WEBVIEW=1`: Disable WebView to isolate UI/DX11 pipeline.
- `CANDLE_IGNORE_CLOSE=1`: Ignore window close request (diagnostics).
- `CANDLE_VIS_DEBUG=1`: Show a small DX11 corner marker (diagnostics).

## WebView2 Stability

- Binds JavaScript before navigation to avoid losing `appReady`.
- Retries navigation to `chart.html` every 2s (up to 60 attempts).
- Ready timeout defaults to 120s (`webview_ready_timeout_ms`).
- Sizes WebView to chart panel each frame.

## Crash Diagnostics

- `crash.log` in the executable folder records SEH/VEH exceptions when possible.
- If you hit fast-fail (e.g., 0xC0000409) early, the process may terminate before logging. Re-run with `CANDLE_DISABLE_WEBVIEW=1` to isolate UI.

