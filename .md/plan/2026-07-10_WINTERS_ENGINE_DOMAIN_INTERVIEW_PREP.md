Session - Winters Engine 전체 도메인을 면접 언어로 구조화한다.

이 문서는 Winters Engine을 "무엇을 만들었는가"가 아니라 "어떤 문제를 어떤 경계로 나누고, 어떻게 검증 가능한 구조로 만들었는가"의 관점에서 설명하기 위한 면접 대비 문서다.

## 1. 한 줄 정체성

```text
Winters Engine은 Engine, Client, Shared GameSim, Server, Tool, Data pipeline을 분리해 LoL 스타일 서버 권위 게임과 EldenRing 계열 클라이언트를 같은 엔진 위에 얹기 위한 C++ 게임 엔진 프로젝트다.
```

면접에서 가장 먼저 남겨야 하는 인상:

```text
저는 게임 기능을 단순히 클라이언트에 구현하는 것이 아니라, gameplay truth, presentation, data authoring, runtime resource, tool/editor, network replication의 책임을 나눠 전체 파이프라인으로 설계했습니다.
```

## 2. 최상위 구조

```text
Engine
  - frame loop, Win32, RHI, renderer, ECS primitive, resource, UI, sound, profiler, jobs

Shared/GameSim
  - deterministic gameplay contract, components, systems, commands, definitions, replicated events

Server
  - IOCP networking, sessions, command ingress, room tick, bot command production, snapshot/event broadcast

Client
  - input, camera, local presentation, weak prediction, snapshot/event application, animation, FX, UI

Tools/Editor
  - asset conversion, WFX/effect editing, tuning panels, validation/harness, Elden world editor

Data/Resource
  - authoring JSON, generated packs, runtime binary assets, texture/model/fx resource paths

EldenRingClient / EldenRingEditor
  - separate action RPG/product slice sharing the same Engine foundation
```

핵심 흐름:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

이 흐름은 Winters의 북극성이다. 전투 결과, 사망, 이동, 쿨타임, 투사체 판정은 Server/GameSim이 만들고, Client는 그 결과를 표현한다.

## 3. 계층별 면접 설명

### 3.1 Engine

역할:

- Win32 window와 frame loop
- RHI/DX11 device와 renderer
- ECS primitive와 system scheduler
- resource cache, model/animation/texture
- `.wmesh`, `.wanim`, `.wtex`, `.wmat`, `.wfx` 같은 Winters runtime asset 방향
- UI, sound, profiler, job system
- `CGameInstance` gateway

코드 근거:

- `Engine/Include/GameInstance.h`
- `Engine/Private/GameInstance.cpp`
- `Engine/Private/RHI/DX11/CDX11Device.cpp`
- `Engine/Private/Renderer/RHISceneRenderer.cpp`
- `Engine/Public/ECS/World.h`
- `Engine/Private/ECS/SystemScheduler.cpp`
- `Engine/Private/Core/JobSystem.cpp`

면접 핵심:

```text
Engine은 제품 코드를 모르는 generic runtime입니다. LoL Client나 Server gameplay 타입을 Engine에 넣지 않고, 공용 기능은 GameInstance/RHI/ECS/Resource 같은 경계로 제공합니다.
```

어려웠던 점:

- Engine public header가 잘못 열리면 Client/Server/Shared 전체에 의존성이 퍼진다.
- DX11 concrete type이 public API에 새면 RHI 확장성이 막힌다.
- `unique_ptr`와 dllexport 클래스가 섞이면 MSVC 경고/복사 생성 문제가 생긴다.

해결 전략:

- public header에서는 `std::` 명시, 불필요한 using 금지.
- Engine 내부 구현은 `Private`에 두고 Client는 `EngineSDK/inc` 또는 Engine public API만 사용.
- RHI는 `IRHIDevice`, RHI handle, command list로 상위 경계를 만든다.

### 3.2 Shared/GameSim

역할:

- 서버 권위 gameplay truth의 결정적 시뮬레이션 계약
- `GameCommand`, `TickContext`, gameplay component, system
- champion skill validation과 damage/death/cooldown/move/AI policy
- snapshot/event로 나갈 상태와 cue 생성

코드 근거:

- `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
- `Shared/GameSim/Systems/Death/DeathSystem.cpp`
- `Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp`
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `Shared/GameSim/Components/ReplicatedEventComponent.h`
- `Shared/GameSim/Definitions/GameplayDefinitionPack.h`

면접 핵심:

```text
Shared/GameSim은 Client visual이 아니라 gameplay result를 만든다. Client가 스킬 성공 여부나 데미지를 결정하지 않도록 GameCommand를 서버가 실행하고, 결과를 Snapshot/Event로 내보낸다.
```

어려웠던 점:

- Client에도 ECS와 visual component가 있고 Shared에도 gameplay component가 있어 경계가 흐려지기 쉽다.
- bot AI가 직접 HP/Transform을 고치면 서버 권위가 무너진다.
- 챔피언별 예외가 `CommandExecutor`에 쌓이면 시스템이 읽기 어려워진다.

해결 전략:

- Bot AI는 `GameCommand` 생산자로 둔다.
- `TickContext`에 walkable query, lag compensation, definitions 등 tick 의존성을 명시한다.
- process-local `EntityID/EntityHandle`과 network `NetEntityId`를 분리한다.
- 결정성이 필요한 순회는 entity ID를 정렬한다.

### 3.3 Server

역할:

- WinSock/IOCP 기반 session accept, recv, send
- command packet 검증과 ingress queue
- session별 sequence gate
- 30Hz room tick에서 command drain, GameSim 실행, bot command 생성
- snapshot/event serialization and broadcast

코드 근거:

- `Server/Private/Network/IOCPCore.cpp`
- `Server/Private/Network/Session.cpp`
- `Server/Private/Game/CommandIngress.cpp`
- `Server/Private/Game/GameRoomTick.cpp`
- `Server/Private/Game/SnapshotBuilder.cpp`
- `Server/Private/Game/ReplicationEmitter.cpp`

면접 핵심:

```text
Server는 네트워크 스레드와 시뮬레이션 tick 스레드의 책임을 분리합니다. IOCP worker는 packet을 받아 ingress queue에 넣고, room tick이 정렬된 command를 drain해서 GameSim truth를 갱신합니다.
```

어려웠던 점:

- IO thread가 gameplay world를 직접 만지면 race가 난다.
- 빠른 move command가 쌓이면 이전 입력이 뒤늦게 적용되어 조작감이 나빠진다.
- schema verify 실패를 조용히 drop하면 client freeze처럼 보인다.

해결 전략:

- ingress queue에 mutex를 두고 tick에서 drain한다.
- Move command는 같은 session의 이전 pending Move를 최신 것으로 coalescing한다.
- command drain은 accepted tick, session id, sequence로 stable sort한다.
- 실패는 `std::cerr` 또는 bounded debug trace로 남긴다.

### 3.4 Client

역할:

- 입력 수집과 command serialization
- camera, scene, local presentation
- server snapshot/event apply
- interpolation, weak prediction, animation/FX/UI playback
- EffectTuner, WFX tool, debug panels

코드 근거:

- `Client/Private/Scene/Scene_InGameInput.cpp`
- `Client/Private/Network/Client/CommandSerializer.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp`
- `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Private/GameObject/FX/FxCuePlayer.cpp`
- `Client/Private/UI/WfxEffectToolPanel.cpp`
- `Client/Private/UI/EffectTuner.cpp`

면접 핵심:

```text
Client는 gameplay truth를 만들지 않고, 서버가 보낸 truth를 보기 좋게 복원합니다. Snapshot은 지속 상태를 맞추고 Event는 1회성 시각/음향/피드백을 재생합니다.
```

어려웠던 점:

- local prediction과 server snapshot이 yaw/animation을 서로 덮어쓰면 조작감이 깨진다.
- server event와 legacy local hook이 동시에 FX를 틀면 중복 재생된다.
- kill feed처럼 1회성 event는 중복 방지가 없으면 점수가 계속 오른다.

해결 전략:

- snapshot path와 event path를 분리한다.
- event cue key를 만들어 중복 effect/kill feed를 걸러낸다.
- animation은 `ActionStart` event와 replicated action component를 기준으로 재생한다.
- FX는 server cue single-source로 플레이한다.

### 3.5 Data and Resource

역할 구분:

```text
Data
  - authoring/cook input
  - gameplay JSON, visual definitions, WFX document, generated packs

Resource
  - runtime asset files
  - Texture, Model, Animation, Sound, cooked assets

FX/WFX
  - effect cue definition
  - emitter transform, lifetime, texture/model path, blend/depth/material values
```

Winters 원칙:

- JSON은 편집/authoring/cook 입력이다.
- runtime frame은 검증된 immutable pack 또는 loaded asset을 본다.
- runtime resource resolve는 `Client/Bin/Resource`가 기준이다.
- `.wfx`는 git으로 관리되면 다른 장비에서도 같은 effect 결과를 받을 수 있다.

면접 핵심:

```text
기획 데이터와 디자이너 리소스, 개발자 코드를 분리하려고 했습니다. 기획자는 gameplay JSON을, 디자이너는 WFX와 texture/model resource를, 개발자는 그 데이터를 읽는 runtime contract를 책임지는 구조입니다.
```

### 3.6 FX and WFX Tool

역할:

- `.wfx` 문서가 cue name과 emitter 배열을 가진다.
- `CFxCuePlayer`가 cue를 찾아 billboard, beam, ribbon, mesh particle component로 변환한다.
- `EventApplier`가 server `EffectTrigger` event를 받아 visual hook 또는 WFX cue로 재생한다.
- `WfxDocument`가 WFX JSON 저장/로드를 담당한다.

코드 근거:

- `Client/Private/GameObject/FX/FxCuePlayer.cpp`
- `Client/Private/GameObject/FX/WfxDocument.cpp`
- `Client/Public/GameObject/FX/FxBillboardComponent.h`
- `Client/Public/GameObject/FX/FxMeshComponent.h`
- `Client/Private/UI/WfxEffectToolPanel.cpp`

면접 핵심:

```text
FX는 코드에 하드코딩된 sprite가 아니라 cue data로 이동시키는 중입니다. 서버는 effect id와 context를 보내고, 클라이언트는 WFX cue를 찾아 여러 emitter를 runtime ECS FX component로 생성합니다.
```

어려웠던 점:

- 특정 챔피언 effect는 예외가 많아 코드에 분기하기 쉽다.
- transform/yaw/lifetime/texture 경로가 tool과 runtime에서 다르게 해석되면 결과가 달라진다.
- mesh particle은 renderer pointer가 필요해 단순 billboard와 생성 경로가 다르다.

해결 전략:

- cue missing/skipped emitter를 bounded log로 남긴다.
- emitter render type을 explicit enum으로 둔다.
- WFX document 저장 시 texture/model/transform/lifetime/material 값을 JSON으로 남긴다.

### 3.7 Tool and Collaboration

역할:

- EffectTuner/WFX editor: 디자이너가 effect 값을 조절하고 저장
- ChampionTuner/SkillTimingPanel: 전투 감각과 animation timing 튜닝
- SimLab/Harness: deterministic simulation, boundary checks
- 문서: plan, handoff, gotcha, architecture compass

협업 분리:

```text
기획자
  - champion stat, skill timing, cooldown, range, gameplay JSON

디자이너
  - texture, model, animation, WFX emitter, visual timing

개발자
  - runtime loader, server authority, ECS systems, renderer, validation, build pipeline
```

면접 핵심:

```text
협업 구조의 본질은 각 직군이 서로의 파일을 직접 고치지 않아도 결과가 runtime에 반영되는 것입니다. 그래서 데이터와 리소스와 코드를 분리하고, 검증/빌드/핸드오프 문서를 같이 관리했습니다.
```

### 3.8 EldenRing Client and Editor

역할:

- `EldenRingClient`는 WintersEngine 위에 별도 action RPG client를 얹는 방향이다.
- `EldenLimgraveShowcaseScene`은 Limgrave/StartingCave vertical slice를 로드하고, map placement와 character placement를 확인한다.
- `EldenRingEditor`는 world cell document와 editor scene을 통해 world partition/editor 방향을 검증한다.

코드 근거:

- `EldenRingClient/Private/EldenRingApp.cpp`
- `EldenRingClient/Private/EldenLimgraveShowcaseScene.cpp`
- `EldenRingEditor/Private/EldenRingEditorApp.cpp`
- `EldenRingEditor/Private/World/WorldCellDocument.cpp`

면접 핵심:

```text
Winters는 LoL 전용 클라이언트 하나가 아니라, 같은 Engine DLL 위에 WintersLOL과 WintersElden 같은 제품 클라이언트를 얹는 방향입니다. Elden slice는 renderer/resource/world placement pipeline이 제품 코드로 분리될 수 있는지 검증하는 실험입니다.
```

어려웠던 점:

- Elden resource는 규모가 크고 source placement -> runtime asset -> spawn closure가 중요하다.
- map/character placement의 transform 조정은 tool/editor 데이터로 저장되어야 한다.
- showcase code에는 아직 ad hoc JSON parsing이 있어 장기적으로 공용 parser/validator가 필요하다.

## 4. 문제 해결 설계 전략

Winters에서 버그를 잡을 때의 기본 순서:

```text
1. 현상
2. 소유 계층 판정
3. 재현 경로
4. authoritative path 추적
5. 관측점 추가
6. 최소 수정
7. 빌드와 runtime 검증
8. 문서화와 gotcha 반영
```

예시 1: 이동/카메라 입력 버그

- 현상: F2 freecam이 아닌데 W 입력으로 카메라가 움직임.
- 소유 계층: Client camera/input presentation.
- 원인 후보: key handling guard 누락.
- 해결 방향: freecam active 상태에서만 WASD camera movement 허용.
- 검증: F2 off에서는 W가 champion/game input에만 작동, F2 on에서는 freecam 이동.

예시 2: 중복 kill feed/score

- 현상: 한 번 죽었는데 kill message와 score가 계속 증가.
- 소유 계층: Server death/event single-source + Client event dedupe.
- 원인 후보: death state transition이 edge가 아니라 level 상태로 반복 처리되거나, client event 중복 방지 부재.
- 해결 방향: `!bIsDead -> bIsDead` transition에서만 event 생성, client는 event key로 duplicate 방지.
- 검증: 같은 target이 죽어도 kill feed는 1회, score 증가 1회.

예시 3: T-pose 중복 생성

- 현상: Viego/Sylas 기준 T-pose model이 중복 생성.
- 소유 계층: Client snapshot entity binding + visual spawn.
- 원인 후보: netId binding 실패, old visual stale, champion mismatch handling, asset fallback spawn.
- 해결 방향: snapshot `EnsureEntity`가 alive/bound entity를 재사용하고, mismatch는 visual 변경 path로 처리.
- 검증: 같은 netId가 여러 visual entity로 갈라지지 않음.

## 5. 구조적인 예외 처리

Winters 에러 처리 원칙:

- 실패는 발생 지점에서 관측 가능해야 한다.
- routine trace와 failure diagnostic은 구분한다.
- server failure는 `std::cerr`, shared sim diagnostics는 `WintersOutputAIDebugStringA`, client/engine debug는 gated `OutputDebugStringA`를 쓴다.
- FlatBuffers verify 실패, RHI resource creation 실패, asset miss, cue miss는 조용히 묻지 않는다.
- 로그는 bounded로 남겨 프레임 로그 폭주를 막는다.

면접 답변:

```text
게임에서는 실패를 무조건 예외로 던지기보다, 실패 위치와 원인을 관측 가능하게 남기고 해당 계층에서 격리하는 것이 중요하다고 봤습니다. 그래서 RHI, network verify, asset/cue loading 같은 경계에 bounded diagnostic을 남기는 정책을 세웠습니다.
```

## 6. 향후 업데이트와 구조 수정 방향

### 6.1 RHI 확장

현재:

- DX11이 기본 runtime path.
- DX12 device가 방향성으로 존재한다.
- 일부 renderer는 아직 DX11 구체 타입과 가까운 경로가 있다.

방향:

- Client/Public와 Engine/Public에서 DX11 concrete type 노출 금지 유지.
- render snapshot과 RHI handle 중심으로 renderer를 backend-neutral화.
- LoL/Elden이 renderer class를 복제하지 않고 서로 다른 world snapshot을 같은 RHI renderer에 공급.

### 6.2 Data-driven migration

현재:

- gameplay/visual data 일부는 JSON/generated pack.
- 일부 champion-specific 값과 fallback은 코드에 남아 있다.

방향:

- reader count가 0이 된 legacy table만 삭제.
- `DefinitionKey`는 stable identity, dense id는 pack-local로 유지.
- 기획자가 바꿀 값은 JSON/cook path로 이동.
- runtime tick은 JSON을 직접 읽지 않고 validated pack을 읽는다.

### 6.3 GameSim boundary cleanup

현재:

- Shared/GameSim은 Engine 직접 include를 줄이고 adapter를 통해 접근하는 방향.
- `Check-SharedBoundary.ps1`로 직접 include 위반을 막는다.

방향:

- Shared가 EngineSDK include path에 의존하지 않도록 ECS adapter 완성.
- Shared에는 renderer/UI/DX/ImGui 타입이 들어오지 않게 유지.
- server authority path를 normal F5 runtime에서도 숨기지 않는다.

### 6.4 Tool pipeline

방향:

- WFX editor는 effect authoring의 중심.
- Content Browser / Asset Catalog / Importer / Validator / Cook pipeline을 연결.
- 디자이너가 저장한 `.wfx`, placement JSON, visual definitions가 git으로 동기화되어 다른 장비에서 같은 결과를 재현.

## 7. 어려웠던 점과 말하는 방식

어려웠던 점:

- LoL식 조작감은 client presentation이 빠르게 반응해야 하지만, gameplay truth는 server가 가져야 한다.
- animation/FX는 감각 작업이라 data/tool이 필요하지만, 서버 event와 동기화되어야 한다.
- C++에서는 헤더와 DLL 경계가 설계 실수 하나로 전체 프로젝트에 퍼진다.
- 멀티스레딩은 성능보다 데이터 소유권과 race 방지가 먼저다.
- 리소스와 데이터가 커질수록 "잘 로드됐다"보다 "무엇이 빠졌는지 보인다"가 중요하다.

좋은 답변 구조:

```text
처음에는 클라이언트 로컬 구현으로 빠르게 감각을 만들 수 있었지만, 멀티클라이언트와 Bot AI가 들어오면서 서버 권위 구조가 필요해졌습니다. 그래서 Client는 presentation, Shared/GameSim은 gameplay contract, Server는 authority tick, Engine은 generic runtime으로 나누는 방향으로 리팩터링했습니다.
```

## 8. 검증과 핸드오프

문서 작성 검증:

- 현재 코드와 architecture 문서를 기준으로 구조를 정리했다.
- 구현 변경은 하지 않았다.
- 면접 대비 시 이 문서는 "전체 지도", C++ 문서는 "기술 깊이", Q/A 문서는 "말하기 연습"으로 사용한다.

면접 전에 확인할 것:

- 최근 git 상태에서 어떤 변경이 아직 push되지 않았는지.
- 빌드가 통과한 마지막 commit/branch.
- 데모 가능한 시나리오: server-client, champion skill/FX, WFX tuning, Elden showcase.
