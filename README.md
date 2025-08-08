# terminal_c

Standalone C++ trading terminal using ImGui and other dependencies without requiring `vcpkg` or additional library installations.

## Состав

- `main.cpp` с ImGui интерфейсом
- Поддержка нескольких торговых пар
- Загрузка свечей с Binance API
- Импортированные библиотеки:
  - ImGui
  - ImPlot
  - CPR (встроен)
  - JSON (встроен)
- `CMakeLists.txt` без `find_package()` и без `vcpkg`

## Инструкция

1. Открой `CMakeLists.txt` в Visual Studio
2. Выбери сборку `x64-Debug`
3. Нажми `Ctrl+Shift+B` для сборки
4. Запусти `TradingTerminal.exe`

📌 Не требует установки библиотек отдельно или подключения к интернету

