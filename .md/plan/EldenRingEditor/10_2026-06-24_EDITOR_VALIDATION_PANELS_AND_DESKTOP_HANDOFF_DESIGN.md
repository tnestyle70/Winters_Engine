# 2026-06-24 Elden Editor Validation Panels + Desktop Handoff Design

## 목표

Elden Editor 5대 시스템(World Editor, FX Graph, Sequencer, World Partition, Boss Testing)을 데스크탑/노트북 협업자가 같은 기준으로 이어받을 수 있게, 에디터 안에서 즉시 누를 수 있는 검증 패널과 문서 기반 handoff 단위를 고정한다.

## 현재 반영 상태

- `EldenRingEditorScene`은 ImGui DockSpace 기반 editor shell을 가진다.
- World Editor는 `.wcell` load/save, seed placement, transaction add/delete/transform, undo/redo를 가진다.
- FX Graph는 `CFxGraph` JSON roundtrip, `CFxGraphValidator`, `CFxGraphCompiler`, `CFxExecPlan`을 가진다.
- Sequencer는 `CSequenceAsset` JSON roundtrip, validation, `CSequencePlayer` discrete key 중복 방지를 가진다.
- World Partition은 cell state/reason/debug stats, visible instance filtering을 가진다.
- Boss Testing은 Engine-side `HitVolume`, sanitize, active window, debug string helper를 가진다.

## 협업 경계

- Owner packet: `.md/collab/work-packets/2026-06-24_elden_editor_validation_panels.md`
- 주 소유 경로:
  - `EldenRingEditor/Public/EldenRingEditorScene.h`
  - `EldenRingEditor/Private/EldenRingEditorScene.cpp`
  - `.md/plan/EldenRingEditor/10_2026-06-24_EDITOR_VALIDATION_PANELS_AND_DESKTOP_HANDOFF_DESIGN.md`
  - `.md/build/2026-06-24_ELDEN_EDITOR_VALIDATION_PANELS_REPORT.md`
- 읽기 전용으로 취급할 경로:
  - `Shared/GameSim/**`
  - `Server/**`
  - `Client/**`
  - `Engine/Private/RHI/**`
  - `Engine/Public/RHI/**`
- `EngineSDK/inc/**`, `.vcxproj`, `.filters`, Engine public header는 충돌 위험 파일이다. 새 public API가 필요해질 때만 별도 packet으로 분리한다.

## 에디터 검증 패널 설계

### FX Graph

입력:
- `.wfx.json` authoring path

동작:
- `CFxGraph::LoadFromJson`
- emitter별 `CFxGraphValidator::Validate`
- validation success인 emitter만 `CFxGraphCompiler::Compile`

출력:
- emitter count
- compiled count
- validation/compile issue rows

### Sequencer

입력:
- `.wseq.json` authoring path

동작:
- `CSequenceAsset::LoadFromJson`
- `CSequenceAsset::Validate`

출력:
- track count
- key count
- duration
- validation issue rows

서버 권위 경계:
- EventTrack은 gameplay truth를 만들지 않는다.
- runtime player는 `ISeqEventSink::PushCandidate` 후보 발행만 한다.

### World Partition

입력:
- `.wmap`/world json path

동작:
- `CAssetStreamingSystem::Create`
- `CWorldPartitionSystem::Create`
- `LoadWorld`
- probe `StreamingSourceComponent` 주입
- `Update` 3회로 Queued -> LoadedHidden -> Visible 전이를 관찰
- `GetDebugStats`, `CollectVisibleInstances`

출력:
- cell state counts
- transition count
- missing asset counts
- visible instance count
- cell별 state/desired/reason row

### Boss Testing

입력:
- 현재는 seed geometry 내장 probe

동작:
- `WintersPhysics3D::MakeAABB`
- `MakeOBB`
- `MakeSphere`
- `Overlap`
- `MakeActiveWindow`

출력:
- hit volume debug string
- overlap 결과
- active/dodge frame count

서버 권위 경계:
- 이 패널은 순수 geometry/debug utility만 호출한다.
- damage, phase transition, GameSim truth mutation은 구현하지 않는다.

## Seed Asset 상태

작성 완료:
- `Client/Bin/Resource/EldenRing/FX/editor_seed.wfx.json`
- `Client/Bin/Resource/EldenRing/Sequences/editor_seed.wseq.json`
- `Client/Bin/Resource/EldenRing/World/editor_world.json`
- `Client/Bin/Resource/EldenRing/World/editor_seed_cell.json`

추적 가능한 source copy:
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_seed.wfx.json`
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_seed.wseq.json`
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_world.json`
- `.md/plan/EldenRingEditor/seed-assets/2026-06-24/editor_seed_cell.json`

주의:
- `Client/Bin/Resource/**`는 `.gitignore` 대상이다. 데스크탑 handoff 시 Plan 사본을 같은 runtime 경로로 복사하거나, 명시적으로 force-add할지 별도 결정한다.

## 다음 코드 게이트

1. World viewport preview
   - `CWorldCellDocument` placement를 `ModelRenderer` 또는 RHI snapshot preview로 연결한다.
   - 정상 F5 runtime을 숨기지 않고 Editor 전용 preview path로 제한한다.
2. Ray pick / gizmo
   - `ModelRenderer` AABB 기반 pick부터 시작한다.
   - transaction commit은 기존 `CTransformPlacementCommand`로만 들어간다.
3. FX/Sequencer/Boss 패널 분리
   - `EldenRingEditorScene`이 비대해지면 panel별 작은 helper class로 분리한다.
   - 분리 시 새 파일은 CMake/VS project 포함 여부를 검증한다.

## 검증 기준

- `git diff --check`
- CMake Debug:
  - `WintersEngine`
  - `WintersElden`
  - `WintersEldenRingEditor`
- MSBuild Debug x64:
  - `Engine/Include/Engine.vcxproj`
  - Client/Server는 Engine public header 또는 SDK 변경 시 재검증한다.
- Desktop 협업 handoff 시:
  - `.md/collab/ACTIVE_WORK_PACKETS.md` packet 상태 갱신
  - `.md/build/YYYY-MM-DD_*.md` 새 결과 보고서 작성
  - 필요 시 `Tools/Harness/Run-S17RhiValidation.ps1 -SkipRuntimeSmoke`
