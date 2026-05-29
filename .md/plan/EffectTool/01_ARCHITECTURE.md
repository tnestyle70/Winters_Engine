# FX 아키텍처 — ECS 통합 + GameInstance 경계 + 디렉토리

## 디렉토리

```
Engine/Public/FX/
├── Core/
│   ├── FxTypes.h                 enum (AttrType/NodeKind/Stage/FxValue variant)
│   ├── FxConfig.h                튜닝 값 (max particles, default capacity)
│   └── FxAttributeRegistry.h     Position/Velocity/Color 등 표준 attribute 등록
├── Graph/
│   ├── FxGraph.h                 NodeId/PinId/Edge/Node + 그래프 컨테이너
│   ├── FxGraphSerializer.h       JSON 입출력 (nlohmann::json 고려)
│   ├── FxTopoSort.h              Kahn's 위상 정렬
│   └── FxGraphValidator.h        DAG 검증 + 타입 체크 + 누락 attr 검출
├── Pool/
│   ├── ParticlePool.h            SoA 저장소 + allocate/kill
│   └── AttributeView.h           타입 안전 Span<T> 래퍼
├── Executor/
│   ├── FxExecutor.h              컴파일 + 스테이지 실행
│   ├── FxExecContext.h           실행 컨텍스트 (dt, rng, spawn range)
│   ├── Nodes/
│   │   ├── SpawnNodes.h          SpawnBurst, SpawnRate
│   │   ├── InitNodes.h           InitPosition, InitVelocity, InitLifetime
│   │   ├── UpdateNodes.h         Gravity, Drag, CurlNoise, AgeAndKill
│   │   └── NodeRegistry.h        NodeKind → NodeExecFn 매핑
│   └── FxNodeImpl.cpp            각 노드 구현 (.cpp 분리)
├── Expression/                   ★ Stage 4
│   ├── FxBytecode.h              Op / Instr 정의
│   ├── FxExprCompiler.h          Expression 서브그래프 → 바이트코드
│   ├── FxExprVM.h                스택 머신 실행기
│   └── FxExprHlslGen.h           HLSL 코드 생성 (Phase 2, Stage 7 대비)
├── Asset/
│   ├── FxAsset.h                 FxGraph + preview config + audio link
│   ├── FxAssetLoader.h           ResourceCache 통합
│   └── FxAssetLibrary.h          태그 기반 조회 ("Irelia_Q_Hit")
├── Instance/
│   ├── EmitterInstance.h         런타임 실행체 (Graph ref + Pool)
│   ├── FxInstance.h              System 레벨 (Emitter N개 + Transform)
│   └── FxInstancePool.h          재활용 풀 (스킬 한타 시 수백 인스턴스)
├── Render/
│   ├── IFxRenderer.h             인터페이스 (Stage 7 GPU 교체 대비)
│   ├── FxBillboardRenderer.h     DX11 인스턴싱 (표준)
│   ├── FxMeshRenderer.h          정적 메시 파티클 (파편)
│   ├── FxRibbonRenderer.h        트레일 / 리본 (광선 궤적)
│   └── FxSortBackend.h           Depth 소팅 (radix sort)
├── GPU/                          ★ Stage 7
│   ├── FxComputeEmitter.h        HLSL CS 기반 시뮬
│   ├── FxComputeCodeGen.h        FxGraph → .hlsl 생성
│   ├── FxIndirectDraw.h          DrawInstancedIndirect
│   └── FxGpuAttributePool.h      RWStructuredBuffer 관리
├── Editor/                       ★ Stage 6
│   ├── FxNodeEditorPanel.h       imgui-node-editor UI
│   ├── FxParamInspector.h        선택 노드 파라미터 편집
│   ├── FxPreviewViewport.h       독립 뷰포트 + RT
│   └── FxAssetBrowser.h          디스크 탐색
└── Systems/
    ├── FxSimSystem.h             ECS — 매 프레임 인스턴스 tick
    ├── FxSpawnSystem.h           SkillHook 이벤트 → FxInstance 생성
    ├── FxRenderSystem.h          수집 + 컬링 + 소팅 + 렌더 호출
    ├── FxLifetimeSystem.h        수명 0 인스턴스 반환
    └── FxGpuSyncSystem.h         ★ Stage 7 — CPU→GPU 자원 업로드
```

## Scene 필터 등록

Engine.vcxproj.filters 에 섹션 추가:

```
14. FX              Core, Graph, Pool, Executor, Expression, Asset, Instance,
                    Render, GPU, Editor, Systems
```

기존 필터 13번 AI 다음 슬롯. Phase G 전용이라 분리.

## ECS 컴포넌트

```cpp
// Engine/Public/ECS/Components/FxComponents.h
#pragma once
#include "WintersMath.h"
#include "Entity.h"
#include <string>
#include <vector>

namespace Engine {

// FxAsset 핸들. ResourceCache 가 관리.
using FxAssetHandle = std::uint32_t;
constexpr FxAssetHandle NULL_FX = 0;

// 스폰된 FX 인스턴스 참조. 실제 EmitterInstance 풀 인덱스.
using FxInstanceID = std::uint32_t;

// Entity 에 FX 인스턴스가 연결되어 있다는 태그
struct FxInstanceComponent {
    FxAssetHandle assetHandle   = NULL_FX;
    FxInstanceID  instanceID    = 0;
    bool_t        bFollowEntity = true;  // true = 매 프레임 entity Transform 따라감
    bool_t        bAutoKill     = true;  // 수명 끝나면 자동 반환
    f32_t         fTimeScale    = 1.f;
};

// 월드 고정 FX (Entity Transform 불필요). SpawnAt 의 임시 엔티티에 붙음.
struct FxWorldSpawnComponent {
    Vec3   vPosition;
    Vec3   vDirection  { 0.f, 1.f, 0.f };   // 정렬 기준 (예: 히트 노말)
    f32_t  fScale      = 1.f;
    f32_t  fLifetime   = 3.f;               // 세이프가드 (수명 >= 0 이면 자동 kill)
};

// SkillHook 에서 이벤트 발행 시 채워서 FxSpawnSystem 이 소비
struct FxSpawnRequest {
    FxAssetHandle asset;
    Vec3          vWorldPos;
    Vec3          vWorldDir;
    f32_t         fScale     = 1.f;
    EntityID      attachTo   = NULL_ENTITY;  // 비-NULL 이면 entity 따라감
};

} // namespace Engine
```

**핵심 설계 결정**: `FxInstanceComponent` 는 **엔티티가 FX 를 달고 다닐 때** (챔피언 발광 버프).
`FxWorldSpawnComponent` 는 **월드 고정 FX** (히트 스파크). 두 케이스가 라이프사이클이 다름.

## FxAsset 구조

```cpp
// Engine/Public/FX/Asset/FxAsset.h
#pragma once
#include "FxGraph.h"
#include "SoundChannel.h"
#include <optional>
#include <string>
#include <vector>

namespace Engine {

// FxGraph 와 외부 연동 정보를 묶은 디스크 에셋
class CFxAsset
{
public:
    static std::unique_ptr<CFxAsset> LoadFromFile(const std::wstring& path);
    static std::unique_ptr<CFxAsset> CreateEmpty();
    HRESULT SaveToFile(const std::wstring& path) const;

    const FxGraph& GetGraph() const { return m_graph; }
    FxGraph&       GetGraphMutable() { return m_graph; }

    const std::string& GetDisplayName() const { return m_displayName; }
    f32_t GetPreviewScale() const { return m_fPreviewScale; }

    // 사운드 동기: FX 스폰 시 자동 재생할 사운드 (optional)
    struct AudioLink {
        std::wstring soundKey;         // "Irelia/Q_Hit.wav"
        eSoundChannel channel;
        f32_t         volume = 1.f;
        f32_t         delaySec = 0.f;  // 스폰 후 재생 지연
    };
    const std::vector<AudioLink>& GetAudioLinks() const { return m_audioLinks; }

    // 네트워크 결정성 플래그 (Phase 4 연동)
    bool_t IsDeterministic() const { return m_bDeterministic; }

private:
    CFxAsset() = default;

    FxGraph                   m_graph;
    std::string               m_displayName;
    f32_t                     m_fPreviewScale  = 1.f;
    std::vector<AudioLink>    m_audioLinks;
    bool_t                    m_bDeterministic = false;   // true = 판정 FX
};

} // namespace Engine
```

## GameInstance Tier 경계

CLAUDE.md §GameInstance 경계 규칙 적용:

| 용도 | Tier | 경로 |
|---|---|---|
| **스킬 시전 → FX 스폰** (저빈도, 이름 기반) | Tier 1 | `CGameInstance::SpawnFx(assetKey, worldPos, worldDir)` |
| **매 프레임 시뮬 tick** (수백 인스턴스 × 수천 파티클) | Tier 2 | `FxSimSystem::Execute(world, dt)` 직접 |
| **렌더 수집** (카메라 기반 컬링 + 소팅) | Tier 2 | `FxRenderSystem::Collect(world, camera)` |
| **에셋 로드** (파일 경로) | Tier 1 | `CGameInstance::LoadFxAsset(path)` → handle |

```cpp
// Engine/Include/GameInstance.h 확장 (예정)
class ENGINE_DLL CGameInstance
{
    // ... 기존 ...

public: // FX (Tier 1)
    FxAssetHandle LoadFxAsset(const std::wstring& path);

    // 월드 고정 FX 스폰. entity 가 NULL_ENTITY 면 FxWorldSpawnComponent 로 임시 엔티티 생성.
    FxInstanceID SpawnFx(FxAssetHandle asset,
                         const Vec3& worldPos,
                         const Vec3& worldDir,
                         EntityID attachTo = NULL_ENTITY);

    void KillFx(FxInstanceID id);

    // Tier 2 접근용 getter (핫패스는 이 포인터 캐시해서 직접 호출)
    class CFxSystem* Get_FxSystem() { return m_pFxSystem.get(); }

private:
    unique_ptr<class CFxSystem> m_pFxSystem = {};
};
```

`CFxSystem` 이 Tier 2 의 "시뮬 + 에셋 + 렌더러" 컨테이너. 인터페이스만 GameInstance 경유.

## FxSimSystem — ECS 통합 지점

```cpp
// Engine/Public/FX/Systems/FxSimSystem.h
#pragma once
#include "ISystem.h"
#include "World.h"

namespace Engine {

class CFxSimSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        // 1. FxInstanceComponent 엔티티는 부모 Transform 따라감
        auto& instStore = world.GetStore<FxInstanceComponent>();
        auto& xfStore   = world.GetStore<TransformComponent>();
        for (uint32_t i = 0; i < instStore.Count(); ++i) {
            EntityID e = instStore.Entities()[i];
            auto& inst = instStore.Data()[i];
            if (inst.bFollowEntity && xfStore.Has(e))
                m_pFxSystem->UpdateInstanceTransform(inst.instanceID,
                                                    xfStore.Get(e).GetWorldMatrix());
        }

        // 2. 전체 인스턴스 tick (JobSystem 병렬 Phase 1b)
        m_pFxSystem->TickAll(dt);

        // 3. 죽은 인스턴스 회수
        m_pFxSystem->ReapDeadInstances([&](FxInstanceID id) {
            // FxInstanceComponent 엔티티 파괴는 호출자 판단
        });
    }

    void SetFxSystem(CFxSystem* p) { m_pFxSystem = p; }

private:
    CFxSystem* m_pFxSystem = nullptr;
};

} // namespace Engine
```

## Scene 연동

`Scene_InGame` / `Scene_Editor` 에서 `CFxSystem` 싱글 인스턴스를 CGameInstance 가 공유:

```cpp
// Client/Private/Scene/Scene_InGame.cpp (예정)
HRESULT Scene_InGame::OnEnter()
{
    // GameInstance 초기화 때 CFxSystem 이미 생성됨
    auto* pFx = CGameInstance::Get()->Get_FxSystem();

    // 스킬별 FX 에셋 프리로드
    m_ireliaQHit  = CGameInstance::Get()->LoadFxAsset(L"FX/Champions/Irelia/Q_Hit.fxg");
    m_ireliaRBlade = CGameInstance::Get()->LoadFxAsset(L"FX/Champions/Irelia/R_Blade.fxg");
    // ...
    return S_OK;
}

// 스킬 히트 콜백 (Phase B-10 SkillHook 연동)
void Scene_InGame::OnSkillHit(u32_t skillId, EntityID attacker, EntityID victim,
                              const Vec3& hitPos)
{
    FxAssetHandle asset = ResolveHitFx(skillId);  // map<skillId, FxHandle>
    Vec3 normal = (GetWorldPos(victim) - GetWorldPos(attacker)).Normalized();
    CGameInstance::Get()->SpawnFx(asset, hitPos, normal);
}
```

## Scene_Editor 연동 (B-6.7 맵 에디터 연계)

기존 `Scene_Editor` 는 맵 오브젝트 배치용. Stage 6 노드 에디터는 **별도 씬**:

```
Scene_Editor         (기존) — 맵 오브젝트 배치
Scene_FxNodeEditor   (신규) — 노드 그래프 편집 + 프리뷰
```

또는 `Scene_Editor` 에 탭 추가로 통합. 후자가 "에디터가 곧 게임 모드" 철학에 부합.

## 메모리 / 수명 관리

- `CFxAsset`: `unique_ptr` + ResourceCache (공유 에셋, 참조 카운트)
- `EmitterInstance`: `FxInstancePool` 내부 풀 (`std::vector` + free list)
- `ParticlePool`: 고정 capacity, 재할당 없음
- DX11 리소스: `ComPtr` (dynamic VB / Shader)

## 스레딩 모델

| 단계 | 스레드 | 비고 |
|---|---|---|
| SpawnFx 호출 | Game thread | 저빈도, Lock-free queue 로 버퍼링 |
| EmitterInstance::tick | Worker (Job) | 인스턴스 단위 병렬 (같은 인스턴스 내 순서 강제) |
| SoA → AoS 패킹 | Worker | 렌더 스레드 직전 |
| DX11 Map/Unmap + Draw | Render thread | Dynamic VB 는 프레임 N 개 이중/삼중 버퍼 |
| GPU Compute dispatch (Stage 7) | Render thread | Indirect Draw 로 피드백 |

## 결정 사항 (Decisions)

| 결정 | 채택 | 이유 |
|---|---|---|
| JSON 포맷 | **nlohmann::json** | 헤더 온리, ThirdPartyLib 편입 쉬움 |
| 노드 ID | `uint32_t` monotonic | 재사용 안 함 → 삭제 시 hole 생김, OK |
| Attribute 스키마 | **런타임 등록** | Niagara 처럼 고정 아님, 노드가 요구하는 attr 를 자동 등록 |
| CPU ↔ GPU 일관성 | **동일 IR (Stage 4 바이트코드)** | VectorVM 개념 차용 |
| Expression 파서 | **후위 순회 기반** | Pratt / Recursive Descent 불필요, 그래프가 이미 AST |
| 난수 | **xorshift32 per-instance seed** | 결정적 재현 필수, per-thread 아님 per-instance |
| 블렌드 모드 | **Add / AlphaBlend / Premultiplied** 3종 | LoL 에는 이걸로 충분 |
| 소팅 | **radix sort (depth 기반)** | 10만 파티클 정렬에 std::sort 부적합 |
| 결정적 FX 구분 | **FxAsset::m_bDeterministic flag** | 판정 영향 FX 는 서버 시드 사용 |
| GPU 용량 단위 | **RWStructuredBuffer (Stage 7)** | Append/Consume buffer 보다 디버깅 쉬움 |

## 결정론 (Determinism) — Phase 4 네트워크 대비

Niagara 의 치명적 약점은 "결정성 없음 → 클라간 시각 불일치 가능". Winters 는 처음부터 분리:

```cpp
// FxExecContext.h 내부
struct FxExecContext {
    ParticlePool* pool;
    f32_t         deltaTime;
    f32_t         emitterAge;
    u32_t         spawnRangeBegin;
    u32_t         spawnRangeEnd;
    u32_t         rngState;       // ← 이게 핵심

    // 결정적 FX 면 서버가 내려준 seed. 시각 전용이면 로컬 seed.
    bool_t        bDeterministic;
};
```

**판정 FX** (스킬샷 히트 영역 링): `bDeterministic = true`, 서버에서 내려준 seed 사용.
**시각 FX** (챔피언 발광 오라): `bDeterministic = false`, 로컬 난수.

## 빌드 플래그

```cpp
// Engine_Macro.h 에 추가
#ifdef WINTERS_EDITOR
    #define FX_ENABLE_EDITOR 1
#else
    #define FX_ENABLE_EDITOR 0
#endif

#ifdef _DEBUG
    #define FX_DEBUG_DRAW(expr) do { expr; } while(0)
    #define FX_PROFILE(name)    PROFILE_SCOPE(name)
#else
    #define FX_DEBUG_DRAW(expr) do { } while(0)
    #define FX_PROFILE(name)    ((void)0)
#endif
```

릴리스 빌드에서 에디터 / 디버그 코드 전부 제거. 런타임 FX 시뮬은 릴리스에서도 동작.

## 성능 목표

| 시나리오 | CPU 목표 | GPU 목표 |
|---|---|---|
| 한타 씬 (5v5, 50 active FX) | < 1.0 ms | < 1.5 ms draw |
| Stage 3 CPU 전용 | 10,000 파티클 / 2 ms | — |
| Stage 7 GPU Compute | — | 1,000,000 파티클 / 8 ms |
| 메모리 (기본 Pool 크기) | 64 MB (인스턴스 1024 × 파티클 2048) | — |

## 다음 단계

Stage 1 로 이동. `FxGraph` 데이터 모델을 가장 먼저 확정해야 이후 Stage 전부가 이 위에 올라감.
