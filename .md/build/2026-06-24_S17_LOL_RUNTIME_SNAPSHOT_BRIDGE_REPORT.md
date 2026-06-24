# 2026-06-24 S17 LoL Runtime Snapshot Bridge Report

기준 문서: `.md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md`

## 이번 반영

- `Client/Private/Scene/Scene_InGameRender.cpp`에 챔피언 snapshot 수집 helper를 추가했다.
- 기존 LoL champion visibility 조건을 재사용했다.
  - `rc.bSceneManaged`, `rc.bVisible`, `rc.pRenderer`
  - local team/replay reveal 필터
  - `MeshGroupVisibilityComponent` mask
  - frustum culling path
- `Champion::Render` 블록 초입에서 `RenderWorldSnapshot`을 만들고 `CRHISceneRenderer::Render`로 넘기도록 연결했다.
- 기존 DX11 champion render loop는 그대로 유지했다. 정상 F5 LoL 시각 경로를 우회하지 않는다.
- Engine public header, `EngineSDK/inc`, project file은 수정하지 않았다.

## 검증

실행:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S17_LOL_RUNTIME_SNAPSHOT_BRIDGE_HARNESS_REPORT.md
```

결과:

- `git diff --check`: PASS
  - CRLF 변환 경고만 있음.
- `Client/Public` + `Shared` concrete graphics audit: PASS, match 0.
- focused common RHI public header audit: PASS, match 0.
- CMake/Ninja S17 targets: PASS.
- `MSBuild Winters.sln` Debug x64: PASS, warning 78, error 0.
- runtime smoke: PASS.
  - `WintersElden_probe_dx12`: alive, cleanup kill.
  - `WintersElden_probe_dx11`: alive, cleanup kill.
  - `WintersEldenRingEditor`: alive, cleanup kill.
  - `WintersGame`: alive, cleanup kill.

상세 로그:

- `.md/build/2026-06-24_S17_LOL_RUNTIME_SNAPSHOT_BRIDGE_HARNESS_REPORT.md`

## 기준 문서 대비 반영률

- RINGFIX: 90-95%
  - 이전 세션에서 DX12 descriptor ring frame partition과 mid-frame initial-data guard가 반영된 상태 유지.
- SNAPSHOT: 73-78%
  - map snapshot 이후 champion `ModelRenderer`까지 `RenderWorldSnapshot` append 경로가 확장됨.
  - 아직 pass ordering, state coverage, FX/debug item, skinned animation parity는 남아 있음.
- LOLPORT: 35-40%
  - LoL normal runtime에서 map과 champion이 RHI scene snapshot 경로에 들어오기 시작함.
  - legacy map/champion render가 여전히 최종 화면 안정성을 담당함.
  - structure, jungle, minion, ambient prop manager는 아직 legacy direct render가 주 경로.
- public boundary cleanup: 55-60%
  - 이번 packet에서 신규 public DX 노출은 없음.
  - `Engine/Public/Renderer/ModelRenderer.h`의 legacy normal-pass DX11 API 정리는 아직 남아 있음.
- 전체 S17 migration: 약 68%

## 남은 작업

- structure, jungle, minion renderer에 snapshot append bridge를 단계적으로 추가.
- map/champion legacy draw를 비교 모드 또는 feature flag로 분리하고 RHI-only visual parity를 확인.
- champion snapshot의 skinned animation parity 확인.
  - 현재 snapshot append는 모델 mesh slice를 RHI로 밀어 넣는 bridge 성격이며, 최종 animated champion parity 검증은 별도 필요.
- full viewport screenshot 비교와 profiler counter JSON 기준을 harness에 추가.
- `Engine/Public/Renderer/ModelRenderer.h` normal pass DX11 API를 public boundary 밖으로 이동.

## Handoff

- Work packet: `.md/collab/work-packets/2026-06-24_s17_lol_runtime_snapshot_bridge.md`
- Owned code path: `Client/Private/Scene/Scene_InGameRender.cpp`
- 다음 작업자는 manager 계층 public header를 건드리기 전에 새 packet을 열고 owned paths를 명시해야 한다.
