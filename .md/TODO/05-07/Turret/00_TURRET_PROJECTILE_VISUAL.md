# Turret Attack Visual — 발사체 텍스쳐 빌보드 박제

**작성일**: 2026-05-07
**전제**: B-13 v2 03 (Tower Attack System) 박제로 `CTurretAISystem` (phase 6) + `CTurretProjectileSystem` (phase 7) + `TurretAIComponent` 부착 + 발사체 entity spawn 까지 완료. **본 계획서 = 발사체 entity 가 시각적으로 0 (RenderComponent 미부착) 인 잔존 결함 정정 — 임시 텍스쳐 빌보드**.
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../../process/PLAN_AUTHORING_PITFALLS.md)
**범위**: PlaneRenderer 기반 단일 인스턴스 빌보드 (텍스쳐 1장). FX 시스템 (FxBillboard / FxMesh) 정식 통합은 후속.
**합격 정의**: 타워가 미니언/챔프 공격 시 시각적 발사체가 타워 → 타겟 방향으로 날아가서 hit 시 사라짐. 텍스쳐 = 임시 white glow PNG.

---

## §0. 본 계획서가 잡는 결함 한 줄

**증상**: [TurretAISystem.cpp:312-330](../../../../Engine/Private/ECS/Systems/TurretAISystem.cpp:312) 의 `SpawnProjectile` 이 발사체 entity 에 부착하는 컴포넌트는 **`TransformComponent` + `TurretProjectileComponent` + `SpatialAgentComponent`** 3개. **`RenderComponent` 0** → ModelRenderer 가 매 frame ForEach 돌 때 발사체 entity 0건 처리 → 시각적 발사체 영원히 안 보임. 단 [TurretProjectileSystem.cpp:60-67](../../../../Engine/Private/ECS/Systems/TurretProjectileSystem.cpp:60) 가 `pos` 갱신 + `xf.SetPosition(pos)` 는 정상 — 즉 데이터는 살아있음, **렌더만 0**.

**핵심**: ModelRenderer 인스턴스를 발사체마다 만드는 건 비효율. **단일 PlaneRenderer + 텍스쳐 1장 + 매 frame ForEach<TurretProjectileComponent> 로 SetWorld → Render 반복** 패턴으로 박제. ImGui 패널 / FX Beam 시스템에서 이미 검증된 패턴.

---

## §1. Preflight Evidence Table — TODO 0

| # | 항목 | 실측값 | 출처 |
|---|---|---|---|
| 1 | TurretAI 발사체 spawn 컴포넌트 | TransformComponent + TurretProjectileComponent + SpatialAgentComponent (kind=Projectile, team, radius=0.2) | [TurretAISystem.cpp:312-330](../../../../Engine/Private/ECS/Systems/TurretAISystem.cpp:312) |
| 2 | RenderComponent 부착 여부 | **0 (미부착)** — 본 결함 | §1.1 cross-check |
| 3 | 발사체 시작 위치 | `{turretPos.x, turretPos.y + 2.5f, turretPos.z}` (타워 머리) | [TurretAISystem.cpp:315](../../../../Engine/Private/ECS/Systems/TurretAISystem.cpp:315) |
| 4 | 발사체 타겟 위치 | `{targetPos.x, targetPos.y + 1.2f, targetPos.z}` (타겟 가슴) | [TurretProjectileSystem.cpp:40](../../../../Engine/Private/ECS/Systems/TurretProjectileSystem.cpp:40) |
| 5 | 발사체 속도 | `ai.projectileSpeed = 18.f` (TurretAIComponent 기본값) | [GameplayComponents.h:108](../../../../Engine/Public/ECS/Components/GameplayComponents.h:108) |
| 6 | 발사체 hit 반경 | `pc.hitRadius = 0.35f` | [GameplayComponents.h:120](../../../../Engine/Public/ECS/Components/GameplayComponents.h:120) |
| 7 | 발사체 destroy 시점 | hit + (sourceEntity 사망) — distSq <= hitRadiusSq 또는 target invalid | [TurretProjectileSystem.cpp:32-50](../../../../Engine/Private/ECS/Systems/TurretProjectileSystem.cpp:32) |
| 8 | PlaneRenderer API | `Create(IRHIDevice*, MeshShader, MeshPipeline)` + `SetTexture` + `SetWorld(Mat4)` + `Render(IRHIDevice*, ViewProj)` | [PlaneRenderer.h:25-35](../../../../Engine/Public/Renderer/PlaneRenderer.h:25) |
| 9 | PlaneRenderer 텍스쳐 타입 | `Engine::CTexture*` | [PlaneRenderer.h:30](../../../../Engine/Public/Renderer/PlaneRenderer.h:30) |
| 10 | 텍스쳐 매니저 / 로딩 위치 | `CGameInstance::Get()->Get_Texture(L"...")` 또는 `CResource_Manager` (검증 필요) | §4.0 진입 직전 grep |
| 11 | Mesh3D 셰이더 슬롯 | b0=PerFrame(VP) / b1=PerObject(World) / t0=Diffuse / s0=Sampler. PlaneRenderer 가 그대로 사용 | [Mesh3D.hlsl:1-14](../../../../Shaders/Mesh3D.hlsl:1) |
| 12 | 임시 텍스쳐 경로 후보 | `Resource/Texture/Effect/turret_orb.png` (신규 — 64×64 white glow) | §4.4 |
| 13 | Scene_InGame 의 PlaneRenderer 보유 패턴 | 기존: 맵 ground / FX 단발 — 검증 후 "발사체 전용 1 인스턴스" 추가 | §4.0 grep |
| 14 | Yaw 박제 정책 | turret → target 방향. CLAUDE.md §5.5 의 모델 yaw `+XM_PI` 보정 — PlaneRenderer 는 평면이라 보정 불필요 (yaw=atan2(deltaX,deltaZ) 직접) | [CLAUDE.md §5.5](../../../../CLAUDE.md) |
| 15 | Hit FX (선택, 1차 외) | TurretProjectileSystem 의 `vecHits` 후 client-only `TurretHitFxRequestComponent` 1-frame 부착 → §6.M-1 후속 | §7 |

**TODO**: §1.10 (텍스쳐 매니저 정확한 호출자), §1.13 (Scene_InGame 의 기존 PlaneRenderer 패턴) — §4.0 grep 단계에서 해소.

---

## §2. Code Reality Snapshot — 직접 인용

### §2.A — TurretAI 발사체 spawn (RenderComponent 부재 증명)

[Engine/Private/ECS/Systems/TurretAISystem.cpp:297-331](../../../../Engine/Private/ECS/Systems/TurretAISystem.cpp:297):

```cpp
void CTurretAISystem::SpawnProjectile(CWorld& world, EntityID turretEntity,
    EntityID targetEntity) const
{
    // ... validation
    const TurretAIComponent& ai = world.GetComponent<TurretAIComponent>(turretEntity);
    const Vec3 turretPos = world.GetComponent<TransformComponent>(turretEntity).GetPosition();

    const EntityID projectile = world.CreateEntity();

    TransformComponent xf{};
    xf.SetPosition({ turretPos.x, turretPos.y + 2.5f, turretPos.z });
    world.AddComponent<TransformComponent>(projectile, xf);

    TurretProjectileComponent pc{};
    pc.sourceEntity = turretEntity;
    pc.targetEntity = targetEntity;
    pc.currentPos = xf.GetPosition();
    pc.speed = ai.projectileSpeed;
    pc.damage = ai.attackDamage;
    world.AddComponent<TurretProjectileComponent>(projectile, pc);

    SpatialAgentComponent agent{};
    agent.kind = eSpatialKind::Projectile;
    agent.team = TeamOf(world.GetComponent<TurretComponent>(turretEntity).team);
    agent.radius = 0.2f;
    world.AddComponent<SpatialAgentComponent>(projectile, agent);
    // ★ RenderComponent 부착 0 — 본 박제가 잡을 잔존 결함
}
```

→ 3개 컴포넌트만 부착. **RenderComponent 부재 → ModelRenderer 가 못 봄 → 시각 0**.

### §2.B — TurretProjectile 이동 / hit (시각 무관 — 데이터만 작동)

[Engine/Private/ECS/Systems/TurretProjectileSystem.cpp:28-68](../../../../Engine/Private/ECS/Systems/TurretProjectileSystem.cpp:28):

```cpp
world.ForEach<TurretProjectileComponent, TransformComponent>(
    function<void(EntityID, TurretProjectileComponent&, TransformComponent&)>(
        [&](EntityID id, TurretProjectileComponent& pc, TransformComponent& xf)
        {
            // ... validation, distSq vs hitRadiusSq
            const Vec3 delta{ targetAim.x - pos.x, targetAim.y - pos.y, targetAim.z - pos.z };
            // ... hit / destroy

            const f32_t step = pc.speed * fTimeDelta;
            const f32_t t = (step >= dist) ? 1.f : (step / dist);
            pos.x += delta.x * t;
            pos.y += delta.y * t;
            pos.z += delta.z * t;

            pc.currentPos = pos;
            xf.SetPosition(pos);   // ★ Transform 갱신은 정상 — 데이터 살아있음
        }));
```

→ **`xf.SetPosition(pos)` 매 frame 갱신** → 외부에서 `TurretProjectileComponent + TransformComponent` ForEach 하면 매 frame 새 위치로 PlaneRenderer SetWorld + Render 가능.

### §2.C — PlaneRenderer API

[Engine/Public/Renderer/PlaneRenderer.h:15-43](../../../../Engine/Public/Renderer/PlaneRenderer.h:15):

```cpp
class WINTERS_ENGINE CPlaneRenderer final
{
public:
    static std::unique_ptr<CPlaneRenderer> Create(
        IRHIDevice* pDevice,
        DX11Shader* pMeshShader,
        DX11Pipeline* pMeshPipeline);

    void SetTexture(Engine::CTexture* pTex);
    void SetWorld(const Mat4& world);
    void SetBlendCache(CBlendStateCache* pCache,
        eBlendPreset ePreset = eBlendPreset::AlphaBlend);

    void Render(IRHIDevice* pDevice, const Mat4& matViewProj);

    void SetFxParams(const Vec4& vTint, const Vec4& vUVRect, const Vec2& vUVScroll,
        f32_t fAlphaClip, f32_t fErodeThreshold);
};
```

→ **`SetWorld + Render` 가 분리** — 단일 인스턴스에서 매 frame 다른 World 로 N 번 Render 호출 가능. 본 박제 핵심 활용.

### §2.D — TurretAIComponent 기본값

[Engine/Public/ECS/Components/GameplayComponents.h:100-111](../../../../Engine/Public/ECS/Components/GameplayComponents.h:100):

```cpp
struct TurretAIComponent
{
    EntityID attackTargetId = NULL_ENTITY;
    EntityID aggroTargetId = NULL_ENTITY;
    f32_t attackRange = 7.75f;
    f32_t attackCooldown = 0.f;
    f32_t attackCooldownMax = 1.0f;
    f32_t attackDamage = 150.f;
    f32_t projectileSpeed = 18.f;
    f32_t aggroLockTimer = 0.f;
    bool_t bActive = true;
};
```

→ 발사체 속도 18m/s + 사거리 7.75m → 평균 비행시간 0.43초.

### §2.E — Mesh3D.hlsl 슬롯 (PlaneRenderer 가 사용)

[Shaders/Mesh3D.hlsl:1-14](../../../../Shaders/Mesh3D.hlsl:1):

```hlsl
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);
```

→ PlaneRenderer 가 b0/b1/t0/s0 사용. cbuffer 충돌 0.

### §2.F — TurretProjectile 의 SpatialAgent (Stage 1 Fog 와의 연계)

[Engine/Private/ECS/Systems/TurretAISystem.cpp:326-330](../../../../Engine/Private/ECS/Systems/TurretAISystem.cpp:326):

```cpp
SpatialAgentComponent agent{};
agent.kind = eSpatialKind::Projectile;
agent.team = TeamOf(world.GetComponent<TurretComponent>(turretEntity).team);
agent.radius = 0.2f;
```

→ 발사체에 `team` 부착됨. **Fog Stage 1 의 `RenderVisibilityFilter::IsRenderableForLocal` 가 자동 적용** — 적 발사체가 시야 밖이면 시각 안 됨 (LoL 정책). VisibilityComponent 부착이 0 이라면 helper §4.1 의 "VisibilityComponent 미부착 = 항상 보임" 분기로 fallback. **본 박제는 추가 visibility 분기 0 — Fog Stage 1 helper 자체 사용**.

---

## §3. 8 GATE 통과 검증

| GATE | 항목 | 통과 |
|---|---|---|
| **A — 사실 수집** | TurretAI/TurretProjectile cpp 본문 + PlaneRenderer.h + Mesh3D.hlsl + TurretAIComponent | ✅ §2.A~F |
| **B — TODO 0** | §1.10 (텍스쳐 매니저), §1.13 (기존 PlaneRenderer 보유 패턴) | ⚠️ §4.0 grep 직전 해소 |
| **C — 호출 경로 grep** | PlaneRenderer 사용처 + Texture loader (CResource_Manager / CGameInstance::Get_Texture) — §4.0 | ⚠️ §4.0 |
| **D — ECS 책임 경계** | `TurretProjectileComponent` 기존 ECS, 본 박제는 Render-only read. write 0 — Scene_InGame::Render 의 단일 ForEach 만 | ✅ |
| **E — 향후 자료형** | `TurretProjectileComponent` 동시 발사체 100~500 발 가정. PlaneRenderer 1 인스턴스로 N draw — frame time 영향 미미 (각 draw = 6 vertex) | ✅ |
| **F — Scheduler 동시성** | TurretProjectile = phase 7. 본 박제는 Render path (Scheduler 외부) — 동일 frame `xf` 신선도 OK | ✅ |
| **G — Owner Scope** | `m_pTurretProjectileRenderer` = Scene 멤버 (unique_ptr<CPlaneRenderer>). 텍스쳐 = CTexture* (Resource_Manager owned) — Scene 측 raw 캐시 | ✅ |
| **H — 인용 의미 + 행동 보존** | TurretAI/Projectile 시스템 코드 변경 0 — 행동 보존. 시각만 추가 | ✅ |

---

## §4. 변경점 — Render Path 박제

### §4.0 — 진입 게이트 (Bash/Grep)

```bash
# §1.10 + §1.13 + GATE C 해소
grep -rn "CPlaneRenderer\|PlaneRenderer::Create\|m_pPlaneRenderer" Client/ Engine/
grep -rn "Get_Texture\|LoadTexture\|CTexture::Create" Client/Private/
grep -rn "Resource/Texture/Effect" Client/ Engine/
```

**기대 산출**:
- 기존 PlaneRenderer 인스턴스 N 곳 (Map ground / FX 단발).
- 텍스쳐 로딩 패턴 (Resource_Manager 또는 CTexture::Create + 경로).
- `Resource/Texture/Effect/` 폴더 기존 텍스쳐들.

### §4.1 — 임시 텍스쳐 자산 신규

**파일**: `Client/Bin/Resource/Texture/Effect/turret_orb.png` (또는 grep 결과 폴더 기준).

**스펙**:
- 64×64 PNG.
- 알파 = radial gradient (중심 1.0 → 가장자리 0.0).
- RGB = 흰색 (1, 1, 1) 또는 약한 청록 (0.8, 1.0, 1.0) — 타워 발사체 LoL 시그니처.
- 합성 도구: GIMP / Photoshop 또는 임시 white circle PNG.

**1차 임시 옵션**: 기존 `Client/Bin/Resource/Texture/` 안에 글로우 류 PNG 가 있으면 그걸 사용 (FxBillboard 텍스쳐 등). §4.0 grep 으로 후보 식별.

### §4.2 — Scene_InGame 멤버 추가

**파일**: `Client/Public/Scene/Scene_InGame.h`

**기존** (예시 — 줄 번호는 grep 으로 확정):

```cpp
class CScene_InGame final : public IScene
{
private:
    // ... 기존 멤버
    std::unique_ptr<CFogOfWarRenderer> m_pFogOfWarRenderer;
    Engine::CVisionSystem* m_pVisionSystem = nullptr;
    // ...
};
```

**변경**:

```cpp
class CScene_InGame final : public IScene
{
private:
    // ... 기존 멤버
    std::unique_ptr<CFogOfWarRenderer> m_pFogOfWarRenderer;
    Engine::CVisionSystem* m_pVisionSystem = nullptr;

    // Phase 6 — Turret Projectile Visual
    std::unique_ptr<CPlaneRenderer> m_pTurretProjectileRenderer;
    Engine::CTexture* m_pTurretProjectileTexture = nullptr;   // Resource_Manager owned, raw 캐시

    static constexpr f32_t TURRET_PROJECTILE_QUAD_SIZE = 0.6f;   // PlaneRenderer SetWorld scale
    static constexpr Vec4 TURRET_PROJECTILE_TINT = { 0.85f, 0.95f, 1.0f, 1.0f };
};
```

**Forward declare 추가** (Scene_InGame.h 상단):

```cpp
class CPlaneRenderer;
namespace Engine { class CTexture; }
```

### §4.3 — Bootstrap (PlaneRenderer + Texture 초기화)

**파일**: `Client/Private/Scene/InGameBootstrapBridge.cpp` 또는 `Scene_InGame::OnEnter`.

**박제** (PlaneRenderer 초기화 — Fog 후 추가):

```cpp
// InGameBootstrapBridge.cpp 또는 Scene_InGame::OnEnter
// (m_pFogOfWarRenderer 초기화 직후 같은 블록)

// Phase 6 — Turret Projectile Visual
{
    auto pInst = CGameInstance::Get();
    IRHIDevice* pRhiDevice = pInst->Get_RHIDevice();
    DX11Shader* pMeshShader = pInst->Get_MeshShader();
    DX11Pipeline* pMeshPipeline = pInst->Get_MeshPipeline();

    scene.m_pTurretProjectileRenderer = CPlaneRenderer::Create(
        pRhiDevice, pMeshShader, pMeshPipeline);

    if (scene.m_pTurretProjectileRenderer)
    {
        // BlendCache 바인딩 (AlphaBlend — 글로우 효과)
        scene.m_pTurretProjectileRenderer->SetBlendCache(
            pInst->Get_BlendStateCache(), eBlendPreset::AlphaBlend);

        // 텍스쳐 로드 — §4.0 grep 결과의 정확한 API 사용
        scene.m_pTurretProjectileTexture = pInst->Get_Texture(L"Effect/turret_orb.png");
        // ↑ §4.0 검증: 정확한 API 명 (Get_Texture / LoadTexture / CTexture::Create) 그대로

        if (scene.m_pTurretProjectileTexture)
            scene.m_pTurretProjectileRenderer->SetTexture(scene.m_pTurretProjectileTexture);

        // FX 파라미터 — alpha clip 낮추고 tint 청록
        scene.m_pTurretProjectileRenderer->SetFxParams(
            CScene_InGame::TURRET_PROJECTILE_TINT,
            { 0.f, 0.f, 1.f, 1.f },     // UV 전체
            { 0.f, 0.f },                // 스크롤 X
            0.02f,                       // alpha clip
            0.f);                        // erode 0
    }
}
```

**근거**:
- `Get_RHIDevice / Get_MeshShader / Get_MeshPipeline` 는 [CGameInstance Tier-2 게터](../../../../CLAUDE.md) (CLAUDE.md §6.5).
- 텍스쳐 로딩 API 는 §4.0 grep 결과 그대로 (P-13 회피 — 미존재 API 호출 금지).
- BlendCache + AlphaBlend = 글로우 발사체 자연스러운 합성.

### §4.4 — Render Loop (단일 인스턴스 N draw)

**파일**: `Client/Private/Scene/Scene_InGame.cpp` (또는 InGameRenderBridge — §4.0 grep).

**박제** (메인 entity 렌더 루프 직후 추가):

```cpp
// Scene_InGame::Render() 또는 InGameRenderBridge::RenderEntities() 끝부분

// Phase 6 — Turret Projectile Visual (단일 PlaneRenderer N draw)
if (m_pTurretProjectileRenderer && m_pTurretProjectileTexture)
{
    IRHIDevice* pRhiDevice = CGameInstance::Get()->Get_RHIDevice();
    const u8_t localTeam = UI::QueryLocalTeam(m_World);   // Fog Stage 1 helper 재사용

    m_World.ForEach<TurretProjectileComponent, TransformComponent>(
        function<void(EntityID, TurretProjectileComponent&, TransformComponent&)>(
            [&](EntityID id, TurretProjectileComponent& pc, TransformComponent& xf)
            {
                // Fog Stage 1 visibility 적용 (적 발사체가 시야 밖이면 hidden)
                if (!UI::IsRenderableForLocal(m_World, id, localTeam))
                    return;

                // Yaw — turret → target 방향 (taeget invalid 시 기존 currentPos 유지)
                f32_t yaw = 0.f;
                if (m_World.IsAlive(pc.targetEntity) &&
                    m_World.HasComponent<TransformComponent>(pc.targetEntity))
                {
                    const Vec3 tgt = m_World.GetComponent<TransformComponent>(pc.targetEntity).GetPosition();
                    const Vec3 src = pc.currentPos;
                    yaw = std::atan2(tgt.x - src.x, tgt.z - src.z);
                }

                // World matrix — Y-axis billboard 와 유사하게 yaw 만 회전, scale 0.6
                const Vec3 pos = xf.GetPosition();
                const f32_t s = CScene_InGame::TURRET_PROJECTILE_QUAD_SIZE;

                XMMATRIX matScale = XMMatrixScaling(s, s, s);
                XMMATRIX matRot = XMMatrixRotationY(yaw);
                XMMATRIX matTrans = XMMatrixTranslation(pos.x, pos.y, pos.z);
                XMMATRIX matWorld = matScale * matRot * matTrans;

                Mat4 world{};
                XMStoreFloat4x4(&world, matWorld);

                m_pTurretProjectileRenderer->SetWorld(world);
                m_pTurretProjectileRenderer->Render(pRhiDevice, m_matViewProj);
            }));
}
```

**근거**:
- **`world.ForEach<TurretProjectileComponent, TransformComponent>`** 매 frame 호출 — entity 가 없으면 0 draw.
- **단일 PlaneRenderer + N draw** — PlaneRenderer 인스턴스 1개. ConstantBuffer 갱신만 매 draw 다름.
- **Yaw**: `atan2(tx-sx, tz-sz)` — 평면 발사체 facing. CLAUDE.md §5.5 의 `+XM_PI` 보정은 "모델 뒷면=정면" 컨벤션용 — PlaneRenderer 의 단일 quad 는 양면 (AlphaBlend + CullNone 가정) 이라 보정 불필요. **`§4.0 grep 으로 PlaneRenderer 의 RasterizerState 확인**: CullBack 이면 그대로면 quad 가 일정 각도에서 사라짐. CullNone 또는 양면 모두 그리는 분기 필요. PlaneRenderer 가 자체 CullNone 박제됐는지 §6.M-2.
- **Fog Stage 1 helper 재사용** — `UI::IsRenderableForLocal` (Fog 박제의 helper).

### §4.5 — Include 의존

`Client/Private/Scene/Scene_InGame.cpp` 상단 include 추가:

```cpp
#include "Renderer/PlaneRenderer.h"
#include "Resource/Texture.h"   // CTexture
#include "Renderer/BlendTypes.h"
#include "Scene/RenderVisibilityFilter.h"   // Fog Stage 1 helper
#include "ECS/Components/GameplayComponents.h"   // TurretProjectileComponent
#include "ECS/Components/TransformComponent.h"
```

`Client/Public/Scene/Scene_InGame.h` 상단 forward declare:

```cpp
class CPlaneRenderer;
namespace Engine { class CTexture; }
struct TurretProjectileComponent;   // 헤더 멤버 변수 0 이라 forward 충분
```

### §4.6 — vcxproj 검증

기존 PlaneRenderer 가 이미 `Client.vcxproj` 에 등록되어 있으면 추가 0. 신규 헤더 (Fog 의 RenderVisibilityFilter.h 또는 본 계획서의 분리 헤더) 만 등록.

---

## §5. 검증 결정 포인트

### §5.1 — 시각 검증

1. F5 빌드 + InGame 진입 (Bot 매치).
2. 미니언 웨이브가 타워 사거리 진입 시 **타워 머리에서 발사체 quad 가 청록 글로우로 미니언 향해 날아감** 확인.
3. **발사체가 미니언 hit 시 사라짐** 확인 (TurretProjectileSystem 의 destroy).
4. **여러 타워 동시 공격 시 N 발사체 모두 보임** 확인 (단일 PlaneRenderer N draw).
5. **적 발사체가 시야 밖에서는 안 보임** 확인 (Fog Stage 1 helper 자동 적용).
6. **타워 비활성 (TurretAIComponent::bActive=false) 시 발사체 0** 확인.
7. **Yaw**: 발사체 quad 가 facing 방향이 타겟을 향함 (옆에서 봐도 평면 보임 — CullNone 검증).

### §5.2 — Profiler 검증

- `WINTERS_PROFILE_COUNT("TurretAI::Shots", count)` 이미 박제됨 ([TurretAISystem.cpp:210](../../../../Engine/Private/ECS/Systems/TurretAISystem.cpp:210)).
- `WINTERS_PROFILE_COUNT("TurretProjectile::Hits", count)` 이미 박제됨 ([TurretProjectileSystem.cpp:79](../../../../Engine/Private/ECS/Systems/TurretProjectileSystem.cpp:79)).
- 추가 옵션: `WINTERS_PROFILE_COUNT("TurretFx::Drawn", drawCount)` — 본 박제 §4.4 의 ForEach 안에서 카운트.

### §5.3 — Definition of Done

- [ ] §4.0 grep 으로 PlaneRenderer 보유 패턴 + 텍스쳐 API 식별
- [ ] §4.1 임시 텍스쳐 PNG 배치 (`Client/Bin/Resource/Texture/Effect/turret_orb.png`)
- [ ] §4.2 Scene_InGame 멤버 추가
- [ ] §4.3 Bootstrap 박제 (Texture + PlaneRenderer 초기화)
- [ ] §4.4 Render Loop 박제 (ForEach<TurretProjectileComponent>)
- [ ] §4.5 Include 추가
- [ ] §5.1 시각 검증 7 항목 모두 통과
- [ ] CullNone (M-2) 확인

---

## §6. Codex Pre-Mortem

### M-1. 타워 발사체 hit 시 시각 효과 0 — 사용자 혼동
**증상**: 발사체가 미니언 hit 후 즉시 사라짐. damage 는 적용되나 hit FX (스파크) 0.
**원인**: `TurretProjectileSystem` 의 `vecHits` 가 단순 `ApplyDamage` 호출만 — FX request 0.
**해결 (후속)**: `TurretHitFxRequestComponent { sourceEntity, hitPos, type }` 1-frame 부착 + 별도 시스템 (또는 Scene::Render) 이 hit 위치에 sprite 단발 표시. 본 박제 1차 = hit 시 발사체만 사라짐 (LoL 도 비슷).

### M-2. PlaneRenderer CullBack 시 측면 발사체 사라짐
**증상**: 카메라 측면에서 본 발사체 quad 가 일정 각도에서 사라짐 (back-face culling).
**원인**: PlaneRenderer 내부 RasterizerState 가 CullBack 이면 quad 의 뒷면이 컬링.
**해결**: §4.0 grep 으로 `PlaneRenderer::Impl` 의 `RSState` 확인. CullBack 이면 다음 옵션 중 1:
- (a) PlaneRenderer 에 `SetCullMode(eCullMode::None)` 신규 (작업 大)
- (b) 본 박제에서 PlaneRenderer Render 직전 RSState 임시 교체 → 복원 (작업 中)
- (c) Yaw + 추가 180도 분기 두 번 그림 (작업 小, 약간 비효율)
- 권장: (a) — 차후 다른 FX 도 동일 요구.
- **CLAUDE.md §5.5 인용**: "CPlaneRenderer 기본 CULL_BACK → 지면 퀘드 특정 각도 컬링" — 이미 알려진 함정.

### M-3. 발사체 entity 가 SpatialIndex 에 박혀서 검색 결과 잡음
**증상**: SpatialIndex.QueryRadius 가 발사체 entity 도 후보로 반환 → MinionAI / TurretAI / Vision 의 검색이 발사체 entity 도 처리 시도.
**원인**: SpawnProjectile 시 `SpatialAgentComponent { kind = eSpatialKind::Projectile }` 부착됨.
**해결**: TurretAI/MinionAI/Vision 의 `QueryRadius` mask 가 `Champion | Minion | Turret | JungleMob` 등 — `Projectile` 비트 미포함 → 자연 제외. **MinionAI 의 mask 가 Projectile 포함 시 P-14 행동 변경** — §4.0 grep 으로 mask 검증.

### M-4. 발사체 텍스쳐 알파 0.05 미만 클립 — 가운데만 보임
**증상**: 글로우 발사체의 가장자리가 안 보이고 가운데만 작게 보임.
**원인**: §4.3 의 `SetFxParams` 의 `fAlphaClip = 0.02f` — 너무 낮으면 OK, 너무 높으면 가장자리 잘림. 0.02 = 작은 alpha 도 통과 (글로우 자연스럽).
**해결**: 슬라이더 노출 (ImGui EffectTuner 패턴 — CLAUDE.md §6.8). `m_fFxAlphaClip` 슬라이더 0~0.5 범위.

### M-5. 단일 PlaneRenderer 의 Render 가 ConstantBuffer 매 draw 갱신
**증상**: 발사체 100 발 = 100 cbuffer update + 100 draw call. CPU bottleneck 우려.
**원인**: PlaneRenderer 내부 cb 갱신 패턴.
**해결**: 1차 박제 = 100~500 발 충분히 빠름 (Mesh3D quad 6 vertex). 1000+ 시 Instancing 박제 (별도 cycle).

### M-6. 발사체 yaw 갱신 시 target invalid → yaw=0 점프
**증상**: target 사망 직후 발사체가 yaw=0 (정북) 으로 갑자기 회전.
**원인**: §4.4 의 `IsAlive(pc.targetEntity) == false` 분기에서 `yaw = 0.f` 기본값.
**해결**: target 사망 = 다음 frame 에 TurretProjectileSystem 이 destroy ([TurretProjectileSystem.cpp:32-37](../../../../Engine/Private/ECS/Systems/TurretProjectileSystem.cpp:32)) — 1 frame 만 yaw 점프. 사용자 체감 미미. 또는 PreviousYaw 캐시 (선택).

### M-7. 텍스쳐 로딩 실패 — 발사체 안 보임
**증상**: `Resource/Texture/Effect/turret_orb.png` 부재 → `Get_Texture` nullptr 반환 → SetTexture 미호출 → Render 시 t0 슬롯 ID3D11ShaderResourceView nullptr 바인딩.
**원인**: 텍스쳐 PNG 배치 누락.
**해결**: §4.3 의 `if (scene.m_pTurretProjectileTexture)` 분기 가드. 텍스쳐 nullptr 시 Render skip — crash 0. 사용자가 텍스쳐 배치 후 재시도 가능.

---

## §7. 후속 (별도 박제)

| Cycle | 내용 |
|---|---|
| M-1 후속 | TurretHitFxRequestComponent + hit FX 단발 sprite |
| Phase G FX 통합 | FxBillboardComponent / FxMeshComponent 정식 — 본 박제 PlaneRenderer 패턴을 FxSystem 으로 흡수 |
| Sound | Turret 발사 sound (`Sound/Turret/shoot.wav`) — `CGameInstance::Get()->PlayEffect` |
| Hit damage popup | 미니언/챔프 hit 시 damage 숫자 popup (UI 위젯) |
| Multi-projectile | 1 타워가 여러 발 (Annie 등 마법사 패턴) — 본 박제는 현재 1 타워 1 발 |

---

## §8. 다음 세션 진입 명령

```
"Turret Projectile Visual 진입.
.md/TODO/05-07/Turret/00_TURRET_PROJECTILE_VISUAL.md §4.0 grep 으로
PlaneRenderer 보유 패턴 + Texture API 식별
→ §4.1 임시 PNG 배치 → §4.2 멤버 추가 → §4.3 Bootstrap → §4.4 Render Loop
→ §4.5 Include → §4.6 vcxproj 검증 → §5.1 시각 검증 7 항목.
M-2 (CullBack) 우선 검증."
```

진입 직전 체크리스트:
- [ ] devenv.exe 종료
- [ ] `git checkout -b feature/turret-projectile-visual`
- [ ] Engine 단독 빌드 1회
- [ ] Fog Stage 1 박제 후 (또는 동시) 진입 — `RenderVisibilityFilter.h` 의존

---

**END OF TURRET PROJECTILE VISUAL PLAN**
