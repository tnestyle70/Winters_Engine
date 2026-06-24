# Work Packet: Elden Editor Validation Panels

## Metadata

- ID: `2026-06-24_elden_editor_validation_panels`
- Status: `Active`
- Owner: Laptop
- Branch: `main`
- Base: local dirty worktree on 2026-06-24

## Owned Paths

- `EldenRingEditor/Public/EldenRingEditorScene.h`
- `EldenRingEditor/Private/EldenRingEditorScene.cpp`
- `.md/plan/EldenRingEditor/10_2026-06-24_EDITOR_VALIDATION_PANELS_AND_DESKTOP_HANDOFF_DESIGN.md`
- `.md/build/2026-06-24_ELDEN_EDITOR_VALIDATION_PANELS_REPORT.md`
- `.md/collab/work-packets/2026-06-24_elden_editor_validation_panels.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `Client/Bin/Resource/EldenRing/FX/editor_seed.wfx.json`
- `Client/Bin/Resource/EldenRing/Sequences/editor_seed.wseq.json`
- `Client/Bin/Resource/EldenRing/World/editor_world.json`
- `Client/Bin/Resource/EldenRing/World/editor_seed_cell.json`
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_seed.wfx.json`
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_seed.wseq.json`
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_world.json`
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_seed_cell.json`

## Read-Only Paths

- `Shared/GameSim/**`
- `Server/**`
- `Client/**`
- `Engine/Private/RHI/**`
- `Engine/Public/RHI/**`
- `EngineSDK/inc/**`

## Validation

- CMake `WintersEldenRingEditor` Debug: PASS
- CMake `WintersEngine` Debug: PASS
- CMake `WintersElden` Debug: PASS
- MSBuild `Engine/Include/Engine.vcxproj` Debug x64: PASS
- `git diff --check`: PASS, CRLF warnings only
- `Tools/Harness/Run-S17RhiValidation.ps1 -SkipRuntimeSmoke -ReportPath .md\build\2026-06-24_ELDEN_EDITOR_VALIDATION_HARNESS_REPORT.md`: PASS

## Handoff Notes

- 이번 packet은 Editor shell에 검증 패널을 붙이는 작업이다.
- seed asset json 4개가 추가되어 각 패널의 기본 경로에서 load/validate를 누를 수 있다.
- `Client/Bin/Resource/**`는 ignored runtime resource 경로다. 데스크탑은 Plan seed copy를 runtime 경로로 복사하면 같은 기본 경로로 검증할 수 있다.
- Desktop은 같은 파일을 수정하기 전에 이 packet 상태를 `Handoff` 또는 `Merged`로 바꾼 뒤 pull/rebase 후 이어간다.
- 다음 구현자는 추가된 seed asset json을 각 패널의 Load/Validate 버튼으로 검증하고, viewport preview/ray-pick/gizmo로 이어간다.
