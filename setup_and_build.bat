@echo off
set SCRIPT_DIR=%~dp0
set VCPKG_PATH=%SCRIPT_DIR%vcpkg

REM Determine vcpkg path
if exist "%VCPKG_PATH%" (
    echo Found vcpkg in script directory.
) else (
    echo vcpkg not found in script directory.
    if defined VCPKG_ROOT if exist "%VCPKG_ROOT%" (
        set VCPKG_PATH=%VCPKG_ROOT%
    ) else (
        set /p VCPKG_PATH=Please enter the path to vcpkg:
    )
    if not exist "%VCPKG_PATH%" (
        echo vcpkg not found. Cloning...
        git clone https://github.com/microsoft/vcpkg "%SCRIPT_DIR%vcpkg"
        set VCPKG_PATH=%SCRIPT_DIR%vcpkg
        call "%VCPKG_PATH%\bootstrap-vcpkg.bat"
    )
)

set TOOLCHAIN_FILE=%VCPKG_PATH%\scripts\buildsystems\vcpkg.cmake

echo Installing required packages...
"%VCPKG_PATH%\vcpkg.exe" install imgui implot cpr nlohmann-json arrow glfw3 opengl

set BUILD_DIR=%SCRIPT_DIR%build
if exist "%BUILD_DIR%" (
    echo Removing existing build directory...
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo Running CMake configuration...
cmake .. -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%"
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b %errorlevel%
)

echo Building the project...
cmake --build .
if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b %errorlevel%
)

echo Launching the application...
if exist "Debug\TradingTerminal.exe" (
    start "" "Debug\TradingTerminal.exe"
) else if exist "Release\TradingTerminal.exe" (
    start "" "Release\TradingTerminal.exe"
) else if exist "TradingTerminal.exe" (
    start "" "TradingTerminal.exe"
) else (
    echo TradingTerminal.exe not found.
)

