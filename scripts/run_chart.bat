@echo off
setlocal

if "%~1"=="" (
    echo Usage: %~nx0 ^<build_path^>
    exit /b 1
)

set "BUILD_DIR=%~1"

call "%~dp0prepare_chart_resources.bat" "%BUILD_DIR%" || exit /b 1

python -c "import requests, pandas, mplfinance" >nul 2>&1
if errorlevel 1 (
    echo Required Python packages not found. Please install requests, pandas, and mplfinance.
    exit /b 1
)

python "%~dp0sample_chart.py"
