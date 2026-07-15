# Engine DNA / Tool Portfolio Strategy - Winters x Unreal x Unity

작성: 2026-07-10 KST

핵심 전략:

```text
엔진 이름을 외우는 문서를 만드는 것이 아니다.
Engine DNA를 읽고, 본질/최적화/검증을 분리한 뒤,
Winters, Unreal, Unity에서 같은 문제를 각 엔진의 방식으로 해결하는 포트폴리오를 만든다.
```

이 문서는 기존 Winters 계획서 양식이 아니라 NYPC 문서에서 쓰던 방식으로 작성한다.
즉 `문제 DNA -> 본질 -> decoy 함정 -> 검증 -> 천장 -> 산출물` 순서로 본다.

대상:

```text
Winters Engine
Unreal Engine
Unity Engine
Engine Tool Portfolio
Asset Store / Marketplace
Optimization
VFX / Animation
Client / Server / Engine / Tool / GameSim / Data Backend / Docker
```

---

## 0. 세 줄 교훈

> **게임 엔진의 본질은 기능 묶음이 아니라, 시간 안에서 데이터를 검증 가능한 runtime 결과로 바꾸는 체계다.**

세 엔진을 비교할 때 가장 큰 decoy는 기능 이름이다.

```text
Nanite, Niagara, Blueprint, Addressables, Package Manager, WFX, GameSim...
```

이 이름들을 많이 아는 것은 좋지만, 포트폴리오에서 이기는 것은 이름 암기가 아니다.
진짜 질문은 다음이다.

```text
그 기능은 어떤 데이터를 받아서,
누가 진실을 소유하고,
어떤 runtime contract로 바뀌며,
어떻게 검증되고,
어떤 예산 안에서 돌아가는가?
```

Winters의 강점은 이 과정을 직접 구현해 보여줄 수 있다는 점이다.
Unreal과 Unity의 강점은 같은 본질을 상용 엔진의 editor/tool 생태계로 보여줄 수 있다는 점이다.

---

## 1. Engine DNA Card

엔진을 받으면 먼저 아래 카드를 채운다.
NYPC에서 문제를 받자마자 Game DNA를 채웠던 것과 같다.

| 항목 | 질문 | Winters | Unreal | Unity |
|---|---|---|---|---|
| 시간 | frame, tick, fixed step, async job은 어디서 통제하는가? | `GameInstance`, Server tick, GameSim tick, Job/Fiber 방향 | Engine loop, Tick, TaskGraph | PlayerLoop, Update/FixedUpdate, Job System |
| 권위 | gameplay truth는 어디에 있는가? | Server + `Shared/GameSim` | Server/Gameplay Framework/GAS/Replication | 프로젝트별, Netcode/custom server 필요 |
| 데이터 | authoring data와 runtime data가 분리되는가? | JSON/authoring -> generated pack, `.w*` cooked asset | `.uasset`, DataAsset, DataTable, cook, DDC | ScriptableObject, `.meta`, Addressables, AssetBundle |
| 렌더 | GPU 명령은 어떤 추상화로 나가는가? | DX11/DX12 RHI 방향 | RHI/RenderCore/RDG | SRP/URP/HDRP, native 내부 |
| 에셋 | source asset을 누가 runtime resource로 바꾸는가? | `WintersAssetConverter`, WMesh/WMat loader | Importer, Cooker, Asset Manager, DDC | AssetDatabase, Importer, Addressables |
| 객체 | world object는 어떻게 표현되는가? | Engine ECS, Client scene, GameSim component 분리 | Actor/Component/UObject | GameObject/Component/Prefab |
| 네트워크 | wire contract가 명확한가? | FlatBuffers Command/Snapshot/Event | RPC/Replication/Iris 방향 | Netcode/custom serialization |
| 툴 | 사람이 데이터를 만들 수 있는가? | ImGui tuner, WFX panel, future editor | Editor modules, Content Browser, Details Panel | EditorWindow, UI Toolkit, Inspector |
| 최적화 | 측정 가능한가? | Profiler/Tracy/JSON scope 방향 | Unreal Insights, stat, GPU tools | Unity Profiler, Frame Debugger |
| 배포 | runtime content와 backend가 업데이트 가능한가? | Services/docker-compose, pack/cook 필요 | cook/chunk/pak/IoStore, OnlineSubsystem | Packages, Addressables, Remote Config |

이 표의 목적은 "누가 더 좋다"가 아니다.
각 엔진이 같은 본질을 어떤 방식으로 구현하는지 분해하는 것이다.

---

## 2. 엔진 분해 템플릿

새 엔진 기능이나 툴을 설계할 때 아래 순서로 채운다.

```text
## <도메인 이름>

### A. 규칙 & 상태
- 입력 데이터:
- 내부 상태:
- runtime 출력:
- 소유권:
- 실패 조건:

### B. 본질
- 본질 1:
- 본질 2:
- 한 줄 요약:
- decoy가 아닌 근거:

### C. 최적화
- 줄여야 하는 work:
- 캐시 가능한 것:
- async 가능한 것:
- runtime hot path에서 금지할 것:

### D. 검증
- correctness oracle:
- performance budget:
- regression check:
- sample/demo:

### E. 산출물
- tool:
- document:
- capture:
- report:
```

이 템플릿은 Winters, Unreal, Unity에 모두 적용된다.

---

## 3. Decoy 함정 7종

NYPC에서 `즉시 점수`, `얕은 heuristic`, `wall-clock noise`가 decoy였듯이, 엔진 포트폴리오에도 decoy가 있다.

### 3.1 기능 이름 decoy

```text
Niagara를 안다
Addressables를 안다
RHI를 안다
```

이건 출발점이다.
검증 질문은 이쪽이다.

```text
Niagara effect budget을 어떻게 제한했는가?
Addressables dependency 문제를 어떻게 잡았는가?
RHI resource lifetime과 render snapshot을 어떻게 분리했는가?
```

### 3.2 UI만 있는 툴 decoy

EditorWindow나 ImGui 패널이 있다고 툴이 완성된 것이 아니다.
진짜 툴은 입력을 검증하고, 결과를 runtime contract로 바꾸고, 실패를 보여준다.

### 3.3 asset import decoy

FBX를 열었다고 asset pipeline이 아니다.

진짜 asset pipeline:

```text
source asset
-> import metadata
-> deterministic cook
-> validation
-> dependency registry
-> runtime loader/cache
-> hot reload or safe fallback
```

### 3.4 client visual decoy

Client에서 멋진 FX를 틀었다고 multiplayer gameplay가 완성된 것이 아니다.
Winters 기준으로는 Server/GameSim cue가 있고 Client가 presentation으로 재생해야 한다.

### 3.5 최적화 decoy

frame이 좋아졌다고 최적화가 아니다.
정상 runtime 기능을 끄고 얻은 수치는 lab result다.
최적화는 같은 시나리오, 같은 capture, 같은 counter에서 비교해야 한다.

### 3.6 툴 개수 decoy

Unreal 10개, Unity 10개, Winters 10개라는 숫자는 좋다.
하지만 "30개 만들었다"는 말보다 강한 것은 "같은 본질 문제 10개를 세 엔진에서 풀었다"는 말이다.

### 3.7 마켓/스토어 decoy

Asset Store나 Fab는 파일을 파는 곳처럼 보이지만, 본질은 versioned package와 update channel이다.
문서, sample, validation, dependency, license, migration이 빠지면 상품이 아니라 파일 묶음이다.

---

## 4. Row 1 - Runtime / RHI / Render DNA

### Row DNA

엔진의 가장 아래층은 화면을 예쁘게 그리는 코드가 아니라, frame 안에서 CPU/GPU 작업을 예측 가능하게 제출하는 구조다.

공통 질문:

```text
frame boundary가 어디인가?
render snapshot은 누가 만드는가?
GPU resource lifetime은 누가 소유하는가?
draw call은 어디서 batch/cull/sort되는가?
backend 교체가 가능한가?
```

### Winters

Winters는 직접 RHI와 renderer 방향을 쌓고 있다.
`Engine/Private/RHI`, DX11/DX12 backend, `Engine/Public/Core/Profiler`, render snapshot 방향이 포트폴리오 핵심이다.

본질:

```text
Client/GameSim truth를 Renderer가 직접 읽지 않는다.
Client가 presentation snapshot을 만들고,
Engine renderer는 generic draw contract를 GPU command로 바꾼다.
```

천장:

```text
DX11 유지
DX12 parity
backend-neutral RHI
render pass graph
GPU/CPU profiler capture
```

검증:

```text
동일 scene을 DX11/DX12에서 render parity
render scope별 ms 측정
draw call, material bind, texture residency counter
normal F5 scene에서 기능 숨기지 않고 capture
```

### Unreal

Unreal은 RHI, RenderCore, RDG가 이 DNA를 상용 엔진급으로 구현한다.
사용자는 low-level RHI를 직접 쓰기보다 render feature, material, plugin, profiling tool을 통해 접근한다.

본질:

```text
AAA renderer를 직접 재발명하지 않고,
Editor/cook/RDG/profiler가 묶인 render production pipeline을 쓴다.
```

### Unity

Unity는 SRP, URP, HDRP로 renderer 선택지를 제공한다.
RHI 자체보다 render pipeline asset, shader graph, renderer feature, profiler를 다루는 방식이 중요하다.

본질:

```text
작은 팀이 C#과 package로 renderer workflow를 빠르게 조정한다.
```

---

## 5. Row 2 - Asset / Data / Marketplace DNA

### Row DNA

게임 엔진의 asset pipeline은 파일 읽기가 아니라 "사람의 작업물을 runtime binary와 stable id로 바꾸는 체계"다.

표준 흐름:

```text
Source Asset
  FBX / glTF / PNG / WAV / JSON / XLSX
Import Metadata
  scale / axis / compression / skeleton / material slots / license
Cooked Asset
  runtime binary
Runtime Resource
  GPU buffer / texture / animation / model / effect instance
```

### Winters

현재 증거:

```text
Tools/WintersAssetConverter
Engine/Public/AssetFormat/Mesh/WMeshLoader.h
Engine/Public/AssetFormat/Mesh/WMeshWriter.h
Engine/Public/AssetFormat/Material/*
Client/Bin/Resource
```

본질:

```text
FBX를 runtime이 읽는 것이 아니다.
WintersAssetConverter가 source를 .wmesh/.wmat/.wskel/.wanim으로 cook하고,
runtime은 검증된 binary만 빠르게 읽는다.
```

다음 천장:

```text
Asset Catalog
Dependency Graph
Import Queue
Validation Report
Hot Reload with safe fallback
Virtual Path
License/Source manifest
```

### Unreal

대응 도메인:

```text
Content Browser
Asset Registry
Asset Manager
Primary Asset
Cook
Chunk
Pak / IoStore
DDC
Data Validation
```

Unreal Marketplace/Fab의 본질:

```text
uasset/source asset/plugin을 versioned package로 배포하고,
engine version compatibility와 cook validation을 보장한다.
```

### Unity

대응 도메인:

```text
AssetDatabase
.meta GUID
AssetImporter / ScriptedImporter
ScriptableObject
Package Manager
UPM package
Addressables
AssetBundles
Asset Store package
```

Unity Asset Store의 본질:

```text
파일 묶음이 아니라,
package metadata, version, docs, samples, dependencies, migration을 갖춘 배포 단위다.
```

### 검증

Asset pipeline의 검증은 다음을 봐야 한다.

```text
같은 source + 같은 import setting = 같은 cooked output
missing dependency 0
runtime loader roundtrip pass
invalid asset은 cook 전에 fail
package update 후 기존 scene/reference 유지
```

---

## 6. Row 3 - GameSim / Server Authority / Network DNA

### Row DNA

멀티플레이 게임의 본질은 "누가 진실을 결정하는가"다.

Winters 기준 정답:

```text
Client Input
-> GameCommand
-> Server GameSim
-> Snapshot / Event / FX cue
-> Client Visual
```

### Winters

현재 증거:

```text
Shared/GameSim/Definitions
Shared/GameSim/Systems
Shared/GameSim/Components
Shared/Schemas/Command.fbs
Shared/Schemas/Snapshot.fbs
Shared/Schemas/Event.fbs
Server/Public/Game/SnapshotBuilder.h
Server/Public/Game/CommandIngress.h
Server/Public/Game/ReplicationEmitter.h
```

본질:

```text
Client는 결과를 만들지 않는다.
Client는 의도를 command로 보낸다.
Server/GameSim이 결과를 만든다.
Snapshot/Event가 presentation을 깨운다.
```

Decoy:

```text
Client에서 HP를 줄이면 빨리 보인다.
Client animation frame에서 damage를 주면 구현이 쉬워 보인다.
FX를 local에서 바로 틀면 즉시 멋져 보인다.
```

실제 본질:

```text
서버 cue와 actionSeq가 있어야 중복 재생, desync, cheat, replay 문제가 닫힌다.
```

### Unreal

대응 도메인:

```text
Gameplay Framework
Actor replication
Dedicated Server
Gameplay Ability System
Gameplay Tags
GameplayCue
Iris / Replication Graph 방향
```

Unreal은 기본적으로 network/gameplay framework가 강하다.
하지만 project별로 server authority 원칙을 명확히 하지 않으면 Blueprint나 local prediction에 truth가 섞일 수 있다.

### Unity

대응 도메인:

```text
Netcode for GameObjects
Netcode for Entities
ScriptableObject data
custom headless server
Addressables content sync
```

Unity는 생산성이 강하지만, 서버 권위 구조는 프로젝트가 직접 설계해야 한다.
이 점을 이해하고 custom server/GameSim을 설계하면 포트폴리오에서 강점이 된다.

### 검증

```text
Command stream replay
Snapshot viewer
Event timeline
server/client state diff
late join or reconnect smoke
invalid command reject counter
same replay -> same result
```

---

## 7. Row 4 - Editor / Tooling DNA

### Row DNA

Editor의 본질은 "사람이 만든 의도를 runtime contract로 변환하는 도구"다.
예쁜 panel은 결과가 아니라 입구다.

필수 기능:

```text
Catalog
Search
Inspect
Edit
Validate
Preview
Cook
Reload
Undo / Redo
Report
```

### Winters

현재 증거:

```text
Engine/Public/Editor/ImGuiLayer.h
Client/Public/Scene/Scene_Editor.h
Client/Public/UI/WfxEffectToolPanel.h
Client/Public/UI/EffectTuner.h
Client/Public/UI/ChampionTuner.h
Client/Public/UI/MapTunerPanel.h
```

본질:

```text
처음에는 ImGui tuner로 충분하다.
하지만 portfolio ceiling은 Content Browser + Asset Import Queue + Validator + Preview + runtime reload다.
```

### Unreal

대응 도메인:

```text
Editor module
Content Browser extension
Details Panel customization
Blueprint tooling
Data Validation
Commandlet
Unreal Insights automation
```

### Unity

대응 도메인:

```text
EditorWindow
UI Toolkit
Custom Inspector
PropertyDrawer
AssetPostprocessor
ScriptedImporter
Package Manager
```

### 검증

Editor tool은 아래 질문을 통과해야 한다.

```text
잘못된 입력을 잡는가?
source와 cooked result를 연결해서 보여주는가?
변경 후 runtime에서 같은 결과가 나오는가?
undo/redo 또는 safe fallback이 있는가?
CI/command line에서도 검증 가능한가?
```

---

## 8. Row 5 - Backend / Docker / LiveOps DNA

### Row DNA

Backend는 engine runtime이 아니다.
runtime은 ms 단위 tick을 지키고, backend는 persistent state와 운영을 맡는다.

### Winters

현재 증거:

```text
Services/docker-compose.yml
Services/cmd/auth
Services/cmd/shop
Services/cmd/matchmaking
Services/cmd/profile
Services/cmd/payment
Services/cmd/leaderboard
Services/internal/*
Services/migrations
```

본질:

```text
GameServer는 전투의 truth를 가진다.
Services는 계정, 매치, 상점, 결제, 프로필, 랭킹, telemetry, liveops를 가진다.
Docker는 같은 backend 환경을 재현하게 한다.
```

Decoy:

```text
REST endpoint 몇 개를 만들었다 = backend 완성
```

실제 본질:

```text
schema migration
auth token flow
game server verification
observability
load test
live data patch
rollback
```

### Unreal / Unity 대응

Unreal:

```text
OnlineSubsystem
EOS / Steam / platform service
backend plugin
dedicated server fleet
```

Unity:

```text
Unity Gaming Services
Authentication
Cloud Save
Remote Config
Economy
Lobby / Relay / Matchmaker
custom backend
```

### 검증

```text
docker-compose up
auth login smoke
matchmaking request smoke
game server token verify
shop/inventory read
migration apply/rollback
p95 latency
structured logs
```

---

## 9. Row 6 - Optimization DNA

### Row DNA

최적화는 빠르게 만드는 것이 아니라, 어떤 work를 줄였는지 증명하는 것이다.

기본 루프:

```text
budget 정의
capture
top cost 확인
원인 분류
하나만 변경
같은 capture로 비교
regression guard
```

### 공통 work 분류

| 분류 | 질문 |
---|---|
| Count | 너무 많이 생성/업데이트/렌더하는가? |
| Bandwidth | CPU/GPU/Network/IO 전송량이 큰가? |
| Sync | CPU/GPU 또는 thread wait가 있는가? |
| Cache | asset/resource/cache miss가 많은가? |
| Allocation | frame 중 heap allocation이 있는가? |
| Overdraw | transparent/pass/material 비용이 큰가? |
| Serialization | command/snapshot/data encode 비용이 큰가? |

### Unreal 최적화 도구

```text
Unreal Insights
stat unit / stat gpu
GPU Visualizer
Niagara Debugger
Animation Budget Allocator
Data Validation
```

### Unity 최적화 도구

```text
Unity Profiler
Frame Debugger
Memory Profiler
Rendering Debugger
ProfilerRecorder
Build Report
```

### Winters 최적화 도구

```text
Profiler JSON
ProfilerOverlay
Tracy integration
GameSim system counters
FX particle counters
network packet counters
RHI render pass counters
```

### Decoy

```text
normal F5에서 roster/map/minion/snapshot/UI/FX를 숨겨서 fps가 오른다.
```

이건 최적화가 아니다.
명시적인 lab path라면 쓸 수 있지만, portfolio 숫자는 정상 runtime capture에서 보여줘야 한다.

---

## 10. Worked Example 1 - VFX / Niagara / WFX / VFX Graph

### A. 규칙 & 상태

입력:

```text
effect definition
emitter graph
texture atlas
material
spawn event
position / orientation / target
```

출력:

```text
particle instances
draw items
GPU buffers
audio/camera/UI cue
```

### B. 본질

VFX의 본질은 "많은 작은 시각 요소를 정해진 예산 안에서 설득력 있게 보이게 하는 것"이다.

한 줄:

```text
spawn count, update count, overdraw, bounds, material cost를 통제해야 한다.
```

### C. 엔진별 대응

Unreal:

```text
Niagara System
Emitter
Effect Type
Scalability
Significance
Niagara Debugger
Unreal Insights
```

Unity:

```text
Visual Effect Graph
Particle System
VFX bounds
Profiler
Frame Debugger
Addressables dependency
```

Winters:

```text
.wfx
FxGraph
FxGraphValidator
WfxEffectToolPanel
ParticlePool
EffectTuner
server FX cue -> client playback
```

### D. 검증

```text
max particle count
spawn rate
bounds 존재 여부
texture/material dependency
transparent overdraw risk
effect instance count
pool miss count
server cue 중복 재생 0
```

### E. 포트폴리오 산출물

```text
Unreal Niagara Budget Dashboard
Unity VFX Graph Budget Reporter
Winters WFX Budget Profiler
```

---

## 11. Worked Example 2 - Animation Optimization

### A. 규칙 & 상태

입력:

```text
skeleton
animation clip
state machine
montage / sequence
IK / rig constraints
server action state
```

출력:

```text
pose
skinned mesh matrices
notify/cue
FX/audio timing
```

### B. 본질

Animation 최적화의 본질은 "플레이어가 눈치채지 못하는 pose update를 줄이는 것"이다.

비용:

```text
bone count
skinned vertex count
clip sampling
blend tree
IK
notify processing
visibility
update frequency
```

### C. 엔진별 대응

Unreal:

```text
Skeletal Mesh LOD
Update Rate Optimization
Animation Budget Allocator
Animation Insights
Control Rig cost
Anim Blueprint profiling
```

Unity:

```text
Animator culling mode
Optimize Game Objects
Animation compression
LODGroup
Animation Rigging
Profiler Timeline
```

Winters:

```text
.wskel
.wanim
skeleton hash
ReplicatedPoseComponent
ReplicatedActionComponent
actionSeq
client visual playback
```

### D. 검증

```text
same actionSeq -> same montage/pose cue
hidden/out-of-range actor update rate 감소
high-bone asset validator
clip channel count report
notify duplicate 0
client visual only, server truth 불변
```

---

## 12. Worked Example 3 - Asset Store / Marketplace

### A. 규칙 & 상태

입력:

```text
package files
metadata
version
license
sample
documentation
dependencies
```

출력:

```text
installed project content
imported/cooked asset
sample scene
tool menu
update path
```

### B. 본질

Asset Store의 본질은 "파일 판매"가 아니라 "신뢰 가능한 업데이트 단위"다.

### C. 엔진별 대응

Unity:

```text
UPM package
package.json
asmdef
Samples~
Documentation~
Asset Store publisher workflow
Package Manager My Assets
.meta GUID stability
```

Unreal:

```text
Plugin
Content folder
uplugin
sample map
engine version compatibility
Data Validation
cook test
Fab/Marketplace metadata
```

Winters:

```text
staging manifest
source asset
license metadata
WintersAssetConverter
.w* cooked asset
Asset Catalog
runtime smoke
```

### D. 검증

```text
fresh install
sample open
missing reference 0
version update migration pass
cook/build pass
license/source manifest present
```

---

## 13. Portfolio Tool Matrix

숫자는 이렇게 본다.

```text
나쁜 해석:
Unreal 10개 + Unity 10개 + Winters 10개 = 30개라서 완성

좋은 해석:
엔진 본질 문제 10개를 잡고,
각 문제를 Unreal, Unity, Winters 방식으로 증명한다.
```

### Theme A. Asset Pipeline

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Asset Dependency Auditor | Asset Registry, hard reference, unused asset, cook risk |
| Unity | Addressables Catalog Auditor | address 중복, bundle size, remote catalog, dependency |
| Winters | Asset Catalog + Import Queue | source -> `.w*` cook -> validation -> runtime load |

### Theme B. VFX Budget

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Niagara Budget Dashboard | Effect Type, spawn, bounds, scalability |
| Unity | VFX Graph Budget Reporter | particle/bounds/material dependency |
| Winters | WFX Budget Profiler | `.wfx` graph, ParticlePool, server cue dedupe |

### Theme C. Animation Budget

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Animation Budget Visualizer | Skeletal LOD, tick rate, Anim Budget Allocator |
| Unity | Animator Complexity Analyzer | state/transition/blend tree/clip/avatar mismatch |
| Winters | Animation Notify/Budget Tool | `.wanim`, skeleton hash, actionSeq, notify/cue |

### Theme D. Runtime Truth

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | GAS/GameplayTag Checker | Ability/Effect/Cue/Tag consistency |
| Unity | ScriptableObject Data Validator | gameplay data schema/reference/range |
| Winters | GameSim Command/Repro Inspector | command stream, deterministic replay, server truth |

### Theme E. Build / Cook / Release

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Cook/Chunk Visualizer | Primary Asset, chunk, package size |
| Unity | Asset Store Package Validator | package.json, samples, docs, asmdef, changelog |
| Winters | Services/Docker LiveOps Dashboard | auth/match/shop/profile smoke, live patch preview |

### Theme F. Profiling

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Insights Capture Assistant | trace preset, capture compare, report |
| Unity | Build/Profiler Report Dashboard | CPU/GPU/memory/build size summary |
| Winters | Profiler Timeline | GameSim, render, FX, network counters |

### Theme G. World / Streaming

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | World Partition Cell Auditor | actor count, HLOD, Data Layer, streaming risk |
| Unity | Scene/Addressable Streaming Auditor | scene split, addressable group, residency |
| Winters | World Cell / AssetStreaming Inspector | cell load, resource residency, runtime path |

### Theme H. UI / Editor Workflow

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Details Panel QC Tool | property metadata, invalid reference, edit condition |
| Unity | UI Toolkit Tool Shell | EditorWindow, custom inspector, batch action |
| Winters | ImGui Tool Shell Upgrade | docked panels, transaction, output log |

### Theme I. Replay / QA

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Sequencer Shot QC or Replay Tagger | unbound actor, camera/audio sync |
| Unity | PlayMode Replay Harness | deterministic input playback |
| Winters | Snapshot/Event Viewer | FlatBuffers timeline, visual diff |

### Theme J. Dependency Boundary

| 엔진 | 툴 | 증명 |
|---|---|---|
| Unreal | Build Module Dependency Viewer | Runtime/Editor leak, circular dependency |
| Unity | Assembly Definition Dependency Viewer | asmdef cycle, Editor/runtime leak |
| Winters | Shared/GameSim Boundary Checker | Engine/Client/UI/DX include 차단 |

---

## 14. 30개 툴 완성 판정

### 완성이라고 보기 어려운 상태

```text
툴 이름 30개가 있다.
각각 작은 UI만 있다.
샘플 프로젝트가 없다.
전후 수치가 없다.
왜 이 툴이 엔진 본질과 연결되는지 설명이 없다.
```

### 포트폴리오 완성에 가까운 상태

```text
10개 Theme가 있다.
각 Theme마다 Unreal/Unity/Winters 버전이 있다.
각 툴은 하나 이상의 sample asset/project로 검증된다.
각 툴은 correctness check와 performance/report output이 있다.
README가 problem -> engine concept -> implementation -> validation -> demo 순서다.
Winters 툴은 실제 코드베이스 경계와 연결된다.
```

### 한 줄 판정

```text
Unreal 10개, Unity 10개, Winters 10개는 충분히 강한 포트폴리오 축이다.
단, 개수로 완성되는 것이 아니라 "같은 Engine DNA 문제를 세 엔진에서 풀었다"는 증거로 완성된다.
```

---

## 15. 우선순위

### Phase 0. 문서와 샘플 기준선

```text
공통 sample asset 1개
공통 VFX sample 1개
공통 animation sample 1개
공통 gameplay data sample 1개
공통 profiling scenario 1개
```

검증:

```text
세 엔진에서 같은 문제를 비교할 수 있는 최소 샘플이 있다.
```

### Phase 1. Winters 3개 먼저

```text
Winters Asset Catalog / Import Queue
Winters WFX Budget Profiler
Winters GameSim Command/Repro Inspector
```

이유:

```text
Winters의 asset, presentation, server truth를 한 번에 보여준다.
이 세 개가 있으면 자체 엔진 이해도가 강하게 드러난다.
```

### Phase 2. Unreal/Unity 대응 2개씩

Unreal:

```text
Asset Dependency Auditor
Niagara Budget Dashboard
```

Unity:

```text
Addressables Catalog Auditor
Asset Store Package Validator
```

이유:

```text
상용 엔진 tool 제작 경험과 Winters 설계가 바로 비교된다.
```

### Phase 3. 최적화와 배포

```text
Animation Budget
Profiler Timeline
Cook/Package Report
Docker LiveOps Dashboard
Dependency Boundary Checker
```

이유:

```text
이 단계부터는 단순 툴 제작자가 아니라 engine/tools/backend 경계까지 이해하는 사람으로 보인다.
```

---

## 16. 검증 프로토콜

각 툴은 아래 체크를 통과해야 한다.

```text
1. Fresh project에서 실행된다.
2. 잘못된 sample을 넣으면 실패를 보여준다.
3. 정상 sample을 넣으면 report를 만든다.
4. report에 숫자가 있다.
5. 같은 sample을 다시 돌리면 같은 결과가 나온다.
6. 전후 비교가 가능하다.
7. 문서가 problem -> DNA -> implementation -> validation 순서다.
```

Winters 툴 추가 체크:

```text
Shared/GameSim 권위 경계를 깨지 않는다.
Client visual을 gameplay truth로 만들지 않는다.
runtime source asset 직접 로딩을 늘리지 않는다.
normal F5 runtime을 숨기지 않는다.
Profiler/counter가 남는다.
```

Unreal 툴 추가 체크:

```text
Runtime module과 Editor module을 분리한다.
Content Browser/Asset Registry/Validation과 연결된다.
cook 또는 PIE smoke가 있다.
engine version compatibility를 적는다.
```

Unity 툴 추가 체크:

```text
Editor/runtime asmdef를 분리한다.
package.json 또는 UPM 구조를 고려한다.
.meta GUID/reference 안정성을 깨지 않는다.
sample scene과 docs가 있다.
```

---

## 17. 최종 기억 문장

```text
엔진 포트폴리오는 기능 이름으로 이기는 것이 아니다.

Winters는 본질을 직접 구현해 증명한다.
Unreal은 AAA editor/cook/runtime 생태계로 증명한다.
Unity는 C# editor/package/workflow 생태계로 증명한다.

Asset은 파일이 아니라 contract다.
Editor는 UI가 아니라 변환기다.
GameSim은 코드가 아니라 truth owner다.
Optimization은 감이 아니라 측정이다.
Marketplace는 zip이 아니라 update channel이다.

30개 툴의 목표는 숫자가 아니다.
10개의 Engine DNA 문제를 세 엔진에서 각각 풀 수 있다는 증거다.
```

---

## 18. 참고한 기준점

Winters local 기준:

```text
AGENTS.md
.claude/gotchas.md
.md/architecture/WINTERS_CODEBASE_COMPASS.md
.md/문서/12_Ch12_Editor.md
.md/문서/13_Ch13_Tooling.md
.md/문서/14_Ch14_Services.md
.md/문서/15_Ch15_Data_Pipeline.md
.md/문서/16_Ch16_Collaboration.md
.md/문서/17_Ch17_Editor_AssetImporter_AssetLoader.md
.md/plan/2026-07-10_ENGINE_ESSENCE_AND_PORTFOLIO_DIRECTION.md
```

NYPC format 기준:

```text
C:/Users/user/Desktop/NYPC/mushroom/docs/nypc_2026_competition_ai_lab_game_dna_strategy.md
C:/Users/user/Desktop/NYPC/mushroom/docs/nypc_2026_essence_optimization_verification_protocol.md
C:/Users/user/Desktop/NYPC/mushroom/docs/winters_ai_learning_ml/session_00_problem_dna.md
C:/Users/user/Desktop/NYPC/mushroom/docs/winters_ai_learning_ml/session_01_rules_engine.md
```

공식 개념 참고:

```text
Unreal Content Browser / Asset Manager / UBT / UHT / DDC / Data Validation / Unreal Insights / Niagara / Animation Budget Allocator
Unity AssetDatabase / ScriptedImporter / Package Manager / Addressables / Asset Store package / Profiler / VFX Graph / Animation Rigging
```

