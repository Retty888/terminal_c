@echo off
setlocal

if "%~1"=="" (
    echo Usage: %~nx0 ^<build_path^>
    exit /b 1
)

set "BUILD_DIR=%~1"

robocopy "resources" "%BUILD_DIR%\resources" "chart.html" /NFL /NDL /NJH /NJS /NC /NS /NP >nul
if errorlevel 8 (
    echo Failed to copy chart.html
    exit /b 1
)

robocopy "third_party\echarts" "%BUILD_DIR%\third_party\echarts" "echarts.min.js" /NFL /NDL /NJH /NJS /NC /NS /NP >nul
if errorlevel 8 (
    echo Failed to copy echarts.min.js
    exit /b 1
)

echo Chart resources prepared.
exit /b 0
