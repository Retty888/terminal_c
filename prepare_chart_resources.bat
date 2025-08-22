@echo off
setlocal

if "%~1"=="" (
    echo Usage: %~nx0 ^<build_path^>
    exit /b 1
)

set "BUILD_DIR=%~1"

set "SRC_DIR=%~dp0resources"
set "LC_FILE=lightweight-charts.standalone.production.js"

if not exist "%SRC_DIR%\%LC_FILE%" (
    echo TradingView script not found, downloading...
    powershell -Command "try {Invoke-WebRequest -Uri https://unpkg.com/lightweight-charts/dist/%LC_FILE% -OutFile '%SRC_DIR%\%LC_FILE%'} catch {exit 1}" || (
        echo Failed to download %LC_FILE%
        exit /b 1
    )
)

robocopy "%SRC_DIR%" "%BUILD_DIR%\resources" "chart.html" "%LC_FILE%" /NFL /NDL /NJH /NJS /NC /NS /NP >nul
if errorlevel 8 (
    echo Failed to copy chart resources
    exit /b 1
)

echo Chart resources prepared.
exit /b 0
