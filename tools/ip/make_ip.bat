@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make_ip.ps1" %*
exit /b %ERRORLEVEL%
