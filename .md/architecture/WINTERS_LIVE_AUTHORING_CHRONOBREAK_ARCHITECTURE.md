# Winters Live Authoring / ChronoBreak Architecture

작성일: 2026-07-12  
적용 범위: LoL Practice/Balance/AI Debug, FX·Animation·Model·UI·Map authoring, Elden boss pattern tool, Replay, Server GameSim, Client presentation

## 0. 북극성

Winters의 제작 도구가 달성해야 할 결과는 “패널이 많다”가 아니다.

```text
한 번 빌드하고 한 번 실행한 상태에서
찾기 -> 배치 -> 편집 -> 미리보기 -> 서버 적용 -> 관찰 -> 되돌리기
-> 같은 조건 재실험 -> 저장 -> canonical data로 승격
```

이 루프가 끊기지 않아야 한다. 단, 한 실행 안에서 모든 것을 한다는 말이 하나의 World와 하나의 Clock에 모든 책임을 섞는다는 뜻은 아니다. 다음 세 진실은 분리한다.

```text
Authoring Document Truth
  파일의 draft, selection, dirty, undo/redo, save revision

Authoritative Simulation Truth
  서버의 entity, HP, 위치, cooldown, AI, boss phase, gameplay patch

Presentation Preview Truth
  model, material instance, animation pose, FX age, UI layout, preview camera
```

Client tool은 authoring document와 격리된 preview object를 직접 편집할 수 있다. Gameplay/AI/boss/scenario 결과는 반드시 typed command를 거쳐 서버 tick boundary에서 원자적으로 적용한다.

## 1. 2026-07-12 현재 기준선

| 영역 | 현재 재사용할 자산 | 현재 한계 |
|---|---|---|
| Practice/Balance | F10, typed PracticeControl, host gate, 14 effect scalar, JSON Load/Edit/Save/Apply, HP/MP/CD/gold/level/teleport/minion | current player 중심, Clear+N Apply 비원자적, 승인 result/revision 없음, direct truncate save |
| Simulation time | Claude S014의 server Pause/Step/TimeScale, WRPL Command record | P0 기반일 뿐 rewind 아님, presentation clock 미연결, pause lane journal 누락 |
| AI | Perception/Valuation/Brain/GameCommand, 14 runtime tuning 값, 16행 trace, F9 force/tune | candidate contribution·Perception 전문·executor result·subscription 없음 |
| FX | F7 WFX catalog/editor/save/preview, 실제 CFxCuePlayer 경로 | dirty/undo/timeline/curve/scrub/solo/atomic save 없음 |
| Animation/Model | per-instance Animator, clip playback, speed/reverse, bone resolve, 2048-bit submesh mask | exact clip/submesh/bone 열거, seek pose, crossfade, safe material instance 없음 |
| Sequence | Camera/Anim/FX/Audio/Event/Visibility/TimeDilation와 Seek | Seek가 discrete Anim/FX를 재구성하지 않아 scrub backbone으로 바로 사용할 수 없음 |
| UI | F8 tuner, HUD numeric layout/save, Lua reload, Shop JSON apply | canvas/hierarchy/anchor/resolution/undo/last-good rollback 없음 |
| Map | 별도 Scene_Editor, hierarchy/inspector/category/save/load/navgrid | live match를 유지하지 않고 scene를 재생성, undo/gizmo/multiselect/atomic save 없음 |
| Replay | WRPL Snapshot/Event, v2 Command record, forward player | full sim checkpoint/restore/branch/epoch/seek 없음 |

문서 상태도 구분한다.

- S009 Practice/Balance: 구현·자동 검증 완료.
- S010 Game Feel: 계획 문서이며 미구현 항목이 남아 있다.
- S011 Kalista/AI: 구현·재검증 완료, oath late-join 부채가 남아 있다.
- S012 VFX authoring: 설계 문서이며 runtime 변경 전이다.
- S013 state/AI foundation: stale action과 DefendMid 수직 슬라이스 구현·검증 완료.
- Claude S014: Active work packet이다. Pause/Step/TimeScale과 Command journal P0이며 아직 ChronoBreak 완료가 아니다.

## 2. 하나의 실행, 두 개의 Workspace

### 2-1. Live Authoritative Lab

정상 Server/GameSim/Snapshot/Event 경로를 그대로 사용한다.

용도:

- 밸런스 patch
- AI·boss policy tuning
- champion/minion/dummy/boss 배치
- HP/레벨/아이템/cooldown/phase 설정
- scenario baseline과 server checkpoint
- LAN snapshot 결과 확인

Mutation 경로:

```text
Designer ImGui
-> typed tool command
-> designer capability / host gate
-> read-only validation
-> atomic tick commit
-> ToolOperationResult
-> Snapshot/Event
-> Client panel refresh
```

### 2-2. Isolated Presentation Preview World

서버 gameplay world를 오염시키지 않는 별도 preview world다.

용도:

- model/submesh/material inspection
- animation seek/crossfade/event marker
- WFX timeline/curve/reference overlay
- UI layout/resolution preview
- camera/light/background preset
- attachment/socket 확인

Preview는 별도 renderer/cache를 만들지 않는다. 동일한 `ModelRenderer`, `CAnimator`, `CFxCuePlayer`, UI runtime contract를 사용하되 preview instance와 clock만 격리한다.

## 3. 세 개의 Clock

```text
SimulationClock
  serverTick, fixed 30 Hz, fDt 불변

PresentationClock
  authoritative tick age + interpolation + render sampling

PreviewClock
  local play/pause/step/scrub/loop, asset authoring 전용
```

모드:

| 모드 | Simulation | Presentation | Preview |
|---|---|---|---|
| Live Linked | Pause/Scale 적용 | 같은 pause/scale과 server phase를 추적 | 독립 |
| Live Simulation Only | Pause/Scale 적용 | 카메라·tool UI는 계속, gameplay visual은 정책별 | 독립 |
| Presentation Inspect | 정상 진행 또는 replay | 선택 FX/anim만 freeze/slow | 독립 |
| Asset Preview | 무관 | 무관 | 자유 scrub |

Server TimeScale은 wall-clock tick pacing만 바꾸며 `fDt`를 바꾸지 않는다. Client animation/FX에 render `dt`를 그대로 넣으면 linked mode가 아니다. Snapshot에는 실제 `simulationPaused`, `simulationTimeScale`, `timelineEpoch`, `branchId`, `toolRevision`을 전달하고 Client는 requested 값이 아니라 authoritative 값을 표시한다.

## 4. Control Plane과 Mutation Plane

명령을 같은 큐에 넣을 수 있어도 의미는 분리한다.

| Domain | 예 | Re-sim 입력 | 권한 |
|---|---|---:|---|
| PlayerInput | Move, BA, Q/W/E/R, item | 예 | controlled actor |
| SimulationMutation | Teleport, spawn, HP/level/item 변경 | 예 | designer host/capability |
| AuthoringPatch | balance profile, AI weights, boss pattern revision | 예 | designer host/capability |
| ControlPlane | Pause, Step, TimeScale, Rewind, Branch | 아니오 | designer host/capability |
| ObservationOnly | trace subscribe/export, breakpoint | 아니오 | debug read capability |

공통 규칙:

1. Designer command는 플레이어 캐릭터 생사와 무관하게 처리한다.
2. 모든 mutation은 요청 session, target stable ID, transaction ID, base revision, requested tick을 가진다.
3. 서버는 accepted/rejected, reason, effective tick, new revision, patch hash를 응답한다.
4. UI는 Draft, Staged, Pending, Applied, Rejected, Canonical 상태를 구분한다.
5. Pause 중 mutation도 journal과 revision에서 빠지지 않는다.
6. ControlPlane command를 Chrono re-sim에 다시 넣어 orchestrator를 스스로 멈추게 하지 않는다.

## 5. 세 종류의 Undo

### 5-1. Draft Undo/Redo

아직 서버나 runtime asset에 적용하지 않은 document edit다.

- Ctrl+Z/Ctrl+Y
- drag 한 번은 transaction 한 건
- multi-field preset apply도 transaction 한 건
- savepoint와 dirty revision 분리

### 5-2. Applied Patch Undo/Redo

서버가 승인한 revisioned patch의 inverse patch다.

```text
TuningPatch
  transactionId
  baseRevision
  targetSelector
  operations[]
  requestedAtTick

TuningPatchResult
  accepted
  reason
  effectiveTick
  newRevision
  patchHash
  fieldErrors[]
```

F10의 `Clear -> N Apply`는 중간 실패 시 partial state가 남으므로 하나의 `ReplaceOverlay` transaction으로 바꾼다.

### 5-3. Chrono Rewind

권위 simulation 전체를 checkpoint로 되돌린다. Document undo나 inverse patch와 동일한 기능이 아니다.

## 6. 공통 ToolDocument

모든 authoring document는 최소 다음 계약을 공유한다.

```text
DocumentId / SchemaVersion
CurrentRevision / SavedRevision / AppliedRevision
DirtyState
UndoStack / RedoStack
ActiveTransaction
ValidationMessages
DependencyList
AutosavePath / RecoveryState
```

저장 규칙:

1. 원본에 `ios::trunc`로 직접 쓰지 않는다.
2. deterministic writer로 temp 파일 작성.
3. flush/close 후 재파싱·validation.
4. 기존 파일을 `.bak` 또는 recovery revision으로 보존.
5. 같은 volume에서 atomic replace.
6. 실패하면 기존 파일을 유지하고 클릭 가능한 진단을 표시.
7. viewport/camera/selection 같은 user state는 authored asset과 별도 저장.

Elden editor의 `IEditorCommand/CEditorTransaction` 패턴은 재사용 후보지만 `WorldCellDocument` 결합 부분을 그대로 LoL Client에 링크하지 않는다. Generic history만 Engine Tools 계층으로 추출한다.

## 7. ChronoBreak 구조

### 7-1. 기본 형태

```text
End-of-tick Keyframe
+ External Command Journal
+ Definition/Tuning Revision Manifest
+ Deterministic RNG
-> Restore nearest keyframe
-> Apply eligible journal records
-> Re-simulate to target tick
-> Verify state hash
-> increment timelineEpoch / branchId
-> full authoritative snapshot
-> client rebase
```

### 7-2. Checkpoint boundary

Checkpoint는 임의 system 중간이 아니라 다음 조건의 end-of-tick에서만 잡는다.

- command queue flush 완료
- damage/status/death/respawn 완료
- replicated event collection 완료
- spatial index phase가 명시됨
- pending async gameplay job 없음

### 7-3. Component store 복원

`raw dense memory dump`를 일반 규칙으로 사용하지 않는다.

- sparse/dense/data의 순서 자체는 보존한다.
- trivially copyable type만 raw payload를 허용한다.
- `vector/deque/string/pointer/runtime handle`은 deep clone 또는 explicit codec을 사용한다.
- 모든 active store는 `CopyClone / Codec / ExternalImmutableRef / Excluded` 중 하나로 분류한다.
- 분류되지 않은 active store가 있으면 checkpoint 생성을 실패시킨다.
- Client renderer pointer가 든 world는 authoritative checkpoint 대상이 아니다.

`TransformComponent`와 `NavAgentComponent`에는 vector가 있고 일부 gameplay component에는 deque/raw pointer가 있으므로 단순 `memcpy`는 금지한다.

### 7-4. ECS 밖의 participant

최소 포함 대상:

- tick, RNG, EntityManager slot/free-list/generation
- EntityIdMap과 next net ID
- server minion wave runtime
- turret activation accumulator
- lag compensation history
- pending gameplay commands
- practice mode/spawn list/overlay revision
- action/event broadcast cache
- match/room gameplay state
- definition/tuning build hash와 revision

Socket session, wall clock, profiler UI, GPU/resource cache는 복원하지 않는다.

### 7-5. Timeline epoch

과거 tick로 돌아간 뒤 기존 tick/action/event 번호를 그대로 쓰면 Client dedupe와 prediction이 새 branch를 과거 중복으로 오인한다.

```text
TimelineReset
  timelineEpoch
  restoredTick
  branchId
  fullSnapshotFollows
```

Client rebase:

- interpolation buffer clear
- local prediction clear
- EventApplier dedupe clear 또는 epoch key 전환
- transient projectile/FX clear
- replicated entity full reconciliation
- animation을 authoritative action age로 seek
- damage number/kill feed의 branch 정책 적용

Network command ack는 절대 뒤로 돌리지 않는다. Simulation journal sequence와 transport ack를 분리한다.

### 7-6. Branch와 Replay

- Faithful Replay: 원래 data/tuning revision으로 재현.
- Counterfactual Branch: checkpoint 뒤 새 patch를 AI phase 전에 적용하고 bot command를 재생성.
- ControlPlane과 ObservationOnly command는 re-sim에서 제외.
- branch가 tick을 뒤로 이동하면 기존 WRPL stream에 무표식 append하지 않는다. 새 branch recorder 또는 epoch/branch record를 사용한다.
- WRPL v1 read compatibility를 유지하고 v2+는 command domain을 검증한다.

## 8. AI Decision Laboratory

사람형 AI를 깎는 루프는 두 단계다.

### 8-1. Read-only Re-score

같은 Perception을 새 weight로 다시 계산한다. World를 돌리지 않으므로 빠르게 식과 가중치의 영향을 확인한다.

### 8-2. Counterfactual Re-sim

같은 checkpoint에서 새 policy/tuning revision을 적용하고 실제 GameCommand, 전투 결과, 이후 상태까지 다시 실행한다.

Trace evidence:

```text
factTick / timelineEpoch / branchId
observationHash / policyRevision / tuningRevision
Perception fields + visibility confidence
candidate list
candidate hard gates / reject reason
raw feature
weight
feature * weight contribution
risk / opportunity cost
final score and rank
commitment before/after
chosen intent / combo / atomic command
executor accepted/rejected
state hash after tick
```

필수 UX:

- 선택한 bot만 상세 trace subscribe
- break on state/intent/action/reject/damage/death
- A/B branch 열 비교와 score contribution diff
- slider drag 중 매 frame 전송하지 않고 edit 종료 때 transaction 한 건 전송
- per-parameter override와 whole-profile replace 분리
- JSONL/CSV export, camera bookmark, deterministic seed 표시
- trace row에서 source formula/parameter로 이동

현재 AIDebugControl은 Practice와 동일한 host/capability gate로 이동해야 하며, dead issuer 때문에 tool command가 막히면 안 된다.

## 9. Designer Hub

```text
Top Bar
  Workspace / Save / Undo / Redo / Validate / Apply / Revert
  Pause / Step / TimeScale / Checkpoint / Rewind / Branch
  Current target / tick / epoch / branch / data hash / revision

Left
  Content Browser / Hierarchy / Scenario Library

Center
  Live Viewport or Isolated Preview Viewport

Right
  Inspector / Properties / Bindings / Validation

Bottom
  Timeline / Curves / AI Trace / Console / Profiler / Diff
```

F7/F8/F9/F10은 기존 단축키로 남기되 같은 panel registry에 등록한다. `Scene_InGameImGui.cpp`의 수기 bool/call 목록을 계속 확장하지 않는다.

공통으로 당연히 있어야 할 기능:

- property unit/range/default/reset/tooltip
- search/filter/favorites/recent
- multi-select/copy/paste/preset
- drag transaction과 Ctrl+Z/Y
- diff, validation, click-to-focus error
- autosave/crash recovery/last-good
- camera bookmark, lighting/background/team/color-blind preset
- input recording과 repeat macro
- capture, A/B overlay, profiler budget
- dependency graph, missing/stale/cook status
- import queue progress/log/cancel
- designer capability/watermark/audit log

## 10. 도메인별 Tool

### 10-1. Balance Lab

- target selector: self, net entity, champion archetype, team, room
- stats/cost/cooldown/range/timing/movement/status/summon/item
- formula metadata와 unit/range
- draft profile, atomic server apply, inverse patch
- canonical export diff

### 10-2. Scenario Composer

- champion/minion/dummy/jungle/boss spawn
- team/position/facing/HP/level/item/cooldown/AI/phase
- navmesh validation
- baseline checkpoint
- Save/Load/ResetScenario/RestartRoom
- match end 뒤 같은 연결에서 baseline 복귀

### 10-3. FX Studio

S012의 단일 WFX owner를 따른다. 두 번째 FX runtime/editor를 만들지 않는다.

- raw asset thumbnail/RGBA channel/role/hash
- reference capture browser와 opacity overlay
- emitter lifetime timeline, solo/mute
- size/RGBA/rotation/velocity/material scalar curve
- PreviewClock scrub/loop/slow motion
- edit debounce 뒤 active preview respawn + age restore
- bounds/overdraw/segment/cache miss budget
- deterministic save와 real hot reload

Full node graph는 WFX curve/module stack으로 표현할 수 없는 실제 세 작품이 확인된 뒤 진행한다.

### 10-4. Animation Studio

- exact clip ID/name/list/duration
- play/pause/step/scrub/reverse/speed/loop
- EvaluateAtTime pose
- crossfade/blend/root motion/event marker
- cast/impact/recovery와 FX/sound marker 공유
- 30/60/144/300 FPS phase 비교

### 10-5. Model Inspector

- submesh name/material index/hash/Solo/All/None
- skeleton/bone/socket hierarchy
- AABB/LOD/normal/tangent/UV view
- attachment preview
- per-instance material override

공유 cached `CModel`의 texture를 preview 한 인스턴스 때문에 직접 바꾸지 않는다. Generation handle과 per-instance material state를 사용한다.

### 10-6. UI Studio

기존 `WINTERS_UI_PIPELINE_ARCHITECTURE`를 owner로 삼는다.

- canvas drag/resize, hierarchy, anchor/pivot/z-order
- resolution/aspect/DPI/safe area preset
- atlas/font/localization/binding/hit-test debug
- style/layout/rule 분리
- atomic save, undo, last-good reload
- 동일 runtime `CUIRoot/widget` path

### 10-7. Map/World Studio

- placement hierarchy, gizmo, snap, multi-select, prefab
- navgrid/walkability/vision/bush/structure/spawn validation
- document transaction과 atomic save
- visual prop edit와 authoritative map reset/reload 분리
- active match를 몰래 변조하지 않고 explicit room restart/full resync

### 10-8. Boss Pattern Lab

공용 shell/transaction/trace UX만 재사용하고 LoL ChampionAI와 Elden boss gameplay type을 합치지 않는다.

- Blackboard/HFSM/BT/phase graph
- telegraph/attack window/cooldown/transition condition
- candidate/gate/score trace
- target and arena scenario
- phase breakpoint, one-step, checkpoint branch
- server command-only mutation

## 11. Asset Import와 Hot Reload

```text
Source Asset
-> external converter job
-> validation
-> deterministic cook/DDC
-> generation-tagged resource
-> frame-boundary swap
-> old generation retirement
```

- render frame에서 raw FBX import 금지.
- stable UTF-8 AssetId와 OS wide path 분리.
- source/cooked/runtime path 분리.
- cache lock 안에서 긴 IO/GPU 생성 최소화.
- 기존 raw pointer consumer가 dangling되지 않도록 generation handle을 사용.
- reload 실패 시 last-good generation 유지.

## 12. Scratch에서 Canonical로

```text
Session Overlay
-> Designer Preset
-> Review Diff
-> Canonical JSON/Sheet
-> validate
-> cook
-> SimLab
-> Server/Client build
-> normal F5/LAN smoke
```

Runtime scratch overlay가 canonical source를 자동 덮어쓰면 안 된다. 화면에는 `Scratch / Applied / Saved Preset / Canonical / Stale Build` 상태를 항상 표시한다.

## 13. 보안과 빌드 모드

- `--practice-server` 또는 명시적인 Designer build capability.
- host 또는 granted role만 mutation 가능.
- 일반 Release에서 조용히 무시하지 말고 명시적 reject result.
- quota, clamp, schema validation, path allowlist.
- watermark, session/transaction/target/tick/revision audit.
- normal F5 renderer/roster/map/network path를 숨기지 않는다.

## 14. 구현 순서

### Gate 0. S014 정확성

- WRPL v1 reader 호환
- Pause 중 tool mutation journal/replay snapshot 보존
- Practice disable 시 pause/scale lifecycle 복구
- authoritative pause/scale/result 표시
- AIDebug host capability와 issuer life 분리
- AI single-param override가 전체 profile을 덮지 않게 수정
- server Pause와 Client presentation clock의 의미 일치

### Gate 1. 공통 제작 기반

- DesignerHub panel registry
- ToolDocument transaction/undo/redo
- AtomicFileWriter/autosave/recovery
- ToolOperationResult + revision
- PreviewClock

### Gate 2. 30% ceiling 산출물

두 packet 안에 다음 두 결과를 실제 화면으로 만든다.

1. AI: 한 tick Perception을 고정하고 weight A/B re-score를 비교해 JSONL로 저장.
2. VFX: Annie Q 하나를 timeline/curve/scrub/undo로 수정하고 normal F5에서 같은 WFX로 재생.

이 두 산출물 전에는 full node graph, 150 champion 일괄 이관, 대규모 GPU particle rewrite를 시작하지 않는다.

### Gate 3. Model/Animation Preview

- inspector API, EvaluateAtTime, isolated preview scene, safe material instance.

### Gate 4. Checkpoint Foundation

- component store classification/clone/codec
- EntityManager/EntityIdMap/non-ECS participants
- complete state hash
- save/restore/continue SimLab golden probe

### Gate 5. Chrono Branch

- rolling keyframe ring
- journal domain filter
- restore/re-sim
- epoch/full resync
- AI counterfactual branch

### Gate 6. UI/Map/Scenario/Boss

- 공통 transaction/preview/result contract 위에서 제품별 tool을 연결.

## 15. 수용 기준

다음이 모두 증명되어야 “한 번의 빌드 안에서 제작한다”고 말할 수 있다.

- save 실패에도 기존 파일이 보존된다.
- Undo 한 번이 사용자 gesture 한 번과 대응한다.
- server rejected patch가 Applied로 표시되지 않는다.
- Pause/Step에서 server tick, animation, FX, AI trace의 시간 관계를 설명할 수 있다.
- 같은 checkpoint+seed+data hash에서 restore 후 complete state hash가 같다.
- branch에서 바꾼 patch 이외의 입력은 동일하다.
- hot reload가 다른 live model instance를 오염시키지 않는다.
- preview와 normal F5가 같은 runtime path를 사용한다.
- LAN client가 동일 authoritative revision/epoch/state를 본다.
- canonical promotion은 validate/cook/SimLab/build를 통과한다.

## 16. 금지

- Network Snapshot을 full simulation checkpoint로 오인하지 않는다.
- non-trivial component를 raw `memcpy`하지 않는다.
- Client ImGui가 gameplay component를 직접 바꾸지 않는다.
- Pause/Step/TimeScale을 re-sim input으로 다시 실행하지 않는다.
- document Undo와 Chrono rewind를 한 stack에 넣지 않는다.
- shared cached model/material을 preview instance가 직접 변조하지 않는다.
- 두 번째 FX renderer/cache/editor를 병렬로 만들지 않는다.
- 툴을 만들었다는 이유로 실제 스킬/AI/asset 산출물 검증을 미루지 않는다.

