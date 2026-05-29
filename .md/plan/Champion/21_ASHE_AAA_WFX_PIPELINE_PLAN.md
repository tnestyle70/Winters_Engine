Session - 애쉬 BA/Q/W/E/R를 Champion Visual Cue와 서버 Projectile Visual Catalog로 분리해 AAA급 WFX 파이프라인에 묶는다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ProjectileKindComponent.h

기존 코드:

```cpp
enum class eProjectileKind : uint16_t
{
    Generic = 0,
    Wind = 1,
    Tornado = 2,
    EQRing = 3,
    MysticShot = 10,
    EssenceFlux = 11,
    GlobalBeam = 12,
    LeeSinQ = 20,
    KindredArrow = 21,
    ZedShuriken = 30,
    PROJECTILE_END
};
```

아래로 교체:

```cpp
enum class eProjectileKind : uint16_t
{
    Generic = 0,
    Wind = 1,
    Tornado = 2,
    EQRing = 3,
    MysticShot = 10,
    EssenceFlux = 11,
    GlobalBeam = 12,
    LeeSinQ = 20,
    KindredArrow = 21,
    ZedShuriken = 30,
    AsheVolleyArrow = 40,
    AsheCrystalArrow = 41,
    PROJECTILE_END
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ashe/AsheGameSim.cpp

기존 코드:

```cpp
                eProjectileKind::Wind,
                kAsheWSpeed,
                kAsheWRange,
```

아래로 교체:

```cpp
                eProjectileKind::AsheVolleyArrow,
                kAsheWSpeed,
                kAsheWRange,
```

기존 코드:

```cpp
            eProjectileKind::GlobalBeam,
            kAsheRSpeed,
            kAsheRRange,
```

아래로 교체:

```cpp
            eProjectileKind::AsheCrystalArrow,
            kAsheRSpeed,
            kAsheRRange,
```

1-3. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Projectile/ProjectileVisualCatalog.h

새 파일:

```cpp
#pragma once

#include "Defines.h"

struct ProjectileVisualDesc
{
    const char* pszSpawnCue = nullptr;
    const char* pszHitCue = nullptr;
    const char* pszAttachedCue = nullptr;
    const wchar_t* pszFallbackSpawnTexture = nullptr;
    const wchar_t* pszFallbackHitTexture = nullptr;
    f32_t fFallbackSpawnWidth = 0.8f;
    f32_t fFallbackSpawnHeight = 0.8f;
    f32_t fFallbackHitWidth = 1.4f;
    f32_t fFallbackHitHeight = 1.4f;
    bool_t bUseGenericSpawnFallback = true;
    bool_t bUseGenericHitFallback = true;
};

namespace ProjectileVisualCatalog
{
    const ProjectileVisualDesc& Resolve(u16_t kind);
}
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

새 파일:

```cpp
#include "GameObject/Projectile/ProjectileVisualCatalog.h"
#include "GameObject/Projectile/ProjectileKind.h"

namespace
{
    constexpr const wchar_t* kGenericProjectileTexture =
        L"Client/Bin/Resource/Texture/FX/Kalista/common_glowring_blue.png";
    constexpr const wchar_t* kGenericProjectileHitTexture =
        L"Client/Bin/Resource/Texture/FX/Kalista/common_fire-sphere32.png";
    constexpr const wchar_t* kEzrealQHitTexture =
        L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_q_hit_spark.png";

    constexpr ProjectileVisualDesc kGenericProjectileVisual{
        nullptr,
        nullptr,
        nullptr,
        kGenericProjectileTexture,
        kGenericProjectileHitTexture,
        0.8f,
        0.8f,
        1.4f,
        1.4f,
        true,
        true
    };

    constexpr ProjectileVisualDesc kNoSpawnGenericHitVisual{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        kGenericProjectileHitTexture,
        0.8f,
        0.8f,
        1.4f,
        1.4f,
        false,
        true
    };

    constexpr ProjectileVisualDesc kEzrealMysticShotVisual{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        kEzrealQHitTexture,
        0.8f,
        0.8f,
        1.4f,
        1.4f,
        false,
        true
    };

    constexpr ProjectileVisualDesc kLeeSinQVisual{
        "LeeSin.Q.Projectile",
        "LeeSin.Q.Hit",
        "LeeSin.Q.Mark",
        kGenericProjectileTexture,
        kGenericProjectileHitTexture,
        0.8f,
        0.8f,
        1.4f,
        1.4f,
        true,
        true
    };

    constexpr ProjectileVisualDesc kZedShurikenVisual{
        "Zed.Q.Projectile",
        "Zed.Q.Hit",
        nullptr,
        kGenericProjectileTexture,
        kGenericProjectileHitTexture,
        0.8f,
        0.8f,
        1.4f,
        1.4f,
        true,
        true
    };

    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow",
        "Ashe.W.Hit",
        nullptr,
        nullptr,
        nullptr,
        0.8f,
        0.8f,
        1.4f,
        1.4f,
        false,
        false
    };

    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow",
        "Ashe.R.Hit",
        nullptr,
        nullptr,
        nullptr,
        0.8f,
        0.8f,
        1.4f,
        1.4f,
        false,
        false
    };
}

namespace ProjectileVisualCatalog
{
    const ProjectileVisualDesc& Resolve(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::Wind:
        case eProjectileKind::Tornado:
        case eProjectileKind::EQRing:
            return kNoSpawnGenericHitVisual;
        case eProjectileKind::MysticShot:
            return kEzrealMysticShotVisual;
        case eProjectileKind::LeeSinQ:
            return kLeeSinQVisual;
        case eProjectileKind::ZedShuriken:
            return kZedShurikenVisual;
        case eProjectileKind::AsheVolleyArrow:
            return kAsheVolleyArrowVisual;
        case eProjectileKind::AsheCrystalArrow:
            return kAsheCrystalArrowVisual;
        default:
            return kGenericProjectileVisual;
        }
    }
}
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
#include "GameObject/Projectile/ProjectileKind.h"
```

아래에 추가:

```cpp
#include "GameObject/Projectile/ProjectileVisualCatalog.h"
```

삭제할 코드:

```cpp
    bool_t UsesChampionProjectileVisual(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::Wind:
        case eProjectileKind::Tornado:
        case eProjectileKind::EQRing:
        case eProjectileKind::MysticShot:
        case eProjectileKind::LeeSinQ:
        case eProjectileKind::ZedShuriken:
            return true;
        default:
            return false;
        }
    }

    const wchar_t* ResolveProjectileHitTexture(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::MysticShot:
            return kEzrealQHitTexture;
        default:
            return kProjectileHitTexture;
        }
    }

    const char* ResolveProjectileSpawnCue(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::LeeSinQ:
            return "LeeSin.Q.Projectile";
        case eProjectileKind::ZedShuriken:
            return "Zed.Q.Projectile";
        default:
            return nullptr;
        }
    }

    const char* ResolveProjectileHitCue(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::LeeSinQ:
            return "LeeSin.Q.Hit";
        case eProjectileKind::ZedShuriken:
            return "Zed.Q.Hit";
        default:
            return nullptr;
        }
    }
```

삭제할 코드:

```cpp
    const wchar_t* ResolveProjectileSpawnTexture(u16_t kind)
    {
        return IsTurretProjectileKind(kind) ? kTurretProjectileTexture : kProjectileTexture;
    }

    f32_t ResolveProjectileSpawnSize(u16_t kind)
    {
        return IsTurretProjectileKind(kind) ? 0.95f : 0.8f;
    }
```

삭제할 코드:

```cpp
    const char* ResolveProjectileAttachedCue(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::LeeSinQ:
            return "LeeSin.Q.Mark";
        default:
            return nullptr;
        }
    }
```

기존 코드:

```cpp
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
    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());
    bool_t bPlayedProjectileWfxCue = false;
    if (visual.pszSpawnCue)
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = dir;
        fx.vVelocity = velocity;
        fx.bOverrideVelocity = true;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        fx.fLifetimeOverride = lifetime;
        fx.bOverrideLifetime = true;
        bPlayedProjectileWfxCue = CFxCuePlayer::Play(world, visual.pszSpawnCue, fx) != NULL_ENTITY;
    }

    const bool_t bShouldSpawnGenericProjectile =
        !bTurretProjectile &&
        visual.bUseGenericSpawnFallback &&
        visual.pszFallbackSpawnTexture &&
        (!visual.pszSpawnCue || !bPlayedProjectileWfxCue);
```

기존 코드:

```cpp
            SpawnBillboard(world, pos, velocity,
                ResolveProjectileSpawnTexture(ev->kind()),
                ResolveProjectileSpawnSize(ev->kind()),
                ResolveProjectileSpawnSize(ev->kind()),
                lifetime);
```

아래로 교체:

```cpp
            SpawnBillboard(world, pos, velocity,
                visual.pszFallbackSpawnTexture,
                visual.fFallbackSpawnWidth,
                visual.fFallbackSpawnHeight,
                lifetime);
```

기존 코드:

```cpp
        SpawnBillboard(world, pos, velocity,
            ResolveProjectileSpawnTexture(ev->kind()),
            ResolveProjectileSpawnSize(ev->kind()),
            ResolveProjectileSpawnSize(ev->kind()),
            lifetime);
```

아래로 교체:

```cpp
        SpawnBillboard(world, pos, velocity,
            visual.pszFallbackSpawnTexture,
            visual.fFallbackSpawnWidth,
            visual.fFallbackSpawnHeight,
            lifetime);
```

기존 코드:

```cpp
    bool_t bPlayedWfxCue = false;
    if (const char* pszHitCueName = ResolveProjectileHitCue(ev->kind()))
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = { 0.f, 0.f, 1.f };
        bPlayedWfxCue = CFxCuePlayer::Play(world, pszHitCueName, fx) != NULL_ENTITY;
    }

    if (const char* pszAttachedCueName = ResolveProjectileAttachedCue(ev->kind()))
    {
        EntityID attachTo = NULL_ENTITY;
        if (ev->targetNet() != NULL_NET_ENTITY)
            attachTo = entityMap.FromNet(ev->targetNet());

        if (attachTo != NULL_ENTITY)
        {
            FxCueContext fx{};
            fx.vWorldPos = pos;
            fx.attachTo = attachTo;
            bPlayedWfxCue = CFxCuePlayer::Play(world, pszAttachedCueName, fx) != NULL_ENTITY || bPlayedWfxCue;
        }
    }

    if (!bPlayedWfxCue)
        SpawnBillboard(world, pos, Vec3{}, ResolveProjectileHitTexture(ev->kind()), 1.4f, 1.4f, 0.35f);
```

아래로 교체:

```cpp
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

    if (visual.pszAttachedCue)
    {
        EntityID attachTo = NULL_ENTITY;
        if (ev->targetNet() != NULL_NET_ENTITY)
            attachTo = entityMap.FromNet(ev->targetNet());

        if (attachTo != NULL_ENTITY)
        {
            FxCueContext fx{};
            fx.vWorldPos = pos;
            fx.attachTo = attachTo;
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            bPlayedWfxCue = CFxCuePlayer::Play(world, visual.pszAttachedCue, fx) != NULL_ENTITY || bPlayedWfxCue;
        }
    }

    if (!bPlayedWfxCue && visual.bUseGenericHitFallback && visual.pszFallbackHitTexture)
    {
        SpawnBillboard(world, pos, Vec3{},
            visual.pszFallbackHitTexture,
            visual.fFallbackHitWidth,
            visual.fFallbackHitHeight,
            0.35f);
    }
```

1-6. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Ashe/AsheVisualCueCatalog.h

새 파일:

```cpp
#pragma once

#include "Defines.h"

namespace Ashe::VisualCue
{
    constexpr const char* kBAArrow = "Ashe.BA.Arrow";
    constexpr const char* kBAHit = "Ashe.BA.Hit";
    constexpr const char* kQReady = "Ashe.Q.Ready";
    constexpr const char* kQActive = "Ashe.Q.Active";
    constexpr const char* kQAttackArrow = "Ashe.Q.AttackArrow";
    constexpr const char* kWCast = "Ashe.W.Cast";
    constexpr const char* kWArrow = "Ashe.W.Arrow";
    constexpr const char* kE = "Ashe.E.Hawkshot";
    constexpr const char* kRCast = "Ashe.R.Cast";
    constexpr const char* kRArrow = "Ashe.R.Arrow";

    constexpr f32_t kBAArrowSpeed = 18.f;
    constexpr f32_t kBAArrowLifetime = 0.4f;
    constexpr f32_t kQAttackArrowSpeed = 18.f;
    constexpr f32_t kQAttackArrowLifetime = 0.4f;
    constexpr u32_t kQAttackArrowCount = 5u;
    constexpr f32_t kQAttackArrowSideSpacing = 0.14f;
    constexpr f32_t kQAttackArrowForwardFan = 0.12f;
    constexpr f32_t kWArrowSpeed = 24.f;
    constexpr f32_t kWArrowLifetime = 0.35f;
    constexpr f32_t kWCastLifetime = 0.45f;
    constexpr f32_t kEHeightOffset = 3.0f;
    constexpr f32_t kRCastLifetime = 0.45f;
    constexpr f32_t kRArrowPreviewLifetime = 0.8f;
}
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp

기존 코드:

```cpp
#include "GameObject/Champion/Ashe/Ashe_FxPresets.h"
```

아래로 교체:

```cpp
#include "GameObject/Champion/Ashe/AsheVisualCueCatalog.h"
#include "GameObject/FX/FxCuePlayer.h"
```

기존 코드:

```cpp
        void ApplyFrostSlowStub(CWorld&, EntityID target, f32_t, f32_t)
        {
            char dbg[96];
            sprintf_s(dbg, "[Ashe Frost] target=%u slow stub\n", static_cast<u32_t>(target));
            OutputDebugStringA(dbg);
        }
```

아래에 추가:

```cpp
        Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
        {
            if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
                return world.GetComponent<TransformComponent>(entity).GetPosition();

            return Vec3{};
        }

        Vec3 ResolveAsheForward(CWorld& world, EntityID caster, const CastSkillCommand* pCommand)
        {
            if (pCommand)
            {
                const Vec3 commandDir = WintersMath::NormalizeXZOrZero(pCommand->direction);
                if (commandDir.x != 0.f || commandDir.z != 0.f)
                    return commandDir;

                if (pCommand->targetEntityId != NULL_ENTITY &&
                    world.HasComponent<TransformComponent>(caster) &&
                    world.HasComponent<TransformComponent>(pCommand->targetEntityId))
                {
                    const Vec3 casterPos = world.GetComponent<TransformComponent>(caster).GetPosition();
                    const Vec3 targetPos =
                        world.GetComponent<TransformComponent>(pCommand->targetEntityId).GetPosition();
                    const Vec3 targetDir = WintersMath::NormalizeXZOrZero(Vec3{
                        targetPos.x - casterPos.x,
                        0.f,
                        targetPos.z - casterPos.z
                    });
                    if (targetDir.x != 0.f || targetDir.z != 0.f)
                        return targetDir;
                }
            }

            if (caster != NULL_ENTITY && world.HasComponent<TransformComponent>(caster))
            {
                const f32_t yaw = world.GetComponent<TransformComponent>(caster).GetRotation().y;
                return WintersMath::DirectionFromYawXZ(yaw);
            }

            return { 0.f, 0.f, 1.f };
        }

        void PlayAsheAttachedCue(
            CWorld& world,
            Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
            const char* pszCueName,
            EntityID attachTo,
            const Vec3& worldPos,
            const Vec3& forward,
            f32_t lifetimeSec)
        {
            FxCueContext fx{};
            fx.vWorldPos = worldPos;
            fx.vForward = forward;
            fx.attachTo = attachTo;
            fx.pFxMeshRenderer = pFxMeshRenderer;
            if (lifetimeSec > 0.f)
            {
                fx.bOverrideLifetime = true;
                fx.fLifetimeOverride = lifetimeSec;
            }
            CFxCuePlayer::Play(world, pszCueName, fx);
        }

        void PlayAsheMovingCue(
            CWorld& world,
            Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
            const char* pszCueName,
            const Vec3& origin,
            const Vec3& forward,
            f32_t speed,
            f32_t lifetimeSec)
        {
            const Vec3 dir = WintersMath::NormalizeXZOrZero(forward);
            FxCueContext fx{};
            fx.vWorldPos = origin;
            fx.vForward = dir;
            fx.vVelocity = { dir.x * speed, 0.f, dir.z * speed };
            fx.bOverrideVelocity = true;
            fx.pFxMeshRenderer = pFxMeshRenderer;
            fx.bOverrideLifetime = true;
            fx.fLifetimeOverride = lifetimeSec;
            CFxCuePlayer::Play(world, pszCueName, fx);
        }

        void PlayAsheQAttackVolleyCue(
            CWorld& world,
            Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
            const Vec3& origin,
            const Vec3& forward)
        {
            const Vec3 dir = WintersMath::NormalizeXZOrZero(forward);
            const Vec3 right{ dir.z, 0.f, -dir.x };
            const f32_t center = static_cast<f32_t>(Ashe::VisualCue::kQAttackArrowCount - 1u) * 0.5f;

            for (u32_t i = 0; i < Ashe::VisualCue::kQAttackArrowCount; ++i)
            {
                const f32_t lane = static_cast<f32_t>(i) - center;
                const f32_t forwardBias =
                    (center - std::fabs(lane)) * Ashe::VisualCue::kQAttackArrowForwardFan;
                const Vec3 arrowOrigin{
                    origin.x + right.x * lane * Ashe::VisualCue::kQAttackArrowSideSpacing + dir.x * forwardBias,
                    origin.y,
                    origin.z + right.z * lane * Ashe::VisualCue::kQAttackArrowSideSpacing + dir.z * forwardBias
                };

                PlayAsheMovingCue(
                    world,
                    pFxMeshRenderer,
                    Ashe::VisualCue::kQAttackArrow,
                    arrowOrigin,
                    dir,
                    Ashe::VisualCue::kQAttackArrowSpeed,
                    Ashe::VisualCue::kQAttackArrowLifetime);
            }
        }

        bool_t IsAsheQActive(CWorld& world, EntityID caster)
        {
            return caster != NULL_ENTITY &&
                world.HasComponent<AsheStateComponent>(caster) &&
                world.GetComponent<AsheStateComponent>(caster).bQActive;
        }

        void MarkAsheQActiveVisualState(CWorld& world, EntityID caster, f32_t durationSec)
        {
            if (caster == NULL_ENTITY || !world.HasComponent<AsheStateComponent>(caster))
                return;

            AsheStateComponent& as = world.GetComponent<AsheStateComponent>(caster);
            as.bQActive = true;
            as.fQTimer = durationSec;
            as.focusStacks = 0;
        }
```

기존 코드:

```cpp
                if (as.focusStacks == as.focusThreshold)
                    Fx::SpawnQReadySparks(*ctx.pWorld, ctx.casterEntity, 0.6f);
```

아래로 교체:

```cpp
                if (as.focusStacks == as.focusThreshold)
                {
                    const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
                    PlayAsheAttachedCue(
                        *ctx.pWorld,
                        ctx.pFxMeshRenderer,
                        Ashe::VisualCue::kQReady,
                        ctx.casterEntity,
                        pos,
                        ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand),
                        0.6f);
                }
```

기존 코드:

```cpp
        as.bQActive = true;
        as.fQTimer = as.fQDurationSec;
        as.focusStacks = 0;

        OutputDebugStringA("[Ashe Q] Ranger's Focus activated\n");
```

아래로 교체:

```cpp
        as.bQActive = true;
        as.fQTimer = as.fQDurationSec;
        as.focusStacks = 0;

        const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
        PlayAsheAttachedCue(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            Ashe::VisualCue::kQActive,
            ctx.casterEntity,
            pos,
            ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand),
            as.fQDurationSec);

        OutputDebugStringA("[Ashe Q] Ranger's Focus activated\n");
```

기존 코드:

```cpp
            const Vec3 arrowDir = WintersMath::RotateXZ(dir, angle);
            Fx::SpawnWVolleyArrow(*ctx.pWorld, ctx.casterEntity, origin, arrowDir, 0.35f);
            CPendingHitSystem::Schedule(*ctx.pWorld,
                ctx.casterEntity, ctx.casterTeam, arrowDir,
                0.f, eProjectileKind::Wind,
                35.f, as.fVolleyRange, 0.35f,
                40.f, 0.f);
```

아래로 교체:

```cpp
            const Vec3 arrowDir = WintersMath::RotateXZ(dir, angle);
            PlayAsheMovingCue(
                *ctx.pWorld,
                ctx.pFxMeshRenderer,
                Ashe::VisualCue::kWArrow,
                origin,
                arrowDir,
                Ashe::VisualCue::kWArrowSpeed,
                Ashe::VisualCue::kWArrowLifetime);
            CPendingHitSystem::Schedule(*ctx.pWorld,
                ctx.casterEntity, ctx.casterTeam, arrowDir,
                0.f, eProjectileKind::AsheVolleyArrow,
                35.f, as.fVolleyRange, 0.35f,
                40.f, 0.f);
```

기존 코드:

```cpp
        char dbg[160];
        sprintf_s(dbg, "[Ashe E] Hawkshot dest=(%.1f,%.1f,%.1f) duration=%.1fs\n",
            dest.x, dest.y, dest.z, as.fHawkshotVisionDurationSec);
        OutputDebugStringA(dbg);
```

아래로 교체:

```cpp
        PlayAsheMovingCue(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            Ashe::VisualCue::kE,
            { origin.x, origin.y + Ashe::VisualCue::kEHeightOffset, origin.z },
            dir,
            as.fHawkshotRange / as.fHawkshotVisionDurationSec,
            as.fHawkshotVisionDurationSec);

        char dbg[160];
        sprintf_s(dbg, "[Ashe E] Hawkshot dest=(%.1f,%.1f,%.1f) duration=%.1fs\n",
            dest.x, dest.y, dest.z, as.fHawkshotVisionDurationSec);
        OutputDebugStringA(dbg);
```

기존 코드:

```cpp
        CPendingHitSystem::Schedule(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, dir,
            0.f, eProjectileKind::GlobalBeam,
            as.fCrystalArrowSpeed, as.fCrystalArrowMaxDist,
            1.0f, 250.f, as.fCrystalArrowStunMin);
```

아래로 교체:

```cpp
        CPendingHitSystem::Schedule(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, dir,
            0.f, eProjectileKind::AsheCrystalArrow,
            as.fCrystalArrowSpeed, as.fCrystalArrowMaxDist,
            1.0f, 250.f, as.fCrystalArrowStunMin);

        const Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);
        PlayAsheMovingCue(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            Ashe::VisualCue::kRArrow,
            origin,
            dir,
            as.fCrystalArrowSpeed,
            Ashe::VisualCue::kRArrowPreviewLifetime);
```

`namespace Visual` 전체를 아래 기존 코드로 찾아 교체:

기존 코드:

```cpp
    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            const EntityID target = ctx.pCommand->targetEntityId;
            const Vec3 origin = ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)
                ? ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition()
                : Vec3{};
            Fx::SpawnBAArrow(*ctx.pWorld, ctx.casterEntity, origin, ctx.pCommand->direction, 0.4f);
            Fx::SpawnBAArrowMesh(*ctx.pWorld, ctx.pFxMeshRenderer, origin, ctx.pCommand->direction, 0.4f);
            if (target != NULL_ENTITY)
                Fx::SpawnFrostHit(*ctx.pWorld, target, 0.4f);
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnQBuffActive(*ctx.pWorld, ctx.casterEntity, 4.0f);
            Fx::SpawnQBuffMesh(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, 4.0f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnWVolleyMuzzle(*ctx.pWorld, ctx.casterEntity, 0.4f);
            const Vec3 origin = ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)
                ? ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition()
                : Vec3{};
            const Vec3 dir = ctx.pCommand ? ctx.pCommand->direction : Vec3{ 0.f, 0.f, 1.f };
            Fx::SpawnWVolleyMesh(*ctx.pWorld, ctx.pFxMeshRenderer, origin, dir, 0.4f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

            const Vec3 origin =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 dir = ctx.pCommand->direction;
            const Vec3 dest = { origin.x + dir.x * 25.f, origin.y, origin.z + dir.z * 25.f };
            Fx::SpawnEHawkshot(*ctx.pWorld, origin, dest, 5.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnRCrystalCharge(*ctx.pWorld, ctx.casterEntity, 0.4f);
            const Vec3 origin = ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)
                ? ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition()
                : Vec3{};
            Fx::SpawnRCrystalArrow(*ctx.pWorld, ctx.casterEntity,
                origin, ctx.pCommand->direction, 0.8f);
            Fx::SpawnRCrystalArrowMesh(*ctx.pWorld, ctx.pFxMeshRenderer,
                origin, ctx.pCommand->direction, 0.8f);
        }
    }
```

아래로 교체:

```cpp
    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const EntityID target = ctx.pCommand->targetEntityId;
            const Vec3 casterPos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            const Vec3 forward = ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            const Vec3 origin{ casterPos.x, casterPos.y + 1.0f, casterPos.z };

            if (IsAsheQActive(*ctx.pWorld, ctx.casterEntity))
            {
                PlayAsheQAttackVolleyCue(*ctx.pWorld, ctx.pFxMeshRenderer, origin, forward);
            }
            else
            {
                PlayAsheMovingCue(
                    *ctx.pWorld,
                    ctx.pFxMeshRenderer,
                    Ashe::VisualCue::kBAArrow,
                    origin,
                    forward,
                    Ashe::VisualCue::kBAArrowSpeed,
                    Ashe::VisualCue::kBAArrowLifetime);
            }

            if (target != NULL_ENTITY)
            {
                const Vec3 targetPos = ResolveEntityPosition(*ctx.pWorld, target);
                PlayAsheAttachedCue(
                    *ctx.pWorld,
                    ctx.pFxMeshRenderer,
                    Ashe::VisualCue::kBAHit,
                    target,
                    targetPos,
                    forward,
                    Ashe::VisualCue::kBAArrowLifetime);
            }
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            f32_t durationSec = 4.0f;
            if (ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity))
                durationSec = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity).fQDurationSec;
            MarkAsheQActiveVisualState(*ctx.pWorld, ctx.casterEntity, durationSec);

            const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            PlayAsheAttachedCue(
                *ctx.pWorld,
                ctx.pFxMeshRenderer,
                Ashe::VisualCue::kQActive,
                ctx.casterEntity,
                pos,
                ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand),
                durationSec);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            PlayAsheAttachedCue(
                *ctx.pWorld,
                ctx.pFxMeshRenderer,
                Ashe::VisualCue::kWCast,
                ctx.casterEntity,
                pos,
                ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand),
                Ashe::VisualCue::kWCastLifetime);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 origin = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            const Vec3 dir = ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            PlayAsheMovingCue(
                *ctx.pWorld,
                ctx.pFxMeshRenderer,
                Ashe::VisualCue::kE,
                { origin.x, origin.y + Ashe::VisualCue::kEHeightOffset, origin.z },
                dir,
                5.0f,
                5.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            PlayAsheAttachedCue(
                *ctx.pWorld,
                ctx.pFxMeshRenderer,
                Ashe::VisualCue::kRCast,
                ctx.casterEntity,
                pos,
                ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand),
                Ashe::VisualCue::kRCastLifetime);
        }
    }
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxLegacyManifest.cpp

`Irelia.E.Connect` 항목 아래에 추가:

```cpp
            {
                "Ashe.BA.Arrow", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "MeshParticle,Ribbon,Billboard",
                L"Data/LoL/FX/Champions/Ashe/ba_arrow.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.BA.Hit", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Ashe/ba_hit.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.Q.Ready", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Ashe/q_ready.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.Q.Active", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "MeshParticle,GroundDecal,Billboard",
                L"Data/LoL/FX/Champions/Ashe/q_active.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.Q.AttackArrow", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "MeshParticle,Ribbon,Billboard",
                L"Data/LoL/FX/Champions/Ashe/q_attack_arrow.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.W.Cast", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ashe/w_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.W.Arrow", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/Network/Client/EventApplier.cpp",
                "MeshParticle,Ribbon,Billboard",
                L"Data/LoL/FX/Champions/Ashe/w_arrow.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.W.Hit", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/Network/Client/EventApplier.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Ashe/w_hit.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.E.Hawkshot", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "Billboard,Ribbon",
                L"Data/LoL/FX/Champions/Ashe/e_hawkshot.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.R.Cast", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "Billboard",
                L"Data/LoL/FX/Champions/Ashe/r_cast.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.R.Arrow", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/Network/Client/EventApplier.cpp",
                "MeshParticle,Ribbon,Billboard",
                L"Data/LoL/FX/Champions/Ashe/r_arrow.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Ashe.R.Hit", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/Network/Client/EventApplier.cpp",
                "Billboard,GroundDecal,ShockwaveRing",
                L"Data/LoL/FX/Champions/Ashe/r_hit.wfx",
                L"",
                "WfxPilot"
            },
```

1-9. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/ba_arrow.wfx

새 파일:

```json
{
  "name": "Ashe.BA.Arrow",
  "emitters": [
    {
      "name": "ba_arrow_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png",
      "lifetime": 0.42,
      "scale": [0.014, 0.014, 0.014],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.65, 1.10, 1.35, 0.92],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.16,
      "blockable_by_wind_wall": true
    },
    {
      "name": "ba_arrow_trail",
      "render_type": "Ribbon",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_mist_trail.png",
      "lifetime": 0.28,
      "width": 0.34,
      "height": 1.2,
      "color": [0.60, 0.95, 1.35, 0.58],
      "attach_offset": [0.0, 0.0, -0.32],
      "end_offset": [0.0, 0.0, -1.35],
      "fade_in": 0.01,
      "fade_out": 0.14,
      "uv_scroll": [0.0, -0.55],
      "blockable_by_wind_wall": true
    },
    {
      "name": "ba_arrow_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_glow.png",
      "lifetime": 0.30,
      "width": 0.82,
      "height": 0.82,
      "color": [0.70, 1.05, 1.45, 0.76],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.14,
      "billboard": true,
      "blockable_by_wind_wall": true
    }
  ]
}
```

1-10. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/ba_hit.wfx

새 파일:

```json
{
  "name": "Ashe.BA.Hit",
  "emitters": [
    {
      "name": "ba_hit_frost_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_color-rampdownfrost.png",
      "lifetime": 0.38,
      "width": 1.25,
      "height": 1.25,
      "color": [0.55, 0.95, 1.45, 0.90],
      "attach_offset": [0.0, 1.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.20,
      "billboard": true
    },
    {
      "name": "ba_hit_sparkle",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_ashe_teal_sparkle.png",
      "lifetime": 0.30,
      "start_delay": 0.03,
      "width": 0.9,
      "height": 0.9,
      "color": [0.80, 1.20, 1.55, 0.95],
      "attach_offset": [0.0, 1.15, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.16,
      "billboard": true
    }
  ]
}
```

1-11. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/q_ready.wfx

새 파일:

```json
{
  "name": "Ashe.Q.Ready",
  "emitters": [
    {
      "name": "q_ready_sparks",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_ready_brightsparks.png",
      "lifetime": 0.60,
      "width": 1.45,
      "height": 1.45,
      "color": [1.10, 1.25, 1.55, 0.95],
      "attach_offset": [0.0, 1.5, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "billboard": true
    },
    {
      "name": "q_ready_star",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_ready_brightsparks_star.png",
      "lifetime": 0.46,
      "start_delay": 0.04,
      "width": 1.15,
      "height": 1.15,
      "color": [1.20, 1.30, 1.60, 0.85],
      "attach_offset": [0.0, 1.62, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "billboard": true
    }
  ]
}
```

1-12. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/q_active.wfx

새 파일:

```json
{
  "name": "Ashe.Q.Active",
  "emitters": [
    {
      "name": "q_buff_sphere",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_q_buf_attack_sphere.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_buf.png",
      "lifetime": 4.0,
      "scale": [0.018, 0.018, 0.018],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.65, 1.15, 1.45, 0.75],
      "attach_offset": [0.0, 1.2, 0.0],
      "fade_in": 0.05,
      "fade_out": 1.10
    },
    {
      "name": "q_buff_ground",
      "render_type": "GroundDecal",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_buff_diffuse.png",
      "lifetime": 4.0,
      "width": 2.0,
      "height": 2.0,
      "color": [0.55, 0.88, 1.25, 0.48],
      "attach_offset": [0.0, 0.05, 0.0],
      "fade_in": 0.08,
      "fade_out": 1.35,
      "billboard": false
    },
    {
      "name": "q_buff_smoke",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_buff_ice_smoke.png",
      "lifetime": 1.0,
      "start_delay": 0.05,
      "width": 1.7,
      "height": 1.7,
      "color": [0.55, 0.95, 1.35, 0.50],
      "attach_offset": [0.0, 1.15, 0.0],
      "fade_in": 0.04,
      "fade_out": 0.55,
      "billboard": true
    }
  ]
}
```

1-13. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/q_attack_arrow.wfx

새 파일:

```json
{
  "name": "Ashe.Q.AttackArrow",
  "emitters": [
    {
      "name": "q_attack_arrow_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_mis_core.png",
      "lifetime": 0.42,
      "scale": [0.016, 0.016, 0.016],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.82, 1.25, 1.65, 1.0],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.16,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_attack_arrow_star",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_mis_star.png",
      "lifetime": 0.34,
      "width": 1.05,
      "height": 1.05,
      "color": [0.95, 1.28, 1.65, 0.86],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.14,
      "billboard": true,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_attack_arrow_trail",
      "render_type": "Ribbon",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_flare.png",
      "lifetime": 0.30,
      "width": 0.42,
      "height": 1.45,
      "color": [0.72, 1.08, 1.55, 0.62],
      "attach_offset": [0.0, 0.0, -0.35],
      "end_offset": [0.0, 0.0, -1.55],
      "fade_in": 0.01,
      "fade_out": 0.15,
      "uv_scroll": [0.0, -0.65],
      "blockable_by_wind_wall": true
    }
  ]
}
```

1-14. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/w_cast.wfx

새 파일:

```json
{
  "name": "Ashe.W.Cast",
  "emitters": [
    {
      "name": "w_bow_sparks",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_bow_sparks.png",
      "lifetime": 0.42,
      "width": 2.0,
      "height": 1.0,
      "color": [1.0, 1.20, 1.45, 0.95],
      "attach_offset": [0.0, 1.4, 0.8],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "billboard": true
    },
    {
      "name": "w_swirl_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_w_swirlmesh03.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_bow_sparks.png",
      "lifetime": 0.42,
      "scale": [0.018, 0.018, 0.018],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.75, 1.15, 1.45, 0.85],
      "attach_offset": [0.0, 1.2, 1.0],
      "fade_in": 0.01,
      "fade_out": 0.22
    },
    {
      "name": "w_wind_swirl",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_w_windswirl01.png",
      "lifetime": 0.36,
      "start_delay": 0.03,
      "width": 1.55,
      "height": 1.55,
      "color": [0.65, 0.95, 1.35, 0.55],
      "attach_offset": [0.0, 1.25, 0.95],
      "fade_in": 0.02,
      "fade_out": 0.20,
      "billboard": true
    }
  ]
}
```

1-15. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/w_arrow.wfx

새 파일:

```json
{
  "name": "Ashe.W.Arrow",
  "emitters": [
    {
      "name": "w_arrow_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png",
      "lifetime": 0.52,
      "scale": [0.014, 0.014, 0.014],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.76, 1.05, 1.38, 0.95],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.16,
      "blockable_by_wind_wall": true
    },
    {
      "name": "w_arrow_trail",
      "render_type": "Ribbon",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_mist_trail.png",
      "lifetime": 0.45,
      "width": 0.30,
      "height": 1.55,
      "color": [0.62, 0.92, 1.30, 0.54],
      "attach_offset": [0.0, 0.0, -0.28],
      "end_offset": [0.0, 0.0, -1.55],
      "fade_in": 0.01,
      "fade_out": 0.20,
      "uv_scroll": [0.0, -0.70],
      "blockable_by_wind_wall": true
    },
    {
      "name": "w_arrow_ice_flare",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_flaretrail.png",
      "lifetime": 0.34,
      "width": 0.75,
      "height": 0.75,
      "color": [0.70, 1.05, 1.45, 0.62],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.15,
      "billboard": true,
      "blockable_by_wind_wall": true
    }
  ]
}
```

1-16. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/w_hit.wfx

새 파일:

```json
{
  "name": "Ashe.W.Hit",
  "emitters": [
    {
      "name": "w_hit_frost",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_color-rampdownfrost.png",
      "lifetime": 0.34,
      "width": 1.2,
      "height": 1.2,
      "color": [0.55, 0.95, 1.40, 0.85],
      "attach_offset": [0.0, 0.75, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true
    }
  ]
}
```

1-17. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/e_hawkshot.wfx

새 파일:

```json
{
  "name": "Ashe.E.Hawkshot",
  "emitters": [
    {
      "name": "e_hawk_owl",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_e_textureowl.png",
      "lifetime": 5.0,
      "width": 1.4,
      "height": 1.0,
      "color": [0.80, 1.10, 1.35, 0.95],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.04,
      "fade_out": 1.15,
      "billboard": true
    },
    {
      "name": "e_hawk_wisp_trail",
      "render_type": "Ribbon",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_e_wispysmoke01.png",
      "lifetime": 5.0,
      "width": 0.58,
      "height": 2.4,
      "color": [0.55, 0.95, 1.30, 0.42],
      "attach_offset": [0.0, -0.05, -0.4],
      "end_offset": [0.0, -0.05, -2.4],
      "fade_in": 0.06,
      "fade_out": 1.20,
      "uv_scroll": [0.0, -0.30]
    },
    {
      "name": "e_hawk_smoke",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_e_smokeerosion.png",
      "lifetime": 0.8,
      "start_delay": 0.05,
      "width": 1.2,
      "height": 1.2,
      "color": [0.55, 0.85, 1.20, 0.42],
      "attach_offset": [0.0, -0.2, -0.5],
      "fade_in": 0.05,
      "fade_out": 0.45,
      "billboard": true
    }
  ]
}
```

1-18. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/r_cast.wfx

새 파일:

```json
{
  "name": "Ashe.R.Cast",
  "emitters": [
    {
      "name": "r_charge_star",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_ready_brightsparks_star.png",
      "lifetime": 0.45,
      "width": 1.7,
      "height": 1.7,
      "color": [1.20, 1.35, 1.60, 1.0],
      "attach_offset": [0.0, 1.5, 0.65],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "billboard": true
    },
    {
      "name": "r_charge_lines",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_ready_firelines.png",
      "lifetime": 0.36,
      "start_delay": 0.04,
      "width": 2.0,
      "height": 1.1,
      "color": [0.80, 1.15, 1.50, 0.72],
      "attach_offset": [0.0, 1.45, 0.8],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true
    }
  ]
}
```

1-19. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/r_arrow.wfx

새 파일:

```json
{
  "name": "Ashe.R.Arrow",
  "emitters": [
    {
      "name": "r_arrow_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_r_arrow.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_r_ice.sparx_ashe_r_update.png",
      "lifetime": 1.0,
      "scale": [0.018, 0.018, 0.018],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.80, 1.25, 1.60, 0.98],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "blockable_by_wind_wall": true
    },
    {
      "name": "r_arrow_core_star",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_mis_star.png",
      "lifetime": 0.75,
      "width": 1.65,
      "height": 1.65,
      "color": [0.95, 1.28, 1.60, 0.92],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.20,
      "billboard": true,
      "blockable_by_wind_wall": true
    },
    {
      "name": "r_arrow_ice_trail",
      "render_type": "Ribbon",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_r_ice_trail.sparx_ashe_r_update.png",
      "lifetime": 0.95,
      "width": 0.72,
      "height": 3.2,
      "color": [0.72, 1.10, 1.55, 0.70],
      "attach_offset": [0.0, 0.0, -0.55],
      "end_offset": [0.0, 0.0, -3.2],
      "fade_in": 0.01,
      "fade_out": 0.35,
      "uv_scroll": [0.0, -0.60],
      "blockable_by_wind_wall": true
    },
    {
      "name": "r_arrow_smoke_trail",
      "render_type": "Ribbon",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_r_smoke_trail.sparx_ashe_r_update.png",
      "lifetime": 0.95,
      "start_delay": 0.03,
      "width": 1.05,
      "height": 3.6,
      "color": [0.50, 0.78, 1.10, 0.36],
      "attach_offset": [0.0, 0.0, -0.75],
      "end_offset": [0.0, 0.0, -3.6],
      "fade_in": 0.03,
      "fade_out": 0.45,
      "uv_scroll": [0.0, -0.25],
      "blockable_by_wind_wall": true
    }
  ]
}
```

1-20. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/r_hit.wfx

새 파일:

```json
{
  "name": "Ashe.R.Hit",
  "emitters": [
    {
      "name": "r_hit_frost_burst",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_ashe_teal_sparkle.png",
      "lifetime": 0.58,
      "width": 2.1,
      "height": 2.1,
      "color": [0.70, 1.20, 1.55, 0.96],
      "attach_offset": [0.0, 1.25, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.32,
      "billboard": true
    },
    {
      "name": "r_hit_ground_frozen",
      "render_type": "GroundDecal",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ground_frozen.png",
      "lifetime": 1.2,
      "start_delay": 0.03,
      "width": 2.6,
      "height": 2.6,
      "color": [0.55, 0.90, 1.25, 0.46],
      "attach_offset": [0.0, 0.04, 0.0],
      "fade_in": 0.04,
      "fade_out": 0.65,
      "billboard": false
    },
    {
      "name": "r_hit_shock_ring",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/common_frost_cloud_01.png",
      "lifetime": 0.56,
      "start_delay": 0.02,
      "start_radius": 0.35,
      "end_radius": 1.75,
      "thickness": 0.14,
      "width": 1.0,
      "height": 1.0,
      "color": [0.70, 1.15, 1.50, 0.52],
      "attach_offset": [0.0, 0.05, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.34,
      "billboard": false
    }
  ]
}
```

확인 필요:
- `ProjectileVisualCatalog.cpp`를 추가하면 `Client.vcxproj`와 `.filters`에 포함해야 한다. XML 직접 패치는 이 계획서에서는 생략하고 빌드 전 프로젝트 등록만 확인한다.
- `Ashe.R.Arrow` FBX 전방축이 실제 런타임에서 뒤집혀 보이면 WFX `rotation` 또는 `CFxCuePlayer` MeshParticle yaw 보정을 R 전용으로 조정한다.
- `Ashe.Q.AttackArrow`가 원격 Ashe 기본 공격에서도 5발로 보이려면 Q active 상태가 서버 effect trigger 또는 replicated champion state로 클라이언트에 유지되는지 확인한다.
- `Ashe.W.Arrow`와 `Ashe.R.Arrow`는 서버 projectile event가 실제 투사체 visual의 단일 출처가 되어야 하므로, 서버 권위 F5에서 legacy local `OnCastFrame_W/R` 경로가 중복 재생되지 않는지 확인한다.

2. 검증

미검증:
- 코드/데이터 계획만 작성. 빌드 및 런타임 확인 미실행.

검증 명령:

```powershell
git diff --check
rg -n "AsheVolleyArrow|AsheCrystalArrow|ProjectileVisualCatalog|AsheVisualCue|Ashe\\.BA\\.Arrow|Ashe\\.Q\\.AttackArrow|Ashe\\.W\\.Arrow|Ashe\\.R\\.Arrow" Shared Client Data
MSBuild.exe Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

수동 확인:
- 서버 권위 F5에서 BA는 `Ashe.BA.Arrow` FBX + ribbon trail + `Ashe.BA.Hit` frost hit가 한 번만 보이는지 확인한다.
- Q ready/active 뒤 기본 공격은 `Ashe.Q.AttackArrow` cue 5개가 좌우 오프셋으로 동시에 나가고, 일반 BA 화살과 중복되지 않는지 확인한다.
- W는 `Ashe.W.Cast`가 시전 순간 재생되고, 서버 projectile spawn event 7개가 각각 `Ashe.W.Arrow` WFX를 재생하는지 확인한다.
- E는 `Ashe.E.Hawkshot` owl billboard와 ribbon/smoke trail이 지정 높이에서 전방으로 이동하는지 확인한다.
- R은 `Ashe.R.Cast`가 시전 순간 재생되고, 서버 projectile spawn event가 `Ashe.R.Arrow` FBX/ribbon/smoke trail을 단일 출처로 재생하며 hit/expire에서 `Ashe.R.Hit`이 한 번만 보이는지 확인한다.
- Debug 출력에 `[FxCuePlayer] Skipped cue emitter`가 나오면 해당 cue가 MeshParticle을 `pFxMeshRenderer` 없이 호출하는 경로가 남았는지 확인한다.
