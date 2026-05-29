# FX Session Recovery - 2026-05-09

## Active Goal

- Primary goal: LoL-style FBX/mesh FX authoring and runtime playback.
- DX12 migration is supporting work only. Stop expanding DX12 unless it directly unblocks FX.
- Current track: stabilize the legacy DX11 FX path, assetize it through `.wfx`, then move the stable ABI to RHI/DX12 later.

## Current User Mode

- The usual rule was manual code output first.
- On 2026-05-09 the user explicitly switched this turn to direct apply mode:
  - "here부터 two tracks까지 directly reflect everything"
  - "run build too"
- Direct code edits are allowed for this turn. Do not assume this remains true in later turns unless the user says so.

## Changes Applied In This Turn

### `.wfx` loader round-trip

File: `Engine/Private/FX/FxAsset.cpp`

- Added helper parsing for `Vec2` and `Vec3` JSON arrays.
- `ParseWfxJson` now reloads fields that the dumper already writes:
  - `fade_in`
  - `fade_out`
  - `start_radius`
  - `end_radius`
  - `thickness`
  - `grow_duration`
  - `uv_scroll`
  - `attach_offset`
  - `end_offset`
  - `velocity`
  - `scale`
  - `rotation`
- `depth_mode` remains the explicit override path for legacy `depth_write`.

### Effect tuner load/spawn smoke path

File: `Client/Private/UI/EffectTuner.cpp`

- Added `SpawnCurrentWfxAsset(CScene_InGame*)`.
- Added UI button: `Load .wfx + Spawn`.
- The button loads the current preset `.wfx` through `CFxAssetRegistry::LoadFromFile`.
- Mesh preset uses `CFxMeshSystem::SpawnFromAsset`.
- Billboard/sprite presets use `CFxSystem::SpawnFromAsset`.

## Verification

- `git diff --check` passed for the touched files. Only LF-to-CRLF warnings were reported.
- Build command run:

```powershell
MSBuild.exe C:\Users\user\Desktop\Winters\Winters.sln /t:Client /p:Configuration=Debug-DX12 /p:Platform=x64 /m /v:minimal /nologo
```

- Result: success, exit code 0.
- Output binary: `Client/Bin/Debug-DX12/WintersGame.exe`.
- Existing warnings remain:
  - C4251/C4275 DLL interface warnings.
  - A non-fatal post-build message: `pwsh.exe` not found.

## Next Track

1. Runtime smoke in Effect Tuner:
   - Dump current `.wfx`.
   - Immediately press `Load .wfx + Spawn`.
   - Confirm color, erode, depth mode, scale, rotation, UV scroll, and fade behavior survive the round trip.
2. If runtime looks correct:
   - Add a small `.wfx` sample preset under `Client/Bin/Resource/FX/...`.
   - Use it from an Irelia/Ezreal FX preset instead of hard-coded legacy fields.
3. After one champion FX is asset-driven:
   - Start the material lifecycle track: hot reload, stable handles, and preview reset.
