@echo off
setlocal
set ROOT=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%convert_all_assets.ps1" %*
exit /b %ERRORLEVEL%
