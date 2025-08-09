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
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(TradingTerminal
    main.cpp
    src/config.cpp
    src/candle.cpp
    src/signal.cpp
    src/core/candle_manager.cpp
    src/core/data_fetcher.cpp
    src/core/backtester.cpp
)

# Attempt to use backend targets if available; otherwise add source files directly
set(IMGUI_BACKEND_LIBS "")
if(TARGET imgui::imgui_impl_glfw AND TARGET imgui::imgui_impl_opengl3)
    list(APPEND IMGUI_BACKEND_LIBS imgui::imgui_impl_glfw imgui::imgui_impl_opengl3)
else()
    get_target_property(IMGUI_INCLUDE_DIRS imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
    list(GET IMGUI_INCLUDE_DIRS 0 IMGUI_MAIN_INCLUDE_DIR)
    set(IMGUI_BACKEND_DIR "${IMGUI_MAIN_INCLUDE_DIR}/backends")
    target_sources(TradingTerminal PRIVATE
        ${IMGUI_BACKEND_DIR}/imgui_impl_glfw.cpp
        ${IMGUI_BACKEND_DIR}/imgui_impl_opengl3.cpp
    )
    target_include_directories(TradingTerminal PRIVATE ${IMGUI_BACKEND_DIR})
endif()

# Link libraries to the executable
target_link_libraries(TradingTerminal PRIVATE
    imgui::imgui
    ${IMGUI_BACKEND_LIBS}
    implot::implot
    cpr::cpr
    nlohmann_json::nlohmann_json
    arrow_shared
    glfw
    OpenGL::GL
)

# Include directories if needed (vcpkg handles most of this automatically)
target_include_directories(TradingTerminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
