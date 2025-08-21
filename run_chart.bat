@echo off
setlocal

if "%~1"=="" (
    echo Usage: %~nx0 ^<build_path^>
    exit /b 1
)

set "BUILD_DIR=%~1"

call "%~dp0prepare_chart_resources.bat" "%BUILD_DIR%" || exit /b 1

python "%~dp0sample_chart.py"
