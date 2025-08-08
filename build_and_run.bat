@echo off
set PROJECT_DIR=C:\Users\User\trading_terminal_cpp
set BUILD_DIR=%PROJECT_DIR%\build
set VCPKG_TOOLCHAIN_FILE=C:\Users\User\vcpkg\scripts\buildsystems\vcpkg.cmake

echo Creating build directory if it doesn't exist...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Changing to build directory...
cd "%BUILD_DIR%"

echo Running CMake configuration...
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN_FILE%"
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    goto :eof
)

echo Building the project...
cmake --build .
if %errorlevel% neq 0 (
    echo Project build failed!
    goto :eof
)

echo Running the application...
rem Try to run Debug version first, then Release if Debug not found
if exist "Debug\TradingTerminal.exe" (
    start "" "Debug\TradingTerminal.exe" > "%PROJECT_DIR%\error_log.txt" 2>&1
) else if exist "Release\TradingTerminal.exe" (
    start "" "Release\TradingTerminal.exe" > "%PROJECT_DIR%\error_log.txt" 2>&1
) else (
    echo Executable not found in Debug or Release folders.
)

echo Check error_log.txt for details.

echo Done.
pause
