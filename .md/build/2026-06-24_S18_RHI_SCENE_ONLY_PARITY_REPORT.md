# 2026-06-24 S18 RHI Scene-Only Parity Gate Report

기준 문서:

- `.md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md`
- `.md/plan/rhi/sessions/S18_2026-06-24_RHI_SCENE_ONLY_PARITY_GATE.md`

## 이번 반영

- LoL normal runtime에 명시적 RHI scene-only 비교 모드를 추가했다.
  - `--rhi-scene-only`
  - `/rhi-scene-only`
- RHI scene renderer가 준비된 경우에만 비교 모드가 켜진다.
  - 준비되지 않으면 legacy draw fallback이 유지된다.
- 비교 모드에서 생략되는 legacy world mesh draw:
  - normal pass/SSAO mesh pass
  - map legacy draw
  - champion legacy draw
  - structure legacy draw
  - jungle legacy draw
  - minion legacy draw
  - ambient prop legacy draw
  - DX11 contact shadow plane
- 비교 모드에서도 유지되는 항목:
  - RHI map/champion/scene object snapshot draw
  - fog overlay
  - debug draw
  - attack range RHI/DX11 utility path
  - FX/UI/minimap overlay
- `Tools/Harness/Run-S17RhiValidation.ps1` runtime smoke에 `WintersGame_rhi_scene_only` 항목을 추가했다.
- `.md/plan/rhi/sessions/00_RHI_SESSION_INDEX.md`에 S18 문서를 등록했다.

## 검증

실행:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S18_RHI_SCENE_ONLY_PARITY_HARNESS_REPORT.md
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
  - `WintersGame_rhi_scene_only`: alive, cleanup kill.

상세 로그:

- `.md/build/2026-06-24_S18_RHI_SCENE_ONLY_PARITY_HARNESS_REPORT.md`

## 기준 문서 대비 반영률

- RINGFIX: 90-95%
  - 변화 없음. 기존 DX12 descriptor ring/frame guard 상태 유지.
- SNAPSHOT: 80-84%
  - map/champion/scene object snapshot append surface에 더해 RHI-only 비교 게이트가 생김.
  - 아직 skinned animation parity, render state/pass ordering, FX/debug/fog snapshot 통합은 남아 있음.
- LOLPORT: 52-58%
  - LoL normal runtime의 주요 world mesh 계층을 RHI snapshot으로 그린 뒤, 명시적 flag에서 legacy mesh draw를 끌 수 있음.
  - normal F5 legacy draw는 유지된다.
- public boundary cleanup: 55-60%
  - 신규 DX11/DX12 concrete public exposure는 없음.
  - `Engine/Public/Renderer/ModelRenderer.h` normal-pass DX11 API 정리는 아직 남아 있음.
- verification pipeline: 75-80%
  - 공용 harness가 RHI scene-only smoke까지 자동 실행한다.
  - full screenshot comparison/profiler JSON threshold는 아직 남아 있음.
- 전체 S17/S18 migration: 약 76%

## 남은 작업

- `--rhi-scene-only`에서 viewport screenshot 비교를 자동 수집하고 baseline과 비교한다.
- champion/minion/jungle skinned animation parity를 확인한다.
- FX mesh/billboard/beam, debug draw, fog/utility plane을 snapshot item 또는 RHI-native path로 단계적으로 묶는다.
- `Engine/Public/Renderer/ModelRenderer.h` normal-pass DX11 API를 private adapter 또는 Engine-internal path로 이동한다.
- legacy world mesh draw를 runtime 기본 경로에서 제거할지 결정하기 전에 visual parity report를 만든다.

## Handoff

- Work packet: `.md/collab/work-packets/2026-06-24_s18_rhi_scene_only_parity_gate.md`
- Plan: `.md/plan/rhi/sessions/S18_2026-06-24_RHI_SCENE_ONLY_PARITY_GATE.md`
- Harness: `.md/build/2026-06-24_S18_RHI_SCENE_ONLY_PARITY_HARNESS_REPORT.md`
