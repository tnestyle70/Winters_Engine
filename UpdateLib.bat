@echo off
REM =============================================================
REM  UpdateLib.bat  |  Engine -> EngineSDK + Client bin deploy
REM
REM  ASCII-only comments (avoids CP949/UTF-8 cmd.exe encoding issues).
REM  %~dp0 makes the script work from any working directory.
REM
REM  CGameInstance boundary rule:
REM    - Internal managers (_Manager.h) are NOT copied to SDK.
REM    - Client accesses managers only through GameInstance.h.
REM =============================================================

setlocal
set ROOT=%~dp0
if "%ROOT:~-1%"=="\" set ROOT=%ROOT:~0,-1%

REM -- Prepare output directories --
REM Subdirectory layout now mirrors Engine\Public exactly so Engine/SDK headers stay identical.
REM Avoid deleting the whole SDK include tree during normal builds. If a file is locked
REM while rd /S /Q runs, the tree can be left half-empty and Client compilation fails.
REM Set WINTERS_SDK_PURGE=1 manually when a one-off stale-file cleanup is needed.
if "%WINTERS_SDK_PURGE%"=="1" (
    if exist "%ROOT%\EngineSDK\inc\" rd /S /Q "%ROOT%\EngineSDK\inc\"
)
if not exist "%ROOT%\EngineSDK\inc\" mkdir "%ROOT%\EngineSDK\inc\"
if not exist "%ROOT%\EngineSDK\lib\" mkdir "%ROOT%\EngineSDK\lib\"
if not exist "%ROOT%\Client\Bin\Debug\"   mkdir "%ROOT%\Client\Bin\Debug\"
if not exist "%ROOT%\Client\Bin\Release\" mkdir "%ROOT%\Client\Bin\Release\"

REM -- Engine public API headers (Include root, currently no subdirs) --
xcopy /Y /S /I "%ROOT%\Engine\Include\*.h" "%ROOT%\EngineSDK\inc\" >nul

REM -- Engine internal headers -- preserve subdirectory structure.
REM    /S keeps Public\Resource\Texture.h -> inc\Resource\Texture.h etc.
xcopy /Y /E /I "%ROOT%\Engine\Public" "%ROOT%\EngineSDK\inc\" >nul

REM -- Purge _Manager.h files from SDK (recursive; internal managers hidden behind CGameInstance) --
del /Q /S "%ROOT%\EngineSDK\inc\*_Manager.h" 2>nul

REM -- ImGui public headers (Client uses #include <imgui.h>) --
if exist "%ROOT%\Engine\External\imgui\imgui.h" (
    xcopy /Y /D "%ROOT%\Engine\External\imgui\imgui.h"    "%ROOT%\EngineSDK\inc\"
    xcopy /Y /D "%ROOT%\Engine\External\imgui\imconfig.h"  "%ROOT%\EngineSDK\inc\"
)

REM -- Engine build artifacts .lib -> EngineSDK/lib (Debug/Release) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)

REM -- Engine DLL + PDB -> Client output dirs (Debug/Release) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" "%ROOT%\Client\Bin\Debug\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.pdb" "%ROOT%\Client\Bin\Debug\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.dll" "%ROOT%\Client\Bin\Release\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.pdb" "%ROOT%\Client\Bin\Release\"
)

REM -- ThirdPartyLib runtime DLLs -> Client output dirs --
REM    Assimp Debug: assimp + transitives (poly2tri, minizip, zlibd1, kubazip, pugixml)
REM    Assimp Release: assimp + transitives (poly2tri, minizip, zlib1, kubazip, pugixml)
REM    DirectXTK Debug/Release: DirectXTK.dll
if exist "%ROOT%\Engine\ThirdPartyLib\Assimp\Bin\Debug\" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\Assimp\Bin\Debug\*.dll" "%ROOT%\Client\Bin\Debug\"
)
if exist "%ROOT%\Engine\ThirdPartyLib\Assimp\Bin\Release\" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\Assimp\Bin\Release\*.dll" "%ROOT%\Client\Bin\Release\"
)
if exist "%ROOT%\Engine\ThirdPartyLib\DirectXTK\Bin\Debug\" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\DirectXTK\Bin\Debug\*.dll" "%ROOT%\Client\Bin\Debug\"
)
if exist "%ROOT%\Engine\ThirdPartyLib\DirectXTK\Bin\Release\" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\DirectXTK\Bin\Release\*.dll" "%ROOT%\Client\Bin\Release\"
)
REM    FMOD: single DLL (no Debug/Release split - fmod.dll used for both)
if exist "%ROOT%\Engine\ThirdPartyLib\FMOD\Bin\fmod.dll" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\FMOD\Bin\fmod.dll" "%ROOT%\Client\Bin\Debug\"
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\FMOD\Bin\fmod.dll" "%ROOT%\Client\Bin\Release\"
)

endlocal
exit /b 0
