# Work Packet: S18 RHI Scene-Only Parity Gate

## Metadata

- ID: `2026-06-24_s18_rhi_scene_only_parity_gate`
- Status: `Handoff`
- Owner: Desktop
- Branch: `main`
- Base: `origin/main`

## Owned Paths

- `Client/Private/Scene/Scene_InGameRender.cpp`
- `Tools/Harness/Run-S17RhiValidation.ps1`
- `.md/plan/rhi/sessions/S18_2026-06-24_RHI_SCENE_ONLY_PARITY_GATE.md`
- `.md/build/2026-06-24_S18_RHI_SCENE_ONLY_PARITY_REPORT.md`
- `.md/build/2026-06-24_S18_RHI_SCENE_ONLY_PARITY_HARNESS_REPORT.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/work-packets/2026-06-24_s18_rhi_scene_only_parity_gate.md`

## Read-Only Paths

- `Engine/Public/**`
- `Engine/Private/**`
- `Shared/**`
- `Server/**`
- `EngineSDK/inc/**`

## Validation

- `Tools/Harness/Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S18_RHI_SCENE_ONLY_PARITY_HARNESS_REPORT.md`
- `git diff --check`

## Handoff Notes

- 반영: `--rhi-scene-only` parity gate와 harness `WintersGame_rhi_scene_only` smoke를 추가했다.
- 검증: `.md/build/2026-06-24_S18_RHI_SCENE_ONLY_PARITY_HARNESS_REPORT.md` PASS.
- 목표: 명시적 command-line flag에서만 LoL world mesh legacy draw를 생략해 RHI scene snapshot parity를 볼 수 있게 한다.
- Normal F5 path는 legacy draw를 유지한다.
- Engine public header, generated SDK, project file은 수정하지 않는다.
