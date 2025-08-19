# terminal-c

Standalone C++ trading terminal using ImGui with an embedded chart powered by [Apache ECharts](https://echarts.apache.org/). The project relies on packages provided by `vcpkg` and `find_package` in CMake.

## Состав

- `main.cpp` с ImGui интерфейсом
- Поддержка нескольких торговых пар
- Загрузка свечей с Binance API
- Потоковое обновление свечей через WebSocket Binance
- Интеграция графиков на Apache ECharts
- Импортированные библиотеки:
  - ImGui
  - ECharts (через встроенный webview)
  - CPR (встроен)
  - JSON (встроен)
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
   - `webview` — обеспечивает окно графиков на ECharts. При отсутствии пакета сборка продолжится без него, и окно графиков будет отключено.
2. Выполните конфигурацию проекта:
   ```
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```
3. Соберите проект:
   ```
   cmake --build build
   ```
4. Готовый исполняемый файл `TradingTerminal` (или `TradingTerminal.exe` на Windows) появится в каталоге `build`.
5. Скопируйте директории `resources` и `third_party/echarts` рядом с исполняемым файлом, чтобы график мог загрузить `echarts.min.js`:
   ```
   cp -r resources third_party/echarts build/
   ```

## График

График свечей отображается через ECharts. Доступны стандартные возможности масштабирования и прокрутки; пользовательские инструменты рисования пока не поддерживаются.
Окно ECharts закрывается программно при завершении приложения, поэтому поток
с веб‑интерфейсом завершает работу без участия пользователя.

## Streaming

Файл `config.json` содержит флаг `enable_streaming`. При значении `true` приложение подключается к `wss://stream.binance.com:9443/ws/{symbol}@kline_{interval}` и обновляет свечи в реальном времени. Если флаг выключен или потоковый канал отключается, используется обычный HTTP-поллинг.

## Лицензии

- Проект использует [Apache ECharts](https://echarts.apache.org/), распространяемый по лицензии [Apache 2.0](third_party/echarts/LICENSE).

