Session - 애쉬 BA/Q/W/E/R 이펙트를 Irelia E polish 기준의 얇은 WFX cue 파이프라인으로 옮긴다.

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

1-3. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

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
```

아래로 교체:

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
        case eProjectileKind::AsheVolleyArrow:
        case eProjectileKind::AsheCrystalArrow:
            return true;
        default:
            return false;
        }
    }

    bool_t ShouldFallbackToGenericProjectileIfCueMissing(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::LeeSinQ:
        case eProjectileKind::ZedShuriken:
        case eProjectileKind::AsheVolleyArrow:
        case eProjectileKind::AsheCrystalArrow:
            return true;
        default:
            return false;
        }
    }
```

기존 코드:

```cpp
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
```

아래로 교체:

```cpp
    const char* ResolveProjectileSpawnCue(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::LeeSinQ:
            return "LeeSin.Q.Projectile";
        case eProjectileKind::ZedShuriken:
            return "Zed.Q.Projectile";
        case eProjectileKind::AsheVolleyArrow:
            return "Ashe.W.Arrow";
        case eProjectileKind::AsheCrystalArrow:
            return "Ashe.R.Arrow";
        default:
            return nullptr;
        }
    }
```

기존 코드:

```cpp
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

아래로 교체:

```cpp
    const char* ResolveProjectileHitCue(u16_t kind)
    {
        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::LeeSinQ:
            return "LeeSin.Q.Hit";
        case eProjectileKind::ZedShuriken:
            return "Zed.Q.Hit";
        case eProjectileKind::AsheVolleyArrow:
            return "Ashe.W.Hit";
        case eProjectileKind::AsheCrystalArrow:
            return "Ashe.R.Hit";
        default:
            return nullptr;
        }
    }
```

기존 코드:

```cpp
        fx.vWorldPos = pos;
        fx.vForward = dir;
        fx.vVelocity = velocity;
        fx.bOverrideVelocity = true;
        fx.fLifetimeOverride = lifetime;
        fx.bOverrideLifetime = true;
        bPlayedProjectileWfxCue = CFxCuePlayer::Play(world, pszCueName, fx) != NULL_ENTITY;
```

아래로 교체:

```cpp
        fx.vWorldPos = pos;
        fx.vForward = dir;
        fx.vVelocity = velocity;
        fx.bOverrideVelocity = true;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        fx.fLifetimeOverride = lifetime;
        fx.bOverrideLifetime = true;
        bPlayedProjectileWfxCue = CFxCuePlayer::Play(world, pszCueName, fx) != NULL_ENTITY;
```

기존 코드:

```cpp
    const bool_t bShouldSpawnGenericProjectile =
        !bTurretProjectile &&
        (!bChampionProjectileVisual ||
         ((static_cast<eProjectileKind>(ev->kind()) == eProjectileKind::LeeSinQ ||
           static_cast<eProjectileKind>(ev->kind()) == eProjectileKind::ZedShuriken) &&
             !bPlayedProjectileWfxCue));
```

아래로 교체:

```cpp
    const bool_t bShouldSpawnGenericProjectile =
        !bTurretProjectile &&
        (!bChampionProjectileVisual ||
            (ShouldFallbackToGenericProjectileIfCueMissing(ev->kind()) &&
                !bPlayedProjectileWfxCue));
```

기존 코드:

```cpp
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = { 0.f, 0.f, 1.f };
        bPlayedWfxCue = CFxCuePlayer::Play(world, pszHitCueName, fx) != NULL_ENTITY;
```

아래로 교체:

```cpp
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = { 0.f, 0.f, 1.f };
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        bPlayedWfxCue = CFxCuePlayer::Play(world, pszHitCueName, fx) != NULL_ENTITY;
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp

기존 코드:

```cpp
#include "GameObject/Champion/Ashe/Ashe_FxPresets.h"
```

아래로 교체:

```cpp
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
            const Vec3& forward,
            f32_t speed,
            f32_t lifetimeSec)
        {
            const Vec3 dir = WintersMath::NormalizeXZOrZero(forward);
            const Vec3 right{ dir.z, 0.f, -dir.x };
            constexpr f32_t kOffsets[] = { -0.28f, -0.14f, 0.f, 0.14f, 0.28f };
            constexpr f32_t kForwardOffsets[] = { -0.10f, 0.06f, 0.18f, 0.06f, -0.10f };

            for (u32_t i = 0; i < 5u; ++i)
            {
                FxCueContext fx{};
                fx.vWorldPos = {
                    origin.x + right.x * kOffsets[i] + dir.x * kForwardOffsets[i],
                    origin.y,
                    origin.z + right.z * kOffsets[i] + dir.z * kForwardOffsets[i]
                };
                fx.vForward = dir;
                fx.vVelocity = { dir.x * speed, 0.f, dir.z * speed };
                fx.bOverrideVelocity = true;
                fx.pFxMeshRenderer = pFxMeshRenderer;
                fx.bOverrideLifetime = true;
                fx.fLifetimeOverride = lifetimeSec;
                CFxCuePlayer::Play(world, "Ashe.Q.AttackArrow", fx);
            }
        }

        bool_t IsAsheQActive(CWorld& world, EntityID caster)
        {
            return caster != NULL_ENTITY &&
                world.HasComponent<AsheStateComponent>(caster) &&
                world.GetComponent<AsheStateComponent>(caster).bQActive;
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
                    PlayAsheAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                        "Ashe.Q.Ready", ctx.casterEntity, pos,
                        ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand), 0.6f);
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
        PlayAsheAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
            "Ashe.Q.Active", ctx.casterEntity, pos,
            ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand), as.fQDurationSec);

        OutputDebugStringA("[Ashe Q] Ranger's Focus activated\n");
```

기존 코드:

```cpp
            const Vec3 arrowDir = WintersMath::RotateXZ(dir, angle);
            Fx::SpawnWVolleyArrow(*ctx.pWorld, ctx.casterEntity, origin, arrowDir, 0.35f);
            CPendingHitSystem::Schedule(*ctx.pWorld,
```

아래로 교체:

```cpp
            const Vec3 arrowDir = WintersMath::RotateXZ(dir, angle);
            PlayAsheMovingCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                "Ashe.W.Arrow", origin, arrowDir, 24.f, 0.35f);
            CPendingHitSystem::Schedule(*ctx.pWorld,
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
        PlayAsheMovingCue(*ctx.pWorld, ctx.pFxMeshRenderer,
            "Ashe.E.Hawkshot", { origin.x, origin.y + 3.0f, origin.z },
            dir, as.fHawkshotRange / as.fHawkshotVisionDurationSec,
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
        PlayAsheMovingCue(*ctx.pWorld, ctx.pFxMeshRenderer,
            "Ashe.R.Arrow", origin, dir, as.fCrystalArrowSpeed, 0.8f);
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
            if (!ctx.pWorld || !ctx.pCommand) return;

            const EntityID target = ctx.pCommand->targetEntityId;
            const Vec3 casterPos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            const Vec3 forward = ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);

            if (IsAsheQActive(*ctx.pWorld, ctx.casterEntity))
            {
                PlayAsheQAttackVolleyCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                    { casterPos.x, casterPos.y + 1.0f, casterPos.z }, forward, 18.f, 0.4f);
            }
            else
            {
                PlayAsheMovingCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                    "Ashe.BA.Arrow", { casterPos.x, casterPos.y + 1.0f, casterPos.z },
                    forward, 18.f, 0.4f);
            }

            if (target != NULL_ENTITY)
            {
                const Vec3 targetPos = ResolveEntityPosition(*ctx.pWorld, target);
                PlayAsheAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                    "Ashe.BA.Hit", target, targetPos, forward, 0.4f);
            }
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;

            const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            PlayAsheAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                "Ashe.Q.Active", ctx.casterEntity, pos,
                ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand), 4.0f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;

            const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            PlayAsheAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                "Ashe.W.Cast", ctx.casterEntity, pos,
                ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand), 0.45f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;

            const Vec3 origin = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            const Vec3 forward = ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            PlayAsheMovingCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                "Ashe.E.Hawkshot", { origin.x, origin.y + 3.0f, origin.z },
                forward, 5.0f, 5.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;

            const Vec3 pos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);
            PlayAsheAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                "Ashe.R.Cast", ctx.casterEntity, pos,
                ResolveAsheForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand), 0.45f);
        }
    }
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxLegacyManifest.cpp

기존 코드:

```cpp
            {
                "Irelia.E.Connect", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "MeshParticle,Beam",
                L"Data/LoL/FX/Champions/Irelia/e_connect.wfx",
                L"",
                "WfxPilot"
            },
```

아래에 추가:

```cpp
            {
                "Ashe.BA.Arrow", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp",
                "MeshParticle,Billboard",
                L"Data/LoL/FX/Champions/Ashe/ba_arrow.wfx",
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
                "Ashe.W.Arrow", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/Network/Client/EventApplier.cpp",
                "MeshParticle,Ribbon,Billboard",
                L"Data/LoL/FX/Champions/Ashe/w_arrow.wfx",
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
                "Ashe.R.Arrow", "Ashe", "CFxCuePlayer::Play",
                "Client/Private/Network/Client/EventApplier.cpp",
                "MeshParticle,Ribbon,Billboard",
                L"Data/LoL/FX/Champions/Ashe/r_arrow.wfx",
                L"",
                "WfxPilot"
            },
```

1-6. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/ba_arrow.wfx

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

1-7. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/ba_hit.wfx

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

1-8. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/q_ready.wfx

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

1-9. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/q_active.wfx

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

1-10. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/q_attack_arrow.wfx

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

1-11. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/w_cast.wfx

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

1-12. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/w_arrow.wfx

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

1-13. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/w_hit.wfx

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

1-14. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/e_hawkshot.wfx

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

1-15. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/r_cast.wfx

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

1-16. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/r_arrow.wfx

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

1-17. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/r_hit.wfx

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
- `Ashe.R.Arrow` FBX의 전방축이 Kalista 창처럼 `+PI` 보정이 필요한지 실제 렌더에서 확인해야 한다. 현재 애쉬 C++ `SpawnMeshVisual`은 `+PI` 없이 쓰고 있으므로 1차 WFX도 `rotation: [0,0,0]`으로 둔다.
- `Ashe.Q.AttackArrow`가 네트워크 경로에서 Q active 상태를 정확히 따라가려면 `AsheStateComponent`가 클라이언트에 동기화되는지 확인해야 한다. 동기화가 없으면 Q 시전 직후 local visual state만 유지된다.
- WFX 파일은 데이터 디렉터리 로더가 `Data/LoL/FX`를 recursive load하므로 `.vcxproj` 등록은 필요 없어 보인다. 단, 패키징 복사 대상에 `Data/LoL/FX/Champions/Ashe/*.wfx`가 포함되는지 확인해야 한다.

2. 검증

미검증
- 코드/데이터 계획만 작성. 빌드 및 런타임 확인 미실행.

검증 명령:

```powershell
git diff --check
rg -n "AsheVolleyArrow|AsheCrystalArrow|Ashe\\.BA\\.Arrow|Ashe\\.Q\\.Active|Ashe\\.W\\.Arrow|Ashe\\.E\\.Hawkshot|Ashe\\.R\\.Arrow" Shared Client Data
MSBuild.exe Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

수동 확인:
- 서버 권위 F5에서 애쉬 기본 공격 시 `Ashe.BA.Arrow` FBX 화살과 `Ashe.BA.Hit` frost hit가 한 번만 보이는지 확인한다.
- Q ready/active 후 기본 공격 시 `Ashe.Q.Active` 버프와 `Ashe.Q.AttackArrow` 5개 cue 인스턴스가 좌우 오프셋으로 동시에 날아가는지 확인한다.
- W 시전 시 `Ashe.W.Cast` muzzle/swirl이 보이고, 서버 projectile spawn 이벤트에서 7발 `Ashe.W.Arrow`가 각 방향으로 이동하는지 확인한다.
- E 시전 시 `Ashe.E.Hawkshot` owl billboard/ribbon이 5초 동안 전방으로 이동하는지 확인한다.
- R 시전 시 `Ashe.R.Cast` charge 후 서버 projectile spawn 이벤트에서 `Ashe.R.Arrow` FBX 수정 화살이 날아가고, hit/expire에서 `Ashe.R.Hit`가 한 번만 보이는지 확인한다.
- `OutputDebugStringA`에 `[FxCuePlayer] Skipped cue emitter`가 남으면 해당 WFX의 `MeshParticle`가 `pFxMeshRenderer` 없이 호출된 경로가 남아 있는지 확인한다.
