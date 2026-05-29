Session - 포탑 projectile를 AAA식 data-driven WFX cue resolver로 정리하고 기존 직접 렌더/빌보드/전용 리소스 경로를 제거한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Projectile/ProjectileVisualCatalog.h

새 파일:

```cpp
#pragma once

#include "Defines.h"

enum class ProjectileVisualStartMode : u8_t
{
    ServerStart,
    OwnerTurretMuzzle
};

struct ProjectileVisualDesc
{
    ProjectileVisualStartMode eStartMode = ProjectileVisualStartMode::ServerStart;
    const char* pszSpawnCue = nullptr;
    const char* pszMuzzleCue = nullptr;
    const char* pszHitCue = nullptr;
    const char* pszAttachedCue = nullptr;
    const wchar_t* pszFallbackSpawnTexture = nullptr;
    const wchar_t* pszFallbackHitTexture = nullptr;
    f32_t fFallbackSpawnSize = 0.8f;
    f32_t fFallbackHitSize = 1.4f;
    bool_t bUseGenericSpawnFallback = true;
    bool_t bUseGenericHitFallback = true;
};

namespace ProjectileVisualCatalog
{
    const ProjectileVisualDesc& Resolve(u16_t kind);
    bool_t IsTurretProjectileKind(u16_t kind);
}
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

새 파일:

```cpp
#include "GameObject/Projectile/ProjectileVisualCatalog.h"

#include "GameObject/Projectile/ProjectileKind.h"

namespace
{
    constexpr u16_t kTurretProjectileKind = 100;

    constexpr const wchar_t* kProjectileTexture =
        L"Client/Bin/Resource/Texture/FX/Kalista/common_glowring_blue.png";
    constexpr const wchar_t* kProjectileHitTexture =
        L"Client/Bin/Resource/Texture/FX/Kalista/common_fire-sphere32.png";
    constexpr const wchar_t* kEzrealQHitTexture =
        L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_q_hit_spark.png";

    ProjectileVisualDesc MakeGenericProjectileVisual()
    {
        ProjectileVisualDesc desc{};
        desc.pszFallbackSpawnTexture = kProjectileTexture;
        desc.pszFallbackHitTexture = kProjectileHitTexture;
        desc.fFallbackSpawnSize = 0.8f;
        desc.fFallbackHitSize = 1.4f;
        desc.bUseGenericSpawnFallback = true;
        desc.bUseGenericHitFallback = true;
        return desc;
    }

    ProjectileVisualDesc MakeNoSpawnFallbackProjectileVisual()
    {
        ProjectileVisualDesc desc = MakeGenericProjectileVisual();
        desc.bUseGenericSpawnFallback = false;
        return desc;
    }

    ProjectileVisualDesc MakeEzrealQProjectileVisual()
    {
        ProjectileVisualDesc desc = MakeGenericProjectileVisual();
        desc.pszFallbackHitTexture = kEzrealQHitTexture;
        return desc;
    }

    ProjectileVisualDesc MakeLeeSinQProjectileVisual()
    {
        ProjectileVisualDesc desc = MakeGenericProjectileVisual();
        desc.pszSpawnCue = "LeeSin.Q.Projectile";
        desc.pszHitCue = "LeeSin.Q.Hit";
        desc.pszAttachedCue = "LeeSin.Q.Mark";
        return desc;
    }

    ProjectileVisualDesc MakeZedQProjectileVisual()
    {
        ProjectileVisualDesc desc = MakeGenericProjectileVisual();
        desc.pszSpawnCue = "Zed.Q.Projectile";
        desc.pszHitCue = "Zed.Q.Hit";
        return desc;
    }

    ProjectileVisualDesc MakeTurretProjectileVisual()
    {
        ProjectileVisualDesc desc{};
        desc.eStartMode = ProjectileVisualStartMode::OwnerTurretMuzzle;
        desc.pszSpawnCue = "Turret.Projectile";
        desc.pszMuzzleCue = "Turret.MuzzleFlash";
        desc.pszHitCue = "Turret.Hit";
        desc.bUseGenericSpawnFallback = false;
        desc.bUseGenericHitFallback = false;
        return desc;
    }

    const ProjectileVisualDesc kGenericProjectileVisual = MakeGenericProjectileVisual();
    const ProjectileVisualDesc kNoSpawnFallbackProjectileVisual = MakeNoSpawnFallbackProjectileVisual();
    const ProjectileVisualDesc kEzrealQProjectileVisual = MakeEzrealQProjectileVisual();
    const ProjectileVisualDesc kLeeSinQProjectileVisual = MakeLeeSinQProjectileVisual();
    const ProjectileVisualDesc kZedQProjectileVisual = MakeZedQProjectileVisual();
    const ProjectileVisualDesc kTurretProjectileVisual = MakeTurretProjectileVisual();
}

namespace ProjectileVisualCatalog
{
    const ProjectileVisualDesc& Resolve(u16_t kind)
    {
        if (IsTurretProjectileKind(kind))
            return kTurretProjectileVisual;

        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::Wind:
        case eProjectileKind::Tornado:
        case eProjectileKind::EQRing:
            return kNoSpawnFallbackProjectileVisual;
        case eProjectileKind::MysticShot:
            return kEzrealQProjectileVisual;
        case eProjectileKind::LeeSinQ:
            return kLeeSinQProjectileVisual;
        case eProjectileKind::ZedShuriken:
            return kZedQProjectileVisual;
        default:
            return kGenericProjectileVisual;
        }
    }

    bool_t IsTurretProjectileKind(u16_t kind)
    {
        return kind == kTurretProjectileKind;
    }
}
```

1-3. C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h

기존 코드:

```cpp
    Vec3 vVelocity = { 0.f, 0.f, 0.f };
    Vec3 vScale = { 1.f, 1.f, 1.f };
    Vec3 vRotation = { 0.f, 0.f, 0.f };
    Vec4 vColor = { 1.f, 1.f, 1.f, 1.f };
```

아래로 교체:

```cpp
    Vec3 vVelocity = { 0.f, 0.f, 0.f };
    Vec3 vScale = { 1.f, 1.f, 1.f };
    Vec3 vRotation = { 0.f, 0.f, 0.f };
    f32_t fWorldYawSpinSpeed = 0.f;
    Vec4 vColor = { 1.f, 1.f, 1.f, 1.f };
```

1-4. C:/Users/user/Desktop/Winters/Engine/Private/FX/FxAsset.cpp

기존 코드:

```cpp
            ExtractVec3(block, "velocity", emitter.vVelocity);
            ExtractVec3(block, "scale", emitter.vScale);
            ExtractVec3(block, "rotation", emitter.vRotation);
```

아래에 추가:

```cpp
            if (!ExtractNumber(block, "world_yaw_spin_speed", emitter.fWorldYawSpinSpeed))
                ExtractNumber(block, "worldYawSpinSpeed", emitter.fWorldYawSpinSpeed);
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp

기존 코드:

```cpp
        mesh.vRotation = emitter.vRotation;
        if (vForward.x != 0.f || vForward.z != 0.f)
            mesh.vRotation.y += WintersMath::YawFromDirectionXZ(vForward);
```

아래로 교체:

```cpp
        mesh.vRotation = emitter.vRotation;
        mesh.fWorldYawSpinSpeed = emitter.fWorldYawSpinSpeed;
        if (vForward.x != 0.f || vForward.z != 0.f)
            mesh.vRotation.y += WintersMath::YawFromDirectionXZ(vForward);
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxMeshSystem.cpp

기존 코드:

```cpp
        mesh.vScale = emitter.vScale;
        mesh.vRotation = emitter.vRotation;
        mesh.SetModelPath(emitter.strModelPath);
```

아래로 교체:

```cpp
        mesh.vScale = emitter.vScale;
        mesh.vRotation = emitter.vRotation;
        mesh.fWorldYawSpinSpeed = emitter.fWorldYawSpinSpeed;
        mesh.SetModelPath(emitter.strModelPath);
```

1-7. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/EventApplier.h

삭제할 코드:

```cpp
    bool_t SpawnTurretProjectileMesh(CWorld& world, const Vec3& pos, const Vec3& dir,
        const Vec3& velocity, f32_t lifetime);
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

삭제할 코드:

```cpp
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/Projectile/ProjectileKind.h"
```

기존 코드:

```cpp
#include "GameObject/FX/FxSystem.h"
#include "GameObject/Projectile/ProjectileKind.h"
#include "GamePlay/ChampionCatalog.h"
```

아래로 교체:

```cpp
#include "GameObject/FX/FxSystem.h"
#include "GameObject/Projectile/ProjectileVisualCatalog.h"
#include "GamePlay/ChampionCatalog.h"
```

기존 코드:

```cpp
    constexpr const wchar_t* kProjectileTexture = L"Client/Bin/Resource/Texture/FX/Kalista/common_glowring_blue.png";
    constexpr u16_t kTurretProjectileKind = 100;
    constexpr const wchar_t* kTurretProjectileTexture =
        L"Client/Bin/Resource/Texture/Object/Turret/particles/turret_base_z_crystals.2025_s3_basesr_env.png";
    constexpr const char* kTurretProjectileMeshPath =
        "Client/Bin/Resource/Texture/Object/Turret/particles/converted/sru_turret_shard.2025_s3_basesr_env.fbx";
    constexpr const wchar_t* kTurretProjectileMeshTexture =
        L"Client/Bin/Resource/Texture/Object/Turret/particles/turret_base_z_crystals.2025_s3_basesr_env.png";
    constexpr f32_t kTurretProjectileMeshScale = 0.34f;
    constexpr const wchar_t* kTurretTopBeamTexture =
        L"Client/Bin/Resource/Texture/Object/Turret/particles/TurretTopBeam.png";
    constexpr const wchar_t* kProjectileHitTexture = L"Client/Bin/Resource/Texture/FX/Kalista/common_fire-sphere32.png";
    constexpr const wchar_t* kEzrealQHitTexture =
        L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_q_hit_spark.png";
    constexpr const wchar_t* kEffectTexture = L"Client/Bin/Resource/Texture/FX/Kalista/common_global_indicator_ring_bright.png";
```

아래로 교체:

```cpp
    constexpr const wchar_t* kEffectTexture = L"Client/Bin/Resource/Texture/FX/Kalista/common_global_indicator_ring_bright.png";
```

기존 코드:

```cpp
    u64_t BuildCueKey(u32_t a, u32_t b, u32_t c, u64_t d, u32_t e)
    {
        u64_t h = 1469598103934665603ull;
        auto Mix = [&h](u64_t v)
            {
                h ^= v;
                h *= 1099511628211ull;
            };

        Mix(a);
        Mix(b);
        Mix(c);
        Mix(d);
        Mix(e);
        return h;
    }
```

아래에 추가:

```cpp
    Vec3 ResolveOwnerTurretMuzzleWorld(CWorld& world, EntityID ownerEntity, const Vec3& fallbackPos)
    {
        CGameInstance* pGameInstance = CGameInstance::Get();
        if (!pGameInstance ||
            ownerEntity == NULL_ENTITY ||
            !world.IsAlive(ownerEntity) ||
            !world.HasComponent<TransformComponent>(ownerEntity))
        {
            return fallbackPos;
        }

        const TransformComponent& tf = world.GetComponent<TransformComponent>(ownerEntity);
        const Vec3& vPos = tf.GetPosition();
        const Vec3& vRot = tf.GetRotation();
        const Vec3 vLocalOffset = pGameInstance->UI_Get_TurretGuideMuzzleLocalOffset();

        const DirectX::XMMATRIX matRot =
            DirectX::XMMatrixRotationRollPitchYaw(vRot.x, vRot.y, vRot.z);
        const DirectX::XMVECTOR vRotated =
            DirectX::XMVector3TransformNormal(vLocalOffset.ToXMVECTOR(), matRot);

        DirectX::XMFLOAT3 vOffset{};
        DirectX::XMStoreFloat3(&vOffset, vRotated);
        return Vec3{
            vPos.x + vOffset.x,
            vPos.y + vOffset.y,
            vPos.z + vOffset.z
        };
    }

    Vec3 ResolveProjectileVisualStart(
        CWorld& world,
        EntityID ownerEntity,
        const ProjectileVisualDesc& visual,
        const Vec3& serverStart)
    {
        switch (visual.eStartMode)
        {
        case ProjectileVisualStartMode::OwnerTurretMuzzle:
            return ResolveOwnerTurretMuzzleWorld(world, ownerEntity, serverStart);
        case ProjectileVisualStartMode::ServerStart:
        default:
            return serverStart;
        }
    }
```

삭제할 범위:
`bool_t UsesChampionProjectileVisual(u16_t kind)` 함수 전체를 삭제.

삭제할 범위:
`const wchar_t* ResolveProjectileHitTexture(u16_t kind)` 함수부터 `void SpawnTurretProjectileFallbackBillboard(` 함수 끝까지 삭제.

삭제할 범위:
`const char* ResolveProjectileAttachedCue(u16_t kind)` 함수 전체를 삭제.

`CEventApplier::ApplyProjectileSpawn` 안에서 아래 기존 코드 블록을:

```cpp
    const EntityID ownerEntity = ev->ownerNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->ownerNet())
        : NULL_ENTITY;
    const bool_t bTurretProjectile = IsTurretProjectileKind(ev->kind());
    if (!IsMinionEntity(world, ownerEntity))
        LogProjectileSpawnCue(ev, serverTick);

    const Vec3 pos{ ev->startX(), ev->startY(), ev->startZ() };
    if (bTurretProjectile)
        SpawnTurretTopBeam(world, ownerEntity, pos);
    const Vec3 dir = WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
    const Vec3 velocity{
        dir.x * ev->speed(),
        dir.y * ev->speed(),
        dir.z * ev->speed()
    };

    const f32_t lifetime = (ev->speed() > 0.01f && ev->maxDist() > 0.f)
        ? ev->maxDist() / ev->speed()
        : 1.0f;
    if (bTurretProjectile &&
        !SpawnTurretProjectileMesh(world, pos, dir, velocity, lifetime))
    {
        SpawnTurretProjectileFallbackBillboard(world, pos, velocity, lifetime);
    }

    const bool_t bChampionProjectileVisual = UsesChampionProjectileVisual(ev->kind());
    bool_t bPlayedProjectileWfxCue = false;
    if (const char* pszCueName = ResolveProjectileSpawnCue(ev->kind()))
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = dir;
        fx.vVelocity = velocity;
        fx.bOverrideVelocity = true;
        fx.fLifetimeOverride = lifetime;
        fx.bOverrideLifetime = true;
        bPlayedProjectileWfxCue = CFxCuePlayer::Play(world, pszCueName, fx) != NULL_ENTITY;
    }

    const bool_t bShouldSpawnGenericProjectile =
        !bTurretProjectile &&
        (!bChampionProjectileVisual ||
         ((static_cast<eProjectileKind>(ev->kind()) == eProjectileKind::LeeSinQ ||
           static_cast<eProjectileKind>(ev->kind()) == eProjectileKind::ZedShuriken) &&
             !bPlayedProjectileWfxCue));
```

아래로 교체:

```cpp
    const EntityID ownerEntity = ev->ownerNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->ownerNet())
        : NULL_ENTITY;
    if (!IsMinionEntity(world, ownerEntity))
        LogProjectileSpawnCue(ev, serverTick);

    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());

    const Vec3 serverStart{ ev->startX(), ev->startY(), ev->startZ() };
    const Vec3 pos = serverStart;
    const Vec3 serverDir =
        WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
    const Vec3 serverEnd{
        serverStart.x + serverDir.x * ev->maxDist(),
        serverStart.y + serverDir.y * ev->maxDist(),
        serverStart.z + serverDir.z * ev->maxDist()
    };

    const Vec3 visualStart =
        ResolveProjectileVisualStart(world, ownerEntity, visual, serverStart);
    const Vec3 visualDelta{
        serverEnd.x - visualStart.x,
        serverEnd.y - visualStart.y,
        serverEnd.z - visualStart.z
    };
    const f32_t visualDistSq = WintersMath::LengthSq(visualDelta);
    const Vec3 dir = (visual.eStartMode == ProjectileVisualStartMode::ServerStart)
        ? serverDir
        : WintersMath::Normalize3D(visualDelta, serverDir);
    const f32_t visualDistance =
        (visual.eStartMode == ProjectileVisualStartMode::ServerStart ||
         visualDistSq <= WintersMath::kEpsilon)
            ? ev->maxDist()
            : std::sqrt(visualDistSq);

    const Vec3 velocity{
        dir.x * ev->speed(),
        dir.y * ev->speed(),
        dir.z * ev->speed()
    };

    const f32_t lifetime = (ev->speed() > 0.01f && visualDistance > 0.f)
        ? visualDistance / ev->speed()
        : 1.0f;

    if (visual.pszMuzzleCue)
    {
        FxCueContext fx{};
        fx.vWorldPos = visualStart;
        fx.vForward = dir;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        CFxCuePlayer::Play(world, visual.pszMuzzleCue, fx);
    }

    bool_t bPlayedProjectileWfxCue = false;
    if (visual.pszSpawnCue)
    {
        FxCueContext fx{};
        fx.vWorldPos = visualStart;
        fx.vForward = dir;
        fx.vVelocity = velocity;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        fx.bOverrideVelocity = true;
        fx.fLifetimeOverride = lifetime;
        fx.bOverrideLifetime = true;
        bPlayedProjectileWfxCue =
            CFxCuePlayer::Play(world, visual.pszSpawnCue, fx) != NULL_ENTITY;
    }

    const bool_t bShouldSpawnGenericProjectile =
        visual.bUseGenericSpawnFallback &&
        visual.pszFallbackSpawnTexture &&
        (!visual.pszSpawnCue || !bPlayedProjectileWfxCue);
```

`CEventApplier::ApplyProjectileSpawn` 안에서 아래 기존 코드 블록을:

```cpp
        if (bShouldSpawnGenericProjectile)
            SpawnBillboard(world, pos, velocity,
                ResolveProjectileSpawnTexture(ev->kind()),
                ResolveProjectileSpawnSize(ev->kind()),
                ResolveProjectileSpawnSize(ev->kind()),
                lifetime);
```

아래로 교체:

```cpp
        if (bShouldSpawnGenericProjectile)
            SpawnBillboard(world, pos, velocity,
                visual.pszFallbackSpawnTexture,
                visual.fFallbackSpawnSize,
                visual.fFallbackSpawnSize,
                lifetime);
```

`CEventApplier::ApplyProjectileSpawn` 안의 두 번째 동일한 fallback spawn 블록도 아래 기존 코드에서:

```cpp
    if (bShouldSpawnGenericProjectile)
        SpawnBillboard(world, pos, velocity,
            ResolveProjectileSpawnTexture(ev->kind()),
            ResolveProjectileSpawnSize(ev->kind()),
            ResolveProjectileSpawnSize(ev->kind()),
            lifetime);
```

아래로 교체:

```cpp
    if (bShouldSpawnGenericProjectile)
        SpawnBillboard(world, pos, velocity,
            visual.pszFallbackSpawnTexture,
            visual.fFallbackSpawnSize,
            visual.fFallbackSpawnSize,
            lifetime);
```

`CEventApplier::ApplyProjectileHit` 안에서 아래 기존 코드 블록을:

```cpp
    const Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
    bool_t bPlayedWfxCue = false;
    if (const char* pszHitCueName = ResolveProjectileHitCue(ev->kind()))
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = { 0.f, 0.f, 1.f };
        bPlayedWfxCue = CFxCuePlayer::Play(world, pszHitCueName, fx) != NULL_ENTITY;
    }

    if (const char* pszAttachedCueName = ResolveProjectileAttachedCue(ev->kind()))
```

아래로 교체:

```cpp
    const Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());

    bool_t bPlayedWfxCue = false;
    if (visual.pszHitCue)
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = { 0.f, 0.f, 1.f };
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        bPlayedWfxCue = CFxCuePlayer::Play(world, visual.pszHitCue, fx) != NULL_ENTITY;
    }

    if (const char* pszAttachedCueName = visual.pszAttachedCue)
```

`CEventApplier::ApplyProjectileHit` 안에서 아래 기존 코드 블록을:

```cpp
    if (!bPlayedWfxCue)
        SpawnBillboard(world, pos, Vec3{}, ResolveProjectileHitTexture(ev->kind()), 1.4f, 1.4f, 0.35f);
```

아래로 교체:

```cpp
    if (!bPlayedWfxCue &&
        visual.bUseGenericHitFallback &&
        visual.pszFallbackHitTexture)
    {
        SpawnBillboard(world, pos, Vec3{},
            visual.pszFallbackHitTexture,
            visual.fFallbackHitSize,
            visual.fFallbackHitSize,
            0.35f);
    }
```

삭제할 범위:
파일 하단의 `bool_t CEventApplier::SpawnTurretProjectileMesh(` 함수 전체를 삭제.

1-9. C:/Users/user/Desktop/Winters/Client/Public/Scene/CombatPresentation.h

기존 코드:

```cpp
struct CombatTurretProjectilePresentationContext
{
	CWorld* pWorld = nullptr;
	u8_t localTeam = 0;
	CRHIFxSpriteRenderer* pRHIUtilityPlaneRenderer = nullptr;
	RHITextureHandle hRHITurretProjectileTex = {};
	RHITextureHandle hRHITurretTargetDotTex = {};
	CPlaneRenderer* pTurretProjectilePlane = nullptr;
	Engine::CTexture* pTurretProjectileTexture = nullptr;
	CPlaneRenderer* pTurretTargetDotPlane = nullptr;
	Engine::CTexture* pTurretTargetDotTexture = nullptr;
	Engine::CFxStaticMeshRenderer* pFxMeshRenderer = nullptr;
	const char* pTurretProjectileMeshPath = nullptr;
	const wchar_t* pTurretProjectileMeshTexturePath = nullptr;
	Vec3 vCameraWorld{ 0.f, 0.f, 0.f };
	Vec4 vTurretGuideColor{ 184.f / 255.f, 133.f / 255.f, 77.f / 255.f, 199.f / 255.f };
	f32_t fTurretGuideDotSize = 0.082f;
	f32_t fTurretGuideDotSpacing = 0.134f;
	Vec3 vTurretGuideMuzzleLocalOffset{ 0.f, 5.78f, 0.f };
	f32_t fTurretGuideTargetYOffset = 1.20f;
	f32_t fProjectileQuadSize = 0.6f;
	f32_t fProjectileMeshScale = 0.34f;
};
```

아래로 교체:

```cpp
struct CombatTurretGuidePresentationContext
{
	CWorld* pWorld = nullptr;
	u8_t localTeam = 0;
	CRHIFxSpriteRenderer* pRHIUtilityPlaneRenderer = nullptr;
	RHITextureHandle hRHITurretTargetDotTex = {};
	CPlaneRenderer* pTurretTargetDotPlane = nullptr;
	Engine::CTexture* pTurretTargetDotTexture = nullptr;
	Vec4 vTurretGuideColor{ 184.f / 255.f, 133.f / 255.f, 77.f / 255.f, 199.f / 255.f };
	f32_t fTurretGuideDotSize = 0.082f;
	f32_t fTurretGuideDotSpacing = 0.134f;
	Vec3 vTurretGuideMuzzleLocalOffset{ 0.f, 5.78f, 0.f };
	f32_t fTurretGuideTargetYOffset = 1.20f;
};
```

기존 코드:

```cpp
	static void RenderTurretProjectiles(
		const CombatTurretProjectilePresentationContext& ctx,
		const Mat4& matViewProjection,
		IRHIDevice* pDevice,
		bool_t bUseDX12RHI);
```

아래로 교체:

```cpp
	static void RenderTurretGuides(
		const CombatTurretGuidePresentationContext& ctx,
		const Mat4& matViewProjection,
		IRHIDevice* pDevice,
		bool_t bUseDX12RHI);
```

1-10. C:/Users/user/Desktop/Winters/Client/Private/Scene/CombatPresentation.cpp

삭제할 코드:

```cpp
#include "Renderer/FxStaticMeshRenderer.h"
```

기존 코드:

```cpp
	constexpr f32_t kTurretFireYOffset = 2.5f;
	constexpr f32_t kTurretTargetYOffset = 1.2f;
	constexpr u32_t kTurretGuideMaxDots = 160u;
	const Vec4 kTurretProjectileCrystalUV{ 0.f, 0.f, 0.5f, 0.5f };
	constexpr f32_t kTurretProjectileYawOffset = 0.f;
```

아래로 교체:

```cpp
	constexpr u32_t kTurretGuideMaxDots = 160u;
```

삭제할 범위:
`Vec3 NormalizeOrFallback(const Vec3& v, const Vec3& fallback)` 함수 전체를 삭제.

삭제할 범위:
`Mat4 BuildTurretProjectileWorld(` 줄부터 `Engine::FxMeshDrawParams BuildTurretProjectileMeshParams(` 함수 끝까지 삭제.

기존 코드:

```cpp
	void RenderTurretTargetGuides(
		const CombatTurretProjectilePresentationContext& ctx,
		const Mat4& matViewProjection,
		IRHIDevice* pDevice,
		bool_t bUseDX12RHI)
```

아래로 교체:

```cpp
	void RenderTurretTargetGuides(
		const CombatTurretGuidePresentationContext& ctx,
		const Mat4& matViewProjection,
		IRHIDevice* pDevice,
		bool_t bUseDX12RHI)
```

삭제할 범위:
`void CCombatPresentation::RenderTurretProjectiles(` 줄부터 해당 함수 끝까지 삭제.

삭제한 위치에 아래 코드 추가:

```cpp
void CCombatPresentation::RenderTurretGuides(
	const CombatTurretGuidePresentationContext& ctx,
	const Mat4& matViewProjection,
	IRHIDevice* pDevice,
	bool_t bUseDX12RHI)
{
	RenderTurretTargetGuides(ctx, matViewProjection, pDevice, bUseDX12RHI);
}
```

1-11. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

기존 코드:

```cpp
    CombatTurretProjectilePresentationContext turretProjectileContext{};
    turretProjectileContext.pWorld = &scene.GetWorld();
    turretProjectileContext.localTeam = localTeam;
    turretProjectileContext.pRHIUtilityPlaneRenderer = scene.GetRHIUtilityPlaneRenderer();
    turretProjectileContext.hRHITurretProjectileTex = scene.GetRHITurretProjectileTexture();
    turretProjectileContext.hRHITurretTargetDotTex = scene.GetRHITurretTargetDotTexture();
    turretProjectileContext.pTurretProjectilePlane = scene.GetTurretProjectilePlane();
    turretProjectileContext.pTurretProjectileTexture = scene.GetTurretProjectileTexture();
    turretProjectileContext.pTurretTargetDotPlane = scene.GetTurretTargetDotPlane();
    turretProjectileContext.pTurretTargetDotTexture = scene.GetTurretTargetDotTexture();
    turretProjectileContext.pFxMeshRenderer = scene.GetFxMeshRenderer();
    turretProjectileContext.pTurretProjectileMeshPath = scene.GetTurretProjectileMeshPath();
    turretProjectileContext.pTurretProjectileMeshTexturePath = scene.GetTurretProjectileMeshTexturePath();
    turretProjectileContext.vCameraWorld = cameraWorld;
    turretProjectileContext.vTurretGuideColor = pGameInstance->UI_Get_TurretGuideColor();
    turretProjectileContext.fTurretGuideDotSize = pGameInstance->UI_Get_TurretGuideDotSize();
    turretProjectileContext.fTurretGuideDotSpacing = pGameInstance->UI_Get_TurretGuideDotSpacing();
    turretProjectileContext.vTurretGuideMuzzleLocalOffset =
        pGameInstance->UI_Get_TurretGuideMuzzleLocalOffset();
    turretProjectileContext.fTurretGuideTargetYOffset = pGameInstance->UI_Get_TurretGuideTargetYOffset();
    turretProjectileContext.fProjectileQuadSize = scene.GetTurretProjectileQuadSize();
    turretProjectileContext.fProjectileMeshScale = scene.GetTurretProjectileMeshScale();

    CCombatPresentation::RenderTurretProjectiles(
        turretProjectileContext,
        vp,
        pDevice,
        bUseDX12RHI);
```

아래로 교체:

```cpp
    CombatTurretGuidePresentationContext turretGuideContext{};
    turretGuideContext.pWorld = &scene.GetWorld();
    turretGuideContext.localTeam = localTeam;
    turretGuideContext.pRHIUtilityPlaneRenderer = scene.GetRHIUtilityPlaneRenderer();
    turretGuideContext.hRHITurretTargetDotTex = scene.GetRHITurretTargetDotTexture();
    turretGuideContext.pTurretTargetDotPlane = scene.GetTurretTargetDotPlane();
    turretGuideContext.pTurretTargetDotTexture = scene.GetTurretTargetDotTexture();
    turretGuideContext.vTurretGuideColor = pGameInstance->UI_Get_TurretGuideColor();
    turretGuideContext.fTurretGuideDotSize = pGameInstance->UI_Get_TurretGuideDotSize();
    turretGuideContext.fTurretGuideDotSpacing = pGameInstance->UI_Get_TurretGuideDotSpacing();
    turretGuideContext.vTurretGuideMuzzleLocalOffset =
        pGameInstance->UI_Get_TurretGuideMuzzleLocalOffset();
    turretGuideContext.fTurretGuideTargetYOffset = pGameInstance->UI_Get_TurretGuideTargetYOffset();

    CCombatPresentation::RenderTurretGuides(
        turretGuideContext,
        vp,
        pDevice,
        bUseDX12RHI);
```

1-12. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

삭제할 코드:

```cpp
    RHITextureHandle GetRHITurretProjectileTexture() const { return m_hRHITurretProjectileTex; }
    CPlaneRenderer* GetTurretProjectilePlane() const { return m_pTurretProjectilePlane.get(); }
    CTexture* GetTurretProjectileTexture() const { return m_pTurretProjectileTex.get(); }
    const char* GetTurretProjectileMeshPath() const { return TURRET_PROJECTILE_MESH_PATH; }
    const wchar_t* GetTurretProjectileMeshTexturePath() const { return TURRET_PROJECTILE_MESH_TEXTURE_PATH; }
    f32_t GetTurretProjectileMeshScale() const { return TURRET_PROJECTILE_MESH_SCALE; }
```

삭제할 코드:

```cpp
    f32_t GetTurretProjectileQuadSize() const { return TURRET_PROJECTILE_QUAD_SIZE; }
```

삭제할 코드:

```cpp
    std::unique_ptr<CPlaneRenderer> m_pTurretProjectilePlane;
    std::unique_ptr<CTexture>       m_pTurretProjectileTex;
```

삭제할 코드:

```cpp
    RHITextureHandle                m_hRHITurretProjectileTex = {};
```

삭제할 코드:

```cpp
    static constexpr f32_t TURRET_PROJECTILE_QUAD_SIZE = 0.6f;
    static constexpr f32_t TURRET_PROJECTILE_MESH_SCALE = 0.34f;
    static constexpr const char* TURRET_PROJECTILE_MESH_PATH =
        "Client/Bin/Resource/Texture/Object/Turret/particles/converted/sru_turret_shard.2025_s3_basesr_env.fbx";
    static constexpr const wchar_t* TURRET_PROJECTILE_MESH_TEXTURE_PATH =
        L"Client/Bin/Resource/Texture/Object/Turret/particles/turret_base_z_crystals.2025_s3_basesr_env.png";
```

1-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp

삭제할 코드:

```cpp
            scene.m_hRHITurretProjectileTex = RHI_CreateTextureFromFile(
                pRhiDevice,
                L"Client/Bin/Resource/Texture/Object/Turret/particles/turret_base_z_crystals.2025_s3_basesr_env.png",
                "RHI_TurretProjectileTexture");
            if (!scene.m_hRHITurretProjectileTex.IsValid())
                scene.m_hRHITurretProjectileTex = CreateDefaultRHITexture(pRhiDevice, "RHI_TurretProjectileFallback");
```

삭제할 범위:
`scene.m_pTurretProjectilePlane = CPlaneRenderer::Create(` 줄부터 `scene.m_pTurretProjectilePlane->SetFxParams(` 블록 끝까지 삭제.

삭제할 범위:
`if (scene.m_pFxMeshRenderer)` 줄부터 `::OutputDebugStringA("[InGameBootstrap] turret projectile mesh preload failed\n");`를 포함한 preload 블록 끝까지 삭제.

1-14. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameLifecycleBridge.cpp

삭제할 코드:

```cpp
        if (scene.m_hRHITurretProjectileTex.IsValid())
            pDevice->DestroyTexture(scene.m_hRHITurretProjectileTex);
```

삭제할 코드:

```cpp
    scene.m_hRHITurretProjectileTex = {};
```

삭제할 코드:

```cpp
    scene.m_pTurretProjectileTex.reset();
    scene.m_pTurretProjectilePlane.reset();
```

1-15. C:/Users/user/Desktop/Winters/Data/LoL/FX/Structures/Turret/projectile.wfx

새 파일:

```json
{
  "name": "Turret.Projectile",
  "emitters": [
    {
      "name": "turret_projectile_shard",
      "render_type": "MeshParticle",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Object/Turret/particles/converted/sru_turret_shard.2025_s3_basesr_env.fbx",
      "texture": "Client/Bin/Resource/Texture/Object/Turret/particles/turret_base_z_crystals.2025_s3_basesr_env.png",
      "lifetime": 1.0,
      "scale": [0.34, 0.34, 0.34],
      "rotation": [0.0, 0.0, 0.0],
      "world_yaw_spin_speed": 4.5,
      "color": [1.12, 1.04, 0.90, 1.0],
      "fade_in": 0.01,
      "fade_out": 0.12,
      "alpha_clip": 0.0,
      "blockable_by_wind_wall": false
    },
    {
      "name": "turret_projectile_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Object/Turret/particles/turret_skin0_glow.png",
      "lifetime": 1.0,
      "width": 0.70,
      "height": 0.70,
      "color": [1.0, 0.78, 0.45, 0.50],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true,
      "alpha_clip": 0.02,
      "blockable_by_wind_wall": false
    },
    {
      "name": "turret_projectile_trail",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Object/Turret/particles/turret_skin0_trail_main.png",
      "lifetime": 1.0,
      "width": 0.95,
      "height": 0.34,
      "color": [1.0, 0.70, 0.38, 0.42],
      "attach_offset": [0.0, 0.0, -0.34],
      "fade_in": 0.01,
      "fade_out": 0.20,
      "billboard": true,
      "alpha_clip": 0.02,
      "blockable_by_wind_wall": false
    }
  ]
}
```

1-16. C:/Users/user/Desktop/Winters/Data/LoL/FX/Structures/Turret/muzzle_flash.wfx

새 파일:

```json
{
  "name": "Turret.MuzzleFlash",
  "emitters": [
    {
      "name": "turret_top_beam_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Object/Turret/particles/TurretTopBeam.png",
      "lifetime": 0.22,
      "width": 1.80,
      "height": 1.80,
      "color": [1.25, 1.15, 0.95, 1.0],
      "fade_in": 0.01,
      "fade_out": 0.14,
      "atlas_cols": 2,
      "atlas_rows": 2,
      "atlas_frame_count": 4,
      "atlas_fps": 18.0,
      "atlas_loop": false,
      "billboard": true,
      "alpha_clip": 0.02
    }
  ]
}
```

1-17. C:/Users/user/Desktop/Winters/Data/LoL/FX/Structures/Turret/hit.wfx

새 파일:

```json
{
  "name": "Turret.Hit",
  "emitters": [
    {
      "name": "turret_hit_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Object/Turret/particles/sru_order_sparks.png",
      "lifetime": 0.28,
      "width": 1.15,
      "height": 1.15,
      "color": [1.20, 0.88, 0.48, 0.92],
      "fade_in": 0.01,
      "fade_out": 0.20,
      "billboard": true,
      "alpha_clip": 0.02
    },
    {
      "name": "turret_hit_shockwave",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Object/Turret/particles/turret_skin0_shockwave.png",
      "lifetime": 0.32,
      "start_radius": 0.08,
      "end_radius": 1.15,
      "thickness": 0.13,
      "grow_duration": 0.08,
      "color": [1.0, 0.70, 0.38, 0.48],
      "fade_in": 0.01,
      "fade_out": 0.24,
      "alpha_clip": 0.02
    }
  ]
}
```

2. 검증

미검증:
- 빌드 미검증.
- 런타임에서 포탑 projectile WFX mesh, muzzle flash, hit cue 재생 미검증.
- 기존 검은 atlas quad, 직접 mesh draw, old billboard fallback 제거 여부 미검증.

검증 명령:
- `git diff --check`
- `rg -n "SpawnTurretProjectileMesh|SpawnTurretTopBeam|SpawnTurretProjectileFallbackBillboard|ResolveProjectileSpawnCue|ResolveProjectileHitCue|ResolveProjectileAttachedCue|hRHITurretProjectileTex|m_pTurretProjectile|TURRET_PROJECTILE|RenderTurretProjectiles|CombatTurretProjectilePresentationContext" C:/Users/user/Desktop/Winters/Client C:/Users/user/Desktop/Winters/Engine`
- `rg -n "ProjectileVisualCatalog|Turret.Projectile|Turret.MuzzleFlash|Turret.Hit|world_yaw_spin_speed" C:/Users/user/Desktop/Winters/Client C:/Users/user/Desktop/Winters/Engine C:/Users/user/Desktop/Winters/Data`
- `MSBuild.exe C:/Users/user/Desktop/Winters/Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64 /m`

확인 필요:
- `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`가 Client 빌드 프로젝트에 포함되는지 확인.
- `Client/Bin/Resource/Texture/Object/Turret/particles/converted/sru_turret_shard.2025_s3_basesr_env.wmesh`가 존재하는지 확인.
- `Data/LoL/FX/Structures/Turret/*.wfx`가 `CFxCuePlayer` 기본 WFX directory load에서 발견되는지 확인.
- 런타임 로그에 `Turret.Projectile`의 `MeshParticle` emitter skip이 없는지 확인.
- 포탑 발사 시작점이 갈색 guide muzzle 위치와 같은 기준으로 맞는지 확인.
- `Wind`, `Tornado`, `EQRing` projectile spawn fallback이 다시 생기지 않는지 확인.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
