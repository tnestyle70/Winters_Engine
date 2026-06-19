# Third-Party Binary Notices

Winters tracks a small set of third-party build/runtime inputs so a desktop and laptop can build after `git pull` without copying local tool binaries by hand.

## Tracked Build Inputs

- `Tools/Bin/flatc.exe`
  - Upstream: Google FlatBuffers
  - License: Apache-2.0
  - Purpose: schema code generation in `Shared/Schemas/run_codegen.bat`

- `Engine/ThirdPartyLib/Assimp/Bin/**`
- `Engine/ThirdPartyLib/Assimp/Lib/**`
  - Upstream: Open Asset Import Library
  - License: BSD-3-Clause based Assimp license
  - Purpose: model import/conversion and runtime support DLLs

- `Engine/ThirdPartyLib/DirectXTK/Bin/**`
- `Engine/ThirdPartyLib/DirectXTK/Lib/**`
  - Upstream: Microsoft DirectX Tool Kit
  - License: MIT
  - Purpose: DirectX helper library used by Engine/Client builds

## Already Tracked Runtime Input

- `Engine/ThirdPartyLib/FMOD/Bin/fmod.dll`
- `Engine/ThirdPartyLib/FMOD/Lib/fmod_vc.lib`
  - Upstream: FMOD
  - License: FMOD terms
  - Purpose: runtime audio dependency

## Boundary

Do not track general build output executables, PDBs, intermediate files, or copied runtime bins under `Client/Bin/Debug*`, `Client/Bin/Release*`, `Tools/Bin/Debug`, or `Tools/Bin/Release`.

Do not track `Client/Bin/Resource`. Runtime assets are restored separately per machine.

Before pushing dependency-boundary changes, run:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1
powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1 -RequireTracked
```
