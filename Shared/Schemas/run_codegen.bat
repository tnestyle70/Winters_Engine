@echo off
REM ASCII-only.
setlocal
set FLATC=%~dp0..\..\Tools\Bin\flatc.exe
set OUT_CPP=%~dp0Generated\cpp
set OUT_GO=%~dp0Generated\go

if not exist "%OUT_CPP%" mkdir "%OUT_CPP%"
if not exist "%OUT_GO%" mkdir "%OUT_GO%"

pushd "%~dp0"

for %%S in (Command Snapshot Event Hello LobbyTypes LobbyState LobbyCommand) do call :generate_cpp %%S
if errorlevel 1 goto :failed

for %%S in (Command Snapshot Event Hello LobbyTypes LobbyState LobbyCommand) do call :generate_go %%S
if errorlevel 1 goto :failed

popd

echo [OK] flatc codegen complete
endlocal
exit /b 0

:generate_cpp
"%FLATC%" --cpp --scoped-enums --no-warnings -o "%OUT_CPP%" "%~1.fbs"
if errorlevel 1 (
    echo [ERROR] flatc cpp codegen failed for %~1
    exit /b 1
)
exit /b 0

:generate_go
"%FLATC%" --go --no-warnings -o "%OUT_GO%" "%~1.fbs"
if errorlevel 1 (
    echo [ERROR] flatc go codegen failed for %~1
    exit /b 1
)
exit /b 0

:failed
popd
endlocal
exit /b 1
