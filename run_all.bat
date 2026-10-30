@echo off
rem One command: refresh and open the showcase page, then start the engine menu. Extra arguments go to the engine.
cd /d "%~dp0"
if not exist "build\quant_engine.exe" (
    echo build\quant_engine.exe was not found. Build the project first: see README.md, "Build and run".
    pause
    exit /b 1
)
rem Refresh the page's numbers from the latest report; skipped quietly if Node or the report is missing.
where node >nul 2>nul
if not errorlevel 1 if exist "reports\latest_recommendation.csv" node site\build.mjs >nul 2>nul
start "" "site\index.html"
build\quant_engine.exe %*
set EXITCODE=%ERRORLEVEL%
echo.
pause
exit /b %EXITCODE%
