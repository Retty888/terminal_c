cmake_minimum_required(VERSION 3.20)
project(TradingTerminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX)

option(BUILD_TRADING_TERMINAL "Build the TradingTerminal application" ON)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)

if(BUILD_TRADING_TERMINAL)
    # Find all necessary packages using vcpkg
    find_package(imgui CONFIG REQUIRED)
    find_package(implot CONFIG REQUIRED)
    find_package(cpr CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)

    add_executable(TradingTerminal
        main.cpp
        src/app.cpp
        src/config.cpp
        src/candle.cpp
        src/signal.cpp
        src/plot/candlestick.cpp
        src/core/candle_manager.cpp
        src/core/data_fetcher.cpp
        src/core/backtester.cpp
        src/logger.cpp
        src/journal.cpp
        src/services/data_service.cpp
        src/services/journal_service.cpp
        src/services/signal_bot.cpp
        src/ui/control_panel.cpp
        src/ui/signals_window.cpp
        src/ui/analytics_window.cpp
        src/ui/journal_window.cpp
        src/ui/chart_window.cpp
        src/ui/tradingview_style.cpp
    )

    target_sources(TradingTerminal PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
        ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
    )

    # Link libraries to the executable
    target_link_libraries(TradingTerminal PRIVATE
        imgui::imgui
        implot::implot
        cpr::cpr
        nlohmann_json::nlohmann_json
        glfw
        OpenGL::GL
    )

    # Include directories if needed (vcpkg handles most of this automatically)
    target_include_directories(TradingTerminal PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    )
endif()

enable_testing()

add_executable(test_backtester
    tests/test_backtester.cpp
    src/core/backtester.cpp
    src/candle.cpp
)

target_include_directories(test_backtester PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(test_backtester PRIVATE
    GTest::gtest_main
)

add_test(NAME test_backtester COMMAND test_backtester)

add_executable(test_candle_manager
    tests/test_candle_manager.cpp
    src/core/candle_manager.cpp
    src/candle.cpp
    src/logger.cpp
)

target_include_directories(test_candle_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(test_candle_manager PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

add_test(NAME test_candle_manager COMMAND test_candle_manager)

add_executable(test_signal
    tests/test_signal.cpp
    src/signal.cpp
    src/candle.cpp
    src/services/signal_bot.cpp
    src/config.cpp
)

target_include_directories(test_signal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(test_signal PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

add_test(NAME test_signal COMMAND test_signal)

add_executable(test_data_fetcher
    tests/test_data_fetcher.cpp
    src/core/data_fetcher.cpp
    src/candle.cpp
    src/logger.cpp
)

target_include_directories(test_data_fetcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(test_data_fetcher PRIVATE
    GTest::gtest_main
    cpr::cpr
    nlohmann_json::nlohmann_json
)

add_test(NAME test_data_fetcher COMMAND test_data_fetcher)

add_custom_target(ctest
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_backtester test_candle_manager test_signal test_data_fetcher
)

