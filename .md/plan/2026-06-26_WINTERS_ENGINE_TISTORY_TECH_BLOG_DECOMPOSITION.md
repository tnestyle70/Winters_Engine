Session - Winters Engine을 면접관이 읽는 기술 블로그 시리즈로 해부한다.

## 0. 목표

Winters Engine 블로그의 목표는 "기술 스택을 많이 나열했다"가 아니다.

목표는 다음 문장을 증명하는 것이다.

> 기존 학습용/레거시 엔진의 한계를 문제로 정의하고, C++ 자체 엔진의 런타임, 렌더링, 서버 권위 시뮬레이션, 데이터 주도 파이프라인, 에셋/툴 체인을 코드와 검증 루프로 해결했다.

면접관이 글을 읽고 얻어야 하는 인상은 다음이다.

- 문제를 감으로 고치지 않고 구조로 정의한다.
- 코드 경계와 의존 방향을 의식한다.
- 런타임, 데이터, 툴, 검증을 한 덩어리의 제품 파이프라인으로 본다.
- 기능 구현보다 재현 가능한 검증 루프를 중시한다.
- LoL형 MOBA와 Elden형 Action RPG를 같은 엔진 위에 올리려는 장기 설계가 있다.

## 1. 블로그 전체 포지셔닝

### 핵심 한 줄

Winters Engine은 MOBA와 Action RPG를 하나의 C++ 런타임 위에 분리해 올리기 위해 RHI, 서버 권위 GameSim, 데이터 주도 정의팩, 렌더링, 에셋/툴 파이프라인을 직접 설계하고 검증한 자체 게임 엔진이다.

### 블로그 시리즈 이름 후보

- Winters Engine 해부기
- 자체 엔진으로 MOBA와 Action RPG를 동시에 지탱하기
- 기능 구현을 넘어 엔진 구조를 의심하기
- 학습용 DX11 엔진의 한계에서 Winters Engine까지

추천:

> Winters Engine 해부기 - 학습용 엔진의 한계를 구조로 해결하기

## 2. 글 하나의 고정 포맷

각 글은 아래 순서를 지킨다.

1. 문제 정의
   - 무엇을 문제라고 봤는가?
   - 왜 단순 구현으로는 충분하지 않았는가?

2. 제약 조건
   - 기존 코드/런타임/협업/성능/리소스 제약은 무엇이었는가?

3. 설계 선택
   - 어떤 경계나 계약을 만들었는가?
   - 왜 그 선택을 했는가?

4. 실제 코드 근거
   - 헤더/구조체/함수 중심으로 짧게 보여준다.
   - 긴 코드 복붙보다 핵심 타입과 흐름만 보여준다.

5. 검증 루프
   - 어떤 명령, 빌드, 스모크, audit, regression으로 확인했는가?

6. 배운 점
   - 다음 설계로 이어지는 한계와 개선 방향을 적는다.

7. 이력서 문장
   - 글의 결론을 이력서 bullet 1개로 압축한다.

## 3. 코드 기반 도메인 지도

### 3-1. Engine Runtime

본질:

게임별 클라이언트가 엔진 코드를 복사하지 않고, 공용 런타임 DLL 위에서 각자의 scene/input/presentation만 구성하도록 만드는 기반이다.

코드 근거:

- `EngineSDK/inc/WintersEngine.h`
- `EngineSDK/inc/GameInstance.h`
- `EngineSDK/inc/Scene`
- `Engine/Public/Core`
- `Engine/Public/Platform`

블로그에서 강조할 문제:

학습용 엔진은 보통 한 게임에 맞춰 직접 결합된다. Winters는 LoL Client와 Elden Client를 같은 `WintersEngine.dll` 위에 올릴 수 있도록 Engine/Client 경계를 먼저 문제로 정의했다.

이력서 문장:

- 게임별 클라이언트가 공용 C++ 런타임을 재사용하도록 Engine/Client 의존 경계를 분리하고 `WintersEngine.dll` 기반 실행 구조를 설계했습니다.

### 3-2. RHI

본질:

DX11 코드가 Client/Public와 게임 로직으로 새지 않게 막고, 장기적으로 DX12/Vulkan/console backend를 붙일 수 있는 렌더링 추상화 경계다.

코드 근거:

- `EngineSDK/inc/RHI/IRHIDevice.h`
- `EngineSDK/inc/RHI/IRHICommandList.h`
- `EngineSDK/inc/RHI/IRHIQueue.h`
- `EngineSDK/inc/RHI/RHIHandles.h`
- `EngineSDK/inc/RHI/RHIDescriptors.h`

핵심 코드:

```cpp
class WINTERS_ENGINE IRHIDevice
{
public:
    virtual eRHIBackend GetBackend() const = 0;
    virtual void BeginFrame(f32_t r, f32_t g, f32_t b, f32_t a) = 0;
    virtual void EndFrame() = 0;
    virtual IRHICommandList* CreateCommandList() { return nullptr; }
    virtual RHIPipelineHandle CreatePipeline(const RHIPipelineDesc& desc) = 0;
    virtual RHIRenderPassHandle CreateRenderPass(const RHIRenderPassDesc& desc) = 0;
};
```

블로그에서 강조할 문제:

"DX11로 일단 그린다"는 단기적으로 빠르지만, 나중에 Elden Client, DX12, editor, renderer reuse가 들어오면 concrete API가 경계를 오염시킨다. 그래서 먼저 `IRHIDevice`와 handle/descriptor 기반 계약을 세웠다.

이력서 문장:

- DX11 concrete renderer의 확장 한계를 문제로 정의하고, `IRHIDevice`/RHI handle/descriptor 기반 추상화로 LoL/Elden 공용 렌더링 경계를 설계했습니다.

### 3-3. RenderWorldSnapshot / Scene Renderer

본질:

게임 월드가 renderer class를 직접 조작하지 않고, "이번 프레임에 그릴 것"을 snapshot 데이터로 넘기는 경계다.

코드 근거:

- `EngineSDK/inc/Renderer/RenderWorldSnapshot.h`
- `EngineSDK/inc/Renderer/RHISceneRenderer.h`
- `EngineSDK/inc/Renderer/RHIMeshResource.h`
- `EngineSDK/inc/Renderer/RHIMaterialResource.h`

핵심 코드:

```cpp
struct RenderWorldSnapshot
{
    RenderViewDesc view{};
    std::vector<RenderMeshItem> meshes{};
    std::vector<RenderFxItem> fx{};
    std::vector<RenderDebugItem> debug{};
};
```

블로그에서 강조할 문제:

LoL과 Elden이 renderer를 각각 복사하면 유지보수와 최적화가 갈라진다. Client는 snapshot을 만들고 Engine renderer는 snapshot을 그리는 구조로 분리해야 한다.

이력서 문장:

- LoL/Elden 클라이언트가 renderer 구현을 복제하지 않도록 `RenderWorldSnapshot` 기반 장면 제출 계약을 설계했습니다.

### 3-4. Server Authority / GameSim

본질:

게임플레이 결과는 Client가 아니라 Server/Shared GameSim이 만든다. Client는 input과 visual presentation만 담당한다.

코드 근거:

- `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`
- `Shared/GameSim/Core/World/World.h`
- `Shared/GameSim/Components`
- `Server/Private/Game`

핵심 흐름:

```text
Client Input
-> GameCommandWire
-> Server BuildServerCommand
-> CDefaultCommandExecutor::ExecuteCommand
-> Shared/GameSim component mutation
-> Snapshot/Event
-> Client Visual
```

핵심 코드:

```cpp
struct GameCommandWire
{
    eCommandKind kind = eCommandKind::None;
    uint64_t clientTick = 0;
    uint32_t sequenceNum = 0;
    uint8_t slot = 0;
    NetEntityId targetNet = NULL_NET_ENTITY;
    Vec3 groundPos{};
};

class ICommandExecutor
{
public:
    virtual void ExecuteCommand(CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd) = 0;
};
```

블로그에서 강조할 문제:

MOBA는 스킬, 이동, 공격, 쿨다운, AI가 모두 판정 신뢰성과 연결된다. Client에서 성공 여부를 만들면 네트워크/동기화/치팅/재현성이 무너진다. 따라서 command와 snapshot을 기준으로 authority를 분리했다.

이력서 문장:

- MOBA gameplay truth를 Client visual에서 분리하고, `GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 서버 권위 파이프라인을 설계했습니다.

### 3-5. Snapshot / Replication

본질:

서버의 권위 상태를 deterministic order로 수집하고, Client가 적용할 수 있는 snapshot/event 데이터로 직렬화하는 경계다.

코드 근거:

- `Server/Private/Game/SnapshotBuilder.cpp`
- `Shared/Schemas/Snapshot.fbs`
- `Shared/GameSim/Replication/EntityIdMap.h`
- `Client/Private/Network/Client/SnapshotApplier.cpp`

블로그에서 강조할 문제:

네트워크 게임은 "현재 CWorld에 뭐가 있나"보다 "어떤 순서와 어떤 identity로 복제할 것인가"가 핵심이다. 그래서 process-local `EntityID`와 network-facing `NetEntityId`를 분리하고, snapshot build 단계에서 정렬한다.

이력서 문장:

- 서버 CWorld 상태를 안정적인 network identity와 deterministic ordering으로 snapshot화해 Client visual에 적용하는 복제 파이프라인을 구현했습니다.

### 3-6. Data Driven Definition Pack

본질:

JSON은 authoring/cook input이고, runtime frame code는 검증된 immutable pack을 읽는다.

코드 근거:

- `Shared/GameSim/Definitions/GameplayDefinitionPack.h`
- `Shared/GameSim/Definitions/GameplayDefinitionQuery.h`
- `Tools/LoLData/Build-LoLDefinitionPack.py`
- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
- `Data/LoL/ServerPrivate`
- `Data/LoL/ClientPublic`

핵심 코드:

```cpp
struct GameplayDefinitionPack
{
    DataPackManifest manifest{};
    const ChampionGameplayDef* champions = nullptr;
    std::size_t championCount = 0u;
    const SkillGameplayDef* skills = nullptr;
    std::size_t skillCount = 0u;
};
```

검증 코드:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
powershell -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1
```

블로그에서 강조할 문제:

챔피언 스킬 수치, visual timing, summon policy, AI policy가 코드에 흩어지면 협업과 회귀 방지가 어렵다. 그래서 ServerPrivate gameplay와 ClientPublic visual을 분리하고 legacy reader count를 줄이는 방식으로 cutover한다.

이력서 문장:

- 챔피언/스킬 하드코딩 값을 JSON authoring에서 generated definition pack으로 cook하고, audit script로 legacy reader count를 추적하는 data-driven migration을 수행했습니다.

### 3-7. Champion Skill / AI

본질:

AI는 상태를 직접 조작하는 치트 로직이 아니라, 관찰한 상태를 바탕으로 서버에 command를 생산하는 의사결정 시스템이어야 한다.

코드 근거:

- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h`
- `Shared/GameSim/Components/ChampionAIComponent.h`
- `Shared/GameSim/Champions/LeeSin`
- `Shared/GameSim/Champions/Sylas`

블로그에서 강조할 문제:

LeeSin Q/Q2/ward W/R combo, Sylas Q/E/W/R steal 같은 행동은 단순 if문 스크립트가 아니라, target valuation, cooldown/rank, skill readiness, command sequencing, server authority가 동시에 걸린 문제다.

이력서 문장:

- LeeSin/Sylas 등 챔피언별 combo AI를 서버 권위 command 생산 문제로 정의하고, skill readiness/target valuation/debug trace 기반으로 확장 가능한 AI 시스템을 구축했습니다.

### 3-8. FX / UI / Debug

본질:

복잡한 gameplay/render 문제를 감으로 고치지 않기 위해 visual cue, debug panel, trace, FX graph를 만들어 관측 가능성을 높인다.

코드 근거:

- `EngineSDK/inc/FX/Graph/FxGraph.h`
- `EngineSDK/inc/FX/Exec/FxExecPlan.h`
- `EngineSDK/inc/Renderer/RHIFxSpriteRenderer.h`
- `Client/Private/UI/AIDebugPanel.cpp`
- `Client/Private/UI/DebugDrawSystem.cpp`

핵심 코드:

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

블로그에서 강조할 문제:

스킬이 맞았는지, AI가 왜 그 행동을 했는지, FX가 중복 재생됐는지 보이지 않으면 디버깅은 추측이 된다. Winters는 FX cue/debug overlay/trace를 통해 관측 가능한 파이프라인을 만든다.

이력서 문장:

- AI/스킬/FX 문제를 재현 가능하게 분석하기 위해 debug overlay, bounded trace, FX graph 기반 관측 파이프라인을 구축했습니다.

### 3-9. Asset Format / Resource Pipeline

본질:

외부 포맷을 runtime에서 직접 끌어 쓰는 것이 아니라 Winters runtime binary format으로 변환하고 검증해 로드한다.

코드 근거:

- `EngineSDK/inc/AssetFormat/Mesh/WMeshLoader.h`
- `EngineSDK/inc/AssetFormat/Material`
- `EngineSDK/inc/AssetFormat/Anim`
- `Tools/WintersAssetConverter`
- `Tools/EldenAssetPipeline`

핵심 코드:

```cpp
struct WMeshLoaded
{
    MeshMetaHeader header;
    std::vector<SubMeshDesc> subMeshes;
    std::vector<BoneEntry> bones;
    const uint8_t* pVertexBlob = nullptr;
    const uint8_t* pIndexBlob = nullptr;
    std::vector<uint8_t> m_vRawFile;
};
```

블로그에서 강조할 문제:

리소스는 "파일을 찾으면 로드"가 아니다. 경로, 포맷, material binding, texture, skeleton, animation, legal boundary, 배포 크기까지 연결된 pipeline 문제다.

이력서 문장:

- `.wmesh/.wmat/.wtex/.wskel/.wanim` 등 Winters runtime binary format 기반 에셋 로딩/변환 파이프라인을 설계했습니다.

### 3-10. World Partition / Elden Direction

본질:

LoL 맵처럼 한정된 arena만 처리하는 것이 아니라, Elden형 Action RPG를 위해 streaming source, cell state, visible instance 수집 구조를 준비한다.

코드 근거:

- `EngineSDK/inc/World/WorldPartitionSystem.h`
- `EngineSDK/inc/World/AssetStreamingSystem.h`
- `EngineSDK/inc/World/WorldPartitionTypes.h`
- `.md/EldenRing/00_ELDENRING_INDEX.md`
- `Tools/EldenAssetPipeline`

핵심 코드:

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

블로그에서 강조할 문제:

Elden Client는 LoL Client의 복붙이 아니라 같은 Engine contract 위에 별도 product client로 올라가야 한다. 이를 위해 world partition, streaming, skinned character, lock-on camera, action combat을 별도 slice로 검증한다.

이력서 문장:

- LoL 전용 구조에 갇히지 않도록 World Partition/Asset Streaming 기반 Elden형 Action RPG client 확장 방향을 설계했습니다.

### 3-11. Verification Pipeline

본질:

엔진 구조 변경은 "빌드 됨"만으로 끝나지 않는다. 데이터 freshness, legacy audit, visual parity, GameSim/Server/Client/SimLab build, deterministic regression을 묶어야 한다.

코드 근거:

- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
- `Tools/SimLab/SimLab.vcxproj`
- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`
- `Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1`

핵심 단계:

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

블로그에서 강조할 문제:

대형 리팩터링은 감으로 진행하면 어느 순간 무엇이 깨졌는지 모른다. 목표 count와 검증 command를 같이 둬야 루프가 닫힌다.

이력서 문장:

- DataDriven/RHI/GameSim 변경마다 audit, build, deterministic regression을 묶은 검증 파이프라인을 운영해 회귀를 추적했습니다.

## 4. 추천 연재 순서

### 1편. 왜 자체 엔진을 만들었나

목표:

Winters Engine의 문제 정의를 설명한다.

핵심 문장:

> 저는 기능을 더 많이 붙이기 전에, 기능이 계속 붙을수록 무너지는 구조 자체를 문제로 봤습니다.

넣을 내용:

- 학습용 DX11 엔진의 한계
- 단일 게임/단일 renderer/단일 스레드/하드코딩 중심 구조의 한계
- LoL과 Elden을 같은 엔진 위에 올리려는 목표
- Engine/Client/Shared/Server/Tools 경계

### 2편. Engine/Client/Shared/Server 경계를 나눈 이유

목표:

게임플레이 truth와 presentation을 분리한 이유를 설명한다.

코드 근거:

- `ICommandExecutor`
- `TickContext`
- `GameCommandWire`
- `SnapshotBuilder`

### 3편. DX11에서 RHI로: renderer를 제품 코드에서 떼어내기

목표:

RHI가 추상화 놀이가 아니라 제품 확장성 문제였음을 설명한다.

코드 근거:

- `IRHIDevice`
- RHI handle/descriptor

### 4편. RenderWorldSnapshot: LoL과 Elden이 같은 renderer를 쓰게 하는 계약

목표:

renderer 재사용과 product client 분리를 설명한다.

코드 근거:

- `RenderWorldSnapshot`
- `RHISceneRenderer`

### 5편. 서버 권위 MOBA 파이프라인

목표:

MOBA에서 왜 Client가 판정을 만들면 안 되는지 설명한다.

코드 근거:

- `GameCommandWire`
- `BuildServerCommand`
- `CDefaultCommandExecutor`
- `SnapshotBuilder`

### 6편. 하드코딩에서 DataDriven Definition Pack으로

목표:

데이터 소유권과 협업 구조를 설명한다.

코드 근거:

- `GameplayDefinitionPack`
- `GameplayDefinitionQuery`
- `Build-LoLDefinitionPack.py`
- `Verify-LoLDataDrivenPipeline.ps1`

### 7편. 챔피언 스킬과 AI를 command 문제로 바라보기

목표:

LeeSin/Sylas combo AI를 단순 if문이 아니라 서버 권위 command sequencing 문제로 설명한다.

코드 근거:

- `ChampionAISystem`
- `ChampionAIPolicy`
- `ChampionAIComponent`
- `LeeSinGameSim`
- `SylasGameSim`

### 8편. Snapshot/Event/FX Cue: 서버 결과를 클라이언트에 보이게 만들기

목표:

결과와 표현을 연결하는 replication/presentation bridge를 설명한다.

코드 근거:

- `SnapshotBuilder`
- `SnapshotApplier`
- FX cue 관련 경로

### 9편. FX Graph와 Debug Overlay: 추측 대신 관측하기

목표:

디버깅 가능성을 기술력으로 보여준다.

코드 근거:

- `FxGraph`
- `AIDebugPanel`
- `DebugDrawSystem`

### 10편. Winters Asset Pipeline

목표:

리소스 로딩을 제품 파이프라인 문제로 설명한다.

코드 근거:

- `WMeshLoader`
- `WintersAssetConverter`
- `EldenAssetPipeline`

### 11편. Elden Client를 위한 World Partition/Streaming

목표:

LoL 전용 엔진이 아니라 Action RPG까지 확장되는 구조를 설명한다.

코드 근거:

- `CWorldPartitionSystem`
- `CAssetStreamingSystem`

### 12편. 검증 파이프라인: 엔진 변경을 끝까지 닫는 법

목표:

개발 태도와 협업 신뢰성을 보여준다.

코드 근거:

- `Verify-LoLDataDrivenPipeline.ps1`
- `SimLab`
- `git diff --check`

## 5. 1편 초안

제목:

> Winters Engine 해부기 1 - 왜 자체 엔진을 만들었나

도입:

게임 프로젝트를 여러 개 만들다 보면 처음에는 "기능을 어떻게 구현할까"가 가장 큰 문제처럼 보인다. 이동, 공격, 스킬, UI, 렌더링, 에셋 로딩을 하나씩 붙이면 게임은 점점 돌아가는 것처럼 보인다.

하지만 어느 순간부터 진짜 문제는 기능이 아니라 구조가 된다.

새로운 챔피언을 추가할 때마다 코드가 흩어진다. 렌더링 코드는 DX11 타입에 묶인다. 클라이언트가 판정과 표현을 동시에 들고 있다. 데이터는 JSON, C++ 상수, 툴 출력물 사이에 흩어진다. 팀 작업을 하려면 어떤 파일을 누가 수정해야 하는지부터 충돌이 난다.

저는 이 지점을 Winters Engine의 출발점으로 잡았다.

본문 핵심:

Winters Engine의 첫 질문은 "어떤 기능을 더 만들까?"가 아니라 "왜 기능을 만들수록 구조가 무너질까?"였다.

그래서 엔진을 다음 다섯 경계로 나눴다.

- Engine: frame loop, renderer, RHI, ECS primitive, resource, editor/runtime service
- Client: input, camera, scene, UI state, animation/FX playback
- Shared/GameSim: deterministic gameplay contract, component, command, skill validation
- Server: command authority, snapshot/event/FX cue 송신
- Tools: asset import, data cook, validation, build verification

가장 중요한 원칙은 이것이다.

```text
Client Input
-> GameCommand
-> Server GameSim
-> Snapshot/Event
-> Client Visual
```

이 흐름을 지키면 Client는 더 화려한 visual을 만들 수 있지만 gameplay truth를 새로 만들지는 않는다. Server는 visual 성공 여부를 추측하지 않고 GameSim 결과만 복제한다. Engine은 LoL이나 Elden 같은 제품 코드를 include하지 않는다.

결론:

Winters Engine은 단순히 C++로 만든 게임 엔진이 아니다. 제가 프로젝트를 진행하면서 마주친 구조적 한계를 문제로 정의하고, 그 문제를 의존성, 런타임 계약, 데이터 소유권, 검증 파이프라인으로 풀어낸 결과물이다.

이력서 문장:

- 학습용 DX11 엔진의 확장 한계를 문제로 정의하고, Engine/Client/Shared GameSim/Server/Tools 경계를 분리한 C++ 자체 엔진 Winters를 설계했습니다.

## 6. 티스토리 작성 원칙

### 해야 할 것

- 글마다 실제 코드 파일 2~4개만 보여준다.
- 코드 전체보다 타입/함수/흐름을 보여준다.
- "무엇을 만들었다"보다 "무엇을 문제로 봤다"로 시작한다.
- 글 끝에는 이력서 bullet로 압축한다.
- 이미지가 가능하면 아키텍처 다이어그램, 흐름도, 실제 실행 캡처를 넣는다.

### 피해야 할 것

- 코드 전체를 길게 붙여넣기
- "AAA급", "언리얼급" 같은 과장 표현
- 아직 검증하지 않은 기능을 완료형으로 쓰기
- Elden 원본 에셋을 공개 가능한 것처럼 보이게 쓰기
- 단순 기능 목록식 글

### 면접관이 좋아할 표현

- 문제 정의
- 권위 경계
- 런타임 계약
- 데이터 소유권
- 회귀 방지
- deterministic regression
- generated definition pack
- product client separation
- backend-neutral renderer
- audit/build/verification loop

## 7. 작성 체크리스트

각 글을 올리기 전에 확인한다.

- 이 글의 문제 정의가 한 문장으로 보이는가?
- 실제 코드 파일이 2개 이상 근거로 들어갔는가?
- 단순 자랑이 아니라 tradeoff가 보이는가?
- 검증 방법이나 실패 경험이 들어갔는가?
- 이력서 bullet 하나로 압축 가능한가?
- 공개하면 안 되는 리소스/저작권/개인 경로가 노출되지 않았는가?

## 8. 다음 작업

1. 1편 글을 완성본으로 다듬는다.
2. 1편에 넣을 아키텍처 다이어그램을 만든다.
3. 2편부터는 각 도메인별 실제 코드 캡처와 짧은 설명을 붙인다.
4. GitHub README의 "Architecture / Verification / Blog Series" 섹션과 연결한다.
5. 이력서 프로젝트 설명은 각 글 끝의 이력서 문장을 모아 압축한다.

## 9. 검증

문서 작성 검증:

```powershell
git diff -- .md/plan/2026-06-26_WINTERS_ENGINE_TISTORY_TECH_BLOG_DECOMPOSITION.md
```

블로그용 코드 근거 검증:

```powershell
Test-Path EngineSDK/inc/RHI/IRHIDevice.h
Test-Path EngineSDK/inc/Renderer/RenderWorldSnapshot.h
Test-Path Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h
Test-Path Shared/GameSim/Definitions/GameplayDefinitionPack.h
Test-Path Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

