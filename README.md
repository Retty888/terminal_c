# terminal-c

Standalone C++ trading terminal using ImGui and other dependencies. The project relies on packages provided by `vcpkg` and `find_package` in CMake.

## Состав

- `main.cpp` с ImGui интерфейсом
- Поддержка нескольких торговых пар
- Загрузка свечей с Binance API
- Потоковое обновление свечей через WebSocket Binance
- Импортированные библиотеки:
  - ImGui
  - ImPlot
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
2. Выполните конфигурацию проекта:
   ```
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```
3. Соберите проект:
   ```
   cmake --build build
   ```
4. Готовый исполняемый файл `TradingTerminal` (или `TradingTerminal.exe` на Windows) появится в каталоге `build`.

## Инструменты графика

- **Add Line / Add Rect** – нажмите кнопку инструмента, затем кликните первую точку на графике и второй клик завершит фигуру. Правый клик удаляет ближайшую фигуру.
- **Measure** – измерение между двумя точками: выберите инструмент, кликните старт и затем конечную точку.
- **SMA7** – на график нанесена простая скользящая средняя с периодом 7 для дополнительного анализа.
- **Колёсико мыши** – масштабирует оси. Удерживайте `Ctrl`/`Shift` или наведите курсор на ось Y, чтобы изменять только вертикальную шкалу. Перетаскивание левой кнопкой сдвигает график по времени, а за область оси Y – по цене. При вертикальном скролле отображается стрелка-индикатор.

## Streaming

Файл `config.json` содержит флаг `enable_streaming`. При значении `true` приложение подключается к `wss://stream.binance.com:9443/ws/{symbol}@kline_{interval}` и обновляет свечи в реальном времени. Если флаг выключен или потоковый канал отключается, используется обычный HTTP-поллинг.

