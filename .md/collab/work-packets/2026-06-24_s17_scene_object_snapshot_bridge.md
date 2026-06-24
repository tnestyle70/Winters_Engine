# Work Packet: S17 Scene Object Snapshot Bridge

## Metadata

- ID: `2026-06-24_s17_scene_object_snapshot_bridge`
- Status: `Handoff`
- Owner: Desktop
- Branch: `main`
- Base: `origin/main`

## Owned Paths

- `Client/Public/Manager/Structure_Manager.h`
- `Client/Private/Manager/Structure_Manager.cpp`
- `Client/Public/Manager/Jungle_Manager.h`
- `Client/Private/Manager/Jungle_Manager.cpp`
- `Client/Public/Manager/Minion_Manager.h`
- `Client/Private/Manager/Minion_Manager.cpp`
- `Client/Public/Manager/AmbientProp_Manager.h`
- `Client/Private/Manager/AmbientProp_Manager.cpp`
- `Client/Private/Scene/Scene_InGameRender.cpp`
- `.md/build/2026-06-24_S17_SCENE_OBJECT_SNAPSHOT_BRIDGE_REPORT.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/work-packets/2026-06-24_s17_scene_object_snapshot_bridge.md`

## Read-Only Paths

- `Engine/Public/**`
- `Engine/Private/**`
- `Shared/**`
- `Server/**`
- `EngineSDK/inc/**`

## Validation

- `Tools/Harness/Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S17_SCENE_OBJECT_SNAPSHOT_BRIDGE_HARNESS_REPORT.md`
- `git diff --check`

## Handoff Notes

- 목표: LoL normal runtime에서 map/champion 다음 scene object manager 계층도 `RenderWorldSnapshot` append path에 노출한다.
- 기존 legacy render는 유지한다.
- Engine public header, generated SDK, project file은 수정하지 않는다.
- 반영: structure/jungle/minion/ambient prop manager append path와 InGame scene object snapshot render call을 추가했다.
- 검증: `.md/build/2026-06-24_S17_SCENE_OBJECT_SNAPSHOT_BRIDGE_HARNESS_REPORT.md` PASS.
