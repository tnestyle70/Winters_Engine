Session - Sylas를 LeeSin WFX 세로 슬라이스처럼 Q/W/E/R FX와 최소 서버 GameSim 로직에 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Sylas/Sylas_Skills.h

새 파일:

```cpp
#pragma once

struct VisualHookContext;

namespace Sylas
{
    namespace Visual
    {
        void OnQCastFrame(VisualHookContext& ctx);
        void OnWCastFrame(VisualHookContext& ctx);
        void OnECastFrame(VisualHookContext& ctx);
        void OnRCastFrame(VisualHookContext& ctx);
    }
}

void Sylas_KeepAlive();
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Sylas/Sylas_Skills.cpp

새 파일:

```cpp
#include "GameObject/Champion/Sylas/Sylas_Skills.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GamePlay/VisualHookRegistry.h"
#include "WintersMath.h"

#include <cmath>

namespace
{
    bool_t IsMeaningfulPosition(const Vec3& v)
    {
        return std::fabs(v.x) > 0.001f ||
            std::fabs(v.y) > 0.001f ||
            std::fabs(v.z) > 0.001f;
    }

    Vec3 ResolveCasterPosition(VisualHookContext& ctx)
    {
        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        }

        return ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
    }

    Vec3 ResolveEffectPosition(VisualHookContext& ctx)
    {
        if (ctx.pCommand && IsMeaningfulPosition(ctx.pCommand->groundPos))
            return ctx.pCommand->groundPos;

        return ResolveCasterPosition(ctx);
    }

    Vec3 ResolveForward(VisualHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 vCommandDir = WintersMath::NormalizeXZOrZero(ctx.pCommand->direction);
            if (vCommandDir.x != 0.f || vCommandDir.z != 0.f)
                return vCommandDir;
        }

        if (ctx.pWorld && ctx.pCommand &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pCommand->targetEntityId != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity) &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
        {
            const Vec3 vCaster =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 vTarget =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
            const Vec3 vToTarget{ vTarget.x - vCaster.x, 0.f, vTarget.z - vCaster.z };
            const Vec3 vTargetDir = WintersMath::NormalizeXZOrZero(vToTarget);
            if (vTargetDir.x != 0.f || vTargetDir.z != 0.f)
                return vTargetDir;
        }

        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            const f32_t yaw =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetRotation().y;
            return WintersMath::DirectionFromYawXZ(yaw);
        }

        return { 0.f, 0.f, 1.f };
    }

    void PlaySylasCueAt(VisualHookContext& ctx, const char* pszCueName, const Vec3& vWorldPos, bool_t bAttachToCaster)
    {
        if (!ctx.pWorld)
            return;

        FxCueContext fx{};
        fx.vWorldPos = vWorldPos;
        fx.vForward = ResolveForward(ctx);
        fx.attachTo = bAttachToCaster ? ctx.casterEntity : NULL_ENTITY;
        CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx);
    }

    void PlaySylasCue(VisualHookContext& ctx, const char* pszCueName, bool_t bAttachToCaster)
    {
        PlaySylasCueAt(ctx, pszCueName, ResolveCasterPosition(ctx), bAttachToCaster);
    }
}

namespace Sylas
{
    namespace Visual
    {
        void OnQCastFrame(VisualHookContext& ctx)
        {
            PlaySylasCue(ctx, "Sylas.Q.Cast", true);
            PlaySylasCueAt(ctx, "Sylas.Q.Explosion", ResolveEffectPosition(ctx), false);
        }

        void OnWCastFrame(VisualHookContext& ctx)
        {
            PlaySylasCue(ctx, "Sylas.W.Cast", true);
        }

        void OnECastFrame(VisualHookContext& ctx)
        {
            if (ctx.skillStage >= 2u)
                PlaySylasCue(ctx, "Sylas.E2.Chain", false);
            else
                PlaySylasCue(ctx, "Sylas.E1.Dash", true);
        }

        void OnRCastFrame(VisualHookContext& ctx)
        {
            PlaySylasCue(ctx, "Sylas.R.Cast", true);
        }
    }
}
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Sylas/Sylas_Registration.cpp

새 파일:

```cpp
#include "GameObject/Champion/Sylas/Sylas_Skills.h"

#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kSylas_BA_Cast = MakeHookId(eChampion::SYLAS, HookVariant::BA_CastFrame);
    constexpr u32_t kSylas_Q_Cast = MakeHookId(eChampion::SYLAS, HookVariant::Q_CastFrame);
    constexpr u32_t kSylas_W_Cast = MakeHookId(eChampion::SYLAS, HookVariant::W_CastFrame);
    constexpr u32_t kSylas_E_Cast = MakeHookId(eChampion::SYLAS, HookVariant::E_CastFrame);
    constexpr u32_t kSylas_R_Cast = MakeHookId(eChampion::SYLAS, HookVariant::R_CastFrame);

    f32_t ResolveRange(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::BasicAttack: return 1.5f;
        case eSkillSlot::Q: return 7.75f;
        case eSkillSlot::W: return 4.0f;
        case eSkillSlot::E: return 8.0f;
        case eSkillSlot::R: return 9.5f;
        default: return 0.f;
        }
    }

    void RegisterSkill(u8_t slot, eTargetMode targetMode, const char* animKey, u32_t hookId)
    {
        SkillDef s{};
        s.champ = eChampion::SYLAS;
        s.slot = slot;
        s.targetMode = targetMode;
        s.cooldownSec = 0.6f;
        s.rangeMax = ResolveRange(slot);
        s.animKey = animKey;
        s.lockDurationSec = 0.55f;
        s.bOneShot = true;
        s.rotate = targetMode == eTargetMode::Self ? eRotateMode::None : eRotateMode::TowardsCursor;
        s.castFrame = 4.f;
        s.recoveryFrame = 12.f;
        s.animPlaySpeed = 1.f;
        s.castFrameHookId = hookId;

        if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            s.stageCount = 2;
            s.stage2TargetMode = eTargetMode::UnitTarget;
            s.stage2AnimKey = "skinned_mesh_sylas_spell3_bhit_cast";
            s.stage2LockSec = 0.50f;
            s.stage2Rotate = eRotateMode::TowardsTarget;
            s.stageWindowSec = 3.f;
            s.stage2CastFrame = 4.f;
            s.stage2RecoveryFrame = 12.f;
            s.stage2PlaySpeed = 1.f;
        }

        CSkillRegistry::Instance().Add(eChampion::SYLAS, slot, s);
    }

    struct SylasAutoRegister
    {
        SylasAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::SYLAS;
            cd.animPrefix = "";
            cd.idleAnimKey = "skinned_mesh_sylas_idle";
            cd.runAnimKey = "skinned_mesh_sylas_run";
            cd.basicAttackKey = "skinned_mesh_sylas_attack_01";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Sylas/sylas.wmesh";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath = L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png";
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = cd.defaultTexturePath;
            cd.texturePath[2] = L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png";
            cd.texturePath[3] = L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png";
            cd.spawnPosition = { -27.f, 1.f, 6.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Sylas";
            CChampionRegistry::Instance().Add(eChampion::SYLAS, cd);

            RegisterSkill(0, eTargetMode::UnitTarget, "skinned_mesh_sylas_attack_01", kSylas_BA_Cast);
            RegisterSkill(1, eTargetMode::GroundTarget, "skinned_mesh_sylas_spell1_cast", kSylas_Q_Cast);
            RegisterSkill(2, eTargetMode::UnitTarget, "skinned_mesh_sylas_spell2", kSylas_W_Cast);
            RegisterSkill(3, eTargetMode::Direction, "skinned_mesh_sylas_spell3_dash", kSylas_E_Cast);
            RegisterSkill(4, eTargetMode::UnitTarget, "skinned_mesh_sylas_spell4_cast", kSylas_R_Cast);

            CVisualHookRegistry::Instance().Register(kSylas_Q_Cast, &Sylas::Visual::OnQCastFrame);
            CVisualHookRegistry::Instance().Register(kSylas_W_Cast, &Sylas::Visual::OnWCastFrame);
            CVisualHookRegistry::Instance().Register(kSylas_E_Cast, &Sylas::Visual::OnECastFrame);
            CVisualHookRegistry::Instance().Register(kSylas_R_Cast, &Sylas::Visual::OnRCastFrame);

            OutputDebugStringA("[Sylas] Registration complete\n");
        }
    };

    static SylasAutoRegister s_register;
}

void Sylas_KeepAlive()
{
    (void)&s_register;
}
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GamePlay/ChampionModuleBootstrap.cpp

기존 코드:

```cpp
extern void MasterYi_KeepAlive();
```

아래에 추가:

```cpp
extern void Sylas_KeepAlive();
```

기존 코드:

```cpp
    MasterYi_KeepAlive();
```

아래에 추가:

```cpp
    Sylas_KeepAlive();
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

기존 코드:

```cpp
    { eChampion::SYLAS, "sylas_", "idle1", "run", "attack1", 1.5f,
      "Client/Bin/Resource/Texture/Character/Sylas/sylas.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
      {
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png"
      },
      { -27.f, 1.f, 6.f },
      0.01f,
      "Sylas" },
```

아래로 교체:

```cpp
    { eChampion::SYLAS, "", "skinned_mesh_sylas_idle", "skinned_mesh_sylas_run", "skinned_mesh_sylas_attack_01", 1.5f,
      "Client/Bin/Resource/Texture/Character/Sylas/sylas.wmesh",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
      {
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png",
          L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png"
      },
      { -27.f, 1.f, 6.f },
      0.01f,
      "Sylas" },
```

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/SylasSimComponent.h

새 파일:

```cpp
#pragma once

#include "WintersMath.h"

struct SylasSimComponent
{
};

struct SylasDashComponent
{
    Vec3 vStart{};
    Vec3 vEnd{};
    f32_t fElapsedSec = 0.f;
    f32_t fDurationSec = 0.14f;
};
```

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/SylasGameSim.h

새 파일:

```cpp
#pragma once

class CWorld;
struct TickContext;

namespace SylasGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
```

1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/SylasGameSim.cpp

새 파일:

```cpp
#include "Shared/GameSim/Champions/SylasGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr f32_t kSylasQDamage = 70.f;
    constexpr f32_t kSylasQRadius = 1.65f;
    constexpr f32_t kSylasWDamage = 75.f;
    constexpr f32_t kSylasWHeal = 65.f;
    constexpr f32_t kSylasWDashGap = 0.85f;
    constexpr f32_t kSylasWDurationSec = 0.12f;
    constexpr f32_t kSylasE1Range = 4.0f;
    constexpr f32_t kSylasE2Damage = 85.f;
    constexpr f32_t kSylasE2DashGap = 0.95f;
    constexpr f32_t kSylasE2DurationSec = 0.16f;
    constexpr f32_t kSylasSpellbookDurationSec = 12.f;

    eTeam ResolveTeam(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).team;
        return eTeam::Neutral;
    }

    bool_t IsAlive(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;
        if (!world.HasComponent<HealthComponent>(entity))
            return true;
        const auto& hp = world.GetComponent<HealthComponent>(entity);
        return !hp.bIsDead && hp.fCurrent > 0.f;
    }

    bool_t IsEnemy(CWorld& world, EntityID source, EntityID target)
    {
        const eTeam sourceTeam = ResolveTeam(world, source);
        const eTeam targetTeam = ResolveTeam(world, target);
        return sourceTeam != targetTeam || sourceTeam == eTeam::Neutral;
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    Vec3 ResolveCasterPosition(CWorld& world, EntityID caster)
    {
        if (caster != NULL_ENTITY && world.HasComponent<TransformComponent>(caster))
            return world.GetComponent<TransformComponent>(caster).GetPosition();
        return {};
    }

    Vec3 ResolveCommandDirection(CWorld& world, const GameCommand& cmd)
    {
        Vec3 dir = WintersMath::NormalizeXZ(cmd.direction);
        if (dir.x != 0.f || dir.z != 0.f)
            return dir;

        if (cmd.targetEntity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(cmd.issuerEntity) &&
            world.HasComponent<TransformComponent>(cmd.targetEntity))
        {
            const Vec3 a = world.GetComponent<TransformComponent>(cmd.issuerEntity).GetPosition();
            const Vec3 b = world.GetComponent<TransformComponent>(cmd.targetEntity).GetPosition();
            dir = WintersMath::NormalizeXZ(Vec3{ b.x - a.x, 0.f, b.z - a.z });
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }

        return { 0.f, 0.f, 1.f };
    }

    void RotateToward(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;

        const Vec3 dir = WintersMath::NormalizeXZ(direction);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawFromDirection(eChampion::SYLAS, dir),
            rot.z });
    }

    void EnqueueMagicDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        f32_t amount,
        u8_t slot,
        u8_t rank)
    {
        if (!IsAlive(world, target) || !world.HasComponent<HealthComponent>(target))
            return;
        if (!IsEnemy(world, source, target))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = ResolveTeam(world, source);
        request.type = eDamageType::Magic;
        request.flatAmount = amount;
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::SYLAS) << 8) | slot);
        request.rank = rank;
        EnqueueDamageRequest(world, request);
    }

    void HealCaster(CWorld& world, EntityID caster, f32_t amount)
    {
        if (!IsAlive(world, caster) || !world.HasComponent<HealthComponent>(caster))
            return;

        auto& hp = world.GetComponent<HealthComponent>(caster);
        hp.fCurrent = std::min(hp.fMaximum, hp.fCurrent + amount);
        if (world.HasComponent<ChampionComponent>(caster))
        {
            auto& champion = world.GetComponent<ChampionComponent>(caster);
            champion.hp = hp.fCurrent;
            champion.maxHp = hp.fMaximum;
        }
    }

    void StartDash(CWorld& world, EntityID caster, const Vec3& end, f32_t durationSec)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        SylasDashComponent dash{};
        dash.vStart = world.GetComponent<TransformComponent>(caster).GetPosition();
        dash.vEnd = end;
        dash.fDurationSec = durationSec;

        if (world.HasComponent<SylasDashComponent>(caster))
            world.GetComponent<SylasDashComponent>(caster) = dash;
        else
            world.AddComponent<SylasDashComponent>(caster, dash);

        ClearMove(world, caster);
    }

    Vec3 ClampDashDestination(CWorld& world, const TickContext& tc, EntityID caster, const Vec3& dest)
    {
        if (!tc.pWalkable || !world.HasComponent<TransformComponent>(caster))
            return dest;

        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        Vec3 guarded = dest;
        if (!tc.pWalkable->TryClampMoveSegmentXZ(origin, dest, 0.5f, guarded))
            return origin;

        f32_t surfaceY = 0.f;
        if (tc.pWalkable->TrySampleHeight(guarded.x, guarded.z, surfaceY))
            guarded.y = surfaceY;
        return guarded;
    }

    void StartDashTowardTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        f32_t gap,
        f32_t durationSec)
    {
        if (!world.HasComponent<TransformComponent>(caster) ||
            target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{
            targetPos.x - start.x,
            0.f,
            targetPos.z - start.z
        });
        const f32_t dx = targetPos.x - start.x;
        const f32_t dz = targetPos.z - start.z;
        const f32_t dist = std::sqrt(dx * dx + dz * dz);
        const f32_t moveDist = std::max(0.f, dist - gap);
        const Vec3 dest = ClampDashDestination(
            world,
            tc,
            caster,
            Vec3{ start.x + dir.x * moveDist, start.y, start.z + dir.z * moveDist });

        RotateToward(world, caster, dir);
        StartDash(world, caster, dest, durationSec);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const Vec3 center = ctx.pCommand->groundPos.x != 0.f || ctx.pCommand->groundPos.z != 0.f
            ? ctx.pCommand->groundPos
            : ResolveCasterPosition(*ctx.pWorld, ctx.casterEntity);

        std::vector<EntityID> targets;
        ctx.pWorld->ForEach<ChampionComponent, HealthComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, HealthComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent&, HealthComponent& health, TransformComponent& transform)
                {
                    if (entity == ctx.casterEntity || health.bIsDead || health.fCurrent <= 0.f)
                        return;
                    if (!IsEnemy(*ctx.pWorld, ctx.casterEntity, entity))
                        return;

                    const Vec3 pos = transform.GetPosition();
                    if (WintersMath::DistanceSqXZ(pos, center) <= kSylasQRadius * kSylasQRadius)
                        targets.push_back(entity);
                }));

        for (EntityID target : targets)
        {
            EnqueueMagicDamage(
                *ctx.pWorld,
                ctx.casterEntity,
                target,
                kSylasQDamage + static_cast<f32_t>(ctx.skillRank - 1u) * 25.f,
                static_cast<u8_t>(eSkillSlot::Q),
                ctx.skillRank);
        }
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        const EntityID target = ctx.pCommand->targetEntity;
        if (!IsAlive(*ctx.pWorld, target) || !IsEnemy(*ctx.pWorld, ctx.casterEntity, target))
            return;

        StartDashTowardTarget(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target,
            kSylasWDashGap,
            kSylasWDurationSec);
        EnqueueMagicDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            target,
            kSylasWDamage + static_cast<f32_t>(ctx.skillRank - 1u) * 25.f,
            static_cast<u8_t>(eSkillSlot::W),
            ctx.skillRank);
        HealCaster(
            *ctx.pWorld,
            ctx.casterEntity,
            kSylasWHeal + static_cast<f32_t>(ctx.skillRank - 1u) * 20.f);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        if (ctx.pCommand->itemId == 2u)
        {
            const EntityID target = ctx.pCommand->targetEntity;
            if (!IsAlive(*ctx.pWorld, target) || !IsEnemy(*ctx.pWorld, ctx.casterEntity, target))
                return;

            StartDashTowardTarget(
                *ctx.pWorld,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target,
                kSylasE2DashGap,
                kSylasE2DurationSec);
            EnqueueMagicDamage(
                *ctx.pWorld,
                ctx.casterEntity,
                target,
                kSylasE2Damage + static_cast<f32_t>(ctx.skillRank - 1u) * 25.f,
                static_cast<u8_t>(eSkillSlot::E),
                ctx.skillRank);
            return;
        }

        const Vec3 start = ResolveCasterPosition(*ctx.pWorld, ctx.casterEntity);
        const Vec3 dir = ResolveCommandDirection(*ctx.pWorld, *ctx.pCommand);
        const Vec3 dest = ClampDashDestination(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            Vec3{ start.x + dir.x * kSylasE1Range, start.y, start.z + dir.z * kSylasE1Range });

        RotateToward(*ctx.pWorld, ctx.casterEntity, dir);
        StartDash(*ctx.pWorld, ctx.casterEntity, dest, 0.12f);
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const EntityID target = ctx.pCommand->targetEntity;
        if (!IsAlive(*ctx.pWorld, target) ||
            !ctx.pWorld->HasComponent<ChampionComponent>(target))
        {
            return;
        }

        const eChampion stolenChampion =
            ctx.pWorld->GetComponent<ChampionComponent>(target).id;
        if (stolenChampion == eChampion::NONE ||
            stolenChampion == eChampion::END ||
            stolenChampion == eChampion::SYLAS)
        {
            return;
        }

        SpellbookOverrideComponent spellbook{};
        spellbook.sourceChampion = stolenChampion;
        spellbook.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.localSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.fRemainingSec = kSylasSpellbookDurationSec;
        spellbook.bActive = true;

        if (ctx.pWorld->HasComponent<SpellbookOverrideComponent>(ctx.casterEntity))
            ctx.pWorld->GetComponent<SpellbookOverrideComponent>(ctx.casterEntity) = spellbook;
        else
            ctx.pWorld->AddComponent<SpellbookOverrideComponent>(ctx.casterEntity, spellbook);

        std::cout << "[SylasSim] hijack sourceChampion="
            << static_cast<u32_t>(stolenChampion)
            << " caster=" << ctx.casterEntity
            << " target=" << target << "\n";
    }
}

namespace SylasGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[SylasSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<SylasDashComponent, TransformComponent>(
            std::function<void(EntityID, SylasDashComponent&, TransformComponent&)>(
                [&](EntityID entity, SylasDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

                    dash.fElapsedSec += tc.fDt;
                    f32_t t = dash.fDurationSec > 0.01f
                        ? dash.fElapsedSec / dash.fDurationSec
                        : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }

                    const Vec3 position{
                        dash.vStart.x + (dash.vEnd.x - dash.vStart.x) * t,
                        dash.vStart.y + (dash.vEnd.y - dash.vStart.y) * t,
                        dash.vStart.z + (dash.vEnd.z - dash.vStart.z) * t
                    };

                    Vec3 guardedPosition = position;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable)
                    {
                        const Vec3 currentPos = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(currentPos, position, 0.5f, guardedPosition))
                        {
                            guardedPosition = currentPos;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(guardedPosition, position) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }
                    }

                    transform.SetPosition(guardedPosition);
                    if (bDashBlocked && t < 1.f)
                        finishedDashes.push_back(entity);
                }));

        for (EntityID entity : finishedDashes)
            world.RemoveComponent<SylasDashComponent>(entity);
    }
}
```

1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

`kChampionSkillTimingTable` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        { eChampion::KALISTA, 4, 1, 0.50f, 1.00f },
```

아래에 추가:

```cpp

        { eChampion::SYLAS, 0, 1, 0.65f, 1.00f },
        { eChampion::SYLAS, 1, 1, 0.55f, 1.00f },
        { eChampion::SYLAS, 2, 1, 0.45f, 1.00f },
        { eChampion::SYLAS, 3, 1, 0.28f, 1.20f },
        { eChampion::SYLAS, 3, 2, 0.50f, 1.00f },
        { eChampion::SYLAS, 4, 1, 0.70f, 1.00f },
```

`kChampionSkillStageTable` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        { eChampion::IRELIA, 3, 2, 2.00f },
```

아래에 추가:

```cpp
        { eChampion::SYLAS, 3, 2, 3.00f },
```

`kChampionSkillValueTable` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        { eChampion::KALISTA, 4, 120.00f, 0.00f },
```

아래에 추가:

```cpp

        { eChampion::SYLAS, 0, 0.60f, 1.50f },
        { eChampion::SYLAS, 1, 0.60f, 7.75f },
        { eChampion::SYLAS, 2, 0.60f, 4.00f },
        { eChampion::SYLAS, 3, 0.60f, 8.00f },
        { eChampion::SYLAS, 4, 0.60f, 9.50f },
```

1-10. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Champions/MasterYiGameSim.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Champions/SylasGameSim.h"
```

기존 코드:

```cpp
#include "Shared/GameSim/Components/MasterYiComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/SylasSimComponent.h"
```

기존 코드:

```cpp
    MasterYiGameSim::Tick(m_world, tc);
```

아래에 추가:

```cpp
    SylasGameSim::Tick(m_world, tc);
```

기존 코드:

```cpp
    MasterYiGameSim::RegisterHooks();
```

아래에 추가:

```cpp
    SylasGameSim::RegisterHooks();
```

기존 코드:

```cpp
    if (slot.champion == eChampion::MASTERYI)
        m_world.AddComponent<MasterYiSimComponent>(entity, MasterYiSimComponent{});
```

아래에 추가:

```cpp
    if (slot.champion == eChampion::SYLAS)
        m_world.AddComponent<SylasSimComponent>(entity, SylasSimComponent{});
```

1-11. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/q_cast.wfx

새 파일:

```json
{
  "name": "Sylas.Q.Cast",
  "emitters": [
    {
      "name": "q_chain_swipe",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_q_ringmult.png",
      "lifetime": 0.34,
      "width": 1.8,
      "height": 1.0,
      "color": [0.42, 0.82, 1.0, 0.72],
      "attach_offset": [0.0, 0.9, 0.45],
      "fade_in": 0.02,
      "fade_out": 0.18,
      "billboard": true
    },
    {
      "name": "q_arm_electric",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_q_electricity.png",
      "lifetime": 0.28,
      "width": 1.2,
      "height": 1.2,
      "color": [0.30, 0.72, 1.0, 0.62],
      "attach_offset": [0.0, 1.05, 0.2],
      "fade_in": 0.02,
      "fade_out": 0.18,
      "billboard": true
    }
  ]
}
```

1-12. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/q_explosion.wfx

새 파일:

```json
{
  "name": "Sylas.Q.Explosion",
  "emitters": [
    {
      "name": "q_ground_flash",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_q_hit_flash.png",
      "lifetime": 0.42,
      "width": 3.0,
      "height": 3.0,
      "color": [0.30, 0.74, 1.0, 0.62],
      "attach_offset": [0.0, 0.07, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "billboard": false
    },
    {
      "name": "q_center_burst",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_q_fb_explosion_centerflare.png",
      "lifetime": 0.36,
      "width": 1.65,
      "height": 1.65,
      "color": [0.46, 0.88, 1.0, 0.86],
      "attach_offset": [0.0, 0.75, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "billboard": true
    },
    {
      "name": "q_smoke_erode",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "SoftParticle",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_q_smoke_erode.png",
      "lifetime": 0.58,
      "width": 2.6,
      "height": 1.4,
      "color": [0.20, 0.30, 0.42, 0.42],
      "attach_offset": [0.0, 0.45, 0.0],
      "fade_in": 0.04,
      "fade_out": 0.38,
      "billboard": true
    }
  ]
}
```

1-13. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/w_cast.wfx

새 파일:

```json
{
  "name": "Sylas.W.Cast",
  "emitters": [
    {
      "name": "w_dash_lightning",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/sylas_base_w_dash_lightning.png",
      "lifetime": 0.36,
      "width": 1.8,
      "height": 0.72,
      "color": [0.34, 0.82, 1.0, 0.68],
      "attach_offset": [0.0, 0.85, -0.35],
      "fade_in": 0.02,
      "fade_out": 0.22,
      "billboard": true
    },
    {
      "name": "w_heal_pop",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_w_glow.png",
      "lifetime": 0.52,
      "width": 1.45,
      "height": 1.45,
      "color": [0.42, 1.0, 0.78, 0.58],
      "attach_offset": [0.0, 1.05, 0.0],
      "fade_in": 0.04,
      "fade_out": 0.30,
      "billboard": true
    },
    {
      "name": "w_hit_shock",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_w_hit_shockwave.png",
      "lifetime": 0.36,
      "width": 1.2,
      "height": 2.2,
      "start_radius": 0.35,
      "end_radius": 1.25,
      "thickness": 0.14,
      "color": [0.36, 0.88, 1.0, 0.58],
      "attach_offset": [0.0, 0.18, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.26,
      "billboard": false
    }
  ]
}
```

1-14. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/e1_dash.wfx

새 파일:

```json
{
  "name": "Sylas.E1.Dash",
  "emitters": [
    {
      "name": "e1_dash_trail",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/sylas_base_e_trail.png",
      "lifetime": 0.34,
      "width": 1.9,
      "height": 0.7,
      "color": [0.36, 0.84, 1.0, 0.62],
      "attach_offset": [0.0, 0.82, -0.45],
      "fade_in": 0.02,
      "fade_out": 0.22,
      "billboard": true
    },
    {
      "name": "e1_ground_decal",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/sylas_base_e_ground_decal.png",
      "lifetime": 0.42,
      "width": 2.2,
      "height": 2.2,
      "color": [0.20, 0.72, 1.0, 0.46],
      "attach_offset": [0.0, 0.06, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "billboard": false
    }
  ]
}
```

1-15. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/e2_chain.wfx

새 파일:

```json
{
  "name": "Sylas.E2.Chain",
  "emitters": [
    {
      "name": "e2_chain_beam",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_e_chain_mult2.png",
      "lifetime": 0.46,
      "width": 0.42,
      "height": 1.0,
      "color": [0.44, 0.86, 1.0, 0.84],
      "attach_offset": [0.0, 1.05, 0.45],
      "end_offset": [0.0, 1.0, 7.8],
      "fade_in": 0.02,
      "fade_out": 0.22,
      "billboard": false,
      "blockable_by_wind_wall": true
    },
    {
      "name": "e2_chain_tip",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_e_mis_tip.png",
      "lifetime": 0.46,
      "width": 0.9,
      "height": 0.9,
      "color": [0.58, 0.92, 1.0, 0.86],
      "attach_offset": [0.0, 1.0, 7.8],
      "fade_in": 0.02,
      "fade_out": 0.20,
      "billboard": true,
      "blockable_by_wind_wall": true
    },
    {
      "name": "e2_hit_spin",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_e_hit_spin.png",
      "lifetime": 0.42,
      "width": 1.4,
      "height": 1.4,
      "color": [0.40, 0.82, 1.0, 0.62],
      "attach_offset": [0.0, 1.0, 7.8],
      "fade_in": 0.02,
      "fade_out": 0.24,
      "billboard": true
    }
  ]
}
```

1-16. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/r_cast.wfx

새 파일:

```json
{
  "name": "Sylas.R.Cast",
  "emitters": [
    {
      "name": "r_lightning_cast",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_r_lightning_cas.png",
      "lifetime": 0.58,
      "width": 1.8,
      "height": 1.8,
      "color": [0.42, 0.82, 1.0, 0.78],
      "attach_offset": [0.0, 1.15, 0.2],
      "fade_in": 0.02,
      "fade_out": 0.34,
      "billboard": true
    },
    {
      "name": "r_shackle_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_r_mis_shackles.png",
      "lifetime": 0.52,
      "width": 1.4,
      "height": 1.4,
      "color": [0.58, 0.92, 1.0, 0.74],
      "attach_offset": [0.0, 1.0, 0.45],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "billboard": true
    },
    {
      "name": "r_spellbook_timer",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/Effects/sylas_base_r_timer_mult.png",
      "lifetime": 0.74,
      "width": 2.2,
      "height": 2.2,
      "color": [0.28, 0.74, 1.0, 0.44],
      "attach_offset": [0.0, 0.06, 0.0],
      "fade_in": 0.04,
      "fade_out": 0.42,
      "billboard": false
    }
  ]
}
```

2. 검증

미검증:
- 빌드 미검증.
- 런타임에서 Sylas Q/W/E/R EffectTrigger가 한 번씩만 재생되는지 미검증.
- E2는 우선 UnitTarget hit 판정 기반이며, 실제 사슬 projectile/충돌체 방식은 미반영.
- R은 적 챔피언 R을 `SpellbookOverrideComponent`로 훔치는 최소 로직이며, 훔친 스킬 UI 표시는 미반영.

검증 명령:
- `git diff --check`
- `& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" "Server/Include/Server.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m`
- `& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" "Client/Include/Client.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m`

확인 필요:
- 새로 추가한 Sylas h/cpp 파일이 Client/Server `.vcxproj`와 `.filters`에 포함되는지 확인.
- 선택한 Sylas `.wfx` 텍스처 경로가 Debug/Release 실행 경로에서 모두 로드되는지 확인.
- `RegisterAllLegacy()`가 ChampionDef를 덮는 흐름 때문에 Sylas legacy row 교체가 반드시 같이 들어가는지 확인.
