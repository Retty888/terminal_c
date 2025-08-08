# Project Log: C++ Trading Terminal

## Session Start: 2025-08-07 (Approximate)

### Project Goal:
Создание высокопроизводительного десктопного приложения на C++ для Windows, которое будет служить терминалом для анализа, бэктестинга и тестирования алгоритмической торговли.

### Chosen Technology Stack:
*   **Язык:** C++20
*   **Система сборки:** CMake
*   **Управление зависимостями:** vcpkg
*   **GUI:** Dear ImGui
*   **Графики:** ImPlot
*   **Сетевые запросы (API):** CPR (C++ Requests)
*   **Работа с JSON:** nlohmann/json
*   **Работа с Parquet/данными:** Apache Arrow C++

### Current Progress:
1.  Создана базовая структура каталогов проекта (`trading_terminal_cpp/src`, `core`, `gui`, `indicators`, `strategies`, `utils`).
2.  Создан базовый `CMakeLists.txt` с объявлением проекта, стандартом C++20 и инструкциями для поиска основных зависимостей.
3.  Создан минимальный `src/main.cpp` для проверки сборки.
4.  Установлены основные зависимости через `vcpkg` (imgui, implot, cpr, nlohmann-json, apache-arrow).
5.  Возникла проблема с поиском `nlohmann_json` CMake'ом, которая была решена переустановкой пакета через `vcpkg`.
6.  Возникла проблема с поиском `Arrow` CMake'ом, которая была решена установкой пакета `arrow` через `vcpkg`.
7.  `src/main.cpp` был обновлен для инициализации ImGui с использованием DirectX 11.
8.  `CMakeLists.txt` был обновлен для линковки с библиотеками DirectX (`d3d11.lib`, `dxgi.lib`).

### Current Issues:
*   **ImGui Feature Support:** При сборке `main.cpp` возникли ошибки, связанные с отсутствием поддержки `ImGuiConfigFlags_DockingEnable`, `ImGuiConfigFlags_ViewportsEnable` и `ImGui::UpdatePlatformWindows()` и `ImGui::RenderPlatformWindowsDefault()`. Это указывало на то, что текущая версия ImGui, предоставляемая `vcpkg`, не включала эти опциональные функции по умолчанию, или они назывались по-другому. Обновление `vcpkg` не решило проблему.

### Changes Made in Current Session:
1.  Удалены неработающие флаги `ImGuiConfigFlags_DockingEnable` и `ImGuiConfigFlags_ViewportsEnable` из `src/main.cpp`.
2.  Удалены вызовы `ImGui::UpdatePlatformWindows()` и `ImGui::RenderPlatformWindowsDefault()` из `src/main.cpp`.

### Next Steps (for User):
Теперь, когда изменения внесены, вам нужно будет снова:
1.  **Сконфигурировать проект CMake** в **Developer Command Prompt for VS**:
    ```bash
    cd C:\Users\User\trading_terminal_cpp\build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\Users\User\vcpkg\scripts\buildsystems\vcpkg.cmake
    ```
2.  **Собрать проект** в той же командной строве:
    ```bash
    cmake --build .
    ```
3.  **Запустить исполняемый файл** `TradingTerminal.exe` из `build\Debug` (или `build\Release`).

