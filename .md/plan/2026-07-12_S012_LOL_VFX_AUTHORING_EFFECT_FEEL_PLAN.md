Session - S012는 LoL식 이펙트의 본질을 단순한 PNG 수나 RGBA 값이 아니라 `게임플레이 의미 -> 동일 시간축의 레이어/커브 -> 서버 cue 위상 -> 명료성/성능 검증`으로 고정하고, 현재 Winters의 WFX를 디자이너가 반복 제작할 수 있는 단일 저작 파이프라인으로 성장시키는 세션이다. 이 문서는 현재 코드와 실제 자산을 감사한 결과에 근거한 설계 결정서다. 이번 세션에서는 FX 런타임 코드를 수정하지 않으며, 구현은 아래 세로 슬라이스와 검증 게이트 순서로 진행한다.

## 1. 반영해야 하는 코드

### 1-1. 먼저 고정할 결론

사용자의 관찰은 방향상 맞지만, 다음과 같이 정확히 나누어야 한다.

```text
PNG / DDS / mesh
  = 실루엣, 마스크, 노이즈, 램프, 표면 형태를 제공하는 재료

flipbook / atlas
  = 손으로 그린 내부 변형을 시간 순서로 보여 주는 한 가지 방법

emitter + curve + trail + shader + phase alignment
  = 재료를 살아 움직이는 스킬로 만드는 레시피

PNG 압축
  = 저장/전송 형식이며 부드러움을 만드는 애니메이션 기법이 아님
```

따라서 하나의 좋은 효과가 많은 PNG로 만들어질 수는 있지만, 많은 PNG 자체가 품질의 원인은 아니다. 소수의 mask/noise/ramp 텍스처를 재사용하면서 크기, 알파, 색, 회전, 속도, UV, erode, trail을 시간에 따라 조절해도 풍부한 결과를 만들 수 있다. 반대로 고해상도 PNG를 수십 장 겹쳐도 모든 값이 선형이고 판정/애니메이션과 위상이 어긋나면 무겁고 흐릿한 결과만 남는다.

이펙트 감각의 북극성은 다음 식으로 고정한다.

```text
Effect Feel
= Gameplay Meaning
  x Temporal Envelope
  x Spatial Silhouette
  x Material Motion
  x Layer Hierarchy
  x Animation / Sound / Camera Phase Alignment
  x Gameplay Clarity
  x Frame-Rate Invariance
```

하나라도 0에 가까우면 나머지 레이어 수를 늘려도 완성도가 올라가지 않는다.

### 1-2. 실제 Riot 공개 자료에서 확인되는 것과 추론을 분리한다

Riot이 공개한 자료에서 확인되는 사실은 다음과 같다.

- League의 효과는 하나 이상의 particle system과 emitter로 구성되고, texture, geometry, 여러 제어값으로 동작한다.
- 기존 툴의 큰 병목은 UI 외형보다 undo 부재, 저장/재실행 대기, live preview 부재, 자산 의존성 추적 부재였다.
- Particle Town은 익숙한 편집 경험을 유지하면서 viewer, undo, 실시간 반영, 구형 데이터 변환을 우선했다. 화려한 graph editor는 MVP 우선순위에서 밀렸다.
- Riot의 아티스트는 keyframe 값을 통해 particle 속성의 시간 변화를 만들었고, 런타임은 curve 평가 비용을 최적화했다.
- VFX는 크게 만드는 것보다 만족감과 전투 명료성을 동시에 지키는 절제가 중요하다.
- particle authoring 단계에서 비용을 보여 주지 않으면 보기에는 거의 같은 trail도 심각한 성능 회귀를 만들 수 있다.

공식 근거:

- https://nexus.leagueoflegends.com/en-us/2017/07/tech-kayn-goes-to-particle-town/
- https://www.riotgames.com/en/news/cleaning-data-debt-league
- https://www.riotgames.com/en/news/random-acts-optimization
- https://www.riotgames.com/en/news/profiling-real-world-performance-league
- https://www.riotgames.com/en/artedu/visual-effects
- https://www.riotgames.com/en/news/trip-down-lol-graphics-pipeline

현재 Riot 아트팀이 정확히 어떤 최신 툴 UI와 프레임워크를 쓰는지는 공개 자료만으로 확정할 수 없다. 공개된 과거 자료에는 Particle Town과 Qt 기반 editor가 나오지만, `ImGui를 쓴다/쓰지 않는다`를 현재 사실로 단정할 근거는 없다.

Winters에서 중요한 결론은 UI 프레임워크가 아니다.

- ImGui는 빠른 인게임 툴, dock, 디버그 시각화, 즉시 반영 MVP에 충분하다.
- 현재 한계는 ImGui라서가 아니라 문서 transaction, undo/redo, timeline, curve, asset role, cook, profiling이 없기 때문이다.
- 동일 기능을 Qt나 별도 에디터로 다시 만들면 두 번째 FX 편집기와 두 번째 데이터 소유자가 생긴다.
- 공식 축은 기존 `CWfxEffectToolPanel + .wfx + CFxCuePlayer` 하나로 유지한다.

### 1-3. 현재 Winters 코드가 실제로 가진 것

| 영역 | 코드 근거 | 현재 판정 |
|---|---|---|
| WFX runtime contract | `Engine/Public/FX/FxAsset.h:80` | Billboard/Ribbon/Beam/Decal/Mesh/Ring, material, atlas, trail scalar가 있음 |
| WFX inspector | `Client/Private/UI/WfxEffectToolPanel.cpp:540` | transform/RGBA/lifetime/atlas/trail을 직접 조절 가능 |
| emitter 목록 | `Client/Private/UI/WfxEffectToolPanel.cpp:663` | start delay와 lifetime을 표로 보지만 timeline/curve는 없음 |
| 실제 preview | `Client/Private/UI/WfxEffectToolPanel.cpp:772` | 현재 게임 world에서 `CFxCuePlayer`의 실제 runtime path 사용 |
| texture catalog | `Client/Private/UI/WfxAssetCatalog.cpp:274` | 이름이 `particles`인 폴더의 PNG만 text table로 수집 |
| 문서 | `Client/Public/GameObject/FX/WfxDocument.h:9` | load/save/set asset뿐이며 dirty/undo/redo/transaction 없음 |
| cue spawn | `Client/Private/GameObject/FX/FxCuePlayer.cpp:420` | WFX emitter 하나당 ECS render entity 하나 생성 |
| server cue tick | `Client/Private/Network/Client/EventApplier.cpp:822` | `startTick`은 dedupe key에만 쓰고 effect 초기 age에는 쓰지 않음 |
| loading | `Client/Private/Scene/Loader.cpp:180` | WFX JSON directory만 등록하고 texture/mesh dependency 전체 warm-up은 하지 않음 |
| graph scaffold | `Engine/Public/FX/Graph/FxGraph.h:17` | Spawn/Init/Update/Render와 curve key 타입은 존재 |
| graph compiler | `Engine/Private/FX/Exec/FxExecPlan.cpp:156` | `InitPosition`, `SizeOverLife`, `ColorOverLife` 등은 실질 실행이 비어 있음 |
| particle pool | `Engine/Public/FX/Exec/FxParticlePool.h:9` | SoA 골격은 있으나 LoL WFX runtime에 연결되지 않음 |

현재 데이터 실측은 다음과 같다.

| 항목 | 수치 |
|---|---:|
| `.wfx` | 112 |
| 전체 emitter | 452 |
| JSON parse 실패 | 0 |
| `start_delay > 0` | 210 |
| Billboard / Decal / Mesh | 259 / 86 / 72 |
| Ring / Beam / Ribbon | 18 / 9 / 8 |
| atlas animation | 2 |
| history trail | 4 |
| `spawn_rate > 0` | 0 |
| `max_particles > 1` | 0 |
| graph/nodes가 저장된 WFX | 0 |

즉 현재 WFX는 이름상 particle schema를 갖지만 실질적으로는 `emitter당 단일 sprite/mesh layer sequencer`다. 현재 품질은 452개 레이어 중 210개의 시작 시간차로 주로 만들어진다. 이 구조는 버릴 대상이 아니라, 실제로 쓰이는 단일 레이어 runtime에 curve와 공통 시간축을 먼저 붙여야 하는 출발점이다.

### 1-4. 사용자가 모은 이펙트 이미지와 실제 raw 자산을 분리한다

`Client/Bin/Resource/Texture/UI/이펙트 이미지`를 직접 집계한 결과는 PNG 235장이다. 대부분 서로 다른 해상도의 인게임 크롭이며 맵, 챔피언, UI가 포함된 완전 불투명 화면이다. 이것은 particle sprite가 아니다.

역할을 다음과 같이 고정한다.

| 자산군 | 역할 | 런타임 직접 사용 |
|---|---|---|
| `Texture/UI/이펙트 이미지/**/*.png` | `ReferenceCapture`: 색, 실루엣, 프레임별 크기, 타이밍 비교 | 금지 |
| `Texture/Character/<Champion>/particles/**/*.png|dds` | raw Color/Mask/Ramp/Noise/Flipbook 후보 | metadata 승인 후 허용 |
| `Texture/FX/**/*` | 공통/추출 raw FX 후보 | 중복/역할 검사 후 허용 |
| `particles/render/*.png` | mesh preview 또는 캡처 이미지 | 기본적으로 diffuse 연결 금지 |
| `.scb/.fbx` | mesh source/intermediate | cook 후 사용 |
| `.wmesh/.wmat` | Winters runtime mesh/material | 허용 |
| `.wfx` | 디자이너 authoring source | validate/cook 후 runtime 사용 |

실제 raw 후보는 충분히 많다.

- Character FX roots: 6,748개, 425.95 MiB
- `Texture/FX`: 1,248개, 142.51 MiB
- Object FX roots: 470개, 20.33 MiB
- 주요 raw PNG: 2,878개
- trail/beam/ribbon 이름 후보 PNG: 248개
- Character raw는 있으나 WFX가 없는 챔피언: Fiora, Garen, Kalista, MasterYi

하지만 “모든 원재료가 있다”와 “원본 효과를 자동 복구할 수 있다”는 같은 말이 아니다. texture/mesh는 많지만 Riot의 원 emitter recipe, material graph, curve, spawn timing, attachment, per-skin override는 완전히 보존되어 있지 않다. 캡처 235장은 이 빠진 레시피를 역설계하는 golden reference로 사용한다.

또한 다음 데이터 부채가 있다.

- `Texture/FX` 1,248개 중 1,186개가 Character 아래 파일과 hash가 같은 중복이다.
- source/intermediate/runtime인 TEX/DDS/PNG/FBX/WMESH가 같은 runtime tree에 섞여 있다.
- MAP 추출 particle corpus는 약 65,067개, 2.56 GiB이므로 일반 Content Browser가 기본 스캔하면 안 된다.
- 현재 ASCII 기반 path 변환은 `이펙트 이미지` 같은 경로를 안정적으로 보존하지 못한다.
- runtime `Client/Bin/Resource`에는 WFX가 없고, `CFxCuePlayer`가 workspace의 `Data/LoL/FX`를 직접 찾는다.

대규모 파일 이동부터 하지 않는다. 먼저 allowlist manifest와 role tagging을 추가하고, 검증 가능한 cook이 생긴 뒤 source/intermediate/runtime를 분리한다.

### 1-5. LoL식 이펙트가 부드럽게 느껴지는 시간 구조

모든 스킬은 하나의 긴 fade가 아니라 의미가 다른 phase들의 합으로 본다.

```text
Intent / Anticipation
  -> Release
  -> Travel or Sustain
  -> Impact
  -> Dissipation
```

| phase | 플레이어가 읽어야 하는 것 | 주로 조절할 값 |
|---|---|---|
| Intent | 누가, 어디에, 무엇을 시전하는가 | ground shape, gather motion, alpha in, source attachment |
| Release | 입력이 실제 행동으로 바뀐 한 순간 | 짧은 밝기/크기 peak, mesh snap, sound transient, camera impulse |
| Travel | 방향, 속도, 위험 범위 | body silhouette, ribbon continuity, UV flow, head/tail hierarchy |
| Impact | 판정이 발생한 위치와 강도 | core flash, outward ring, target response, debris, hit-stop 계열 연출 |
| Dissipation | 판정 종료와 잔상 | alpha/size curve, slow drift, smoke/fragment separation |

핵심은 phase 경계가 서버의 cast/impact tick, 캐릭터 애니메이션 event, sound transient와 같은 시간축을 공유하는 것이다. 늦게 받은 cue가 항상 frame 0부터 시작하면 레이어를 아무리 정교하게 만들어도 LAN 환경에서 애니메이션과 효과가 분리되어 보인다.

필수 curve는 처음부터 모든 속성을 노드화하지 않고 다음 다섯 개로 제한한다.

- size
- RGBA 또는 color + alpha
- rotation
- velocity multiplier
- material scalar: 우선 erode/UV phase 중 실제 대표 효과에 필요한 하나

curve 입력은 `normalized emitter age`이며, authoring에서는 keyframe을 보존하고 runtime cook에서는 compact key 또는 LUT로 변환한다. 하나의 key인 상수 값은 scalar fast path를 유지한다.

### 1-6. 디자이너 친화적 WFX Tool의 본질

최종 workspace는 다음과 같다.

```text
Top: Save / Undo / Redo / Validate / Cook / Hot Reload / Capture

Left A: Raw Asset Browser
  thumbnail, alpha checker, RGBA channel, role, dimensions, hash, dependency

Left B: Capture Reference Browser
  champion/skill/phase/frame, storyboard, overlay opacity

Center: Runtime Preview Viewport
  same CFxCuePlayer path, fixed camera/seed, target dummy, light/dark ground

Bottom: Emitter Timeline + Curve Editor
  start/lifetime bars, play/pause/scrub/loop/slow motion, event markers

Right: System / Emitter / Module Stack + Details
  Spawn / Init / Update / Render, material and attachment properties

Overlay: Budget / Bounds / Overdraw / Phase Error
```

우선순위는 다음과 같다.

1. undo/redo와 dirty state
2. fixed time scrub과 emitter timeline
3. curve editor
4. raw/reference 이중 browser와 thumbnail/channel view
5. dependency validation과 real hot reload
6. profiling/bounds/overdraw 표시
7. 반복 작업을 줄이는 emitter template/module stack
8. full node graph는 실제 세 작품에서 stack/curve로 표현이 불가능한 문제가 확인된 뒤 진행

graph를 먼저 만들지 않는 이유는 현재 runtime이 graph를 소비하지 않기 때문이다. 그래프 UI만 추가하면 저장되는 placebo 데이터가 늘어난다. 먼저 WFX v2 curve를 같은 runtime path에서 재생하고, 필요성이 검증된 module만 graph compiler에 연결한다.

### 1-7. 단일 데이터/런타임 구조

두 번째 renderer, 두 번째 cache, 두 번째 editor를 만들지 않는다.

```text
Raw Source + Capture Reference
  -> deterministic scan / classify / hash
  -> source manifest
  -> WFX authoring JSON v2
  -> schema/dependency/budget validation
  -> curve compile + dependency cook
  -> .wtex / .wmesh / .wmat / future .wfxbin
  -> Client/Bin/Resource/FX runtime manifest

Client Input
  -> GameCommand
  -> Server GameSim
  -> EffectTrigger(startTick, source, target, pos, dir, duration, seed)
  -> Client EventApplier computes authoritative initial age
  -> CFxCuePlayer
  -> existing FxSystem / FxMeshSystem / FxBeamSystem
```

소유 경계:

- Shared/Server는 hit, damage, status, cast/impact tick, cue identity만 소유한다.
- Client는 initial age, interpolation, FX/animation/sound/camera presentation을 소유한다.
- Engine은 generic curve/LUT, asset role/manifest primitive, render/runtime primitive를 소유한다.
- Tool은 authoring document, undo/redo, thumbnail, timeline, validation을 소유한다.
- gameplay truth에 texture path, color, particle count를 넣지 않는다.

WFX v2에 필요한 논리 데이터:

- stable system/emitter ID: 이름 변경에도 timeline selection과 diff 유지
- authored keyframe curve와 interpolation mode
- optional deterministic seed
- explicit asset role과 stable dependency ID
- authoring-only reference link
- budget metadata: max instances, estimated segments, overdraw class
- cooked dependency list

기존 scalar WFX v1은 key 하나인 curve로 자동 승격하고 계속 읽을 수 있어야 한다. v2 저장 후 v1을 다시 열 수 있을 필요는 없지만, v1 파일을 일괄 파괴적으로 재작성하지 않는다.

### 1-8. 실제 디자이너 작업 루프

목표 UX는 “수치를 많이 노출하는 것”이 아니라 아이디어를 짧은 폐루프로 검증하는 것이다.

1. Champion/Skill/Phase template을 고른다.
2. Raw Browser에서 mask/noise/trail/mesh를 thumbnail과 채널로 확인한다.
3. Capture Reference의 동일 phase를 storyboard로 연다.
4. emitter template을 추가하고 runtime viewport에서 즉시 본다.
5. timeline에서 레이어 시작/종료를 맞춘다.
6. curve에서 anticipation, peak, settle을 조절한다.
7. target 방향, 거리, 밝고 어두운 배경, red/blue team을 한 번에 순회한다.
8. server delay preset 0/50/100/200ms로 phase alignment를 본다.
9. budget/overdraw warning을 확인하고 save한다.
10. hot reload된 normal F5에서 같은 cue를 재생하고 golden capture를 남긴다.

저장 파일은 diff 친화적이어야 한다.

- stable ID와 field order 고정
- float precision 정책 고정
- transient viewport/camera state는 별도 user settings에 저장
- 한 emitter 수정이 전체 JSON 재정렬로 보이지 않도록 writer 고정
- validation error는 파일, cue, emitter, field, dependency를 함께 표시

### 1-9. 협업 역할

| 역할 | 툴에서 하는 일 | 하지 않는 일 |
|---|---|---|
| 기획/디자인 | timing, size, color, readability, team/quality variant 조절 | C++ rebuild, gameplay 판정 변경 |
| VFX 아트 | raw 역할 지정, emitter/curve/material 조합, golden capture 승인 | server damage/cooldown 변경 |
| Gameplay 개발 | authoritative cue와 phase tick 방출 | client texture/curve 하드코딩 |
| Engine/Rendering | curve runtime, shader slot, batching, budget counter | champion별 look 수치 소유 |
| Tools/TA | import/cook, document transaction, preview, validator | 별도 FX runtime 제작 |
| QA | fixed scenario/camera/tick capture와 성능 회귀 확인 | 감각만으로 합격 처리 |

### 1-10. 구현 순서와 30% ceiling

#### Phase A - Annie Q: 기존 WFX를 살아 움직이게 만들기

가장 작은 유효 세로 슬라이스다.

- WFX v2 curve 5종 중 Annie Q에 필요한 최소 curve 적용
- emitter timeline, play/pause/scrub/loop, undo/redo
- Annie Q 캡처 reference strip과 opacity overlay
- `EffectTrigger.startTick` 기반 initial age/late cue fast-forward
- texture/mesh dependency warm-up
- normal F5와 tool preview가 같은 `CFxCuePlayer` 경로 사용

이번 단계에서는 GPU particle, SpawnRate population, full graph, collision, 새 renderer를 추가하지 않는다.

#### Phase B - Kalista R: raw에서 새 효과를 만드는 파이프라인 검증

Kalista는 raw 자산이 있지만 WFX champion folder가 없다. 따라서 새 제작 흐름 검증에 적합하다.

- 계약 대상 흡수/숨김/보유/발사/충돌 phase를 각각 cue 또는 emitter group으로 분리
- raw role manifest와 thumbnail browser 사용
- attachment/target segment/launch direction 검증
- gameplay의 계약/이동/에어본 truth는 기존 Shared/Server 구현과 분리
- capture reference가 있으면 비교하고, 없으면 Winters golden baseline을 새로 생성

#### Phase C - Yasuo Q3: 복합 이동/impact 명료성 검증

- projectile head/body/tail과 impact ring을 timeline에서 동기화
- trail sampling을 30/60/144/300 FPS에서 동일하게 유지
- Q3 airborne 판정 tick과 impact peak를 한 render frame 이내로 맞춤
- red/blue team, fog/배경 명료성 검증

#### 30% ceiling gate

Annie Q, Kalista R, Yasuo Q3 세 작품을 normal F5에서 side-by-side capture로 완성하기 전에는 다음 대규모 작업에 진입하지 않는다.

- full Niagara형 node graph
- GPU-driven particle rewrite
- 150 챔피언 일괄 migration
- MAP 2.56 GiB 전체 자동 import
- 새 material system을 기존 것 옆에 추가

툴 개발은 작품 생산으로 환전되어야 한다. 각 Phase는 최소 하나의 실제 스킬 before/after capture, WFX 파일, profiler JSON, QA checklist를 산출해야 완료다.

### 1-11. 구현 파일 경계

첫 구현 세션에서 수정할 기존 경계는 다음으로 제한한다.

| 파일 | 의도 |
|---|---|
| `Engine/Public/FX/FxAsset.h` | backward-compatible curve/runtime metadata contract |
| `Engine/Private/FX/FxAsset.cpp` | v1/v2 parse와 validation |
| `Client/Public/GameObject/FX/WfxDocument.h` | dirty, transaction, undo/redo 경계 |
| `Client/Private/GameObject/FX/WfxDocument.cpp` | deterministic v2 writer/round-trip |
| `Client/Private/UI/WfxEffectToolPanel.cpp` | timeline/curve/reference preview UI |
| `Client/Private/UI/WfxAssetCatalog.cpp` | RawAsset/ReferenceCapture role 분리와 UTF-8-safe identity |
| `Client/Public/GameObject/FX/FxCuePlayer.h` | initial age를 포함한 presentation context |
| `Client/Private/GameObject/FX/FxCuePlayer.cpp` | existing renderer component에 initial age 전달 |
| `Client/Private/Network/Client/EventApplier.cpp` | server tick에서 cue age 계산, dedupe 경계 유지 |
| `Client/Private/Scene/Loader.cpp` | WFX dependency warm-up과 first-use cache miss 제거 |

정확한 신규 utility 파일명과 전체 본문은 Phase A 구현 직전 code-preview plan에서 고정한다. 현재 `Engine/Public/FX/ParticlePool.h`와 `Engine/Public/FX/Exec/FxParticlePool.h`처럼 중복될 수 있는 owner가 있어, generic curve를 새 파일로 만들지 기존 cinematic curve에서 분리할지는 `CONFIRM_NEEDED`다. 이 결정을 하지 않은 채 세 번째 curve/particle runtime을 추가하지 않는다.

## 2. 검증

### 2-1. 이번 S012 문서 검증

- S010 reader 실험으로 만든 PDF/HTML/script/CSS 변경이 남아 있지 않아야 한다.
- S010 원본 Markdown은 수정하지 않는다.
- S012의 자산 수치는 workspace 재집계 결과와 일치해야 한다.
- 이번 세션에서는 runtime C++/HLSL/WFX를 수정하지 않는다.

재집계 기준:

```powershell
Get-ChildItem Data/LoL/FX -Recurse -File -Filter *.wfx
Get-ChildItem 'Client/Bin/Resource/Texture/UI/이펙트 이미지' -Recurse -File -Filter *.png
Get-ChildItem Client/Bin/Resource/Texture/Character -Recurse -File
```

### 2-2. Phase A 데이터 게이트

- WFX v1 112개가 모두 계속 load되어야 한다.
- v1 -> v2 -> save -> reload 후 scalar/curve 평가값이 동일해야 한다.
- stable emitter ID가 rename/reorder 후에도 유지되어야 한다.
- Korean UTF-8 path와 ASCII stable ID를 혼동하지 않아야 한다.
- `ReferenceCapture`를 runtime texture로 저장하면 validator가 실패해야 한다.
- missing dependency와 `particles/render` 역할 오용을 cue/emitter 단위로 보고해야 한다.
- 동일 입력을 두 번 cook한 output hash가 같아야 한다.

### 2-3. 시간/FPS 불변성

고정 seed와 같은 authoritative age에서 다음 FPS의 결과를 비교한다.

- 30 FPS
- 60 FPS
- 144 FPS
- 300 FPS

고정 sample time:

- 0ms
- 50ms
- 100ms
- 150ms
- 250ms
- effect 종료 직전

비교 값:

- size
- RGBA
- rotation
- position/velocity
- atlas frame
- ribbon point count/spacing
- alive/dead state

성공 조건은 FPS가 바뀌어도 같은 age의 결과가 허용 오차 안에서 같고, trail이 속도나 frame 간격 때문에 과밀/희박해지지 않는 것이다.

### 2-4. 네트워크/위상 검증

인위적 cue 지연을 0/50/100/200ms로 주입한다.

- 늦은 cue가 frame 0부터 다시 시작하지 않아야 한다.
- cast release, projectile 출발, impact peak와 server tick 오차가 한 render frame 이내여야 한다.
- local prediction과 authoritative cue가 중복 spawn되지 않아야 한다.
- replay/LAN 재접속에서도 같은 start phase가 재현되어야 한다.
- gameplay damage/hit/status 결과는 FX 유무와 무관해야 한다.

추가 profiler counter:

- `Fx::ActiveInstances`
- `Fx::CacheMiss`
- `Fx::FirstUseLoadMs`
- `Fx::CurveEvalUs`
- `Fx::CuePhaseErrorMs`

### 2-5. 시각/디자이너 UX 검증

Annie Q 기준으로 다음 작업을 C++ 수정 없이 수행할 수 있어야 한다.

1. reference 3장을 선택한다.
2. raw texture의 alpha/R/G/B 채널을 본다.
3. emitter를 solo/mute한다.
4. 0.25배속과 scrub으로 원하는 frame을 찾는다.
5. size/alpha/color curve key를 옮긴다.
6. undo/redo한다.
7. 밝은/어두운 ground와 target 방향을 바꾼다.
8. save/hot reload 후 normal F5에서 동일 결과를 본다.

합격 조건:

- save/reload 대기 없이 preview가 갱신된다.
- reference overlay opacity를 0~100%로 조절할 수 있다.
- emitter별 lifetime bar와 현재 playhead가 보인다.
- 과도한 bounds, segment, overdraw, texture 역할 오류가 시각 경고로 보인다.
- tool에서 만든 결과와 normal F5 결과가 같은 runtime path를 사용한다.

### 2-6. 성능 게이트

300 FPS 연구 목표의 전체 frame budget은 3.33ms다. normal F5의 roster/map/minion/UI/network/FX를 숨기지 않고 측정한다.

Phase A 임시 FX ceiling:

- FX CPU simulation + submission p95: 0.50ms 이하
- FX GPU p95: 0.75ms 이하
- loading 종료 후 Annie Q 첫 사용 cache miss: 0
- 첫 사용 동기 texture/mesh load hitch: 0ms
- curve 도입 후 기존 Annie Q draw call 증가: 0

기존 counter와 함께 확인한다.

- `Fx::Drawn`
- `Fx::CullSkipped`
- `FxMesh::Drawn`
- `FxBeam::Segments`
- `FxBeam::BudgetSkipped`

목표를 넘기면 레이어를 무작정 줄이기 전에 overdraw, trail segment 밀도, texture cache miss, curve common path, draw batching 순서로 원인을 분리한다.

### 2-7. 빌드와 인게임 인계

구현 세션의 기본 빌드:

```powershell
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

인게임 최종 확인 순서:

1. normal F5로 서버/클라이언트 권위 경로를 실행한다.
2. Annie Q를 근거리/최대거리/움직이는 대상에 각각 사용한다.
3. 0/50/100/200ms cue delay preset을 순회한다.
4. 30/60/144/300 FPS cap을 순회한다.
5. reference overlay와 effect-only capture를 저장한다.
6. profiler JSON에서 frame, first-use load, curve, segment counter를 확인한다.

S012의 완료 기준은 툴 창이 커지는 것이 아니라, Annie Q 하나가 캡처 기준으로 명백히 개선되고 같은 WFX가 tool, normal F5, LAN/replay에서 같은 phase와 비용으로 재생되는 것이다.
