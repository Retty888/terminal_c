cmake_minimum_required(VERSION 3.20)
project(TradingTerminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX)
add_compile_definitions(GTEST_HAS_STD_FORMAT=0)
add_compile_definitions(GTEST_HAS_STD_WSTRING=0)
add_compile_definitions(IMGUI_HAS_DOCKING=1)

option(BUILD_TRADING_TERMINAL "Build the TradingTerminal application" ON)

set(USE_OPENGL_BACKEND ON)

# WebView/HTML chart was removed; native ImPlot chart is used exclusively.

find_package(GTest CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
set(IMGUI_TARGET imgui::imgui)
set(IMPLOT_TARGET implot::implot)
if(BUILD_TRADING_TERMINAL)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(cpr CONFIG REQUIRED)
  find_package(ixwebsocket CONFIG QUIET)

  if(NOT TARGET glfw::glfw)
    add_library(glfw::glfw ALIAS glfw)
  endif()

  add_executable(TradingTerminal
    main.cpp
    src/app.cpp
    src/config_manager.cpp
    src/config_schema.cpp
    src/config_path.cpp
    src/candle.cpp
    src/journal.cpp
    src/core/candle_manager.cpp
    src/core/net/binance_data_provider.cpp
    src/core/net/hyperliquid_data_provider.cpp
    src/core/interval_utils.cpp
    src/core/candle_utils.cpp
    src/core/data_dir.cpp
    src/core/exchange_utils.cpp
    src/core/net/token_bucket_rate_limiter.cpp
    src/core/net/cpr_http_client.cpp
    src/core/kline_stream.cpp
    src/core/iwebsocket.cpp
    src/core/logger.cpp
    src/core/path_utils.cpp
    src/core/glfw_context.cpp
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${USE_OPENGL_BACKEND}>>>:src/core/dx11_context.cpp>
    src/core/backtester.cpp
    src/services/data_service.cpp
    src/services/journal_service.cpp
    src/services/signal_bot.cpp
    src/ui/control_panel.cpp
    src/ui/analytics_window.cpp
    src/ui/journal_window.cpp
    src/ui/backtest_window.cpp
    src/ui/ui_manager.cpp
    src/signal.cpp
  )

  if (WIN32 AND NOT USE_OPENGL_BACKEND)
    target_compile_definitions(TradingTerminal PRIVATE UI_BACKEND_DX11)
  endif()

  target_link_libraries(TradingTerminal PRIVATE
      ${IMGUI_TARGET}
      ${IMPLOT_TARGET}
    glfw::glfw
    $<$<OR:$<NOT:$<PLATFORM_ID:Windows>>,$<BOOL:${USE_OPENGL_BACKEND}>>:OpenGL::GL>
    cpr::cpr
    $<$<TARGET_EXISTS:ixwebsocket::ixwebsocket>:ixwebsocket::ixwebsocket>
    $<$<AND:$<PLATFORM_ID:Windows>,$<TARGET_EXISTS:ixwebsocket::ixwebsocket>>:Bcrypt>
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${USE_OPENGL_BACKEND}>>>:d3d11>
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${USE_OPENGL_BACKEND}>>>:dxgi>
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${USE_OPENGL_BACKEND}>>>:d3dcompiler>
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${USE_OPENGL_BACKEND}>>>:Windowscodecs>
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${USE_OPENGL_BACKEND}>>>:Shlwapi>
  )

  target_include_directories(TradingTerminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $<TARGET_PROPERTY:imgui::imgui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:glfw::glfw,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:cpr::cpr,INTERFACE_INCLUDE_DIRECTORIES>
    $<$<TARGET_EXISTS:ixwebsocket::ixwebsocket>:$<TARGET_PROPERTY:ixwebsocket::ixwebsocket,INTERFACE_INCLUDE_DIRECTORIES>>
  )

  # WebView embedding removed; native chart path only.

  # WebView HTML chart disabled; no resource copying in native build.

endif()

enable_testing()

# Only add tests if the tests directory exists (some distributions omit it)
if (EXISTS "${CMAKE_SOURCE_DIR}/tests")
  add_executable(test_candle_manager
    tests/test_candle_manager.cpp
    src/core/candle_manager.cpp
    src/core/candle_utils.cpp
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

  add_executable(test_data_fetcher
    tests/test_data_fetcher.cpp
    src/core/interval_utils.cpp
    src/core/candle_utils.cpp
    src/core/exchange_utils.cpp
    src/candle.cpp
    src/core/logger.cpp
    src/services/data_service.cpp
    src/core/net/binance_data_provider.cpp
    src/core/net/hyperliquid_data_provider.cpp
    src/config_manager.cpp
    src/config_schema.cpp
    src/config_path.cpp
    src/core/path_utils.cpp
    src/core/candle_manager.cpp
    src/core/net/cpr_http_client.cpp
    src/core/net/token_bucket_rate_limiter.cpp
    src/core/data_dir.cpp
  )
  target_include_directories(test_data_fetcher PRIVATE src include)
  target_link_libraries(test_data_fetcher PRIVATE GTest::gtest_main cpr::cpr)
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
    src/core/candle_utils.cpp
    src/core/data_dir.cpp
    src/core/interval_utils.cpp
    src/candle.cpp
    src/core/logger.cpp
    src/config_path.cpp
    src/core/path_utils.cpp
    src/core/exchange_utils.cpp
  )
  target_include_directories(test_kline_stream PRIVATE src include)
  target_link_libraries(test_kline_stream PRIVATE 
    GTest::gtest_main 
    $<$<TARGET_EXISTS:ixwebsocket::ixwebsocket>:ixwebsocket::ixwebsocket>
    $<$<AND:$<PLATFORM_ID:Windows>,$<TARGET_EXISTS:ixwebsocket::ixwebsocket>>:Bcrypt>
  )
  add_test(NAME test_kline_stream COMMAND test_kline_stream)

  add_executable(test_logger
    tests/test_logger.cpp
    src/core/logger.cpp
  )
  target_include_directories(test_logger PRIVATE src include)
  target_link_libraries(test_logger PRIVATE GTest::gtest_main)
  add_test(NAME test_logger COMMAND test_logger)

  add_executable(test_signal
    tests/test_signal.cpp
    src/signal.cpp
    src/services/signal_bot.cpp
    src/config_manager.cpp
    src/config_schema.cpp
    src/config_path.cpp
    src/candle.cpp
    src/core/path_utils.cpp
    src/core/logger.cpp
  )
  target_include_directories(test_signal PRIVATE src include)
  target_link_libraries(test_signal PRIVATE GTest::gtest_main)
  add_test(NAME test_signal COMMAND test_signal)

  add_executable(test_ui_manager
      tests/test_ui_manager.cpp
      src/ui/ui_manager.cpp
      src/core/data_dir.cpp
      src/core/interval_utils.cpp
      src/core/path_utils.cpp
      src/config_path.cpp
      src/core/logger.cpp
    )
    target_include_directories(test_ui_manager PRIVATE tests/stubs src include)
    target_link_libraries(test_ui_manager PRIVATE GTest::gtest_main imgui::imgui implot::implot glfw::glfw OpenGL::GL)
  add_test(NAME test_ui_manager COMMAND test_ui_manager)

  add_custom_target(ctest COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
      DEPENDS test_candle_manager test_scheduler test_kline_stream test_data_fetcher test_interval_utils test_logger test_signal test_ui_manager
    )
endif()

