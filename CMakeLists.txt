cmake_minimum_required(VERSION 3.20)
project(TradingTerminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find all necessary packages using vcpkg
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Arrow CONFIG REQUIRED)

add_executable(TradingTerminal
    src/main.cpp
    src/config.cpp
    src/candle.cpp
    src/signal.cpp
    src/core/candle_manager.cpp
    src/core/data_fetcher.cpp
)

# Link libraries to the executable
target_link_libraries(TradingTerminal PRIVATE
    imgui::imgui
    implot::implot
    cpr::cpr
    nlohmann_json::nlohmann_json
    arrow_shared
    d3d11.lib
    dxgi.lib
)

# Include directories if needed (vcpkg handles most of this automatically)
target_include_directories(TradingTerminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
