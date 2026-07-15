Session - 게임 엔진의 본질, Winters/Unreal/Unity 도메인 대응, 툴 포트폴리오 방향

작성일: 2026-07-10

목표: Winters Engine을 기준점으로 삼아 Unreal Engine, Unity Engine의 특징을 비교하고, 게임 엔진의 가장 밑바닥 원리부터 client/server/engine/tool/gamesim/data backend/docker까지 위로 쌓이는 구조를 설명한다. 마지막에는 Unreal 툴 10개, Unity 툴 10개, Winters 툴 10개를 만드는 포트폴리오 전략이 충분한지 검토한다.

## 0. 결론 먼저

게임 엔진의 본질은 "시간 안에서 데이터를 안전하게 변환하는 시스템"이다.

```text
Input / Network / Data
  -> authoritative state transition
  -> resource lookup and streaming
  -> animation / FX / audio / UI presentation
  -> GPU command submission
  -> measured frame result
```

더 줄이면 다음 네 가지다.

1. 시간: 프레임, tick, fixed step, async job, latency를 통제한다.
2. 데이터: source asset, authored data, cooked runtime data, replicated data의 소유권을 분리한다.
3. 권위: 어떤 계층이 진실을 결정하는지 정한다. Winters에서는 `Shared/GameSim`과 Server가 gameplay truth다.
4. 변환: 사람이 만든 파일과 의도를 기계가 빠르게 읽는 binary, command, snapshot, draw call로 바꾼다.

Unreal은 이 본질을 AAA 통합 생태계로 만든 엔진이다. Unity는 이 본질을 C# 생산성과 패키지 생태계로 낮은 진입 비용에 제공하는 엔진이다. Winters는 이 본질을 직접 구현하며, 특히 "서버 권위 MOBA/GameSim + 자체 RHI/asset/tool/backend"를 포트폴리오의 중심 증거로 만들 수 있는 엔진이다.

툴 10개씩, 총 30개를 만드는 전략은 포트폴리오로 강하다. 다만 "숫자"만으로 완성은 아니다. 완성 기준은 30개의 목록이 아니라, 각 툴이 다음을 증명하는가다.

- 실제 문제를 해결한다.
- 엔진 도메인 경계를 이해한다.
- import/cook/runtime/validation/profiling 중 하나 이상의 본질을 다룬다.
- 샘플 프로젝트, 스크린샷/영상, 전후 성능 수치, 테스트, 문서가 있다.
- Unreal/Unity 툴 경험이 Winters 설계로 귀결된다.

권장 방향은 "30개 독립 미니툴"보다 "3개 엔진을 관통하는 10개 주제"다. 예를 들어 Asset Validator를 Unreal/Unity/Winters 버전으로 만들고, 같은 문제를 세 엔진에서 어떻게 해결했는지 보여주면 훨씬 선명하다.

## 1. 현재 Winters 코드 증거

이 문서는 추상론이 아니라 현재 Winters 구조 위에 쌓는다.

- Engine core: `Engine/Public`, `Engine/Private`, `Engine/Include`
- RHI/Renderer 방향: `Engine/Private/RHI`, `Engine/Public/Core/Profiler`, `Engine/Public/Renderer` 계열 문서
- ECS/World: `Engine/Public/ECS`, `Engine/Private/ECS`
- World/streaming: `Engine/Public/World/WorldPartitionSystem.h`, `Engine/Public/World/AssetStreamingSystem.h`
- Asset format/cook: `Engine/Public/AssetFormat/Mesh/WMeshLoader.h`, `WMeshWriter.h`, `Engine/Public/AssetFormat/Material/*`, `Tools/WintersAssetConverter`
- FX/WFX: `Engine/Private/FX/Graph`, `Client/Public/UI/WfxEffectToolPanel.h`, `Client/Public/UI/WfxAssetCatalog.h`
- Cinematic: `Engine/Public/Cinematic/CSequenceAsset.h`, `CSequencePlayer.h`
- Editor/tuner: `Engine/Public/Editor/ImGuiLayer.h`, `Client/Public/Scene/Scene_Editor.h`, `Client/Public/UI/*Tuner*`
- GameSim: `Shared/GameSim/Definitions`, `Shared/GameSim/Systems`, `Shared/GameSim/Components`
- Server authority: `Server/Public/Game/CommandIngress.h`, `SnapshotBuilder.h`, `ReplicationEmitter.h`
- Network schema: `Shared/Schemas/Command.fbs`, `Snapshot.fbs`, `Event.fbs`
- Backend: `Services/docker-compose.yml`, `Services/internal/*`, `Services/cmd/*`, `Services/migrations`
- Profiling base: `Engine/Public/Core/Profiler/*`, `Engine/Public/Manager/Profiler/ProfilerOverlay.h`, `Engine/External/tracy`

Winters의 중요한 현재 원칙은 다음이다.

- `Shared/GameSim`은 Engine, Client, UI, Renderer, DX 타입을 몰라야 한다.
- Client는 입력, 약한 예측, 보간, animation/FX/audio/UI presentation을 담당한다.
- Server는 `GameCommand`를 받아 `GameSim`을 실행하고 snapshot/event/FX cue를 보낸다.
- Runtime은 source JSON/FBX를 매 프레임 읽는 곳이 아니다. 검증된 cooked pack과 binary asset을 읽는 곳이다.
- Tools/Editor는 runtime contract를 생산하고 검증하는 계층이지, gameplay truth를 임의로 만드는 계층이 아니다.

## 2. 세 엔진의 개별 특징

### 2.1 Winters Engine

Winters는 "직접 만든 엔진"이기 때문에, 상용 엔진처럼 모든 기능이 이미 완성되어 있다는 점이 강점이 아니다. 강점은 엔진의 원리를 직접 증명할 수 있다는 점이다.

주요 특징:

- C++ 기반 custom runtime.
- DX11/DX12 방향의 RHI, renderer, resource pipeline.
- `WintersEngine.dll` 위에 LoL/Elden 계열 client를 얹는 제품 분리 방향.
- server-authoritative GameSim 구조.
- FlatBuffers 기반 command/snapshot/event schema.
- `.wmesh`, `.wmat`, `.wskel`, `.wanim`, `.wfx`, `.wseq` 같은 Winters binary/cooked asset 방향.
- `Tools/WintersAssetConverter`를 통한 import/cook 파이프라인.
- ImGui 기반 editor/tuner와 WFX/Effect Tool 방향.
- Go 기반 Services와 docker-compose backend.
- 자체 profiler/Tracy 연동 흔적.

Winters의 본질적 포트폴리오 가치는 "엔진 사용 능력"이 아니라 "엔진을 구성하는 경계와 변환을 설계하는 능력"이다.

### 2.2 Unreal Engine

Unreal은 AAA 제작을 위한 통합형 엔진이다. 특징은 기능 하나하나보다 "Editor, Runtime, Build, Cook, Asset, Reflection, Tooling이 한 생태계로 묶여 있다"는 점이다.

핵심 특징:

- C++ source access와 강한 native runtime.
- UObject/reflection/GC/serialization/property system.
- UBT/UHT 기반 build와 code generation.
- Editor 중심 workflow: Content Browser, Details Panel, Blueprint, Niagara, Sequencer, Control Rig, World Partition.
- RHI/RenderCore/RDG 기반 multi-platform rendering.
- Lumen, Nanite, Virtual Shadow Maps 등 high-end renderer stack.
- Asset Manager, Primary Asset, cook/chunk, DDC 기반 asset deployment.
- Gameplay Framework와 GAS, Gameplay Tags, Replication, Dedicated Server.
- Unreal Insights, stat/gpu tools, Data Validation, commandlet 기반 대규모 팀 workflow.

Unreal의 본질은 "대규모 팀이 같은 asset truth와 module boundary 위에서 빠르게 제작하게 하는 Editor-first AAA platform"이다.

### 2.3 Unity Engine

Unity는 C# 생산성, 빠른 iteration, 패키지 생태계가 강점인 엔진이다. Unreal보다 runtime source를 깊게 바꾸는 방식은 약하지만, Editor 확장과 cross-platform 생산성은 매우 강하다.

핵심 특징:

- C# scripting, GameObject/Component/Prefab 중심 authoring.
- MonoBehaviour 기반 빠른 gameplay iteration.
- ScriptableObject로 데이터 자산을 쉽게 정의.
- AssetDatabase, `.meta` GUID, importer setting 기반 asset identity.
- Package Manager, UPM package, Asset Store 생태계.
- Addressables/AssetBundles 기반 content loading/update.
- URP/HDRP/SRP로 renderer stack 선택.
- Timeline, Animator, Animation Rigging, Visual Effect Graph.
- UI Toolkit/EditorWindow으로 Editor tool 제작이 쉽다.
- Unity Profiler, Frame Debugger, Memory Profiler 등 실용 profiling tool.

Unity의 본질은 "작은 팀과 툴 제작자가 빠르게 데이터를 만들고, C#으로 workflow를 확장하며, 패키지로 배포하는 creator platform"이다.

## 3. 게임 엔진의 밑바닥부터 위로 쌓기

### 3.1 0층: 하드웨어와 시간

가장 밑바닥에는 CPU, GPU, Memory, Disk, Network, OS가 있다. 엔진은 결국 이 자원을 제한된 시간 안에 나누는 시스템이다.

기본 단위:

- CPU time: gameplay, animation, physics, AI, resource loading, UI, network 처리.
- GPU time: draw, compute, post-process, particles, shadows, lighting.
- Memory: runtime object, asset cache, GPU resource, transient buffer.
- IO: asset streaming, patch, replay, save/load.
- Network: packet, jitter, loss, latency, bandwidth.

게임 엔진의 모든 고급 기능은 이 밑바닥 예산을 숨기지 않는다. 단지 사람이 다루기 좋은 툴과 추상화로 감싼다.

### 3.2 1층: Platform, Window, Input

엔진은 먼저 OS와 대화한다.

```text
Win32 / SDL / platform layer
  -> window
  -> input event
  -> time source
  -> file system
  -> thread/fiber/job primitive
```

Winters에서는 `Engine/Include`, `Engine/Public/Core`, `Engine/Public/Core/Fiber`, `Engine/Public/Core/JobSystem` 같은 영역이 이 층과 연결된다. Unreal은 Platform abstraction과 ApplicationCore 계열, Unity는 native engine 내부와 C# Input System으로 감싼다.

### 3.3 2층: Memory, Job, Profiler

큰 엔진은 기능보다 먼저 "측정 가능한 실행 기반"이 필요하다.

- allocator와 resource lifetime.
- main thread, render thread, worker thread.
- job system과 work stealing.
- fiber/async IO.
- CPU/GPU profiler marker.
- frame capture와 trace.

이 층이 약하면 최적화가 감이 된다. 이 층이 강하면 최적화가 실험이 된다.

Winters에는 `Engine/Public/Core/JobSystem.h`, `Engine/Public/Core/Fiber/*`, `Engine/Public/Core/Profiler/*`, `Engine/External/tracy`가 이 방향의 증거다.

### 3.4 3층: RHI와 GPU resource

Renderer보다 먼저 RHI가 있다.

```text
Texture / Buffer / Shader / Pipeline / CommandList / Fence / SwapChain
  -> backend DX11/DX12/Vulkan/Metal
```

RHI의 본질은 graphics API 차이를 지우는 것이 아니라, engine이 GPU 작업을 "언제 만들고, 언제 제출하고, 언제 해제할지"를 통제하게 하는 것이다.

Unreal은 RHI/RenderCore/RDG로 이 층을 아주 강하게 갖고 있다. Unity는 사용자가 직접 RHI를 만지기보다 SRP, Graphics API 설정, Native Plugin 일부로 접근한다. Winters는 DX11/DX12 방향을 직접 넓히는 중이므로, 이 층이 포트폴리오 핵심이다.

### 3.5 4층: Renderer와 Render Graph

Renderer는 GPU resource를 써서 화면을 만든다.

```text
world snapshot
  -> culling
  -> draw item build
  -> pass graph
  -> shadow / gbuffer / lighting / transparent / post
  -> UI composite
  -> present
```

중요한 분리는 "game world"와 "render snapshot"이다. Renderer가 GameSim truth를 직접 들여다보면 엔진 경계가 무너진다. Client가 snapshot을 renderable data로 바꾸고, Engine renderer는 generic draw contract를 처리하는 쪽이 좋다.

### 3.6 5층: Asset source, cook, runtime resource

엔진을 엔진답게 만드는 핵심은 asset pipeline이다.

```text
Source Asset
  FBX, glTF, PNG, PSD, WAV, JSON, XLSX

Import Metadata
  scale, axis, material mapping, compression, skeleton, tags

Cooked Asset
  .wmesh, .wmat, .wskel, .wanim, .wtex, .wfx, .wseq

Runtime Resource
  CModel, CTexture, CAnimation, GPU buffer, SRV, pipeline resource
```

Importer와 Loader는 다르다.

- Importer는 느려도 된다. 사람이 만든 파일을 해석하고 경고를 내고 deterministic cooked output을 만든다.
- Loader는 빨라야 한다. runtime binary를 검증하고 cache/GPU resource로 만든다.

Winters는 이미 `WMeshLoader/Writer`, `WMaterialLoader/Writer`, `Tools/WintersAssetConverter`가 있다. 다음 본질은 Asset Registry, dependency graph, validation, hot reload다.

### 3.7 6층: World, ECS/Object Model, Scene

게임 속 객체는 단순히 "클래스 인스턴스"가 아니다. 위치, 가시성, 충돌, 소유권, replication, lifetime, streaming 경계가 같이 있다.

Unreal은 Actor/Component/UObject, Unity는 GameObject/Component/Prefab, Winters는 Engine ECS와 Shared/GameSim component가 나뉜다.

Winters에서 조심할 점:

- Engine ECS는 generic runtime primitive다.
- Shared/GameSim component는 gameplay truth다.
- Client scene object는 presentation이다.

이 셋을 섞으면 처음에는 빠르지만, 나중에 server authority, replay, editor, profiling이 모두 꼬인다.

### 3.8 7층: GameSim과 gameplay truth

게임은 "보이는 것"보다 먼저 "결정되는 것"이 있다.

Winters의 권위 flow:

```text
Client Input
  -> GameCommand
  -> Server GameSim
  -> Snapshot / Event / FX cue
  -> Client Visual
```

이 구조의 본질은 치트 방지뿐 아니라 협업 안정성이다. Designer가 data를 바꿔도, Client가 animation을 바꿔도, gameplay result는 Server/GameSim에 모인다.

Unreal은 Replication, Gameplay Framework, GAS가 이 영역과 연결된다. Unity는 Netcode, Entities/DOTS, ScriptableObject data, custom server를 통해 구성한다. 단, Unity 프로젝트는 client-authoritative로 흐르기 쉬워서 별도 server architecture를 더 의식해야 한다.

### 3.9 8층: Network, Snapshot, Event, Replay

멀티플레이에서 network는 단순 send/recv가 아니다.

- input command serialization.
- command sequence/ack.
- snapshot delta.
- event stream.
- interpolation/reconciliation.
- AOI/interest management.
- replay and deterministic inspection.

Winters의 `Shared/Schemas/Command.fbs`, `Snapshot.fbs`, `Event.fbs`, `Server/Public/Game/SnapshotBuilder.h`, `ReplicationEmitter.h`가 이 층의 뼈대다.

### 3.10 9층: Animation, FX, Audio, UI

이 층은 presentation이다. 그러나 presentation이 gameplay와 완전히 무관한 것은 아니다. skill cast timing, hit frame, FX cue, sound cue는 server event/action sequence와 연결되어야 한다.

좋은 구조:

```text
Server action/cue
  -> Client action state
  -> animation montage/state
  -> notify/cue track
  -> FX/audio/UI playback
```

나쁜 구조:

```text
Client animation frame
  -> 직접 damage 결정
```

Winters는 `ReplicatedPoseComponent`, `ReplicatedActionComponent`, WFX panel, sequence asset 방향이 있으므로 "server cue 기반 presentation"을 밀면 Unreal의 GameplayCue/Niagara/Sequencer와 비교 가능한 포트폴리오가 된다.

### 3.11 10층: Editor와 Tools

Editor의 본질은 예쁜 UI가 아니다. 사람이 만든 의도를 runtime contract로 안전하게 바꾸는 것이다.

Editor가 해야 하는 일:

- asset catalog.
- import/reimport.
- validation.
- dependency view.
- property editing.
- preview.
- undo/redo.
- cook/build integration.
- profiling/debug capture.
- source control and team workflow.

Unreal은 Content Browser, Details Panel, Blueprint, Data Validation, Sequencer, Niagara, Control Rig, World Partition Editor가 강하다. Unity는 EditorWindow, UI Toolkit, AssetDatabase, ScriptedImporter, ScriptableObject, Package Manager가 강하다. Winters는 Content Browser/AssetConverter/WFX/DataPack/Snapshot tooling을 만들면 이 본질을 직접 증명할 수 있다.

### 3.12 11층: Backend, Services, Docker, LiveOps

Backend는 engine runtime과 다르다.

- Engine/GameServer: low latency, stateful, frame/tick 중심.
- Services: throughput, persistence, stateless API, database, queue, observability 중심.

Winters의 `Services`는 Go monorepo, docker-compose, auth/shop/matchmaking/profile/payment/leaderboard/inventory 방향이 있다.

Services의 본질:

- identity: 누가 접속했는가.
- inventory/economy: 무엇을 소유하는가.
- matchmaking: 누구와 어디서 플레이하는가.
- telemetry/liveops: 플레이 후 어떤 결정을 내리는가.
- deployment: 어떤 버전의 data와 binary를 배포하는가.

Docker는 여기서 "개발자가 같은 backend 환경을 재현하게 하는 최소 운영 단위"다. 포트폴리오에서는 docker-compose up으로 auth/match/profile/telemetry mock이 뜨고 client/server smoke가 연결되면 매우 강한 증거가 된다.

## 4. Winters 도메인과 Unreal/Unity 도메인 연결

| Winters 도메인 | Unreal 대응 | Unity 대응 | 본질 |
|---|---|---|---|
| `Engine/Public`, `Engine/Private` | Runtime modules, Core, Engine modules | native engine, packages | 공통 runtime capability |
| RHI/Renderer | RHI, RenderCore, RDG | SRP, URP/HDRP, Graphics APIs | GPU 작업 추상화와 제출 |
| Client | Game module, Pawn/Controller, UMG | Scene, MonoBehaviour, Prefab, UI | 입력과 presentation |
| Server | Dedicated Server target, Replication/Iris | headless server, Netcode, custom C# server | authoritative simulation host |
| `Shared/GameSim` | GAS, GameplayTags, gameplay C++ module | ScriptableObject data, DOTS/Systems, custom sim | gameplay truth and deterministic rule |
| FlatBuffers schema | USTRUCT/RPC/Net serialization | C# DTO/Netcode serialization | wire contract |
| AssetConverter / `.w*` | Asset import, cook, DDC, Pak/IoStore | AssetDatabase, ScriptedImporter, Addressables/AssetBundles | source to runtime binary |
| WFX/EffectTool | Niagara | Visual Effect Graph, Particle System | VFX authoring to runtime execution plan |
| Sequence | Sequencer/MovieScene | Timeline | time-based authored tracks |
| Editor/ImGui tools | Editor modules, Content Browser, Details Panel | EditorWindow, UI Toolkit, Inspector | authoring UI |
| Data pack | DataAsset, DataTable, Asset Manager | ScriptableObject, Addressables catalog | stable game data identity |
| Services/docker | OnlineSubsystem, backend plugins, external services | Unity Gaming Services/custom backend | account, match, shop, telemetry, liveops |
| Profiler | Unreal Insights, Trace, stat tools | Unity Profiler, Frame Debugger, Memory Profiler | measurement before optimization |

## 5. 에셋 스토어와 마켓플레이스의 본질

Asset Store나 Fab의 본질은 "파일 판매소"가 아니다. 신뢰 가능한 재사용 단위와 업데이트 채널이다.

### 5.1 Unity Asset Store의 원리

Unity Asset Store package는 Unity project의 파일/데이터 묶음이다. Unity 공식 문서 기준으로 Asset Store package는 `.unitypackage` 형식 또는 UPM package 형식을 가질 수 있고, Unity Package Manager가 package를 project에 추가하고 관리한다.

본질:

- publisher가 package draft를 만들고 metadata, version, price, dependency, samples, docs를 관리한다.
- 사용자는 Unity ID로 구매/다운로드하고 Package Manager의 My Assets에서 project에 import한다.
- `.meta` GUID가 reference stability의 핵심이다.
- import setting, folder structure, sample scene, documentation, changelog가 package 품질을 결정한다.
- 업데이트는 "새 파일 덮어쓰기"가 아니라 versioned package 배포와 compatibility 관리다.

Unity에서 툴을 Asset Store급으로 만들려면:

- `Editor` 폴더와 runtime 폴더 분리.
- asmdef/package.json/UPM 구조.
- sample scene.
- documentation.
- versioned changelog.
- user data migration.
- import 후 깨지지 않는 GUID/reference 관리.
- Unity 버전 호환성 표기.

### 5.2 Unreal Fab/Marketplace의 원리

Unreal 생태계도 본질은 같다.

- Content Browser에 들어오는 asset identity.
- plugin/module 구조.
- `.uasset`과 source asset의 import/cook.
- project setting, sample map, docs.
- engine version compatibility.
- Data Validation과 cook 검증.

Winters에서 Fab/Unity Asset Store를 흉내 낼 때 핵심은 "외부 asset을 직접 runtime에 넣는 것"이 아니다.

```text
External asset
  -> staging manifest
  -> import settings
  -> WintersAssetConverter cook
  -> Validator
  -> Asset Catalog
  -> runtime .w* load
```

즉 Winters 마켓/스토어의 본질은 `.w*` cooked contract와 validator다.

### 5.3 협업 상의 구조

큰 팀에서 asset/package workflow가 깨지는 이유는 파일이 없어서가 아니라, source of truth가 여러 개가 되기 때문이다.

필수 구조:

- stable asset id or virtual path.
- source path와 cooked path 분리.
- dependency graph.
- import version 기록.
- validation result 기록.
- owner와 review rule.
- source control LFS/Perforce 전략.
- package version과 changelog.
- CI에서 import/cook/load smoke.

Winters에서 바로 필요한 형태:

```text
AssetRecord
  guid
  virtualPath
  sourcePath
  cookedPrimaryPath
  type
  importerVersion
  sourceHash
  dependencies
  lastImportStatus
  owner
  license
```

## 6. 최적화의 본질

최적화는 "빠르게 보이게 고치는 것"이 아니다. 최적화는 예산을 정의하고, 측정하고, 가장 비싼 변환을 줄이는 일이다.

기본 루프:

```text
1. target budget 정의
2. capture
3. 가장 비싼 scope/counter 확인
4. 원인 분류: count, bandwidth, sync, cache miss, overdraw, allocation, serialization
5. 하나만 변경
6. 같은 capture로 비교
7. regression guard 추가
```

최적화의 공통 도구:

- profiler: 어디서 시간이 쓰이는지.
- frame debugger/renderdoc/pix: 어떤 GPU pass/draw가 비싼지.
- memory profiler: allocation과 asset residency.
- stat/counter: 시스템별 예산.
- validation: budget을 넘는 asset을 사전 차단.

Winters gotcha와 연결하면, normal F5에서 roster/map/minion/snapshot/UI/FX를 숨겨서 수치를 좋게 만드는 것은 최적화가 아니다. 명시적인 lab path와 정상 runtime capture를 분리해야 한다.

## 7. Niagara, VFX, Animation 최적화

### 7.1 Niagara 최적화의 본질

Niagara 최적화의 핵심은 particle 하나의 아름다움보다 "시스템이 생성하는 총 work"다.

줄여야 하는 work:

- spawn count.
- update count.
- simulation cost.
- renderer count.
- material overdraw.
- collision/event handler cost.
- bounds/culling failure.
- tick frequency.
- CPU/GPU sync.

Unreal 도구/개념:

- Niagara Effect Type asset과 scalability setting.
- System/Emitter/Renderer level override.
- fixed bounds와 culling.
- significance와 distance based budget.
- Niagara Debugger.
- Unreal Insights/Trace.
- GPU profiler, RenderDoc/PIX.
- platform scalability levels.

Winters WFX로 번역하면:

- `.wfx` graph compile 시 particle budget을 계산한다.
- `FxGraphValidator`가 spawn rate, max particle, texture overdraw risk, bounds 누락을 잡는다.
- `ParticlePool`과 runtime effect instance 수를 profiler counter로 노출한다.
- EffectTuner/WfxEffectToolPanel은 "예쁘게 보이는 UI"보다 budget preview와 validation result를 먼저 보여줘야 한다.

### 7.2 Unity VFX 최적화의 본질

Unity에서는 Visual Effect Graph와 Particle System 모두 같은 원리다.

- GPU simulation은 많은 particle에 유리하지만, 모든 플랫폼에서 공짜가 아니다.
- bounds가 틀리면 culling이 깨진다.
- transparent overdraw가 GPU를 태운다.
- texture resolution, shader variant, material pass 수가 비용을 만든다.
- Addressables/AssetBundle로 effect dependency loading을 관리해야 한다.

Unity 도구:

- Unity Profiler CPU/GPU modules.
- Frame Debugger.
- Rendering Debugger.
- Memory Profiler.
- VFX Graph stats/Inspector.
- Quality settings and LODGroup.

### 7.3 Animation 최적화의 본질

Animation 최적화는 "애니메이션을 줄인다"가 아니라 "플레이어가 눈치채지 못하는 업데이트를 줄인다"다.

비용 요소:

- skeleton bone count.
- skinned mesh vertex count.
- animation graph complexity.
- blend tree/motion matching search.
- IK/rig constraints.
- notify/event processing.
- update frequency.
- visibility/culling.
- animation compression.
- retargeting/runtime pose conversion.

Unreal 도구/개념:

- Animation Budget Allocator: skeletal mesh component tick을 예산 안에서 동적으로 조절.
- Animation Insights/Unreal Insights.
- Skeletal Mesh LOD.
- Update Rate Optimization.
- Animation Blueprint profiling.
- Control Rig cost 관리.

Unity 도구/개념:

- Animator culling mode.
- Optimize Game Objects.
- Animation compression.
- LODGroup.
- Animation Rigging C# Jobs.
- Profiler timeline and hierarchy.

Winters로 번역하면:

- `.wanim` cook 단계에서 skeleton hash, key count, duration, channel count를 검증한다.
- Client는 server `actionSeq`, pose/action snapshot을 받아 presentation만 재생한다.
- distance/visibility/team importance에 따라 animation update frequency를 낮춘다.
- high-bone asset은 cook validator에서 shader skinning limit과 cbuffer budget을 체크한다.
- Animation/FX cue는 server event와 중복 재생되지 않게 action sequence로 dedupe한다.

## 8. Unreal 툴 10개 제안

Unreal 포트폴리오 툴은 "Unreal을 잘 쓴다"보다 "대규모 제작 문제를 해결한다"에 맞춰야 한다.

1. Asset Dependency Auditor
   - Content Browser/Asset Registry 기반으로 circular dependency, unused asset, hard reference를 분석.

2. Data Validation Rule Pack
   - naming, texture size, material shader platform, skeleton/bone budget, Niagara budget을 validate.

3. Niagara Budget Dashboard
   - Effect Type, spawn rate, renderer count, bounds, scalability setting을 한 화면에서 점검.

4. Animation Budget Visualizer
   - Skeletal Mesh LOD, tick rate, Anim Budget Allocator 상태, distance/significance를 표시.

5. Cook and Chunk Visualizer
   - Primary Asset rule, chunk assignment, package size, missing dependency를 시각화.

6. GameplayTag/GAS Consistency Checker
   - GameplayTag, GameplayEffect, Ability reference 누락과 naming drift를 검사.

7. World Partition Cell Audit Tool
   - cell size, actor count, HLOD state, data layer, streaming source risk를 분석.

8. Sequencer Shot QC Tool
   - missing camera, unbound actor, audio sync, frame range, render setting 문제 검사.

9. Build Module Dependency Viewer
   - Build.cs dependency, Runtime/Editor module leak, circular dependency를 그래프로 표시.

10. Performance Capture Assistant
   - Unreal Insights trace channel preset, capture naming, baseline compare report 자동화.

## 9. Unity 툴 10개 제안

Unity 툴은 AssetDatabase, Package Manager, Addressables, ScriptableObject, UI Toolkit 확장성을 보여주는 것이 좋다.

1. Addressables Catalog Auditor
   - address duplication, remote catalog setting, missing dependency, bundle size를 검사.

2. Asset Store Package Validator
   - folder layout, package.json, samples, docs, asmdef, version/changelog를 검사.

3. ScriptableObject Data Schema Validator
   - gameplay data asset의 required field, range, reference validity를 검사.

4. Texture Import Preset Enforcer
   - sRGB/linear, normal map, compression, max size, platform override를 일괄 점검/수정.

5. VFX Graph Budget Reporter
   - particle count, bounds, exposed properties, texture/material dependencies를 리포트.

6. Animator Complexity Analyzer
   - state count, transition count, blend tree depth, clip length, humanoid/avatar mismatch를 분석.

7. Prefab Dependency Graph
   - prefab nesting, missing script, hard reference, scene dependency를 그래프로 표시.

8. Build Report Dashboard
   - build size, asset contribution, shader variant, managed stripping 결과를 요약.

9. Batch Reimport and Import Queue Tool
   - AssetDatabase/ScriptedImporter workflow를 UI Toolkit 창으로 관리.

10. Live Config Preview Tool
   - Remote Config/JSON override를 Editor Play Mode에서 preview하고 diff한다.

## 10. Winters Engine 툴 10개 제안

Winters 툴은 Unreal/Unity를 흉내 내되, Winters의 권위 경계와 cooked runtime contract를 증명해야 한다.

1. Winters Asset Catalog / Content Browser MVP
   - `.w*` asset, source path, cooked path, dependency, import status를 보여준다.

2. AssetConverter Import Queue
   - FBX/PNG/WAV를 import job으로 등록하고 `.wmesh/.wmat/.wskel/.wanim/.wtex` 출력을 검증한다.

3. WFX Budget Profiler
   - `.wfx` graph의 spawn count, max particle, texture, bounds, pool usage를 분석한다.

4. Animation Montage/Notify Editor
   - `.wanim`과 action sequence/cue timing을 연결하고 hit/FX/sound notify를 편집한다.

5. GameSim Command/Repro Inspector
   - `GameCommand` stream을 보고 특정 tick부터 재생해 server authoritative bug를 재현한다.

6. Snapshot/Event Viewer
   - FlatBuffers snapshot/event를 timeline으로 보여주고 client visual state와 비교한다.

7. Gameplay Definition Pack Editor
   - champion/skill/item definition을 편집하고 ServerPrivate/ClientPublic pack validation을 수행한다.

8. Dependency Boundary Checker
   - `Shared/GameSim`이 Engine/Client/UI/DX include를 갖는지 검사하고 CI report를 만든다.

9. Winters Profiler Timeline
   - CPU scope, GameSim system, render pass, FX count, network packet을 한 timeline에 표시한다.

10. Services/Docker LiveOps Dashboard
   - docker-compose service 상태, auth/match/profile/shop smoke, live data patch preview를 보여준다.

## 11. 10개씩 만들면 포트폴리오로 완성인가?

판정: 방향은 좋다. 그러나 "10개씩"은 완성 조건이 아니라 분량 조건이다.

완성으로 보이려면 다음 조건이 필요하다.

1. 각 엔진별 툴 10개가 아니라, "동일한 엔진 문제를 세 엔진에서 다르게 푼 비교"가 있어야 한다.
2. 각 툴은 최소 하나의 실제 샘플 프로젝트/asset으로 검증되어야 한다.
3. 실행 가능한 데모가 있어야 한다.
4. 전후 수치가 있어야 한다. 예: cook time, load time, particle count, missing refs, frame ms.
5. 문서가 있어야 한다. 사용법보다 "왜 이 경계가 맞는지"가 중요하다.
6. Unreal/Unity 툴은 marketplace-ready packaging 감각을 보여줘야 한다.
7. Winters 툴은 엔진 내부 이해도를 보여줘야 한다.

추천 포트폴리오 구성:

```text
Portfolio Theme A: Asset Pipeline
  Unreal Asset Auditor
  Unity Addressables Auditor
  Winters Asset Catalog + Import Queue

Portfolio Theme B: VFX Optimization
  Unreal Niagara Budget Dashboard
  Unity VFX Graph Budget Reporter
  Winters WFX Budget Profiler

Portfolio Theme C: Animation Optimization
  Unreal Animation Budget Visualizer
  Unity Animator Complexity Analyzer
  Winters Animation Notify/Budget Tool

Portfolio Theme D: Runtime Truth
  Unreal GAS/GameplayTag Checker
  Unity ScriptableObject Data Validator
  Winters GameSim Command/Repro Inspector

Portfolio Theme E: Build/Cook/Release
  Unreal Cook/Chunk Visualizer
  Unity Package Validator
  Winters Docker/LiveOps Dashboard
```

이렇게 묶으면 "툴을 많이 만들었다"가 아니라 "엔진 도메인의 본질을 알고, 각 엔진의 방식으로 구현할 수 있다"가 된다.

## 12. 우선순위

### 12.1 1순위: Winters 자체 툴 3개

1. Asset Catalog / Content Browser MVP
2. WFX Budget Profiler
3. GameSim Command/Repro Inspector

이 세 개가 있으면 Winters의 asset, rendering/FX, server authority가 모두 보인다.

### 12.2 2순위: Unreal/Unity 대응 툴 2개씩

Unreal:

- Niagara Budget Dashboard.
- Asset Dependency Auditor.

Unity:

- Addressables Catalog Auditor.
- Asset Store Package Validator.

이 네 개는 상용 엔진 경험을 보여주기 쉽고, Winters tool과 직접 비교된다.

### 12.3 3순위: 최적화/배포/협업 툴 확장

- Unreal Animation Budget Visualizer.
- Unity Animator Complexity Analyzer.
- Winters Profiler Timeline.
- Winters Services/Docker LiveOps Dashboard.

이 단계부터는 "engine programmer + tools programmer + backend 이해"가 동시에 보인다.

## 13. 공식 참고 자료

Unreal 공식 문서:

- Content Browser: https://dev.epicgames.com/documentation/unreal-engine/content-browser-in-unreal-engine
- Asset Management: https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine
- Cooking Content and Chunks: https://dev.epicgames.com/documentation/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine
- Unreal Build Tool: https://dev.epicgames.com/documentation/unreal-engine/unreal-build-tool-in-unreal-engine
- UBT Target Reference: https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-build-tool-target-reference
- Unreal Header Tool: https://dev.epicgames.com/documentation/unreal-engine/unreal-header-tool-for-unreal-engine
- Derived Data Cache: https://dev.epicgames.com/documentation/unreal-engine/using-derived-data-cache-in-unreal-engine
- World Partition: https://dev.epicgames.com/documentation/unreal-engine/world-partition-in-unreal-engine
- Data Validation: https://dev.epicgames.com/documentation/unreal-engine/data-validation-in-unreal-engine
- Unreal Insights: https://dev.epicgames.com/documentation/unreal-engine/unreal-insights-in-unreal-engine
- Niagara Scalability and Best Practices: https://dev.epicgames.com/documentation/unreal-engine/scalability-and-best-practices-for-niagara
- Animation Budget Allocator: https://dev.epicgames.com/documentation/unreal-engine/animation-budget-allocator-in-unreal-engine
- Sequencer Overview: https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-sequencer-movie-tool-overview
- Control Rig: https://dev.epicgames.com/documentation/unreal-engine/control-rig-in-unreal-engine

Unity 공식 문서:

- Asset Store introduction: https://docs.unity3d.com/Manual/asset-store-introduction.html
- Publishing Asset Store packages: https://docs.unity3d.com/Manual/asset-store-publishing-introduction.html
- Validate/upload Asset Store package: https://docs.unity3d.com/Manual/AssetStoreUpload.html
- Package Manager: https://docs.unity3d.com/Manual/PackagesList.html
- Addressables: https://docs.unity3d.com/Packages/com.unity.addressables@latest/
- Addressables development cycle: https://docs.unity3d.com/Packages/com.unity.addressables@1.4/manual/AddressableAssetsDevelopmentCycle.html
- AssetDatabase API: https://docs.unity3d.com/ScriptReference/AssetDatabase.html
- AssetImporter API: https://docs.unity3d.com/ScriptReference/AssetImporter.html
- Scripted Importers: https://docs.unity3d.com/Manual/ScriptedImporters.html
- ScriptableObject: https://docs.unity3d.com/Manual/class-ScriptableObject.html
- UI Toolkit: https://docs.unity3d.com/Manual/UIElements.html
- EditorWindow API: https://docs.unity3d.com/ScriptReference/EditorWindow.html
- Unity Profiler: https://docs.unity3d.com/Manual/Profiler.html
- CPU Usage Profiler: https://docs.unity3d.com/Manual/ProfilerCPU.html
- GPU Usage Profiler: https://docs.unity3d.com/Manual/ProfilerGPU.html
- Visual Effect Graph: https://docs.unity3d.com/Packages/com.unity.visualeffectgraph@latest/
- Animation Rigging: https://docs.unity3d.com/Manual/com.unity.animation.rigging.html
- Timeline: https://docs.unity3d.com/Manual/com.unity.timeline.html

## 14. 검증과 핸드오프

문서 검증:

- Winters local architecture/gotcha 문서를 확인했다.
- `.md/문서`의 Editor/Tooling/Data/Services/Collaboration/AssetImporter 계열 문서를 참고했다.
- Unreal/Unity는 공식 문서 위주로 도구명과 개념을 확인했다.
- 코드 변경은 하지 않았고, 문서만 추가한다.

후속 작업 추천:

1. 이 문서를 기준으로 `Portfolio Theme A: Asset Pipeline`을 별도 구현 계획으로 쪼갠다.
2. 첫 구현은 Winters Asset Catalog / Content Browser MVP로 잡는다.
3. 같은 주제의 Unity Addressables Auditor와 Unreal Asset Dependency Auditor를 작게 만들어 3엔진 비교 샘플을 만든다.
4. 각 툴마다 `problem -> engine concept -> implementation -> validation -> demo` 형식의 README를 붙인다.

