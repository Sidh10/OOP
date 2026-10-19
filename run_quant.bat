@echo off
rem One-click launcher for the CLI (D-032). Runs from this folder so data\, reports\ and logs\ resolve.
cd /d "%~dp0"
if not exist "build\quant_engine.exe" (
    echo build\quant_engine.exe was not found. Build the project first: see README.md, "Build and run".
    pause
    exit /b 1
)
build\quant_engine.exe %*
set EXITCODE=%ERRORLEVEL%
echo.
pause
exit /b %EXITCODE%
