# terminal-c

Standalone C++ trading terminal using ImGui. The project relies on packages provided by `vcpkg` and `find_package` in CMake. The chart panel is powered by TradingView's Lightweight Charts rendered inside an embedded WebView.

## Состав

- `main.cpp` с ImGui интерфейсом
- Поддержка нескольких торговых пар
- Загрузка свечей с Binance API
- Потоковое обновление свечей через WebSocket Binance
- Панель графиков на базе TradingView Lightweight Charts (через WebView)
- Импортированные библиотеки:
  - ImGui
  - CPR (встроен)
  - JSON (встроен)
  - webview (встроен)
- `CMakeLists.txt` использует `find_package()` для зависимостей через `vcpkg`

## Инструкция

1. Открой `CMakeLists.txt` в Visual Studio
2. Выбери сборку `x64-Debug`
3. Нажми `Ctrl+Shift+B` для сборки
4. Запусти `TradingTerminal.exe`

📌 Требуется установленный `vcpkg` и предварительная загрузка зависимостей

## Сборка

1. Установите [vcpkg](https://github.com/microsoft/vcpkg) и настройте переменную `CMAKE_TOOLCHAIN_FILE` на `scripts/buildsystems/vcpkg.cmake`.
   Опциональные зависимости:
   - `imgui` — используется из пакета, если он установлен; иначе проект собирает встроенные исходники из `third_party/imgui`.
2. Выполните конфигурацию проекта:
   ```
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```
3. Соберите проект:
   ```
   cmake --build build
   ```
4. Готовый исполняемый файл `TradingTerminal` (или `TradingTerminal.exe` на Windows) появится в каталоге `build`.

## Charting

The HTML file under `resources/` embeds [TradingView Lightweight Charts](https://github.com/tradingview/lightweight-charts) and is displayed through the cross‑platform [`webview`](https://github.com/webview/webview) library.

### WebView requirements

- **Windows:** requires the [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/).
- **Linux:** depends on WebKitGTK packages (`libwebkit2gtk-4.1-0` and related).

### TradingView script

`prepare_chart_resources.bat` downloads `lightweight-charts.standalone.production.js` from the official CDN if it is not already present and copies it along with `chart.html` next to the build output. The script is invoked automatically by `build_and_run.bat` for each build configuration or can be run manually:

```
prepare_chart_resources.bat <build_directory>
```

### Marking trades

Trades can be visualised on the chart with markers using `UiManager::set_markers`, which forwards the data to `series.setMarkers` in JavaScript. Each marker specifies a time, position and shape:

```cpp
ui.set_markers(R"([
  {"time": 1716900000, "position": "belowBar", "shape": "arrowUp", "color": "green", "text": "BUY"},
  {"time": 1716950000, "position": "aboveBar", "shape": "arrowDown", "color": "red", "text": "SELL"}
])");
```


## Streaming

Файл `config.json` содержит флаг `enable_streaming`. При значении `true` приложение подключается к `wss://stream.binance.com:9443/ws/{symbol}@kline_{interval}` и обновляет свечи в реальном времени. Если флаг выключен или потоковый канал отключается, используется обычный HTTP-поллинг.

