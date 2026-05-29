@echo off
REM ASCII-only.
setlocal
set FLATC=%~dp0..\..\Tools\Bin\flatc.exe
set OUT_CPP=%~dp0Generated\cpp
set OUT_GO=%~dp0Generated\go

if not exist "%OUT_CPP%" mkdir "%OUT_CPP%"
if not exist "%OUT_GO%" mkdir "%OUT_GO%"

"%FLATC%" --cpp --scoped-enums --no-warnings -o "%OUT_CPP%" "%~dp0Command.fbs" "%~dp0Snapshot.fbs" "%~dp0Event.fbs" "%~dp0Hello.fbs" "%~dp0LobbyTypes.fbs" "%~dp0LobbyState.fbs" "%~dp0LobbyCommand.fbs"
if errorlevel 1 (
    echo [ERROR] flatc cpp codegen failed
    exit /b 1
)

"%FLATC%" --go --no-warnings -o "%OUT_GO%" "%~dp0Command.fbs" "%~dp0Snapshot.fbs" "%~dp0Event.fbs" "%~dp0Hello.fbs" "%~dp0LobbyTypes.fbs" "%~dp0LobbyState.fbs" "%~dp0LobbyCommand.fbs"
if errorlevel 1 (
    echo [ERROR] flatc go codegen failed
    exit /b 1
)

echo [OK] flatc codegen complete
endlocal
exit /b 0
