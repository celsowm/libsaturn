@echo off
REM Quick start wrapper for setup.ps1

echo Starting Saturn Development Setup...
echo.

REM Check if PowerShell is available
where powershell >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: PowerShell not found!
    echo Please install PowerShell to run setup.
    pause
    exit /b 1
)

REM Run setup script with current directory
powershell -ExecutionPolicy Bypass -File "%~dp0setup.ps1" -InstallPath "%~dp0"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Setup completed successfully!
) else (
    echo.
    echo Setup encountered errors. Please review output above.
)

pause
