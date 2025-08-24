@echo off
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%..\build
set VCPKG_TOOLCHAIN_FILE=%PROJECT_DIR%..\vcpkg\scripts\buildsystems\vcpkg.cmake
set VCPKG_OVERLAY_PORTS=%PROJECT_DIR%..\ports

echo Creating build directory if it doesn't exist...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Changing to build directory...
cd "%BUILD_DIR%"

echo Running CMake configuration...
cmake .. -DBUILD_TRADING_TERMINAL=ON -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN_FILE%" -DVCPKG_OVERLAY_PORTS="%VCPKG_OVERLAY_PORTS%"
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

echo Preparing chart resources...
for %%c in (Debug Release) do (
    if exist "%%c" (
        echo Preparing resources in %%c...
        call "%PROJECT_DIR%prepare_chart_resources.bat" "%BUILD_DIR%\%%c" || goto :eof
    )
)

echo Running the application...
pushd "%BUILD_DIR%"
if exist "Debug\TradingTerminal.exe" (
    start "" cmd /K ".\Debug\TradingTerminal.exe"
) else if exist "Release\TradingTerminal.exe" (
    start "" cmd /K ".\Release\TradingTerminal.exe"
) else if exist "TradingTerminal.exe" (
    start "" cmd /K "TradingTerminal.exe"
) else (
    echo Executable not found in Debug or Release folders.
)
popd

echo Done.
pause
