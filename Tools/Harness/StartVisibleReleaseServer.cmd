@echo off
setlocal
title Winters Release Server
cd /d "%~dp0\..\.."

if "%~1"=="" (
    echo Usage: StartVisibleReleaseServer.cmd GAME_SESSION_ID
    exit /b 2
)

set "WINTERS_GAME_SESSION_ID=%~1"
set "WINTERS_MATCH_TICKET_SECRET=winters-match-ticket-secret-change-in-production"
set "WINTERS_MATCHMAKING_SERVICE_URL=http://127.0.0.1:8083"
set "WINTERS_MATCHMAKING_INTERNAL_TOKEN=winters-matchmaking-internal-token-change-in-production"
set "WINTERS_REPLAY_SERVICE_URL=http://127.0.0.1:8087"
set "WINTERS_REPLAY_INTERNAL_TOKEN=winters-replay-internal-token-change-in-production"

echo [ReleaseCapture] session=%WINTERS_GAME_SESSION_ID%
echo [ReleaseCapture] type q then press Enter, or close this CMD window, to stop WintersServer.
Server\Bin\Release\WintersServer.exe --net-transport=udp

echo.
echo WintersServer exited with code %ERRORLEVEL%.
