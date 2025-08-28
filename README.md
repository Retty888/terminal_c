# terminal-c

Standalone C++ trading terminal using ImGui. The project relies on packages provided by `vcpkg` and `find_package` in CMake. The chart panel is powered by TradingView's Lightweight Charts rendered inside an embedded WebView.

## Быстрый обзор

- `main.cpp`: входная точка приложения.
- UI: ImGui + ImPlot + OpenGL + GLFW.
- Данные: REST/WS Binance (обёртки в `core/*`).
- Чарт: TradingView Lightweight Charts внутри WebView (отдельное окно).
- Зависимости: `vcpkg`, `find_package` в CMake.
- Библиотеки: ImGui, ImPlot, nlohmann-json, cpr, WebView2 (Windows) / WebKitGTK (Linux).

## Сборка и запуск

1. Откройте `CMakeLists.txt` в Visual Studio или используйте CMake CLI.
2. Выберите конфигурацию `x64-Debug` (или `Release`).
3. Соберите проект: `cmake --build build` или `Ctrl+Shift+B` в VS.
4. Запустите `TradingTerminal.exe` из каталога сборки.

Примечание: требуется установленный `vcpkg` и корректный `CMAKE_TOOLCHAIN_FILE`.

## Charting

The HTML file under `resources/` embeds [TradingView Lightweight Charts](https://github.com/tradingview/lightweight-charts) and is displayed through a WebView.

### WebView requirements

- Windows: [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)
- Linux: WebKitGTK packages (`libwebkit2gtk-4.1-0` and related)

If the platform WebView is unavailable, a legacy single-header implementation (`third_party/webview_legacy/webview`) may be used on supported platforms.

### Ресурсы чарта

Скрипты Windows находятся в `scripts/`.

`scripts/prepare_chart_resources.bat` копирует `chart.html` и `lightweight-charts.standalone.production.js` в подкаталог `resources` рядом с бинарником. Скрипт вызывается автоматически пост-степом сборки или вручную:

```
scripts/prepare_chart_resources.bat <build_directory>
```

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

