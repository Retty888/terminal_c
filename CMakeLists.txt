cmake_minimum_required(VERSION 3.20)
project(TradingTerminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX)
add_compile_definitions(GTEST_HAS_STD_FORMAT=0)
add_compile_definitions(GTEST_HAS_STD_WSTRING=0)

option(BUILD_TRADING_TERMINAL "Build the TradingTerminal application" ON)

find_package(GTest CONFIG REQUIRED)
find_package(imgui CONFIG QUIET)
if(NOT imgui_FOUND)
  message(WARNING "imgui not found, using bundled sources")
  add_library(imgui
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
  )
  target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
  add_library(imgui::imgui ALIAS imgui)
endif()
find_package(cpr CONFIG QUIET)
if(NOT cpr_FOUND)
  message(WARNING "cpr not found, skipping targets that require it")
endif()

if(BUILD_TRADING_TERMINAL AND cpr_FOUND)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(webview CONFIG REQUIRED)

  add_executable(TradingTerminal
    main.cpp
    src/app.cpp
    src/config_manager.cpp
    src/config_schema.cpp
    src/config_path.cpp
      src/candle.cpp
      src/signal.cpp
      src/core/candle_manager.cpp
    src/core/data_fetcher.cpp
    src/core/interval_utils.cpp
    src/core/candle_utils.cpp
    src/core/data_dir.cpp
    src/core/exchange_utils.cpp
    src/core/net/token_bucket_rate_limiter.cpp
    src/core/net/cpr_http_client.cpp
    src/core/backtester.cpp
    src/core/kline_stream.cpp
    src/core/iwebsocket.cpp
    src/core/logger.cpp
    src/core/path_utils.cpp
    src/core/glfw_context.cpp
    src/journal.cpp
    src/services/data_service.cpp
    src/services/journal_service.cpp
    src/services/signal_bot.cpp
    src/ui/control_panel.cpp
    src/ui/signals_window.cpp
      src/ui/analytics_window.cpp
      src/ui/journal_window.cpp
      src/ui/ui_manager.cpp
  )

  target_sources(TradingTerminal PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
  )

  target_link_libraries(TradingTerminal PRIVATE
      imgui::imgui
      cpr::cpr
    glfw
    OpenGL::GL
    webview::webview
  )

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
target_include_directories(test_backtester PRIVATE src include)
target_link_libraries(test_backtester PRIVATE GTest::gtest_main)
add_test(NAME test_backtester COMMAND test_backtester)

add_executable(test_candle_manager
  tests/test_candle_manager.cpp
  src/core/candle_manager.cpp
  src/core/data_dir.cpp
  src/core/interval_utils.cpp
  src/candle.cpp
  src/core/logger.cpp
  src/config_path.cpp
  src/core/path_utils.cpp
)
target_include_directories(test_candle_manager PRIVATE src include)
target_link_libraries(test_candle_manager PRIVATE GTest::gtest_main)
add_test(NAME test_candle_manager COMMAND test_candle_manager)

add_executable(test_signal
  tests/test_signal.cpp
  src/signal.cpp
  src/candle.cpp
  src/services/signal_bot.cpp
  src/config_manager.cpp
  src/config_schema.cpp
  src/core/logger.cpp
  src/config_path.cpp
  src/core/path_utils.cpp
)
target_include_directories(test_signal PRIVATE src include)
target_link_libraries(test_signal PRIVATE GTest::gtest_main)
add_test(NAME test_signal COMMAND test_signal)

add_executable(test_data_fetcher
  tests/test_data_fetcher.cpp
  src/core/data_fetcher.cpp
  src/core/interval_utils.cpp
  src/core/candle_utils.cpp
  src/core/exchange_utils.cpp
  src/candle.cpp
  src/core/logger.cpp
)
target_include_directories(test_data_fetcher PRIVATE src include)
target_link_libraries(test_data_fetcher PRIVATE GTest::gtest_main)
add_test(NAME test_data_fetcher COMMAND test_data_fetcher)

add_executable(test_interval_utils
  tests/test_interval_utils.cpp
  src/core/interval_utils.cpp
)
target_include_directories(test_interval_utils PRIVATE src include)
target_link_libraries(test_interval_utils PRIVATE GTest::gtest_main)
add_test(NAME test_interval_utils COMMAND test_interval_utils)

add_executable(test_scheduler
  tests/test_scheduler.cpp
  src/core/interval_utils.cpp
  src/candle.cpp
)
target_include_directories(test_scheduler PRIVATE src include)
target_link_libraries(test_scheduler PRIVATE GTest::gtest_main)
add_test(NAME test_scheduler COMMAND test_scheduler)

add_executable(test_kline_stream
  tests/test_kline_stream.cpp
  src/core/kline_stream.cpp
  src/core/iwebsocket.cpp
  src/core/candle_manager.cpp
  src/core/data_dir.cpp
  src/core/interval_utils.cpp
  src/candle.cpp
  src/core/logger.cpp
  src/config_path.cpp
  src/core/path_utils.cpp
)
target_include_directories(test_kline_stream PRIVATE src include)
target_link_libraries(test_kline_stream PRIVATE GTest::gtest_main)
add_test(NAME test_kline_stream COMMAND test_kline_stream)

add_executable(test_journal
  tests/test_journal.cpp
  src/journal.cpp
  src/core/logger.cpp
)
target_include_directories(test_journal PRIVATE src include)
target_link_libraries(test_journal PRIVATE GTest::gtest_main)
add_test(NAME test_journal COMMAND test_journal)

  add_executable(test_logger
    tests/test_logger.cpp
    src/core/logger.cpp
  )
  target_include_directories(test_logger PRIVATE src include)
  target_link_libraries(test_logger PRIVATE GTest::gtest_main)
  add_test(NAME test_logger COMMAND test_logger)


  add_custom_target(ctest COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
      DEPENDS test_backtester test_candle_manager test_signal test_scheduler test_kline_stream test_data_fetcher test_interval_utils test_logger test_journal
    )

