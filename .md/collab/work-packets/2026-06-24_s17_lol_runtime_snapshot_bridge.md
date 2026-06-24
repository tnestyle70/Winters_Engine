# Work Packet: S17 LoL Runtime Snapshot Bridge

## Metadata

- ID: `2026-06-24_s17_lol_runtime_snapshot_bridge`
- Status: `Handoff`
- Owner: Desktop
- Branch: `main`
- Base: `origin/main`

## Owned Paths

- `Client/Private/Scene/Scene_InGameRender.cpp`
- `.md/build/2026-06-24_S17_LOL_RUNTIME_SNAPSHOT_BRIDGE_REPORT.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/work-packets/2026-06-24_s17_lol_runtime_snapshot_bridge.md`

## Read-Only Paths

- `Engine/Public/**`
- `Engine/Private/**`
- `Shared/**`
- `Server/**`
- `EngineSDK/inc/**`

## Validation

- `Tools/Harness/Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S17_LOL_RUNTIME_SNAPSHOT_BRIDGE_HARNESS_REPORT.md`
- `git diff --check`

## Handoff Notes

- 목적은 LoL normal runtime에서 map 다음의 scene objects를 `RenderWorldSnapshot` 경로로 더 많이 제출하는 것이다.
- Engine public header나 SDK generated file은 이 packet에서 수정하지 않는다.
