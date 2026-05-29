# 이렐리아 Phase D — 이펙트 + 상태이상 시스템 (완전 구현 계획서)

> **작성**: 2026-04-24 초안 → 2026-04-25 완전 코드 재작성
> **선행 완료**: FX PNG 99개 변환, castFrame 훅, PlaneRenderer(AttackRange), Dash 로직, PreemptAction
> **이번 목표**: FxBillboard 시스템 + CStatusEffectSystem + Q 마크 / E 검 / R 칼날 벽 + ChampionTuner 연장
> **스코프**: **Irelia only** — Yasuo/Kalista 는 FX PNG 추출 후 다음 세션 (동일 패턴 재사용)

---

## 0. 2026-04-24 초안 대비 드리프트 (실제 코드 재검증 결과)

| 가정 | 실제 | 조치 |
|------|------|------|
| StunComponent/SlowComponent/DisarmComponent 는 이 계획에서 신규 추가 | **이미 존재** — [GameplayComponents.h:215-232](../../../Engine/Public/ECS/Components/GameplayComponents.h) | Step 1 은 **시스템만** 작성, 컴포넌트 추가 코드 제거 |
| `StatusEffectSystem.h` 신규 등록 필요 | vcxproj+filters 에 **엔트리만** 존재, .h/.cpp 파일은 미생성 | 파일 생성만 하면 자동 빌드 포함. .cpp 는 vcxproj 수동 등록 |
| castFrame 감지 블록 569-598 | Profiler 스코프 삽입으로 **602-631** 이동 | 계획서 라인 참조 전수 갱신 |
| PlaneRenderer 정점 = pos+UV (32B) 가정 | 실제 `VtxMesh` = pos + normal + UV + tangent = **44B** | FX 는 같은 VB/셰이더 재사용 (신규 정점 포맷 불필요) |
| `CWorld::DestroyEntity` 안전성 미확인 | **즉시 삭제**, 내부 vector 포지션 쉬프트. ForEach 중 호출 = UB | **지연 삭제 큐** 강제 |
| PNG 상대경로 가능 가정 | WIC 로드 실패로 AttackRange 가 절대경로 사용 중 ([Scene_InGame.cpp:289](../../../Client/Private/Scene/Scene_InGame.cpp)) | FX 텍스처도 절대경로 규약 |
| 카메라 Getter 가정 | `CDynamicCamera` 가 `CCamera` 상속 → `GetRight/GetUp/GetForward/GetViewProjection` 실존 | 빌보드 행렬 구성 가능 |

---

## 1. 디렉토리 / 필터 배치

```
Engine/
├── Public/ECS/Systems/StatusEffectSystem.h          [신규]
└── Private/ECS/Systems/StatusEffectSystem.cpp       [신규]
                       └── NavigationSystem.cpp      [수정 — Stun/Slow 가드]

Client/
├── Public/GameObject/
│   ├── FxBillboardComponent.h                        [신규]
│   ├── FxSystem.h                                    [신규]
│   ├── IreliaBladeSystem.h                           [신규]
│   └── UltWaveSystem.h                               [신규]
├── Private/
│   ├── GameObject/
│   │   ├── FxSystem.cpp                              [신규]
│   │   ├── IreliaBladeSystem.cpp                     [신규]
│   │   ├── UltWaveSystem.cpp                         [신규]
│   │   └── SkillTable.cpp                            [수정 — E stageCount=2]
│   ├── Scene/Scene_InGame.cpp                        [수정]
│   └── UI/ChampionTuner.cpp                          [수정]
└── Public/Scene/Scene_InGame.h                       [수정]
```

Engine 필터: `05. ECS\01. System\04. StatusEffectSystem`
Client 필터: `02. GameObject\FX`, `02. GameObject\Irelia`, `02. GameObject\Ult`

---

## 2. 아키텍처

```
CSystemSchedular (Scene_InGame::OnEnter 에서 RegisterSystem 순)
├── [Phase 0] CTransformSystem          (기존)
├── [Phase 1] CNavigationSystem         (수정 — Stun continue + Slow speed mul)
├── [Phase 2] CMinionAISystem           (기존)
├── [Phase 3] CStatusEffectSystem       (신규 — duration tick)
└── [Phase 4] Client 로컬 시스템 (Scheduler 외부, Scene_InGame 직접 호출)
    ├── CFxSystem::Update / Render
    ├── CIreliaBladeSystem::Execute
    └── CUltWaveSystem::Execute
```

Scene_InGame 은 castFrame 훅에서 각 시스템의 `Spawn` 정적 메서드를 호출하여 이펙트/발사체를 생성.

---

## 3. 신규 파일 — 완전 코드

### 3.1 `Engine/Public/ECS/Systems/StatusEffectSystem.h`

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"
#include <memory>

class CWorld;

class WINTERS_ENGINE CStatusEffectSystem final : public ISystem
{
public:
    ~CStatusEffectSystem() override = default;

    static std::unique_ptr<CStatusEffectSystem> Create()
    {
        return std::unique_ptr<CStatusEffectSystem>(new CStatusEffectSystem());
    }

    uint32_t    GetPhase() const override  { return 3; }
    const char* GetName()  const override  { return "StatusEffectSystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CStatusEffectSystem() = default;
};
```

### 3.2 `Engine/Private/ECS/Systems/StatusEffectSystem.cpp`

```cpp
#include "ECS/Systems/StatusEffectSystem.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"
#include <vector>

void CStatusEffectSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("Status::Execute");

    std::vector<EntityID> vecRemoveStun;
    std::vector<EntityID> vecRemoveSlow;
    std::vector<EntityID> vecRemoveDisarm;

    world.ForEach<StunComponent>(
        std::function<void(EntityID, StunComponent&)>(
            [&](EntityID e, StunComponent& s)
            {
                s.fRemaining -= fTimeDelta;
                if (s.fRemaining <= 0.f)
                    vecRemoveStun.push_back(e);
            }));

    world.ForEach<SlowComponent>(
        std::function<void(EntityID, SlowComponent&)>(
            [&](EntityID e, SlowComponent& s)
            {
                s.fRemaining -= fTimeDelta;
                if (s.fRemaining <= 0.f)
                    vecRemoveSlow.push_back(e);
            }));

    world.ForEach<DisarmComponent>(
        std::function<void(EntityID, DisarmComponent&)>(
            [&](EntityID e, DisarmComponent& d)
            {
                d.fRemaining -= fTimeDelta;
                if (d.fRemaining <= 0.f)
                    vecRemoveDisarm.push_back(e);
            }));

    for (EntityID e : vecRemoveStun)   world.RemoveComponent<StunComponent>(e);
    for (EntityID e : vecRemoveSlow)   world.RemoveComponent<SlowComponent>(e);
    for (EntityID e : vecRemoveDisarm) world.RemoveComponent<DisarmComponent>(e);
}
```

> **Note**: `CWorld::RemoveComponent<T>(EntityID)` 가 없으면 L220~ 에 `DestroyEntity` 로 대체 불가 (다른 컴포넌트까지 날라감) → World.h 에 `template<typename T> void RemoveComponent(EntityID e)` 가 필수. **사전 확인 필요**. 없으면 `ComponentStore<T>::Remove(EntityID)` 를 공개 래핑하거나 `fRemaining` 만 0 으로 두고 가드 쪽에서 `> 0.f` 체크로 우회.

### 3.3 `Client/Public/GameObject/FxBillboardComponent.h`

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

// 월드 공간 알파 쿼드 이펙트 (LoL 탑뷰 기준, 기본 카메라 바라보기)
// POD — CWorld 컴포넌트 스토어에 값 복사로 보관.
struct FxBillboardComponent
{
    Vec3           vWorldPos      { 0.f, 0.f, 0.f };   // attachTo == NULL_ENTITY 일 때 사용
    EntityID       attachTo       = NULL_ENTITY;       // 유효하면 Transform 추종 + vAttachOffset
    Vec3           vAttachOffset  { 0.f, 3.f, 0.f };

    const wchar_t* texturePath    = nullptr;           // 절대경로 (WIC 상대경로 실패 회피)

    f32_t          fWidth         = 1.f;
    f32_t          fHeight        = 1.f;
    f32_t          fLifetime      = 3.f;
    f32_t          fElapsed       = 0.f;

    bool_t         bBillboard     = true;              // true: 카메라 바라보기(카드), false: 지면 퀘드
    bool_t         bPendingDelete = false;
};
```

### 3.4 `Client/Public/GameObject/FxSystem.h`

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameObject/FxBillboardComponent.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <d3d11.h>

class CWorld;
class CPlaneRenderer;
class CBlendStateCache;
class CDX11Device;
class CDynamicCamera;
class DX11Shader;
class DX11Pipeline;

namespace Engine { class CTexture; }

class CFxSystem final
{
public:
    ~CFxSystem() = default;

    static std::unique_ptr<CFxSystem> Create(
        CDX11Device*      pDevice,
        DX11Shader*       pShader,
        DX11Pipeline*     pPipeline,
        CBlendStateCache* pBlendCache);

    // 매 프레임 tick (lifetime 감소, attachTo 추종, 만료 시 Destroy)
    void Update(CWorld& world, f32_t fTimeDelta);

    // 모든 살아있는 빌보드 렌더 (알파 블렌드)
    void Render(CWorld& world, const CDynamicCamera* pCamera);

    // 정적 스폰 헬퍼 — FxBillboardComponent 템플릿 입력 → 신규 엔티티 반환
    static EntityID Spawn(CWorld& world, const FxBillboardComponent& tmpl);

    // 리소스 해제 (Scene 종료 시 호출 — 캐시 텍스처 먼저 해제 후 PlaneRenderer)
    void Shutdown();

private:
    CFxSystem() = default;

    Engine::CTexture* GetOrLoadTexture(const wchar_t* wszPath);

    std::unique_ptr<CPlaneRenderer>                                  m_pPlane;
    std::unordered_map<std::wstring, std::unique_ptr<Engine::CTexture>> m_TexCache;

    CDX11Device*      m_pDevice     = nullptr;
    CBlendStateCache* m_pBlendCache = nullptr;
};
```

### 3.5 `Client/Private/GameObject/FxSystem.cpp`

```cpp
#include "GameObject/FxSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/FxBillboardComponent.h"
#include "Renderer/PlaneRenderer.h"
#include "Resource/Texture.h"
#include "RHI/CDX11Device.h"
#include "RHI/DX11/BlendStateCache.h"
#include "DynamicCamera.h"
#include "ProfilerAPI.h"
#include <DirectXMath.h>
#include <vector>

std::unique_ptr<CFxSystem> CFxSystem::Create(
    CDX11Device* pDevice, DX11Shader* pShader, DX11Pipeline* pPipeline,
    CBlendStateCache* pBlendCache)
{
    if (!pDevice || !pShader || !pPipeline || !pBlendCache)
        return nullptr;

    auto p = std::unique_ptr<CFxSystem>(new CFxSystem());
    p->m_pDevice     = pDevice;
    p->m_pBlendCache = pBlendCache;
    p->m_pPlane      = CPlaneRenderer::Create(pDevice->GetDevice(), pShader, pPipeline);
    if (!p->m_pPlane) return nullptr;
    p->m_pPlane->SetBlendCache(pBlendCache, eBlendPreset::AlphaBlend);
    return p;
}

EntityID CFxSystem::Spawn(CWorld& world, const FxBillboardComponent& tmpl)
{
    EntityID e = world.CreateEntity();
    world.AddComponent<FxBillboardComponent>(e, tmpl);
    return e;
}

void CFxSystem::Update(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("Fx::Update");

    std::vector<EntityID> vecDelete;

    world.ForEach<FxBillboardComponent>(
        std::function<void(EntityID, FxBillboardComponent&)>(
            [&](EntityID e, FxBillboardComponent& fx)
            {
                fx.fElapsed += fTimeDelta;

                if (fx.attachTo != NULL_ENTITY
                    && world.HasComponent<TransformComponent>(fx.attachTo))
                {
                    const Vec3& tp = world.GetComponent<TransformComponent>(fx.attachTo).m_LocalPosition;
                    fx.vWorldPos = { tp.x + fx.vAttachOffset.x,
                                     tp.y + fx.vAttachOffset.y,
                                     tp.z + fx.vAttachOffset.z };
                }

                if (fx.bPendingDelete || fx.fElapsed >= fx.fLifetime)
                    vecDelete.push_back(e);
            }));

    for (EntityID e : vecDelete)
        world.DestroyEntity(e);
}

void CFxSystem::Render(CWorld& world, const CDynamicCamera* pCamera)
{
    WINTERS_PROFILE_SCOPE("Fx::Render");
    if (!pCamera || !m_pPlane) return;

    const Mat4 matVP = pCamera->GetViewProjection();
    auto* pCtx = m_pDevice->GetContext();

    const Vec3 vCamRight = pCamera->GetRight();
    const Vec3 vCamUp    = pCamera->GetUp();
    const Vec3 vCamFwd   = pCamera->GetForward();

    world.ForEach<FxBillboardComponent>(
        std::function<void(EntityID, FxBillboardComponent&)>(
            [&](EntityID /*e*/, FxBillboardComponent& fx)
            {
                if (fx.bPendingDelete || fx.fLifetime <= 0.f) return;
                if (!fx.texturePath) return;

                Engine::CTexture* pTex = GetOrLoadTexture(fx.texturePath);
                if (!pTex) return;

                using namespace DirectX;
                XMMATRIX mWorld;

                if (fx.bBillboard)
                {
                    // 카메라 바라보는 쿼드 — quad(XZ plane) 를 (Right, -Fwd, Up) 축으로 회전 후 스케일
                    const XMVECTOR vR = XMVectorSet(vCamRight.x, vCamRight.y, vCamRight.z, 0.f);
                    const XMVECTOR vU = XMVectorSet(vCamUp.x,    vCamUp.y,    vCamUp.z,    0.f);
                    const XMVECTOR vN = XMVectorSet(-vCamFwd.x,  -vCamFwd.y,  -vCamFwd.z,  0.f);

                    XMMATRIX mRot = XMMatrixIdentity();
                    mRot.r[0] = XMVectorScale(vR, fx.fWidth);
                    mRot.r[1] = vN;
                    mRot.r[2] = XMVectorScale(vU, fx.fHeight);
                    mRot.r[3] = XMVectorSet(fx.vWorldPos.x, fx.vWorldPos.y, fx.vWorldPos.z, 1.f);
                    mWorld = mRot;
                }
                else
                {
                    // 지면 퀘드 — quad(XZ plane) 그대로, 스케일 + 이동
                    const XMMATRIX mS = XMMatrixScaling(fx.fWidth, 1.f, fx.fHeight);
                    const XMMATRIX mT = XMMatrixTranslation(fx.vWorldPos.x, fx.vWorldPos.y + 0.02f, fx.vWorldPos.z);
                    mWorld = mS * mT;
                }

                Mat4 world;
                XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&world.m), mWorld);

                m_pPlane->SetTexture(pTex);
                m_pPlane->SetWorld(world);
                m_pPlane->Render(pCtx, matVP);
            }));
}

Engine::CTexture* CFxSystem::GetOrLoadTexture(const wchar_t* wszPath)
{
    if (!wszPath) return nullptr;
    std::wstring key(wszPath);

    auto it = m_TexCache.find(key);
    if (it != m_TexCache.end()) return it->second.get();

    auto p = Engine::CTexture::Create(m_pDevice->GetDevice(), key, Engine::eTexSamplerMode::Clamp);
    if (!p)
    {
        ::OutputDebugStringW((L"[FxSystem] Texture load fail: " + key + L"\n").c_str());
        return nullptr;
    }
    Engine::CTexture* raw = p.get();
    m_TexCache.emplace(std::move(key), std::move(p));
    return raw;
}

void CFxSystem::Shutdown()
{
    m_TexCache.clear();   // CTexture 들이 먼저 해제
    m_pPlane.reset();     // PlaneRenderer 는 GameInstance 의 Shader/Pipeline 를 raw pointer 로 참조 중이므로 순서 주의
}
```

> **중요**: `CWorld::RemoveComponent<T>` 와 마찬가지로 `CWorld::DestroyEntity` 가 ForEach 밖에서만 호출됨을 보장했음. Fx / Status / Blade 세 시스템 모두 같은 패턴 준수.

### 3.6 `Client/Public/GameObject/IreliaBladeSystem.h`

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <memory>
#include <unordered_set>

class CWorld;

// E 검 상태머신 — Placed → Returning → Dead
struct IreliaBladeComponent
{
    enum class eState : uint8_t { Placed, Returning, Dead };

    eState   state          = eState::Placed;
    Vec3     vWorldPos      { 0.f, 0.f, 0.f };
    EntityID ownerEntity    = NULL_ENTITY;
    f32_t    fTravelSpeed   = 22.f;
    f32_t    fBindStunSec   = 1.25f;
    f32_t    fHitRadius     = 1.5f;
    EntityID fxBillboardId  = NULL_ENTITY;   // Placed 상태의 바닥 표식 FX
    std::unordered_set<uint32_t> hitSet;
};

class CIreliaBladeSystem final
{
public:
    ~CIreliaBladeSystem() = default;

    static std::unique_ptr<CIreliaBladeSystem> Create()
    {
        return std::unique_ptr<CIreliaBladeSystem>(new CIreliaBladeSystem());
    }

    void Execute(CWorld& world, f32_t fTimeDelta);

    // E 1타 — 지면 스폰. 발동자(이렐리아)의 팀 정보를 caller 가 넘김.
    static EntityID SpawnPlaced(CWorld& world, const Vec3& vGround, EntityID owner);

    // E 2타 — 기존 Placed Blade 를 Returning 상태로 전환
    static bool TriggerReturn(CWorld& world, EntityID bladeEntity);

private:
    CIreliaBladeSystem() = default;
};
```

### 3.7 `Client/Private/GameObject/IreliaBladeSystem.cpp`

```cpp
#include "GameObject/IreliaBladeSystem.h"
#include "GameObject/FxSystem.h"
#include "GameObject/FxBillboardComponent.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr const wchar_t* kPathBladeGround =
        L"C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_blades_erode.png";
    constexpr const wchar_t* kPathStunBeam =
        L"C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam_dark.png";
}

EntityID CIreliaBladeSystem::SpawnPlaced(CWorld& world, const Vec3& vGround, EntityID owner)
{
    EntityID e = world.CreateEntity();

    TransformComponent tf{};
    tf.m_LocalPosition = vGround;
    world.AddComponent<TransformComponent>(e, tf);

    IreliaBladeComponent b{};
    b.state        = IreliaBladeComponent::eState::Placed;
    b.vWorldPos    = vGround;
    b.ownerEntity  = owner;
    world.AddComponent<IreliaBladeComponent>(e, b);

    // 바닥 표식 FX (bBillboard=false 지면 퀘드)
    FxBillboardComponent fx{};
    fx.vWorldPos   = vGround;
    fx.attachTo    = NULL_ENTITY;
    fx.texturePath = kPathBladeGround;
    fx.fWidth      = 1.8f;
    fx.fHeight     = 1.8f;
    fx.fLifetime   = 999.f;        // Placed 동안 무한
    fx.bBillboard  = false;
    fx.bPendingDelete = false;
    const EntityID fxId = CFxSystem::Spawn(world, fx);

    world.GetComponent<IreliaBladeComponent>(e).fxBillboardId = fxId;
    return e;
}

bool CIreliaBladeSystem::TriggerReturn(CWorld& world, EntityID bladeEntity)
{
    if (bladeEntity == NULL_ENTITY) return false;
    if (!world.HasComponent<IreliaBladeComponent>(bladeEntity)) return false;
    auto& b = world.GetComponent<IreliaBladeComponent>(bladeEntity);
    if (b.state != IreliaBladeComponent::eState::Placed) return false;
    b.state = IreliaBladeComponent::eState::Returning;
    return true;
}

void CIreliaBladeSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("IreliaBlade::Execute");

    std::vector<EntityID> vecDelete;
    std::vector<EntityID> vecMarkFxDelete;

    world.ForEach<IreliaBladeComponent, TransformComponent>(
        std::function<void(EntityID, IreliaBladeComponent&, TransformComponent&)>(
            [&](EntityID e, IreliaBladeComponent& b, TransformComponent& tf)
            {
                switch (b.state)
                {
                case IreliaBladeComponent::eState::Placed:
                    // FX 가 대신 렌더, 여기선 no-op
                    break;

                case IreliaBladeComponent::eState::Returning:
                {
                    if (!world.HasComponent<TransformComponent>(b.ownerEntity))
                    {
                        b.state = IreliaBladeComponent::eState::Dead;
                        return;
                    }
                    const Vec3 ownerPos = world.GetComponent<TransformComponent>(b.ownerEntity).m_LocalPosition;
                    const f32_t dx = ownerPos.x - b.vWorldPos.x;
                    const f32_t dz = ownerPos.z - b.vWorldPos.z;
                    const f32_t dist = std::sqrtf(dx * dx + dz * dz);
                    if (dist < 0.5f)
                    {
                        b.state = IreliaBladeComponent::eState::Dead;
                        return;
                    }

                    const f32_t inv = 1.f / dist;
                    const f32_t step = b.fTravelSpeed * fTimeDelta;
                    b.vWorldPos.x += dx * inv * step;
                    b.vWorldPos.z += dz * inv * step;
                    tf.m_LocalPosition = b.vWorldPos;

                    // 경로상 적 스턴 — owner 팀과 다른 팀만
                    eTeam ownerTeam = eTeam::Neutral;
                    if (world.HasComponent<ChampionComponent>(b.ownerEntity))
                        ownerTeam = world.GetComponent<ChampionComponent>(b.ownerEntity).team;

                    const Vec3 bladePos = b.vWorldPos;
                    const f32_t rSq = b.fHitRadius * b.fHitRadius;

                    world.ForEach<ChampionComponent, TransformComponent>(
                        std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                            [&](EntityID target, ChampionComponent& cc, TransformComponent& ttf)
                            {
                                if (b.hitSet.count(target)) return;
                                if (cc.team == ownerTeam)   return;

                                const f32_t tdx = ttf.m_LocalPosition.x - bladePos.x;
                                const f32_t tdz = ttf.m_LocalPosition.z - bladePos.z;
                                if (tdx * tdx + tdz * tdz < rSq)
                                {
                                    StunComponent s{};
                                    s.fRemaining   = b.fBindStunSec;
                                    s.sourceEntity = b.ownerEntity;
                                    if (world.HasComponent<StunComponent>(target))
                                        world.GetComponent<StunComponent>(target) = s;
                                    else
                                        world.AddComponent<StunComponent>(target, s);

                                    FxBillboardComponent fx{};
                                    fx.attachTo      = target;
                                    fx.vAttachOffset = { 0.f, 2.f, 0.f };
                                    fx.texturePath   = kPathStunBeam;
                                    fx.fWidth        = 1.2f;
                                    fx.fHeight       = 2.4f;
                                    fx.fLifetime     = 0.5f;
                                    fx.bBillboard    = true;
                                    CFxSystem::Spawn(world, fx);

                                    b.hitSet.insert(target);
                                }
                            }));
                    break;
                }

                case IreliaBladeComponent::eState::Dead:
                    if (b.fxBillboardId != NULL_ENTITY)
                        vecMarkFxDelete.push_back(b.fxBillboardId);
                    vecDelete.push_back(e);
                    break;
                }
            }));

    for (EntityID fxId : vecMarkFxDelete)
    {
        if (world.HasComponent<FxBillboardComponent>(fxId))
            world.GetComponent<FxBillboardComponent>(fxId).bPendingDelete = true;
    }
    for (EntityID e : vecDelete) world.DestroyEntity(e);
}
```

### 3.8 `Client/Public/GameObject/UltWaveSystem.h`

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <memory>
#include <unordered_set>

class CWorld;

// R 칼날 벽 — 전진(관통 스턴+Disarm) → 최대거리 도달 시 벽(Slow 영역) → 소멸
struct UltWaveComponent
{
    Vec3      vWorldPos     { 0.f, 0.f, 0.f };
    Vec3      vDirection    { 1.f, 0.f, 0.f };
    f32_t     fLength       = 12.f;
    f32_t     fWidth        =  3.f;
    f32_t     fSpeed        = 25.f;
    f32_t     fMaxDist      = 15.f;
    f32_t     fTravelled    =  0.f;
    f32_t     fWallDuration =  2.5f;
    f32_t     fDamage       = 250.f;
    EntityID  ownerEntity   = NULL_ENTITY;
    bool_t    bInWallPhase  = false;
    std::unordered_set<uint32_t> hitSet;
};

class CUltWaveSystem final
{
public:
    ~CUltWaveSystem() = default;

    static std::unique_ptr<CUltWaveSystem> Create()
    {
        return std::unique_ptr<CUltWaveSystem>(new CUltWaveSystem());
    }

    void Execute(CWorld& world, f32_t fTimeDelta);

    static EntityID Spawn(CWorld& world,
                          const Vec3& vOrigin, const Vec3& vForward, EntityID owner,
                          f32_t fLength, f32_t fWidth, f32_t fSpeed, f32_t fMaxDist,
                          f32_t fDamage);

private:
    CUltWaveSystem() = default;
};
```

### 3.9 `Client/Private/GameObject/UltWaveSystem.cpp`

```cpp
#include "GameObject/UltWaveSystem.h"
#include "GameObject/FxSystem.h"
#include "GameObject/FxBillboardComponent.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr const wchar_t* kPathWave =
        L"C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/FX/Irelia/r_mis_bladetex.png";
    constexpr const wchar_t* kPathDisarmRing =
        L"C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/FX/Irelia/irelia_base_disarm_ring.png";
}

EntityID CUltWaveSystem::Spawn(CWorld& world,
    const Vec3& vOrigin, const Vec3& vForward, EntityID owner,
    f32_t fLength, f32_t fWidth, f32_t fSpeed, f32_t fMaxDist, f32_t fDamage)
{
    EntityID e = world.CreateEntity();

    TransformComponent tf{};
    tf.m_LocalPosition = vOrigin;
    world.AddComponent<TransformComponent>(e, tf);

    UltWaveComponent w{};
    w.vWorldPos    = vOrigin;
    w.vDirection   = vForward;
    w.ownerEntity  = owner;
    w.fLength      = fLength;
    w.fWidth       = fWidth;
    w.fSpeed       = fSpeed;
    w.fMaxDist     = fMaxDist;
    w.fDamage      = fDamage;
    w.fWallDuration = 2.5f;
    w.bInWallPhase = false;
    world.AddComponent<UltWaveComponent>(e, w);

    FxBillboardComponent fx{};
    fx.vWorldPos   = vOrigin;
    fx.attachTo    = e;
    fx.vAttachOffset = { 0.f, 0.02f, 0.f };
    fx.texturePath = kPathWave;
    fx.fWidth      = fWidth;
    fx.fHeight     = fLength;
    fx.fLifetime   = 999.f;
    fx.bBillboard  = false;
    CFxSystem::Spawn(world, fx);

    return e;
}

void CUltWaveSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("UltWave::Execute");

    std::vector<EntityID> vecDelete;

    world.ForEach<UltWaveComponent, TransformComponent>(
        std::function<void(EntityID, UltWaveComponent&, TransformComponent&)>(
            [&](EntityID e, UltWaveComponent& w, TransformComponent& tf)
            {
                eTeam ownerTeam = eTeam::Neutral;
                if (world.HasComponent<ChampionComponent>(w.ownerEntity))
                    ownerTeam = world.GetComponent<ChampionComponent>(w.ownerEntity).team;

                if (!w.bInWallPhase)
                {
                    const f32_t step = w.fSpeed * fTimeDelta;
                    w.vWorldPos.x += w.vDirection.x * step;
                    w.vWorldPos.z += w.vDirection.z * step;
                    w.fTravelled += step;
                    tf.m_LocalPosition = w.vWorldPos;

                    // AABB 관통 체크 — 전진 방향으로 길이 / 횡방향으로 폭 절반
                    const f32_t halfL = w.fLength * 0.5f;
                    const f32_t halfW = w.fWidth  * 0.5f;

                    world.ForEach<ChampionComponent, TransformComponent>(
                        std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                            [&](EntityID target, ChampionComponent& cc, TransformComponent& ttf)
                            {
                                if (w.hitSet.count(target)) return;
                                if (cc.team == ownerTeam)   return;

                                const f32_t dx = ttf.m_LocalPosition.x - w.vWorldPos.x;
                                const f32_t dz = ttf.m_LocalPosition.z - w.vWorldPos.z;
                                // 방향축 투영 (along) + 수직축 투영 (perp)
                                const f32_t along = dx * w.vDirection.x + dz * w.vDirection.z;
                                const f32_t perp  = dx * (-w.vDirection.z) + dz * w.vDirection.x;
                                if (std::fabs(along) <= halfL && std::fabs(perp) <= halfW)
                                {
                                    // 피해 — HP 감소
                                    cc.hp -= w.fDamage;
                                    if (cc.hp < 0.f) cc.hp = 0.f;

                                    // Disarm 1.5s
                                    DisarmComponent d{}; d.fRemaining = 1.5f; d.sourceEntity = w.ownerEntity;
                                    if (world.HasComponent<DisarmComponent>(target))
                                        world.GetComponent<DisarmComponent>(target) = d;
                                    else
                                        world.AddComponent<DisarmComponent>(target, d);

                                    // 짧은 Stun 0.3s
                                    StunComponent s{}; s.fRemaining = 0.3f; s.sourceEntity = w.ownerEntity;
                                    if (world.HasComponent<StunComponent>(target))
                                        world.GetComponent<StunComponent>(target) = s;
                                    else
                                        world.AddComponent<StunComponent>(target, s);

                                    // Disarm 링 FX
                                    FxBillboardComponent fx{};
                                    fx.attachTo      = target;
                                    fx.vAttachOffset = { 0.f, 2.5f, 0.f };
                                    fx.texturePath   = kPathDisarmRing;
                                    fx.fWidth        = 1.6f;
                                    fx.fHeight       = 1.6f;
                                    fx.fLifetime     = 1.5f;
                                    fx.bBillboard    = true;
                                    CFxSystem::Spawn(world, fx);

                                    w.hitSet.insert(target);
                                }
                            }));

                    if (w.fTravelled >= w.fMaxDist)
                        w.bInWallPhase = true;
                }
                else
                {
                    // 벽 단계 — 내부 적에 Slow 갱신
                    w.fWallDuration -= fTimeDelta;
                    if (w.fWallDuration <= 0.f)
                    {
                        vecDelete.push_back(e);
                        return;
                    }

                    const f32_t halfL = w.fLength * 0.5f;
                    const f32_t halfW = w.fWidth  * 0.5f;

                    world.ForEach<ChampionComponent, TransformComponent>(
                        std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                            [&](EntityID target, ChampionComponent& cc, TransformComponent& ttf)
                            {
                                if (cc.team == ownerTeam) return;
                                const f32_t dx = ttf.m_LocalPosition.x - w.vWorldPos.x;
                                const f32_t dz = ttf.m_LocalPosition.z - w.vWorldPos.z;
                                const f32_t along = dx * w.vDirection.x + dz * w.vDirection.z;
                                const f32_t perp  = dx * (-w.vDirection.z) + dz * w.vDirection.x;
                                if (std::fabs(along) <= halfL && std::fabs(perp) <= halfW)
                                {
                                    SlowComponent sl{};
                                    sl.fRemaining    = 0.5f;
                                    sl.fMoveSpeedMul = 0.5f;
                                    sl.sourceEntity  = w.ownerEntity;
                                    if (world.HasComponent<SlowComponent>(target))
                                        world.GetComponent<SlowComponent>(target) = sl;
                                    else
                                        world.AddComponent<SlowComponent>(target, sl);
                                }
                            }));
                }
            }));

    for (EntityID e : vecDelete) world.DestroyEntity(e);
}
```

---

## 4. 기존 파일 수정 — Before/After

### 4.1 `Engine/Private/ECS/Systems/NavigationSystem.cpp`

**Before** (L56-63):
```cpp
void CNavigationSystem::ProcessAgent(CWorld& world, EntityID id)
{
    WINTERS_PROFILE_SCOPE("Nav::ProcessAgent");
    if (!world.HasComponent<NavAgentComponent>(id)) return;
    if (!world.HasComponent<TransformComponent>(id)) return;
    if (!world.HasComponent<VelocityComponent>(id))  return;
```

**After**:
```cpp
void CNavigationSystem::ProcessAgent(CWorld& world, EntityID id)
{
    WINTERS_PROFILE_SCOPE("Nav::ProcessAgent");

    // [Phase T-8] Stun 가드 — 스턴 시 이동 정지, 속도 0
    if (world.HasComponent<StunComponent>(id))
    {
        if (world.HasComponent<VelocityComponent>(id))
        {
            auto& v = world.GetComponent<VelocityComponent>(id);
            v.vDirection = { 0.f, 0.f, 0.f };
            v.fSpeed = 0.f;
        }
        return;
    }

    if (!world.HasComponent<NavAgentComponent>(id)) return;
    if (!world.HasComponent<TransformComponent>(id)) return;
    if (!world.HasComponent<VelocityComponent>(id))  return;
```

**그리고** 같은 함수 L136 부근 (`vel.fSpeed = agent.fSpeed;`) 을 다음으로 교체:
```cpp
// [Phase T-8] Slow 반영
f32_t fFinalSpeed = agent.fSpeed;
if (world.HasComponent<SlowComponent>(id))
    fFinalSpeed *= world.GetComponent<SlowComponent>(id).fMoveSpeedMul;
vel.fSpeed = fFinalSpeed;
```

include 추가 (파일 상단):
```cpp
#include "ECS/Components/GameplayComponents.h"   // Stun/Slow/Disarm
```

### 4.2 `Client/Public/Scene/Scene_InGame.h` — 멤버 추가

기존 `m_pAttackRangePlane` / `m_pAttackRangeTex` 근처에 추가:

```cpp
// [Phase T-8] 이펙트/스킬 시스템 멤버
std::unique_ptr<CFxSystem>          m_pFxSystem;
std::unique_ptr<CIreliaBladeSystem> m_pIreliaBladeSystem;
std::unique_ptr<CUltWaveSystem>     m_pUltWaveSystem;

// E 활성 검 엔티티 (E 1타 시 저장, 2타 시 참조)
EntityID m_IreliaActiveBladeId = NULL_ENTITY;

// 튜닝 필드 (ChampionTuner 슬라이더가 읽고 쓴다)
f32_t m_fBladeTravelSpeed = 22.f;
f32_t m_fBladeStunSec     = 1.25f;
f32_t m_fWaveLength       = 12.f;
f32_t m_fWaveWidth        =  3.f;
f32_t m_fWaveSpeed        = 25.f;
f32_t m_fWaveMaxDist      = 15.f;
f32_t m_fWaveDamage       = 250.f;

public:
f32_t GetBladeTravelSpeed() const { return m_fBladeTravelSpeed; }
f32_t GetBladeStunSec()     const { return m_fBladeStunSec; }
f32_t GetWaveLength()       const { return m_fWaveLength; }
f32_t GetWaveWidth()        const { return m_fWaveWidth; }
f32_t GetWaveSpeed()        const { return m_fWaveSpeed; }
f32_t GetWaveMaxDist()      const { return m_fWaveMaxDist; }
f32_t GetWaveDamage()       const { return m_fWaveDamage; }
void  SetBladeTravelSpeed(f32_t v) { m_fBladeTravelSpeed = v; }
void  SetBladeStunSec(f32_t v)     { m_fBladeStunSec = v; }
void  SetWaveLength(f32_t v)       { m_fWaveLength = v; }
void  SetWaveWidth(f32_t v)        { m_fWaveWidth = v; }
void  SetWaveSpeed(f32_t v)        { m_fWaveSpeed = v; }
void  SetWaveMaxDist(f32_t v)      { m_fWaveMaxDist = v; }
void  SetWaveDamage(f32_t v)       { m_fWaveDamage = v; }
```

include 추가 (파일 상단):
```cpp
#include "GameObject/FxSystem.h"
#include "GameObject/IreliaBladeSystem.h"
#include "GameObject/UltWaveSystem.h"
```

### 4.3 `Client/Private/Scene/Scene_InGame.cpp` — OnEnter 수정

**시스템 생성** — AttackRange Plane 생성 블록(L277-312) **직후** 추가:
```cpp
{
    CGameInstance* pGI  = CGameInstance::Get();
    CDX11Device*   pDev = pGI->Get_RHIDevice();

    m_pFxSystem = CFxSystem::Create(
        pDev,
        pGI->Get_MeshShader(),
        pGI->Get_MeshPipeline(),
        pGI->Get_BlendStateCache());

    m_pIreliaBladeSystem = CIreliaBladeSystem::Create();
    m_pUltWaveSystem     = CUltWaveSystem::Create();
}
```

**CStatusEffectSystem 등록** — TransformSystem 등록 블록(L61-84) 바로 뒤에 추가:
```cpp
{
    auto pStatus = CStatusEffectSystem::Create();
    m_pScheduler->RegisterSystem(std::move(pStatus));
}
```

include 추가:
```cpp
#include "ECS/Systems/StatusEffectSystem.h"
```

### 4.4 `Client/Private/Scene/Scene_InGame.cpp` — OnUpdate 수정

기존 castFrame 블록(L602-631) 바로 뒤에 **다음 블록** 삽입:

```cpp
// [Phase T-8] Q 마크 + R 칼날 벽 — castFrame 시점에 1회 스폰
if (m_pActiveSkillDef && m_pPlayerRenderer)
{
    const Engine::CAnimator* pAnim = m_pPlayerRenderer->GetAnimator();
    if (pAnim)
    {
        const SkillDef& d = *m_pActiveSkillDef;
        if (d.castFrame > 0.f && pAnim->HasFramePassed(d.castFrame, m_fActivePrevFrame))
        {
            // Q — 타겟 머리 위 마크
            if (d.champ == eChampion::IRELIA
                && d.slot == static_cast<uint8_t>(eSkillSlot::Q)
                && m_DashTargetEntity != NULL_ENTITY)
            {
                FxBillboardComponent fx{};
                fx.attachTo      = m_DashTargetEntity;
                fx.vAttachOffset = { 0.f, 3.5f, 0.f };
                fx.texturePath   = L"C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/FX/Irelia/irelia_base_q_mark_pulse_erode.png";
                fx.fWidth        = 1.5f;
                fx.fHeight       = 1.5f;
                fx.fLifetime     = 3.f;
                fx.bBillboard    = true;
                CFxSystem::Spawn(m_World, fx);
            }

            // R — 전방 칼날 벽 스폰
            if (d.champ == eChampion::IRELIA
                && d.slot == static_cast<uint8_t>(eSkillSlot::R)
                && m_pPlayerTransform)
            {
                const Vec3 origin = m_pPlayerTransform->GetPosition();
                // 이렐리아 forward — Transform 의 yaw 기반
                const f32_t yaw = m_pPlayerTransform->GetRotation().y;
                const Vec3 fwd { std::sinf(yaw), 0.f, std::cosf(yaw) };

                CUltWaveSystem::Spawn(m_World, origin, fwd, m_PlayerEntity,
                    m_fWaveLength, m_fWaveWidth, m_fWaveSpeed, m_fWaveMaxDist, m_fWaveDamage);
            }
        }
    }
}

// [Phase T-8] 이펙트 / Blade / UltWave Tick
if (m_pFxSystem)          m_pFxSystem->Update(m_World, dt);
if (m_pIreliaBladeSystem) m_pIreliaBladeSystem->Execute(m_World, dt);
if (m_pUltWaveSystem)     m_pUltWaveSystem->Execute(m_World, dt);
```

include 추가 (파일 상단):
```cpp
#include "GameObject/FxSystem.h"
#include "GameObject/IreliaBladeSystem.h"
#include "GameObject/UltWaveSystem.h"
#include "GameObject/FxBillboardComponent.h"
#include <cmath>
```

### 4.5 `Client/Private/Scene/Scene_InGame.cpp` — OnRender 수정

AttackRange Plane 렌더 블록(L1010-1029) **뒤에** 추가:

```cpp
// [Phase T-8] FX 빌보드 전부 렌더 (알파 마지막 단계)
if (m_pFxSystem && m_pCamera)
    m_pFxSystem->Render(m_World, m_pCamera.get());
```

### 4.6 `Client/Private/Scene/Scene_InGame.cpp` — UpdateCombatInput + DispatchSkillInput 가드

**UpdateCombatInput 첫 줄에 추가** (L1109 직후):
```cpp
void CScene_InGame::UpdateCombatInput(bool& outSkipGroundMove)
{
    outSkipGroundMove = false;
    if (!m_pPlayerRenderer) return;

    // [Phase T-8] Stun 상태 중에는 입력 전체 차단
    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<StunComponent>(m_PlayerEntity))
        return;
    ...
}
```

**DispatchSkillInput 최상단에 Disarm 가드** (L1334 `using namespace Engine;` 바로 전):
```cpp
bool CScene_InGame::DispatchSkillInput(uint8_t slot)
{
    if (!m_pPlayerRenderer || m_PlayerEntity == NULL_ENTITY)
        return false;

    // [Phase T-8] Disarm — 평타만 차단
    if (slot == static_cast<uint8_t>(eSkillSlot::BasicAttack)
        && m_World.HasComponent<DisarmComponent>(m_PlayerEntity))
        return false;

    using namespace Engine;
    ...
}
```

### 4.7 `Client/Private/Scene/Scene_InGame.cpp` — E 2-stage 분기

`DispatchSkillInput` 내 2-stage 분기 (L1347 부근) 에서 E 슬롯 감지 후 Blade 조작:

기존 stage2 처리 블록에 다음을 **추가** (stage2 true 분기 내부):
```cpp
// [Phase T-8] Irelia E 전용 — stage2 는 귀환 트리거
if (def->champ == eChampion::IRELIA
    && def->slot == static_cast<uint8_t>(eSkillSlot::E))
{
    CIreliaBladeSystem::TriggerReturn(m_World, m_IreliaActiveBladeId);
    m_IreliaActiveBladeId = NULL_ENTITY;
}
```

stage1 처리 (최초 E) 시 Blade 스폰 — `ApplyLocalPrediction` 에서 eChampion::IRELIA && slot==E 분기 추가하거나, `DispatchSkillInput` 의 stage1 분기(`slotState.stageWindow = def->stageWindowSec;` 바로 전) 에서:
```cpp
if (def->champ == eChampion::IRELIA
    && def->slot == static_cast<uint8_t>(eSkillSlot::E)
    && cmd.resolvedTargetMode == static_cast<uint8_t>(eTargetMode::GroundTarget))
{
    m_IreliaActiveBladeId = CIreliaBladeSystem::SpawnPlaced(m_World, cmd.groundPos, m_PlayerEntity);
}
```

### 4.8 `Client/Private/GameObject/SkillTable.cpp` — Irelia E → stageCount=2

**Before** (L54-64):
```cpp
{ eChampion::IRELIA, 3, eTargetMode::GroundTarget,
  0.6f, 9.0f, 80.f,
  "spell3", nullptr, nullptr,
  1.f, true, eRotateMode::TowardsCursor,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
  10.f, 20.f, 0.f, 0.f,
  1.f, 1.f,
  "spell3_to_idle", "spell3_run", 0.05f },
```

**After**:
```cpp
{ eChampion::IRELIA, 3, eTargetMode::GroundTarget,
  0.6f, 9.0f, 80.f,
  "spell3", nullptr, nullptr,
  1.f, true, eRotateMode::TowardsCursor,
  2, eTargetMode::Self, "spell3", 0.4f, eRotateMode::None, 4.f,    // stageCount=2, stage2=same anim, 4s 창
  10.f, 20.f, 6.f, 14.f,                                            // stage2 castFrame=6, recovery=14
  1.f, 1.f,
  "spell3_to_idle", "spell3_run", 0.05f },
```

### 4.9 `Client/Private/UI/ChampionTuner.cpp` — W/E/R 슬라이더

**Before** (L57):
```cpp
if (ImGui::CollapsingHeader("Skill Parameters (WIP)"))
{
    ImGui::Text("Q, W, E, R — 추후 연결");
}
```

**After**:
```cpp
if (ImGui::CollapsingHeader("Skill Parameters"))
{
    ImGui::Text("Q (Dash) — m_fDashDuration etc.");
    ImGui::Spacing();

    ImGui::Text("E (Blade / Bind)");
    f32_t eSpeed = pScene->GetBladeTravelSpeed();
    if (ImGui::SliderFloat("E Travel Speed", &eSpeed, 5.f, 40.f, "%.1f"))
        pScene->SetBladeTravelSpeed(eSpeed);
    f32_t eStun  = pScene->GetBladeStunSec();
    if (ImGui::SliderFloat("E Stun Duration", &eStun, 0.5f, 3.f, "%.2f"))
        pScene->SetBladeStunSec(eStun);
    ImGui::Spacing();

    ImGui::Text("R (Wave)");
    f32_t rLen  = pScene->GetWaveLength();
    f32_t rWid  = pScene->GetWaveWidth();
    f32_t rSpd  = pScene->GetWaveSpeed();
    f32_t rMax  = pScene->GetWaveMaxDist();
    f32_t rDmg  = pScene->GetWaveDamage();
    if (ImGui::SliderFloat("R Length",   &rLen, 6.f, 20.f, "%.1f")) pScene->SetWaveLength(rLen);
    if (ImGui::SliderFloat("R Width",    &rWid, 1.f,  8.f, "%.1f")) pScene->SetWaveWidth(rWid);
    if (ImGui::SliderFloat("R Speed",    &rSpd,10.f, 50.f, "%.1f")) pScene->SetWaveSpeed(rSpd);
    if (ImGui::SliderFloat("R MaxDist",  &rMax, 6.f, 30.f, "%.1f")) pScene->SetWaveMaxDist(rMax);
    if (ImGui::SliderFloat("R Damage",   &rDmg,50.f,500.f, "%.0f")) pScene->SetWaveDamage(rDmg);
}
```

---

## 5. vcxproj / 필터 등록

### 5.1 Engine (`Engine/Include/Engine.vcxproj`)

`<ClCompile>` 그룹 NavigationSystem.cpp 항목(L132) **뒤에** 추가:
```xml
<ClCompile Include="..\Private\ECS\Systems\StatusEffectSystem.cpp" />
```

### 5.2 Engine 필터 (`Engine/Include/Engine.vcxproj.filters`)

`<ClInclude>` StatusEffectSystem.h 엔트리(L460-462) 는 **이미 존재** (Filter `05. ECS\01. System`). .cpp 엔트리 추가 — NavigationSystem.cpp ClCompile 필터 항목 뒤:
```xml
<ClCompile Include="..\Private\ECS\Systems\StatusEffectSystem.cpp">
  <Filter>05. ECS\01. System</Filter>
</ClCompile>
```

### 5.3 Client (`Client/Include/Client.vcxproj`)

`<ClInclude>` 그룹:
```xml
<ClInclude Include="..\Public\GameObject\FxBillboardComponent.h" />
<ClInclude Include="..\Public\GameObject\FxSystem.h" />
<ClInclude Include="..\Public\GameObject\IreliaBladeSystem.h" />
<ClInclude Include="..\Public\GameObject\UltWaveSystem.h" />
```

`<ClCompile>` 그룹:
```xml
<ClCompile Include="..\Private\GameObject\FxSystem.cpp" />
<ClCompile Include="..\Private\GameObject\IreliaBladeSystem.cpp" />
<ClCompile Include="..\Private\GameObject\UltWaveSystem.cpp" />
```

### 5.4 Client 필터 (`Client/Include/Client.vcxproj.filters`)

```xml
<ClInclude Include="..\Public\GameObject\FxBillboardComponent.h">
  <Filter>02. GameObject\FX</Filter>
</ClInclude>
<ClInclude Include="..\Public\GameObject\FxSystem.h">
  <Filter>02. GameObject\FX</Filter>
</ClInclude>
<ClInclude Include="..\Public\GameObject\IreliaBladeSystem.h">
  <Filter>02. GameObject\Irelia</Filter>
</ClInclude>
<ClInclude Include="..\Public\GameObject\UltWaveSystem.h">
  <Filter>02. GameObject\Irelia</Filter>
</ClInclude>

<ClCompile Include="..\Private\GameObject\FxSystem.cpp">
  <Filter>02. GameObject\FX</Filter>
</ClCompile>
<ClCompile Include="..\Private\GameObject\IreliaBladeSystem.cpp">
  <Filter>02. GameObject\Irelia</Filter>
</ClCompile>
<ClCompile Include="..\Private\GameObject\UltWaveSystem.cpp">
  <Filter>02. GameObject\Irelia</Filter>
</ClCompile>
```

상단 `<Filter Include>` 섹션에 미존재 시:
```xml
<Filter Include="02. GameObject\FX">
  <UniqueIdentifier>{8c62afx1-...}</UniqueIdentifier>
</Filter>
<Filter Include="02. GameObject\Irelia">
  <UniqueIdentifier>{8c62afx2-...}</UniqueIdentifier>
</Filter>
```

---

## 6. EngineSDK 동기화

StatusEffectSystem.h 는 Engine/Public 에서 생성 후 Post-Build Event 가 EngineSDK/inc/ 로 복사. 자동 실패 시 수동:
```
cmd /c "C:\Users\user\Desktop\Winters\UpdateLib.bat"
```

---

## 7. 실행 순서 (권장)

1. **Step 1** — StatusEffectSystem.h/.cpp 작성, vcxproj 등록, NavigationSystem 가드 삽입, UpdateCombatInput/DispatchSkillInput 가드 추가. Engine 빌드 → Client 빌드. 실행 후 `m_World.AddComponent<StunComponent>(m_PlayerEntity, {1.5f, NULL_ENTITY})` 수동 주입으로 차단 검증.
2. **Step 2** — FxBillboardComponent / FxSystem 작성, Scene_InGame 에 CFxSystem::Create + Update + Render 체인 연결. 임시로 `SetShowCombatDebug()` 토글 시 테스트 빌보드 1장 스폰.
3. **Step 3** — castFrame Q 분기 추가. 칼리스타 Q → 마크 펄스 3초 확인.
4. **Step 4** — IreliaBladeSystem + SkillTable E stageCount=2 + DispatchSkillInput stage1/stage2 분기. 칼리스타 배치 후 E 1타→2타 관통 스턴 검증.
5. **Step 5** — UltWaveSystem + castFrame R 분기. 전방 R → 칼날 벽 → Disarm 1.5s + 벽 Slow 2.5s 확인.
6. **Step 6** — ChampionTuner 슬라이더 추가. 실시간 튜닝 검증.

각 Step 빌드 통과 + 인게임 확인 후 다음 Step 진입. **Step 3 통과 = 전체 시스템 동작 확정** (나머지는 패턴 반복).

---

## 8. 리스크 / 검증 필수 항목 (사전 체크)

1. **`CWorld::RemoveComponent<T>` 존재 확인** — 없으면 CStatusEffectSystem 만료 제거 로직 대체 필요. Engine/Public/ECS/World.h L80 부근 확인.
2. **`m_pCamera` 멤버 타입 확인** — Scene_InGame.h 에 `std::unique_ptr<CDynamicCamera>` 또는 raw. 계획서는 `.get()` 가정.
3. **`CTransform::GetRotation()` y 축 yaw 확인** — R forward 계산이 `sin(yaw), 0, cos(yaw)` 이 맞는지. 회전 표현이 오일러(pitch/yaw/roll) 인지 쿼터니언인지에 따라 다름. 틀리면 카메라 forward 기준으로 대체.
4. **`ChampionComponent.hp` 피해 직접 감소** — 서버 권위 모드 전환 시 변경될 위치. 현재는 로컬 시뮬.
5. **Post-Build Event StatusEffectSystem.h SDK 복사** — 실패 시 `UpdateLib.bat` 수동.
6. **FxBillboardComponent `unordered_set<uint32_t>` hitSet** (Blade/UltWave) — 복사 비용 존재. ECS 컴포넌트 저장 시 비용 OK (엔티티 수 적음).
7. **텍스처 캐시 수명** — `m_TexCache` 는 `m_pFxSystem` 소멸 시 자동 해제. Scene_InGame 소멸 순서가 `m_pFxSystem` → `m_pPlaneShader` (GameInstance) 이면 OK.

---

## 9. 완료 후 갱신 대상

1. `memory/project_session_2026_04_25.md` 신규 — 당일 세션 성과
2. `CLAUDE.md` "직전 완료" 블록 갱신 + 다음 단계 (Yasuo 특화)
3. CLAUDE.md Gotcha 3건 추가:
   - ECS ForEach 순회 중 DestroyEntity 안전성 — 지연 큐 강제
   - 빌보드 World 행렬 (카메라 Right / -Forward / Up 축)
   - FX 텍스처 캐시 수명 (m_pFxSystem 보유)
4. `.md/plan/Champion/` 에 Yasuo / Kalista 후속 계획서 작성 (이 계획서의 FxSystem / Status 재사용 + 챔프별 특수 로직)

---

## 10. 결정 로그

1. **Tier 1 선택**: PlaneRenderer + AlphaBlend 재사용. 정식 파티클 시스템은 Phase G 대기
2. **시스템 독립성**: Fx / Status / Blade / UltWave 전부 분리. Scene_InGame 은 castFrame 훅으로 Spawn 호출만
3. **상태이상 3종**: Stun/Slow/Disarm 기존 컴포넌트 재사용, 시스템만 신규
4. **지연 삭제 패턴**: 모든 시스템이 ForEach 중 DestroyEntity 금지 → std::vector 에 수집 후 루프 종료 후 일괄 삭제
5. **텍스처 절대경로**: WIC 상대경로 로드 실패 이슈 회피 (AttackRange 검증된 패턴)
6. **빌보드 축**: Quad XZ → (camRight, -camFwd, camUp) 로 매핑. 지면 퀘드는 XZ 그대로 스케일+이동
7. **E 2-stage**: SkillTable stageCount=2, stage2 는 같은 anim + 4s 창. stage1 에서 Blade 스폰, stage2 에서 TriggerReturn
8. **UltWave OBB 대신 축 투영**: `dx*fx + dz*fz` (along) / `dx*(-fz) + dz*fx` (perp) 로 AABB 충돌 근사. 정확한 OBB 불필요.
9. **Team 판별**: `ChampionComponent.team` 사용. Blade/UltWave 모두 owner 의 `ChampionComponent.team` 참조
10. **장비 상자 렌더 순서**: FxSystem Render 는 AttackRange Plane 뒤 (가장 마지막). 알파가 캐릭터/메시 위에 찍히도록.
