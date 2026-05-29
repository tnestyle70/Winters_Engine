#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"

#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace
{
    constexpr f32_t kAsheWRange = 12.0f;
    constexpr f32_t kAsheWSpeed = 24.0f;
    constexpr f32_t kAsheWDamage = 45.0f;
    constexpr f32_t kAsheRRange = 200.0f;
    constexpr f32_t kAsheRSpeed = 20.0f;
    constexpr f32_t kAsheRDamage = 250.0f;
    constexpr f32_t kAsheWMoveSpeedMul = 0.85f;
    constexpr f32_t kAsheRStunDurationSec = 1.5f;

    AsheSimComponent& EnsureAsheState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<AsheSimComponent>(caster))
            world.AddComponent<AsheSimComponent>(caster, AsheSimComponent{});

        return world.GetComponent<AsheSimComponent>(caster);
    }

    constexpr u16_t MakeStatusStackGroup(eChampion champion, u8_t slot)
    {
        return static_cast<u16_t>(
            (static_cast<u32_t>(champion) << 8) | static_cast<u32_t>(slot));
    }

    Vec3 ResolveDirection(const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    EntityID SpawnProjectile(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& direction,
        eProjectileKind kind,
        f32_t speed,
        f32_t maxDistance,
        f32_t radius,
        f32_t damage,
        u8_t slot,
        u8_t rank,
        const StatusEffectApplyDesc& onHitStatus)
    {
        if (caster == NULL_ENTITY || !world.HasComponent<TransformComponent>(caster))
            return NULL_ENTITY;

        Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        origin.y += 1.0f;

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = caster;
        projectile.sourceTeam = casterTeam;
        projectile.kind = kind;
        projectile.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::ASHE) << 8) | slot);
        projectile.rank = rank;
        projectile.currentPos = origin;
        projectile.direction = WintersMath::NormalizeXZ(direction);
        projectile.speed = speed;
        projectile.maxDistance = maxDistance;
        projectile.hitRadius = radius;
        projectile.damage = damage;
        if (onHitStatus.effectId != eStatusEffectId::None &&
            onHitStatus.fDurationSec > 0.f)
        {
            projectile.bApplyOnHitStatus = true;
            projectile.onHitStatus = onHitStatus;
        }

        const EntityID projectileEntity = world.CreateEntity();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);
        return projectileEntity;
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        AsheSimComponent& state = EnsureAsheState(*ctx.pWorld, ctx.casterEntity);
        if (state.focusStacks < state.focusThreshold)
        {
            std::cout << "[AsheSim] Q debug activate without full focus caster="
                << ctx.casterEntity << " stacks=" << static_cast<u32_t>(state.focusStacks)
                << "/" << static_cast<u32_t>(state.focusThreshold) << "\n";
        }

        state.bQActive = true;
        state.qTimerSec = state.qDurationSec;
        state.focusStacks = 0;
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        constexpr u32_t kArrowCount = 8;
        constexpr f32_t kConeDeg = 45.f;
        const Vec3 dir = ResolveDirection(ctx);
        const f32_t halfCone = (kConeDeg * 0.5f) * (WintersMath::kPi / 180.f);
        const AsheSimComponent& state = EnsureAsheState(*ctx.pWorld, ctx.casterEntity);

        StatusEffectApplyDesc slow{};
        slow.effectId = eStatusEffectId::AsheVolleySlow;
        slow.stackPolicy = eStatusStackPolicy::RefreshDuration;
        slow.sourceEntity = ctx.casterEntity;
        slow.stackGroup = MakeStatusStackGroup(
            eChampion::ASHE,
            static_cast<u8_t>(eSkillSlot::W));
        slow.stateFlags = kGameplayStateSlowedFlag;
        slow.fDurationSec = state.frostSlowDurationSec;
        slow.fMoveSpeedMul = kAsheWMoveSpeedMul;

        for (u32_t i = 0; i < kArrowCount; ++i)
        {
            const f32_t t = (kArrowCount > 1)
                ? static_cast<f32_t>(i) / static_cast<f32_t>(kArrowCount - 1u)
                : 0.5f;
            const f32_t angle = -halfCone + halfCone * 2.f * t;
            SpawnProjectile(
                *ctx.pWorld,
                ctx.casterEntity,
                ctx.casterTeam,
                WintersMath::RotateXZ(dir, angle),
                eProjectileKind::AsheVolleyArrow,
                kAsheWSpeed,
                kAsheWRange,
                0.45f,
                kAsheWDamage,
                static_cast<u8_t>(eSkillSlot::W),
                ctx.skillRank,
                slow);
        }

        std::cout << "[AsheSim] W volley caster=" << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        std::cout << "[AsheSim] E hawkshot cue caster=" << ctx.casterEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        StatusEffectApplyDesc stun{};
        stun.effectId = eStatusEffectId::AsheCrystalArrowStun;
        stun.stackPolicy = eStatusStackPolicy::RefreshDuration;
        stun.sourceEntity = ctx.casterEntity;
        stun.stackGroup = MakeStatusStackGroup(
            eChampion::ASHE,
            static_cast<u8_t>(eSkillSlot::R));
        stun.stateFlags =
            kGameplayStateStunnedFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        stun.fDurationSec = kAsheRStunDurationSec;

        SpawnProjectile(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.casterTeam,
            ResolveDirection(ctx),
            eProjectileKind::AsheCrystalArrow,
            kAsheRSpeed,
            kAsheRRange,
            0.8f,
            kAsheRDamage,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            stun);

        std::cout << "[AsheSim] R crystal arrow caster=" << ctx.casterEntity << "\n";
    }
}

namespace AsheGameSim
{
    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID /*target*/,
        eTeam /*casterTeam*/,
        f32_t baseDamage)
    {
        AsheSimComponent& state = EnsureAsheState(world, caster);
        if (state.bQActive)
            return baseDamage + state.qBonusDamage;

        state.focusStacks = static_cast<u8_t>(std::min<u32_t>(
            state.focusThreshold,
            static_cast<u32_t>(state.focusStacks) + 1u));
        return baseDamage;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<AsheSimComponent>(
            std::function<void(EntityID, AsheSimComponent&)>(
                [&](EntityID, AsheSimComponent& state)
                {
                    if (!state.bQActive)
                        return;

                    state.qTimerSec = std::max(0.f, state.qTimerSec - tc.fDt);
                    if (state.qTimerSec <= 0.f)
                        state.bQActive = false;
                }));
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ASHE, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ASHE, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ASHE, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ASHE, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[AsheSim] hooks registered\n";
    }
}
