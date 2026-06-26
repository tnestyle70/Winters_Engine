# Winters Engine 도메인별 본질 해부 원고

작성일: 2026-06-26

목표:

이 문서는 Winters Engine을 티스토리 기술 블로그와 이력서에 동시에 사용할 수 있도록, 도메인별 본질을 실제 코드 구조와 연결해 설명하는 원고다.

핵심 방향:

> 기능을 나열하지 않는다. 문제를 정의하고, 왜 그 문제가 구조적 문제인지 설명하고, Winters Engine에서 어떤 경계와 파이프라인으로 풀었는지 코드 근거와 함께 쌓아올린다.

---

## 0. 전체 관통 질문

Winters Engine을 설명할 때 가장 먼저 던져야 하는 질문은 "무엇을 만들었는가?"가 아니다.

더 좋은 질문은 이것이다.

> 기능이 많아질수록 왜 기존 구조는 유지보수하기 어려워지는가?

처음에는 이동, 공격, 스킬, UI, 렌더링을 하나씩 붙이면 게임이 만들어지는 것처럼 보인다. 하지만 프로젝트가 커질수록 진짜 문제는 기능의 개수가 아니라 기능 사이의 경계가 된다.

챔피언 스킬 수치가 C++ 상수와 JSON, 툴 출력물에 흩어진다. DX11 포인터가 Client public API까지 새어 나온다. 서버가 판정해야 할 것을 Client가 visual 편의 때문에 먼저 결정한다. 렌더러를 LoL에 맞춰 만들었더니 Elden형 Action RPG를 붙일 때 다시 복사해야 한다. 리소스는 어디서 읽어야 하는지, 어떤 파일을 git에 올려야 하는지, 어떤 파일은 Drive로 공유해야 하는지 불분명해진다.

Winters Engine은 이 지점을 문제로 정의한 프로젝트다.

그래서 Winters의 핵심은 "자체 엔진을 만들었다"가 아니라, 다음 구조를 세웠다는 데 있다.

```text
WintersEngine.dll
├─ WintersLOL.exe      // MOBA product client
└─ WintersElden.exe    // Action RPG product client

Client Input
-> GameCommand
-> Server GameSim
-> Snapshot/Event
-> Client Visual
```

이 한 줄은 엔진, 클라이언트, 서버, 데이터, 렌더링, 에셋 파이프라인 전체를 관통한다.

면접관에게 보여줘야 하는 역량은 다음이다.

- 문제를 "기능 부족"이 아니라 "구조적 경계 부재"로 정의했다.
- Engine / Client / Shared GameSim / Server / Tools의 책임을 나눴다.
- DX11 고정 구조를 RHI와 RenderWorldSnapshot으로 분리했다.
- gameplay truth와 presentation을 분리했다.
- JSON authoring과 runtime immutable definition pack을 분리했다.
- 검증 스크립트로 대형 리팩터링의 회귀를 추적했다.

이 관점으로 각 도메인을 하나씩 해부한다.

---

## 1. Product Client Separation

### 본질

Winters Engine의 첫 번째 본질은 "하나의 엔진 위에 여러 게임 클라이언트를 올릴 수 있어야 한다"는 것이다.

LoL형 MOBA와 Elden형 Action RPG는 게임 장르가 다르다. 입력 방식, 카메라, 전투, 월드 구성, AI, UI가 다르다. 그런데 렌더링, 리소스, RHI, ECS primitive, editor/runtime service까지 매번 복사한다면 그것은 엔진이 아니라 게임별 코드 더미가 된다.

따라서 Winters는 처음부터 다음 질문을 가진다.

> LoL Client와 Elden Client가 달라야 하는 부분은 무엇이고, 반드시 공유해야 하는 부분은 무엇인가?

### 문제 인식

학습용 엔진은 보통 하나의 게임을 만들기 위해 빠르게 결합된다. Scene이 renderer를 직접 알고, UI가 product-specific manager를 직접 읽고, resource path가 코드에 박히고, DX11 device accessor가 여기저기 퍼진다.

이 상태에서는 두 번째 게임을 만들 때 선택지가 둘뿐이다.

1. 기존 LoL 코드를 복사해서 Elden을 만든다.
2. LoL 코드를 계속 if문으로 분기해서 Elden도 얹는다.

둘 다 장기적으로는 실패한다. renderer, asset cache, UI, debug, editor가 두 갈래로 갈라지거나 하나의 클라이언트가 모든 장르의 예외를 품게 된다.

### Winters의 접근

Winters는 Engine과 Product Client를 분리한다.

Engine은 generic runtime을 소유한다.

- frame loop
- RHI
- renderer
- resource/cache
- ECS primitive
- UI rendering
- editor/runtime service

Client는 product-specific presentation을 소유한다.

- scene 구성
- input mapping
- camera
- UI state
- animation/FX playback
- network snapshot 적용
- LoL/Elden별 gameplay presentation bridge

중요한 점은 Engine이 LoL이나 Elden을 include하지 않는다는 것이다. Engine은 generic API를 제공하고, Product Client가 필요한 view data나 render snapshot을 만들어 Engine에 전달한다.

### 코드에서 볼 지점

주요 코드 근거:

- `EngineSDK/inc/WintersEngine.h`
- `EngineSDK/inc/GameInstance.h`
- `EngineSDK/inc/Renderer/RenderWorldSnapshot.h`
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`

Compass의 제품 방향은 다음 구조를 기준으로 한다.

```text
WintersEngine.dll
├─ WintersLOL.exe
└─ WintersElden.exe
```

### 면접관에게 보이는 역량

이 도메인에서 보여줄 수 있는 역량은 "게임 하나를 만든 경험"이 아니라 "게임이 늘어나도 무너지지 않는 제품 경계를 고민한 경험"이다.

이력서 문장:

> LoL형 MOBA와 Elden형 Action RPG를 같은 C++ 런타임 위에 분리해 올리기 위해 Engine/Product Client 경계를 설계했습니다.

---

## 2. Engine Runtime

### 본질

Engine Runtime은 게임별 로직을 담는 곳이 아니라, 모든 게임 클라이언트가 공유하는 실행 기반이다.

본질은 다음이다.

> Engine은 게임의 의미를 몰라도 프레임을 돌리고, 리소스를 관리하고, 렌더링과 UI와 에디터 서비스를 제공해야 한다.

### 문제 인식

게임 클라이언트 코드가 커지면 "이 기능은 Engine에 있어야 하나, Client에 있어야 하나?"라는 질문이 계속 나온다.

예를 들어 UI texture loading이 필요하다고 해서 Client가 DX11 SRV를 직접 만들기 시작하면, 어느새 Client public API에 DX11 타입이 새어 나온다. AI debug panel이 필요하다고 해서 Engine UI가 GameSim component를 직접 조회하면, Engine이 product gameplay를 알아야 한다.

이렇게 되면 Engine은 더 이상 공용 엔진이 아니라 LoL 전용 런타임이 된다.

### Winters의 접근

Winters에서는 Engine이 generic primitive와 service를 제공하고, Product Client가 product-specific state를 만들어 넘긴다.

Engine이 소유해야 하는 것:

- 창, 입력, 프레임 루프
- RHI와 renderer
- resource cache
- ECS primitive
- world partition, streaming
- FX graph/runtime
- UI rendering substrate
- profiler/debug output infrastructure

Client가 소유해야 하는 것:

- 이 UI 패널에 어떤 데이터를 보여줄지
- 이 entity가 어떤 챔피언인지
- 서버 snapshot을 어떤 animation/FX로 보여줄지
- LoL과 Elden의 scene/camera/input 차이

### 코드에서 볼 지점

주요 폴더:

- `Engine/Public/Core`
- `Engine/Public/Platform`
- `Engine/Public/Renderer`
- `Engine/Public/RHI`
- `Engine/Public/Resource`
- `Engine/Public/World`
- `Engine/Public/FX`
- `EngineSDK/inc`

`EngineSDK/inc`는 Engine public header가 외부 클라이언트에서 소비되는 형태로 배포되는 경계다. 즉 Engine public API를 바꾸면 SDK sync와 Client build까지 같이 검증해야 한다.

### 면접관에게 보이는 역량

Engine Runtime 도메인은 "C++ 엔진 코드를 짤 수 있다"보다 "public API가 다른 모듈에 주는 영향을 알고 있다"를 보여준다.

이력서 문장:

> Engine public API와 Product Client 의존 방향을 분리하고, SDK sync/build 검증을 고려한 C++ 런타임 경계를 설계했습니다.

---

## 3. RHI

### 본질

RHI의 본질은 "DX11을 숨기는 추상화"가 아니다.

더 정확히는 다음이다.

> 렌더링 backend의 선택이 Product Client와 gameplay 구조를 오염시키지 않도록 막는 경계다.

### 문제 인식

DX11로 빠르게 구현하면 처음에는 쉽다. `ID3D11Device`, `ID3D11ShaderResourceView`, `DX11Pipeline` 같은 concrete type을 바로 쓰면 원하는 화면을 빨리 볼 수 있다.

하지만 시간이 지나면 문제가 생긴다.

- Client public header가 DX11 타입에 묶인다.
- UI와 renderer가 backend 교체를 방해한다.
- DX12/Vulkan/console 확장 가능성이 닫힌다.
- Elden Client가 LoL DX11 renderer 구현을 복사해야 한다.
- 테스트나 editor tool도 DX11 runtime에 강하게 묶인다.

이 문제는 "나중에 바꾸자"로 해결되지 않는다. concrete type이 public API에 퍼진 뒤에는 바꿀 때마다 전체 모듈이 흔들린다.

### Winters의 접근

Winters는 RHI 경계를 세운다.

핵심은 handle, descriptor, command list, queue, swap chain, pipeline state 같은 개념을 Engine public API로 끌어올리는 것이다.

코드 근거:

```cpp
class WINTERS_ENGINE IRHIDevice
{
public:
    virtual eRHIBackend GetBackend() const = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) const = 0;

    virtual void BeginFrame(f32_t r = 0.0f,
                            f32_t g = 0.0f,
                            f32_t b = 0.0f,
                            f32_t a = 1.0f) = 0;
    virtual void EndFrame() = 0;

    virtual IRHICommandList* CreateCommandList() { return nullptr; }
    virtual RHIPipelineHandle CreatePipeline(const RHIPipelineDesc& desc) = 0;
    virtual RHIRenderPassHandle CreateRenderPass(const RHIRenderPassDesc& desc) = 0;
    virtual RHIBindGroupHandle CreateBindGroup(const RHIBindGroupDesc& desc) = 0;
};
```

파일:

- `EngineSDK/inc/RHI/IRHIDevice.h`
- `EngineSDK/inc/RHI/RHIHandles.h`
- `EngineSDK/inc/RHI/RHIDescriptors.h`
- `EngineSDK/inc/RHI/IRHICommandList.h`

### 중요한 tradeoff

RHI는 비용이 있다. 추상화 계층이 생기고, DX11에서 바로 접근하던 기능을 descriptor/handle로 포장해야 한다. 구현 속도도 느려진다.

하지만 Winters에서 RHI를 선택한 이유는 "멋진 구조" 때문이 아니라, LoL과 Elden이 renderer를 공유해야 하고, DX11 고정 구조를 장기적으로 벗어나야 하기 때문이다.

### 면접관에게 보이는 역량

RHI 도메인에서는 단순 렌더링 구현보다 "backend concrete dependency가 어디까지 퍼지면 위험한지 아는 감각"을 보여줄 수 있다.

이력서 문장:

> DX11 concrete type이 Client/Public API를 오염시키는 문제를 정의하고, `IRHIDevice`와 RHI handle/descriptor 기반 backend-neutral 렌더링 경계를 설계했습니다.

---

## 4. RenderWorldSnapshot / Scene Renderer

### 본질

RenderWorldSnapshot의 본질은 renderer와 game world 사이의 계약이다.

> Client는 이번 프레임에 무엇을 그릴지 snapshot으로 만들고, Engine renderer는 그 snapshot을 backend에 맞게 그린다.

### 문제 인식

게임 클라이언트가 renderer object를 직접 만지기 시작하면 scene logic과 render submission이 엉킨다.

예를 들어 LoL Client에서 champion, minion, map, projectile, FX, debug draw를 직접 renderer class 호출로 뿌리면, Elden Client도 같은 작업을 다시 해야 한다. renderer 최적화도 LoL 코드 안에 묶인다.

더 큰 문제는 "그리는 방식"과 "게임 월드의 의미"가 섞인다는 점이다.

Renderer는 이 mesh가 챔피언인지, 보스인지, 포탑인지 몰라도 된다. Renderer는 mesh, material, texture, transform, camera, depth, tint만 알면 된다.

### Winters의 접근

Winters는 render submission을 snapshot으로 만든다.

코드 근거:

```cpp
struct RenderWorldSnapshot
{
    RenderViewDesc view{};
    std::vector<RenderMeshItem> meshes{};
    std::vector<RenderFxItem> fx{};
    std::vector<RenderDebugItem> debug{};

    void Clear()
    {
        meshes.clear();
        fx.clear();
        debug.clear();
    }
};
```

파일:

- `EngineSDK/inc/Renderer/RenderWorldSnapshot.h`
- `EngineSDK/inc/Renderer/RHISceneRenderer.h`
- `EngineSDK/inc/Renderer/RHIMeshResource.h`
- `EngineSDK/inc/Renderer/RHIMaterialResource.h`

이 구조에서 Product Client가 할 일은 "내 월드를 render item으로 번역하는 것"이다. Renderer가 할 일은 "render item을 효율적으로 제출하는 것"이다.

### 왜 면접에서 매력적인가

이 구조는 실무적인 고민을 보여준다.

- renderer reuse
- draw submission 최적화
- game logic과 render backend 분리
- debug draw와 FX를 같은 제출 구조로 통합
- LoL/Elden product client 분리

### 이력서 문장

> Product Client가 renderer 구현을 직접 조작하지 않도록 `RenderWorldSnapshot` 기반 장면 제출 계약을 설계하고, LoL/Elden 공용 renderer 확장 방향을 마련했습니다.

---

## 5. ECS / Runtime Systems

### 본질

ECS 도메인의 본질은 "모든 것을 ECS로 만들었다"가 아니다.

Winters에서 중요한 것은 다음이다.

> runtime entity state를 component로 분리하고, system이 명시적인 책임 단위로 갱신하도록 만들어 복잡한 gameplay/runtime 흐름을 관측 가능하게 만든다.

### 문제 인식

게임 로직이 커지면 객체 하나가 너무 많은 책임을 가진다.

Champion object가 transform, hp, skill cooldown, buff, animation, input, AI, network replication까지 모두 들고 있으면, 어느 기능이 어떤 상태를 바꾸는지 추적하기 어렵다.

특히 MOBA에서는 한 tick 안에서 이동, 공격, 스킬, buff, death, projectile, AI, replication이 모두 얽힌다. 이때 상태를 명확히 나누지 않으면 버그를 추적하기 어렵다.

### Winters의 접근

Winters는 runtime state를 component로 쪼개고 system 단위로 처리한다.

주요 도메인:

- Transform
- Health
- Stat
- SkillState
- SkillRank
- MoveTarget
- ReplicatedAction
- ChampionAI
- Vision
- Rune
- Buff
- Projectile

주요 코드:

- `Shared/GameSim/Components`
- `Shared/GameSim/Systems`
- `EngineSDK/inc/ECS`

중요한 점은 Shared/GameSim과 Engine ECS의 경계를 구분하는 것이다. Shared/GameSim은 gameplay truth를 위한 deterministic contract를 소유하고, Engine은 generic ECS primitive와 runtime service를 소유한다.

### 면접에서 말할 포인트

ECS 자체보다 중요한 것은 "상태 변경의 책임을 추적 가능하게 만들었다"는 점이다.

예를 들어 AI가 직접 HP를 깎으면 안 된다. AI는 command를 만든다. Damage system이 피해를 처리하고, Death system이 사망을 처리하고, SnapshotBuilder가 결과를 복제한다.

### 이력서 문장

> Champion/Skill/AI/Replication 상태를 component와 system 책임으로 분리해 MOBA gameplay tick에서 상태 변경 경로를 추적 가능하게 구성했습니다.

---

## 6. Server Authority / GameSim

### 본질

Server Authority의 본질은 "서버도 있다"가 아니다.

> gameplay 결과를 누가 만들 권한이 있는가를 명확히 정하는 것이다.

MOBA에서 이 질문은 핵심이다. 스킬이 맞았는지, 쿨다운이 돌았는지, 이동이 막혔는지, 피해가 들어갔는지, 죽었는지는 Client가 결정하면 안 된다.

### 문제 인식

로컬 게임에서 시작하면 Client가 모든 것을 처리하기 쉽다. 입력을 받고, 스킬을 쏘고, 충돌을 보고, 이펙트를 틀고, HP를 깎는다.

하지만 네트워크 게임이 되면 이 구조는 바로 문제가 된다.

- Client마다 판정이 달라질 수 있다.
- cheating에 취약하다.
- replay/regression이 어렵다.
- AI와 human input이 같은 경로를 타지 않는다.
- FX가 gameplay 결과보다 먼저 성공한 것처럼 보인다.

### Winters의 접근

Winters는 command pipeline을 기준으로 나눈다.

코드 근거:

```cpp
struct GameCommandWire
{
    eCommandKind kind = eCommandKind::None;
    uint64_t clientTick = 0;
    uint32_t sequenceNum = 0;
    uint8_t slot = 0;
    NetEntityId targetNet = NULL_NET_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
};

class ICommandExecutor
{
public:
    virtual void ExecuteCommand(CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd) = 0;
};
```

파일:

- `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`
- `Server/Private/Game`
- `Client/Private/Network/Client/CommandSerializer.cpp`

흐름:

```text
Client Input
-> GameCommandWire
-> Server BuildServerCommand
-> CDefaultCommandExecutor::ExecuteCommand
-> Shared/GameSim component mutation
-> Snapshot/Event
-> Client Visual
```

### 면접에서 말할 포인트

여기서 핵심은 AI도 같은 경로를 타야 한다는 점이다.

AI가 직접 적 HP를 깎는 것이 아니라, AI가 command를 생산하고 Server GameSim이 command를 검증/실행한다. Human input과 bot input이 같은 권위 경로를 타면 디버깅과 확장이 쉬워진다.

### 이력서 문장

> Client 입력과 Bot AI 행동을 모두 `GameCommand`로 수렴시키고, Server GameSim이 gameplay truth를 생성하는 권위 파이프라인을 설계했습니다.

---

## 7. Snapshot / Replication

### 본질

Snapshot/Replication의 본질은 서버의 상태를 "그냥 보내는 것"이 아니다.

> 서버 내부 entity state를 Client가 안정적으로 해석할 수 있는 network identity와 deterministic order로 변환하는 것이다.

### 문제 인식

서버 내부의 `EntityID`는 process-local identity다. 이것을 그대로 네트워크로 보내면 안 된다. Client와 Server의 entity allocation 순서가 다를 수 있고, 저장/재접속/복제 경계에서도 의미가 없다.

따라서 network-facing identity가 필요하다.

또한 snapshot 생성 순서가 매번 달라지면 diff, debug, replay, regression이 어려워진다.

### Winters의 접근

SnapshotBuilder는 서버 CWorld를 순회하고, `EntityIdMap`을 통해 network id로 변환한 뒤, 정렬된 snapshot을 만든다.

코드 근거:

- `Server/Private/Game/SnapshotBuilder.cpp`
- `Shared/GameSim/Replication/EntityIdMap.h`
- `Shared/Schemas/Snapshot.fbs`
- `Client/Private/Network/Client/SnapshotApplier.cpp`

SnapshotBuilder 안에서는 Transform을 가진 entity를 수집하고, NetEntityId 기준으로 정렬한다.

```cpp
struct SnapshotEntity
{
    NetEntityId netId = NULL_NET_ENTITY;
    EntityID entity = NULL_ENTITY;
};

std::sort(sorted.begin(), sorted.end(),
    [](const SnapshotEntity& lhs, const SnapshotEntity& rhs)
    {
        return lhs.netId < rhs.netId;
    });
```

### 면접에서 말할 포인트

복제는 "데이터 전송"이 아니라 identity 설계다.

Server 내부 lifetime identity와 network contract identity를 분리해야 한다. 이 관점은 DataDriven의 `DefinitionKey`와도 연결된다.

### 이력서 문장

> Server 내부 `EntityID`와 network-facing `NetEntityId`를 분리하고, deterministic ordering 기반 snapshot 생성/적용 파이프라인을 구현했습니다.

---

## 8. Data Driven Definition Pack

### 본질

Data Driven의 본질은 "JSON을 읽는다"가 아니다.

오히려 Winters의 원칙은 반대다.

> JSON은 authoring/cook input이고, runtime frame code는 검증된 immutable definition pack을 읽는다.

### 문제 인식

초기에는 챔피언 스킬 수치나 쿨다운, 사거리, visual timing을 C++에 직접 박기 쉽다. 하지만 챔피언이 늘어나면 문제가 커진다.

- gameplay 수치가 코드 곳곳에 흩어진다.
- visual timing과 gameplay lock duration이 섞인다.
- designer가 수정해야 할 값과 programmer가 수정해야 할 값이 구분되지 않는다.
- ClientPublic visual 값이 Server authority에 새어 들어간다.
- legacy table을 언제 지워도 되는지 알 수 없다.

### Winters의 접근

Winters는 data ownership을 나눈다.

- ServerPrivate: gameplay truth, skill effect, cooldown, summon policy, AI policy
- ClientPublic: visual timing, animation, cast/recovery frame, model yaw, presentation cue
- Shared/GameSim: definition type과 deterministic lookup behavior

핵심 구조:

```cpp
struct GameplayDefinitionPack
{
    DataPackManifest manifest{};
    const ChampionGameplayDef* champions = nullptr;
    std::size_t championCount = 0u;
    const SkillGameplayDef* skills = nullptr;
    std::size_t skillCount = 0u;
    const SummonerSpellGameplayDef* summonerSpellDefs = nullptr;
    std::size_t summonerSpellCount = 0u;
};
```

파일:

- `Shared/GameSim/Definitions/GameplayDefinitionPack.h`
- `Shared/GameSim/Definitions/GameplayDefinitionQuery.h`
- `Tools/LoLData/Build-LoLDefinitionPack.py`
- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
- `Data/LoL/ServerPrivate`
- `Data/LoL/ClientPublic`

### 검증 루프

Winters는 data migration을 감으로 하지 않는다.

`Verify-LoLDataDrivenPipeline.ps1`은 다음 단계를 묶는다.

```text
Definition pack freshness
Legacy ownership audit
Data-driven goal status
Raw product asset path audit
Client visual timing parity
Build GameSim / Server / Client / SimLab
SimLab deterministic regression
git diff --check
```

### 면접에서 말할 포인트

DataDriven은 "기획자가 JSON을 고치게 했다" 정도로 말하면 약하다.

강한 설명은 이것이다.

> runtime이 읽는 데이터의 소유권, 배포 경계, 검증 루프를 분리했다.

### 이력서 문장

> 챔피언/스킬 하드코딩 값을 generated `GameplayDefinitionPack`으로 이전하고, legacy reader count와 visual/gameplay ownership을 audit하는 DataDriven 전환 파이프라인을 구축했습니다.

---

## 9. Champion Skill / AI

### 본질

Champion AI의 본질은 "그럴듯하게 움직이는 봇"이 아니다.

> AI가 game state를 관찰하고, 서버 권위 경로로 실행 가능한 command sequence를 생산하는 것이다.

### 문제 인식

LeeSin이나 Sylas 같은 챔피언은 단순히 "적이 가까우면 Q" 같은 로직으로는 부족하다.

LeeSin은 다음 흐름이 필요하다.

```text
Q 사용
-> Q 적중 여부 관찰
-> Q2 재사용
-> BA / E / BA / E2
-> ward 설치
-> ward 또는 아군 대상 W
-> R
```

Sylas도 마찬가지다.

```text
Q
-> E1 접근
-> E2 사슬 적중
-> W
-> R 강탈
-> 훔친 궁극기 사용
-> 스킬 후 passive BA timing
```

이것은 단순 스킬 호출 문제가 아니다.

- target selection
- cooldown/rank
- skill readiness
- projectile hit result
- two-stage skill window
- ward entity
- ally/ward targeting
- passive window
- server command sequencing
- debug trace

모두 연결된다.

### Winters의 접근

Winters AI는 상태를 직접 조작하지 않고 command를 만든다. AI context는 world state를 읽고, policy와 valuation을 통해 다음 행동을 고른다.

코드 근거:

- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h`
- `Shared/GameSim/Components/ChampionAIComponent.h`
- `Shared/GameSim/Champions/LeeSin`
- `Shared/GameSim/Champions/Sylas`

AI context 예시:

```cpp
struct ChampionAIContext
{
    EntityID enemyChampion = NULL_ENTITY;
    EntityID lowHpEnemyChampion = NULL_ENTITY;
    EntityID enemyMinion = NULL_ENTITY;
    EntityID enemyStructure = NULL_ENTITY;

    f32_t selfHpRatio = 1.f;
    f32_t enemyHpRatio = 1.f;
    f32_t enemyDistance = 999.f;
    f32_t attackRange = 1.5f;
    f32_t turretDanger = 0.f;
};
```

### 면접에서 말할 포인트

AI를 설명할 때 "리신 콤보를 구현했다"만 말하면 기능 소개로 끝난다.

더 강한 설명은 이것이다.

> 챔피언별 복합 행동을 server-authoritative command sequencing 문제로 정의했다.

AI가 직접 성공을 만들지 않고, command를 통해 Server GameSim에 요청한다는 점이 중요하다.

### 이력서 문장

> LeeSin/Sylas 등 챔피언별 복합 combo를 skill readiness, target valuation, two-stage window, ward/ally targeting을 고려한 server-authoritative command sequencing 문제로 구현했습니다.

---

## 10. FX / UI / Debug Observability

### 본질

FX와 Debug의 본질은 "보기 좋게 만든다"가 아니다.

> 복잡한 gameplay/render 문제를 관측 가능한 상태로 만들어 추측 대신 증거로 고치게 하는 것이다.

### 문제 인식

게임에서는 "뭔가 이상하다"가 자주 발생한다.

- 스킬이 맞은 것 같은데 피해가 안 들어간다.
- AI가 왜 갑자기 뒤로 빠졌는지 모르겠다.
- FX가 두 번 재생된다.
- Client에서는 맞았는데 Server에서는 빗나갔다.
- visual timing과 gameplay lock duration이 어긋난다.

이런 문제를 화면만 보고 고치면 감으로 튜닝하게 된다.

### Winters의 접근

Winters는 debug overlay, bounded trace, FX cue, graph 기반 FX authoring을 통해 관측성을 높인다.

코드 근거:

- `EngineSDK/inc/FX/Graph/FxGraph.h`
- `EngineSDK/inc/FX/Exec/FxExecPlan.h`
- `EngineSDK/inc/Renderer/RHIFxSpriteRenderer.h`
- `Client/Private/UI/AIDebugPanel.cpp`
- `Client/Private/UI/DebugDrawSystem.cpp`

FX Graph 구조:

```cpp
struct CFxGraph
{
    std::vector<FxGraphUserParam> userParams;
    std::vector<FxEmitterGraph> emitterGraphs;

    static bool_t LoadFromJson(
        const std::string& strPath,
        CFxGraph& outGraph,
        std::string* pOutError = nullptr);
};
```

### 면접에서 말할 포인트

Debug UI는 부가 기능이 아니다. 복잡한 시스템을 실제로 완성하려면 관측 가능성이 필요하다.

특히 AI, networking, FX는 "보이는 결과"와 "서버 truth"가 다를 수 있다. 그래서 DebugDraw, AI trace, FX cue가 중요하다.

### 이력서 문장

> AI/스킬/FX/네트워크 문제를 재현 가능하게 분석하기 위해 debug overlay, bounded trace, FX cue, graph 기반 FX 파이프라인을 구축했습니다.

---

## 11. Asset Format / Resource Pipeline

### 본질

Asset Pipeline의 본질은 "모델 파일을 로드한다"가 아니다.

> 외부 리소스를 runtime이 안정적으로 읽을 수 있는 엔진 고유 포맷과 검증 가능한 배포 단위로 변환하는 것이다.

### 문제 인식

처음에는 FBX, PNG, extracted texture를 바로 읽으면 빠르다. 하지만 프로젝트가 커지면 문제가 생긴다.

- 원본 에셋 경로가 장비마다 다르다.
- Map/Texture 폴더가 10GB, 50GB 단위로 커진다.
- git에 올릴 것과 Drive로 공유할 것이 불분명하다.
- material/texture binding이 깨진다.
- skeleton/animation이 runtime에서 매번 해석된다.
- 저작권상 공개 가능한 asset과 code pipeline이 섞인다.

### Winters의 접근

Winters는 runtime binary format을 지향한다.

대표 포맷:

- `.wmesh`
- `.wmat`
- `.wtex`
- `.wskel`
- `.wanim`
- `.wfx`
- `.wseq`
- `.wmap`

코드 근거:

- `EngineSDK/inc/AssetFormat/Mesh/WMeshLoader.h`
- `EngineSDK/inc/AssetFormat/Material`
- `EngineSDK/inc/AssetFormat/Anim`
- `Tools/WintersAssetConverter`
- `Tools/EldenAssetPipeline`

`WMeshLoaded` 예시:

```cpp
struct WMeshLoaded
{
    MeshMetaHeader header;
    std::vector<SubMeshDesc> subMeshes;
    std::vector<BoneEntry> bones;
    std::vector<SubMeshBounds> bounds;

    const uint8_t* pVertexBlob = nullptr;
    size_t uVertexBlobBytes = 0;
    const uint8_t* pIndexBlob = nullptr;
    size_t uIndexBlobBytes = 0;

    std::vector<uint8_t> m_vRawFile;
};
```

### 면접에서 말할 포인트

리소스 파이프라인은 엔진 개발자의 중요한 역량이다. 게임은 코드만으로 돌아가지 않고, asset import, conversion, validation, runtime loading, packaging이 함께 맞아야 한다.

Elden 방향에서도 중요한 점은 "원본 에셋을 공개한다"가 아니라 "각 장비에서 추출한 리소스를 자체 포맷과 공유 가능한 패키지로 정리한다"는 것이다.

### 이력서 문장

> 외부 리소스를 runtime에서 직접 해석하지 않고 `.wmesh/.wmat/.wtex/.wskel/.wanim` 등 Winters binary format으로 변환·검증하는 에셋 파이프라인을 설계했습니다.

---

## 12. World Partition / Elden Direction

### 본질

World Partition의 본질은 "큰 맵을 불러온다"가 아니다.

> 플레이어나 카메라 같은 streaming source를 기준으로 world cell의 상태를 전환하고, 필요한 asset과 visible instance를 runtime에 공급하는 구조다.

### 문제 인식

LoL형 MOBA는 제한된 arena map을 기준으로 한다. 하지만 Elden형 Action RPG는 훨씬 넓은 공간, streaming, visibility, skinned character, lock-on camera, boss arena, asset loading boundary가 필요하다.

LoL 구조에 Elden을 억지로 붙이면 두 문제가 생긴다.

1. LoL Client가 Elden 예외를 품는다.
2. Elden Client가 LoL renderer/runtime을 복사한다.

둘 다 피해야 한다.

### Winters의 접근

Winters는 World Partition과 Asset Streaming을 Engine generic domain으로 둔다.

코드 근거:

- `EngineSDK/inc/World/WorldPartitionSystem.h`
- `EngineSDK/inc/World/AssetStreamingSystem.h`
- `EngineSDK/inc/World/WorldPartitionTypes.h`
- `Tools/EldenAssetPipeline`

핵심 API:

```cpp
class WINTERS_ENGINE CWorldPartitionSystem final
{
public:
    bool_t LoadWorld(const std::string& strWorldJsonPath);
    void SetSource(u32_t uSourceId, const StreamingSourceComponent& src);
    void Update(f32_t fDeltaTime);
    void CollectVisibleInstances(std::vector<VisibleInstance>& out) const;
};
```

### 면접에서 말할 포인트

Elden은 "앞으로 만들 예정인 게임"만으로 쓰면 약하다. 대신 이렇게 말해야 한다.

> Winters가 LoL 전용 엔진이 되지 않도록 두 번째 product client 요구사항으로 Action RPG를 설정했고, 그 요구사항을 World Partition, Asset Streaming, RHI renderer reuse 문제로 분해했다.

### 이력서 문장

> LoL arena 구조에 갇히지 않도록 Elden형 Action RPG 요구사항을 World Partition, Asset Streaming, skinned animation, lock-on camera slice로 분해해 엔진 확장 방향을 설계했습니다.

---

## 13. Verification Pipeline

### 본질

검증 파이프라인의 본질은 "빌드가 된다"가 아니다.

> 구조 변경이 실제 runtime/data/client/server 경계를 깨지 않았는지 반복 가능한 명령으로 확인하는 것이다.

### 문제 인식

엔진 리팩터링은 위험하다. RHI 경계를 바꾸면 Client가 깨질 수 있고, DataDriven 전환을 하면 gameplay 수치가 달라질 수 있고, Server authority를 바꾸면 snapshot이 어긋날 수 있다.

이때 "한 번 실행해봤는데 괜찮았다"는 검증이 아니다.

필요한 것은 루프다.

```text
변경
-> 정의팩 freshness
-> legacy audit
-> goal status
-> visual parity
-> GameSim/Server/Client/SimLab build
-> deterministic regression
-> whitespace/diff check
-> report
```

### Winters의 접근

검증 스크립트:

- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`
- `Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1`
- `Tools/SimLab/SimLab.vcxproj`

검증 단계:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1
msbuild Shared/GameSim/Include/GameSim.vcxproj
msbuild Server/Include/Server.vcxproj
msbuild Client/Include/Client.vcxproj
Tools/Bin/Debug/SimLab.exe
git diff --check
```

### 면접에서 말할 포인트

이 도메인은 너의 강한 차별점이다.

많은 포트폴리오는 "제가 이렇게 구현했습니다"에서 끝난다. Winters는 "이 변경이 어디까지 반영됐고, 어떤 count가 줄었고, 어떤 검증이 통과했는지"를 추적한다.

이건 실무에서 매우 중요하다. 대형 리팩터링은 계획보다 검증 루프가 더 중요하기 때문이다.

### 이력서 문장

> DataDriven/RHI/GameSim 리팩터링마다 definition freshness, legacy audit, visual parity, multi-project build, SimLab deterministic regression을 묶은 검증 루프를 운영했습니다.

---

## 14. Collaboration / Documentation Pipeline

### 본질

협업 문서의 본질은 "문서를 많이 쓴다"가 아니다.

> 여러 작업자가 같은 코드베이스를 건드릴 때 충돌을 줄이고, 변경의 의도와 검증 기준을 공유하는 것이다.

### 문제 인식

혼자 작업하더라도 데스크탑과 노트북에서 동시에 작업하면 사실상 협업 문제와 비슷해진다.

- 같은 파일을 수정하면 conflict가 난다.
- 어떤 세션이 어떤 목표를 진행 중인지 모른다.
- build 중간 산출물이 충돌한다.
- Resource와 code ownership이 섞인다.
- 문서 없이 작업하면 다음 세션에서 맥락이 끊긴다.

### Winters의 접근

Winters는 문서와 규칙을 코드베이스 안에 둔다.

주요 문서:

- `AGENTS.md`
- `.claude/gotchas.md`
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`
- `.md/plan`
- `.md/TODO`
- `.md/이력서`

핵심 규칙:

- Engine은 product client code를 include하지 않는다.
- Shared/GameSim은 Engine/Client/Renderer/UI/DX type에 의존하지 않는다.
- Client는 presentation을 담당하고 gameplay truth를 만들지 않는다.
- Runtime resource는 `Client/Bin/Resource`에서만 해석한다.
- architecture decision은 Compass에, 반복 실수는 gotchas에 남긴다.

### 면접에서 말할 포인트

협업 경험은 팀 프로젝트만으로 증명되지 않는다. 더 중요한 것은 "충돌이 날 수밖에 없는 작업을 어떻게 나누고 기록했는가"다.

SR_MinecraftDungeons 팀 프로젝트 경험과 Winters의 문서/검증 파이프라인을 연결하면 좋다.

### 이력서 문장

> 데스크탑/노트북 및 팀 작업 환경에서 파일 ownership, architecture compass, gotcha log, 검증 report를 통해 충돌을 줄이는 문서 기반 협업 파이프라인을 운영했습니다.

---

## 15. 티스토리 글로 바꿀 때의 순서

이 문서는 한 번에 올리는 글이 아니다. 아래 순서로 쪼개서 올린다.

1. 왜 자체 엔진을 만들었나
2. Product Client Separation
3. Engine Runtime 경계
4. DX11에서 RHI로
5. RenderWorldSnapshot과 공용 Scene Renderer
6. ECS와 Runtime Systems
7. Server Authority / GameSim
8. Snapshot / Replication
9. DataDriven Definition Pack
10. Champion Skill / AI
11. FX / UI / Debug Observability
12. Asset Format / Resource Pipeline
13. World Partition / Elden Direction
14. Verification Pipeline
15. Collaboration / Documentation Pipeline

각 글 마지막에는 반드시 이 문장을 넣는다.

```text
이 글을 이력서 문장으로 압축하면:
...
```

---

## 16. 전체 압축 이력서 문장

Winters Engine 전체를 하나의 이력서 bullet로 압축하면 다음이다.

> 학습용 DX11 엔진의 단일 클라이언트/하드코딩/렌더러 고정 한계를 문제로 정의하고, Engine/Product Client 분리, RHI, RenderWorldSnapshot, 서버 권위 GameSim, DataDriven Definition Pack, Asset/World/Verification 파이프라인을 직접 설계해 LoL형 MOBA와 Elden형 Action RPG 확장 기반을 구축했습니다.

조금 더 짧게 쓰면:

> C++ 자체 엔진 Winters에서 RHI, 서버 권위 GameSim, DataDriven Definition Pack, RenderWorldSnapshot, Asset/World 파이프라인을 설계해 LoL형 MOBA와 Elden형 Action RPG 클라이언트 확장 기반을 구축했습니다.

