# 2026-06-24 S17 Scene Object Snapshot Bridge Report

기준 문서: `.md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md`

## 이번 반영

- LoL scene object manager 계층에 `RenderWorldSnapshot` append API를 추가했다.
  - `CStructure_Manager::AppendRenderSnapshotMeshes`
  - `CJungle_Manager::AppendRenderSnapshotMeshes`
  - `CMinion_Manager::AppendRenderSnapshotMeshes`
  - `CAmbientProp_Manager::AppendRenderSnapshotMeshes`
- `CScene_InGame::OnRender`에서 champion legacy render 이후, structure/jungle/minion/ambient prop을 하나의 `SceneObjects::RHISceneSnapshot`으로 수집해 `CRHISceneRenderer::Render`에 넘긴다.
- 기존 legacy render 순서는 그대로 유지했다.
  - structure
  - jungle
  - minion
  - ambient prop
- FOW/local-team 조건은 기존 structure/minion render 조건을 그대로 따른다.
- Engine public header, generated SDK, project file은 수정하지 않았다.

## 검증

실행:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S17_SCENE_OBJECT_SNAPSHOT_BRIDGE_HARNESS_REPORT.md
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

- `.md/build/2026-06-24_S17_SCENE_OBJECT_SNAPSHOT_BRIDGE_HARNESS_REPORT.md`

## 기준 문서 대비 반영률

- RINGFIX: 90-95%
  - 변화 없음. 기존 DX12 descriptor ring/frame guard 상태 유지.
- SNAPSHOT: 78-82%
  - map, champion, structure, jungle, minion, ambient prop까지 `RenderWorldSnapshot` append surface가 열렸다.
  - 아직 RHI-only parity와 pass ordering/state coverage 검증은 남아 있다.
- LOLPORT: 45-50%
  - LoL normal runtime의 주요 world mesh 계층이 RHI scene snapshot 경로에 노출됐다.
  - legacy draw가 여전히 최종 화면 안정성을 담당한다.
  - FX/UI/debug/fog/utility plane과 legacy draw 제거는 남아 있다.
- public boundary cleanup: 55-60%
  - 신규 DX11/DX12 concrete public exposure는 없음.
  - `ModelRenderer` legacy normal-pass DX11 API 정리는 아직 남아 있다.
- 전체 S17 migration: 약 72%

## 남은 작업

- RHI-only 비교 모드를 만들어 map/champion/scene object legacy draw를 끄고 visual parity를 확인.
- skinned animation parity를 snapshot renderer에서 확인.
- FX mesh/billboard/beam, debug draw, fog/utility plane을 snapshot item 또는 RHI-native path로 묶기.
- full viewport screenshot 비교와 profiler counter JSON 검증을 harness에 추가.
- Engine public `ModelRenderer` normal-pass DX11 API를 private adapter로 밀어 public boundary를 더 줄이기.

## Handoff

- Work packet: `.md/collab/work-packets/2026-06-24_s17_scene_object_snapshot_bridge.md`
- Owned code paths:
  - `Client/Public/Manager/*_Manager.h`
  - `Client/Private/Manager/*_Manager.cpp`
  - `Client/Private/Scene/Scene_InGameRender.cpp`
- 다음 packet은 RHI-only 비교 모드 또는 FX/debug snapshot item 중 하나로 좁히는 것을 권장한다.
