# terminal-c

Standalone C++ trading terminal using ImGui. The project relies on packages provided by `vcpkg` and `find_package` in CMake. The chart panel is uses a native ImPlot-based renderer (no WebView/HTML).

## Build & Test (vcpkg + CMakePresets)

This repository includes `CMakePresets.json` configured to use the local `vcpkg` toolchain.

- Configure Release: `cmake --preset default-vcpkg-Release`
- Build Release: `cmake --build --preset build-rel`
- Run tests (Release): `ctest --preset test-rel --output-on-failure`

Debug configuration is also available via `default-vcpkg-Debug` and `build-dbg` / `test-dbg`.

To run the app with a custom config file from the repo root:

```
$env:CANDLE_CONFIG_PATH = (Resolve-Path 'config.json')
build_vs_rel/Release/TradingTerminal.exe
```

## Быстрый обзор

- `main.cpp`: входная точка приложения.
- UI: ImGui + ImPlot + OpenGL + GLFW.
- Данные: REST/WS Binance (обёртки в `core/*`).
- Чарт: ����: �������� ImPlot (�����/�����/�������), ��� WebView.
- Зависимости: `vcpkg`, `find_package` в CMake.
- Библиотеки: ImGui, ImPlot, nlohmann-json, cpr, (no WebView dependency).

## Сборка и запуск

1. Откройте `CMakeLists.txt` в Visual Studio или используйте CMake CLI.
2. Выберите конфигурацию `x64-Debug` (или `Release`).
3. Соберите проект: `cmake --build build` или `Ctrl+Shift+B` в VS.
4. Запустите `TradingTerminal.exe` из каталога сборки.

Примечание: требуется установленный `vcpkg` и корректный `CMAKE_TOOLCHAIN_FILE`.

## Renderer Requirement (Important)

- Windows: migrate from OpenGL to DirectX 11 backend for ImGui/ImPlot. This is an explicit product requirement. Current builds still use OpenGL while migration is in progress.

## Charting

The application renders charts natively with ImPlot (candlestick/line/area). Embedded WebView/HTML charts have been removed. Any configuration such as `chart_html_path` is ignored.

### Marking trades

Trades can be visualised on the chart with markers using `UiManager::set_markers`, which forwards the data to `series.setMarkers` in JavaScript. Each marker specifies a time, position and shape:

```cpp
ui.set_markers(R"([
  {"time": 1716900000, "position": "belowBar", "shape": "arrowUp", "color": "green", "text": "BUY"},
  {"time": 1716950000, "position": "aboveBar", "shape": "arrowDown", "color": "red", "text": "SELL"}
])");
```

### Journal export

Application trades are recorded in `journal.json`. A CSV copy (`journal.csv`) is written alongside it for analysis in spreadsheets. To disable exporting the CSV file, set `"save_journal_csv": false` in `config.json`.

## Streaming

Управляется флагом `enable_streaming` в `config.json`. При `true` приложение подключается к `wss://stream.binance.com:9443/ws/{symbol}@kline_{interval}` и обрабатывает закрытые свечи. При `false` — загрузка истории по HTTP.

## Examples

The `examples/sample_chart.py` script demonstrates how to download and display BTC/USDT candles using Python.




