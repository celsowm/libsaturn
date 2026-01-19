@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_bios_mpr17933.ps1" %*
exit /b %ERRORLEVEL%
