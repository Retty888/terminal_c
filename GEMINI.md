# Project Log: C++ Trading Terminal

## Session Start: 2025-08-07 (Approximate)
_Updated after removing the previous charting implementation._

### Main Goal
Build a high-performance C++ desktop terminal centered on trade journaling, strategy testing and backtesting, analytics, and signal generation, with room for potential exchange integration.

### Upcoming Capabilities
- Selectable trading pairs
- Candle data storage and loading
- Chart display
- Minimal user interface

### Future Plans
- Detailed journaling features
- Strategy management tools
- Automated trading signals
- Exchange connectivity

### Chosen Technology Stack
- **Language:** C++20
- **Build system:** CMake
- **Dependency management:** vcpkg
- **GUI:** Dear ImGui
- **Charting:** placeholder panel (future TradingView integration)
- **Networking:** CPR (C++ Requests)
- **JSON handling:** nlohmann/json

### Current Progress
1. Established a modular project structure (`src`, `include`, `resources`, `tests`).
2. Configured `CMakeLists.txt` for C++20, dependency discovery, and unit tests.
3. Created `main.cpp` that launches an ImGui interface with a candlestick chart placeholder.
4. Implemented HTTP fetching of candlesticks and optional WebSocket streaming from Binance.
5. Added services for journaling and signal bots.
6. Added comprehensive unit tests covering core logic.

### Current Issues
No outstanding issues recorded in this log.

### Next Steps
1. Configure the project with CMake using the vcpkg toolchain.
2. Build and run the unit tests via `ctest`.
