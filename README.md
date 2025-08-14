# terminal_c

Standalone C++ trading terminal using ImGui and other dependencies. The project relies on packages provided by `vcpkg` and `find_package` in CMake.

## Состав

- `main.cpp` с ImGui интерфейсом
- Поддержка нескольких торговых пар
- Загрузка свечей с Binance API
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

## Каталог данных свечей

Приложение определяет, где хранить свечные данные, в следующем порядке:

1. Если установлена переменная окружения `CANDLE_DATA_DIR`, используется она.
2. Иначе рядом с исполнимым файлом ищется `config.json`; значение `data_dir` в нём интерпретируется как абсолютный путь.
3. Если ничего не найдено, данные сохраняются в `~/trading_terminal/candle_data`.

Файл `config.json` в репозитории содержит пример абсолютного пути.

