# Rendering & Troubleshooting (Windows)

## Default backend

- Windows builds use DirectX 11 (ImGui+ImPlot over GLFW).
- WebView embedding is disabled at build time; charts are native (ImPlot).

## Useful environment variables

- `CANDLE_VIS_DEBUG=1`: draw a small red corner triangle (DX11) to validate visibility.
- `CANDLE_RESET_LAYOUT=1`: reset ImGui layout once (avoids off-screen windows).
- `CANDLE_NO_MAXIMIZE=1`: do not maximize window on startup.
- `CANDLE_FORCE_CENTER=1`: center a 1280x720 window on the primary monitor.
- `CANDLE_DISABLE_WEBVIEW=1`: force-disable WebView (should be already off).

## Symptoms and fixes

- Window appears but UI invisible:
  - Ensure `CANDLE_RESET_LAYOUT=1` once to discard stale INI.
  - Run with `CANDLE_VIS_DEBUG=1` to confirm DX11 path (see red marker).
  - Try `CANDLE_FORCE_CENTER=1` to avoid edge multi-monitor cases.
- Grey screen with DX11 on rare drivers:
  - Rebuild with `-D USE_OPENGL_BACKEND=ON` to validate OpenGL path.

## Logs

- Application writes to `terminal.log` (path can be changed in `config.json`).
- Look for lines like `D3D11 context ready`, `ImGui DX11 device objects created`.

