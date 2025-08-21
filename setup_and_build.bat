@echo off

winget --version >nul 2>&1
if %errorlevel% neq 0 (
    echo winget is not installed.
    echo Please install winget or download the Microsoft Edge WebView2 Runtime installer directly from:
    echo https://go.microsoft.com/fwlink/p/?LinkId=2124703
    exit /b 1
)

set SCRIPT_DIR=%~dp0
set VCPKG_PATH=%SCRIPT_DIR%vcpkg

REM Check for Microsoft Edge WebView2 Runtime across possible registry locations
set EDGE_FOUND=0
reg query "HKLM\Software\Microsoft\EdgeUpdate\Clients" /s | findstr /I "WebView2" >nul 2>&1 && set EDGE_FOUND=1
if %EDGE_FOUND%==0 reg query "HKLM\Software\WOW6432Node\Microsoft\EdgeUpdate\Clients" /s | findstr /I "WebView2" >nul 2>&1 && set EDGE_FOUND=1
if %EDGE_FOUND%==0 reg query "HKCU\Software\Microsoft\EdgeUpdate\Clients" /s | findstr /I "WebView2" >nul 2>&1 && set EDGE_FOUND=1

if %EDGE_FOUND%==0 (
    echo Microsoft Edge WebView2 Runtime not found. Attempting installation...
    winget install -e --id Microsoft.EdgeWebView2Runtime
    if %errorlevel% neq 0 (
        echo Failed to install Microsoft Edge WebView2 Runtime via winget.
        echo Please ensure winget is available and try again.
        exit /b 1
    )
    REM Recheck all registry locations after installation
    set EDGE_FOUND=0
    reg query "HKLM\Software\Microsoft\EdgeUpdate\Clients" /s | findstr /I "WebView2" >nul 2>&1 && set EDGE_FOUND=1
    if %EDGE_FOUND%==0 reg query "HKLM\Software\WOW6432Node\Microsoft\EdgeUpdate\Clients" /s | findstr /I "WebView2" >nul 2>&1 && set EDGE_FOUND=1
    if %EDGE_FOUND%==0 reg query "HKCU\Software\Microsoft\EdgeUpdate\Clients" /s | findstr /I "WebView2" >nul 2>&1 && set EDGE_FOUND=1
    if %EDGE_FOUND%==0 (
        echo WebView2 installation failed!
        pause
        exit /b 1
    )
)

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
set OVERLAY_PORTS=%SCRIPT_DIR%ports

REM Install dependencies using manifest if available
if exist "%SCRIPT_DIR%vcpkg.json" (
    echo Installing packages from manifest...
    "%VCPKG_PATH%\vcpkg.exe" install --recurse --overlay-ports="%OVERLAY_PORTS%"
) else (
    echo Installing required packages...
    "%VCPKG_PATH%\vcpkg.exe" install imgui[core,docking-experimental,glfw-binding,opengl3-binding] cpr nlohmann-json arrow glfw3 opengl --recurse --overlay-ports="%OVERLAY_PORTS%"
)
if %errorlevel% neq 0 (
    echo Dependency installation failed!
    pause
    exit /b %errorlevel%
)
"%VCPKG_PATH%\vcpkg.exe" list

set BUILD_DIR=%SCRIPT_DIR%build
if exist "%BUILD_DIR%" (
    echo Removing existing build directory...
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo Running CMake configuration...
cmake .. -DBUILD_TRADING_TERMINAL=ON -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" -DVCPKG_OVERLAY_PORTS="%OVERLAY_PORTS%"
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

REM Ensure Python is available and install required packages
where python >nul 2>nul
if %errorlevel% neq 0 (
    echo Python not found. Please install Python and ensure it is in your PATH.
echo Preparing chart resources...
if not exist "%BUILD_DIR%\Debug" (
    mkdir "%BUILD_DIR%\Debug"
)
call "%SCRIPT_DIR%prepare_chart_resources.bat" "%BUILD_DIR%\Debug"
if %errorlevel% neq 0 (
    echo Failed to prepare chart resources for Debug!
    pause
    exit /b %errorlevel%
)

python -m pip install --upgrade pip
if %errorlevel% neq 0 (
    echo Failed to upgrade pip.
    pause
    exit /b %errorlevel%
)
python -m pip install requests pandas mplfinance
if %errorlevel% neq 0 (
    echo Failed to install Python packages.
if not exist "%BUILD_DIR%\Release" (
    mkdir "%BUILD_DIR%\Release"
)
call "%SCRIPT_DIR%prepare_chart_resources.bat" "%BUILD_DIR%\Release"
if %errorlevel% neq 0 (
    echo Failed to prepare chart resources for Release!
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

