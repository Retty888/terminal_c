# Project Log: C++ Trading Terminal

## Session Start: 2025-08-07 (Approximate)
_Updated to reflect the current ECharts-based implementation._

### Project Goal
Create a high-performance C++ desktop application for Windows that serves as a terminal for analysis, backtesting, and algorithmic trading experiments.

### Chosen Technology Stack
- **Language:** C++20
- **Build system:** CMake
- **Dependency management:** vcpkg
- **GUI:** Dear ImGui
- **Charting:** Apache ECharts embedded via webview
- **Networking:** CPR (C++ Requests)
- **JSON handling:** nlohmann/json

### Current Progress
1. Established a modular project structure (`src`, `include`, `resources`, `tests`).
2. Configured `CMakeLists.txt` for C++20, dependency discovery, and unit tests.
3. Created `main.cpp` that launches an ImGui interface with an ECharts-powered candlestick chart.
4. Implemented HTTP fetching of candlesticks and optional WebSocket streaming from Binance.
5. Added services for journaling, signal bots, and serialization utilities for ECharts.
6. Added comprehensive unit tests covering core logic and chart serialization.

### Current Issues
No outstanding issues recorded in this log.

### Next Steps
1. Configure the project with CMake using the vcpkg toolchain.
2. Build and run the unit tests via `ctest`.
