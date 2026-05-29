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

- `imgui` had a tracked gitlink but no `.gitmodules` mapping. The mapping has been restored.
- ThirdParty headers exist, but required `.lib` and `.dll` inputs are missing in this clone.
- `.gitignore` now explicitly unignores `Engine/ThirdPartyLib/**/Lib/**` and `Engine/ThirdPartyLib/**/Bin/**` so these SDK inputs can be committed.
- `.gitignore` now explicitly unignores `Tools/Bin/flatc.exe` so schema codegen works after pull.

## Verification

```powershell
git submodule update --init --recursive
Test-Path Engine/External/imgui/imgui.cpp
Test-Path Engine/ThirdPartyLib/Assimp/Lib/Debug/assimp-vc143-mtd.lib
Test-Path Engine/ThirdPartyLib/DirectXTK/Lib/Debug/DirectXTK.lib
Test-Path Engine/ThirdPartyLib/FMOD/Lib/fmod_vc.lib
Test-Path Tools/Bin/flatc.exe
```
