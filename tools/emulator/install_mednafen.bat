@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_mednafen.ps1"
exit /b %ERRORLEVEL%
