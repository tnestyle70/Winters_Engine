# Clone Build Repro Requirements

Date: 2026-05-25

This repo is intended to build from a fresh clone without relying on files that only exist on one desktop.

## Required External Inputs

- `Engine/External/imgui` is a git submodule pinned by the root tree.
- `Engine/ThirdPartyLib/Assimp/Lib/{Debug,Release}` and `Engine/ThirdPartyLib/Assimp/Bin/{Debug,Release}` must exist.
- `Engine/ThirdPartyLib/DirectXTK/Lib/{Debug,Release}` and `Engine/ThirdPartyLib/DirectXTK/Bin/{Debug,Release}` must exist.
- `Engine/ThirdPartyLib/FMOD/Lib/fmod_vc.lib` and `Engine/ThirdPartyLib/FMOD/Bin/fmod.dll` must exist.
- `Tools/Bin/flatc.exe` must exist because Client and Server run `Shared/Schemas/run_codegen.bat` before C++ compilation.

## Current Clone Finding

- `Engine/External/imgui` is currently vendored as tracked files in this tree; there is no active `.gitmodules` file.
- ThirdParty headers exist. Required `.lib` and `.dll` inputs must either be committed or restored by an explicit dependency package before build.
- `.gitignore` explicitly unignores `Engine/ThirdPartyLib/**/Lib/**` and `Engine/ThirdPartyLib/**/Bin/**` so these SDK inputs can be committed.
- `.gitignore` explicitly unignores `Tools/Bin/flatc.exe` so schema codegen can work after pull once the file is tracked.
- `Client/Bin/Resource` stays local-only. Desktop/laptop sync assumes runtime assets are restored separately, while source, generated schema, EngineSDK headers, tools, and required SDK binaries come from git or a documented restore step.

## Verification

```powershell
git submodule update --init --recursive
Test-Path Engine/External/imgui/imgui.cpp
Test-Path Engine/ThirdPartyLib/Assimp/Lib/Debug/assimp-vc143-mtd.lib
Test-Path Engine/ThirdPartyLib/DirectXTK/Lib/Debug/DirectXTK.lib
Test-Path Engine/ThirdPartyLib/FMOD/Lib/fmod_vc.lib
Test-Path Tools/Bin/flatc.exe
powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1
powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1 -RequireTracked
```
