#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"

#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"

namespace
{
    struct AsheVolleyHitLedgerComponent
    {
        std::array<EntityID, kMaxPiercingProjectileHits> hitEntities{};
        u8_t hitCount = 0u;
        f32_t fRemainingSec = 0.f;
    };

    // Chrono Break: 익명 네임스페이스 컴포넌트는 소유 TU에서 자기등록한다.
    const bool_t s_bAsheVolleyLedgerKeyframeRegistered = []()
    {
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<AsheVolleyHitLedgerComponent>("AsheVolleyHitLedgerComponent");
        return true;
    }();

    f32_t ResolveAsheSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::ASHE,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveAsheSkillEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::ASHE,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    AsheSimComponent& EnsureAsheState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<AsheSimComponent>(caster))
            world.AddComponent<AsheSimComponent>(caster, AsheSimComponent{});

        return world.GetComponent<AsheSimComponent>(caster);
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
        const StatusEffectApplyDesc& onHitStatus,
        EntityID sharedHitLedgerEntity = NULL_ENTITY)
    {
        if (caster == NULL_ENTITY || !world.HasComponent<TransformComponent>(caster))
            return NULL_ENTITY;

        Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        origin.y += 1.0f;

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = caster;
        projectile.sourceHandle = world.GetEntityHandle(caster);
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
        projectile.sourceSlot = slot;
        projectile.sharedHitLedgerEntity = sharedHitLedgerEntity;
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
        const u8_t focusThreshold = static_cast<u8_t>(ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::MaxStacks,
            static_cast<f32_t>(state.focusThreshold)));
        if (state.focusStacks < focusThreshold)
        {
            std::cout << "[AsheSim] Q debug activate without full focus caster="
                << ctx.casterEntity << " stacks=" << static_cast<u32_t>(state.focusStacks)
                << "/" << static_cast<u32_t>(focusThreshold) << "\n";
        }

        state.bQActive = true;
        state.qTimerSec = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::EffectDurationSec,
            state.qDurationSec);
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
        const f32_t moveSpeedMul = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::MoveSpeedMul);
        const f32_t slowDurationSec = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::SlowDurationSec,
            state.frostSlowDurationSec);
        const f32_t projectileSpeed = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Speed);
        const f32_t projectileRange = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Range);
        const f32_t projectileDamage = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::BaseDamage);

        const StatusEffectApplyDesc slow = GameplayStatus::MakeSlowDesc(
            ctx.casterEntity,
            eChampion::ASHE,
            eSkillSlot::W,
            slowDurationSec,
            moveSpeedMul,
            eStatusEffectId::AsheVolleySlow);

        const EntityID hitLedgerEntity = ctx.pWorld->CreateEntity();
        AsheVolleyHitLedgerComponent hitLedger{};
        hitLedger.fRemainingSec = projectileSpeed > 0.f
            ? projectileRange / projectileSpeed + 1.f
            : 3.f;
        ctx.pWorld->AddComponent<AsheVolleyHitLedgerComponent>(
            hitLedgerEntity,
            hitLedger);

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
                projectileSpeed,
                projectileRange,
                0.45f,
                projectileDamage,
                static_cast<u8_t>(eSkillSlot::W),
                ctx.skillRank,
                slow,
                hitLedgerEntity);
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

        const f32_t stunDurationSec = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::StunDurationSec);
        const f32_t projectileSpeed = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Speed);
        const f32_t projectileRange = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Range);
        const f32_t projectileDamage = ResolveAsheSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage);

        const StatusEffectApplyDesc stun = GameplayStatus::MakeStunDesc(
            ctx.casterEntity,
            eChampion::ASHE,
            eSkillSlot::R,
            stunDurationSec,
            eStatusEffectId::AsheCrystalArrowStun);

        SpawnProjectile(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.casterTeam,
            ResolveDirection(ctx),
            eProjectileKind::AsheCrystalArrow,
            projectileSpeed,
            projectileRange,
            0.8f,
            projectileDamage,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            stun);

        std::cout << "[AsheSim] R crystal arrow caster=" << ctx.casterEntity << "\n";
    }
}

namespace AsheGameSim
{
    bool_t TryRegisterVolleyHit(
        CWorld& world,
        EntityID ledgerEntity,
        EntityID target)
    {
        if (ledgerEntity == NULL_ENTITY ||
            target == NULL_ENTITY ||
            !world.IsAlive(ledgerEntity) ||
            !world.HasComponent<AsheVolleyHitLedgerComponent>(ledgerEntity))
        {
            return false;
        }

        auto& ledger =
            world.GetComponent<AsheVolleyHitLedgerComponent>(ledgerEntity);
        for (u8_t i = 0u; i < ledger.hitCount; ++i)
        {
            if (ledger.hitEntities[i] == target)
                return false;
        }
        if (ledger.hitCount >= ledger.hitEntities.size())
            return false;

        ledger.hitEntities[ledger.hitCount++] = target;
        return true;
    }

    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID /*target*/,
        eTeam /*casterTeam*/,
        f32_t baseDamage)
    {
        AsheSimComponent& state = EnsureAsheState(world, caster);
        if (state.bQActive)
        {
            const f32_t qBonusDamage = ResolveAsheSkillEffectParam(
                world,
                tc,
                caster,
                eSkillSlot::Q,
                eSkillEffectParamId::BaseDamage,
                state.qBonusDamage);
            return baseDamage + qBonusDamage;
        }

        const u8_t focusThreshold = static_cast<u8_t>(ResolveAsheSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::Q,
            eSkillEffectParamId::MaxStacks,
            static_cast<f32_t>(state.focusThreshold)));
        state.focusStacks = static_cast<u8_t>(std::min<u32_t>(
            focusThreshold,
            static_cast<u32_t>(state.focusStacks) + 1u));
        return baseDamage;
    }

    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest)
    {
        if (!world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !world.HasComponent<StatComponent>(attacker) ||
            world.GetComponent<StatComponent>(attacker).championId !=
                eChampion::ASHE ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        Vec3 origin =
            world.GetComponent<TransformComponent>(attacker).GetPosition();
        origin.y += 1.f;
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        const f32_t damage = damageRequest.flatAmount != 0.f
            ? damageRequest.flatAmount
            : damageRequest.amount;
        const AsheSimComponent& state = EnsureAsheState(world, attacker);
        const f32_t moveSpeedMul = ResolveAsheSkillEffectParam(
            world,
            tc,
            attacker,
            eSkillSlot::W,
            eSkillEffectParamId::MoveSpeedMul);
        const f32_t slowDurationSec = ResolveAsheSkillEffectParam(
            world,
            tc,
            attacker,
            eSkillSlot::Q,
            eSkillEffectParamId::SlowDurationSec,
            state.frostSlowDurationSec);
        const f32_t projectileSpeed = ResolveAsheSkillEffectParam(
            world,
            tc,
            attacker,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::Speed,
            18.f);
        const f32_t projectileRadius = ResolveAsheSkillEffectParam(
            world,
            tc,
            attacker,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::Radius,
            0.35f);

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = attacker;
        projectile.targetEntity = target;
        projectile.sourceHandle = world.GetEntityHandle(attacker);
        projectile.targetHandle = world.GetEntityHandle(target);
        projectile.sourceTeam = damageRequest.sourceTeam;
        projectile.kind = eProjectileKind::AsheBasicAttack;
        projectile.unitHitPolicy = eProjectileUnitHitPolicy::Destroy;
        projectile.targetKindMask = ProjectileTarget_Champion |
            ProjectileTarget_MinionOrSummon |
            ProjectileTarget_JungleMonster |
            ProjectileTarget_Structure;
        projectile.bCollidesWithTerrain = false;
        projectile.bPersistAfterSourceDeath = true;
        projectile.bApplyDamageOnHit = true;
        projectile.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::ASHE) << 8) |
            static_cast<u8_t>(eSkillSlot::BasicAttack));
        projectile.rank = damageRequest.rank;
        projectile.currentPos = origin;
        projectile.direction = WintersMath::DirectionXZ(
            origin,
            targetPos,
            Vec3{ 0.f, 0.f, 1.f });
        projectile.speed = projectileSpeed;
        projectile.maxDistance = (std::numeric_limits<f32_t>::max)();
        projectile.hitRadius = projectileRadius;
        projectile.damage = damage;
        projectile.totalAdRatio = damageRequest.adRatioOverride;
        projectile.bonusAdRatio = damageRequest.bonusAdRatioOverride;
        projectile.apRatio = damageRequest.apRatioOverride;
        projectile.damageType = damageRequest.type;
        projectile.damageSourceKind = eDamageSourceKind::BasicAttack;
        projectile.sourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        projectile.damageFlags = damageRequest.flags;
        projectile.bApplyOnHitStatus = true;
        projectile.onHitStatus = GameplayStatus::MakeSlowDesc(
            attacker,
            eChampion::ASHE,
            eSkillSlot::BasicAttack,
            slowDurationSec,
            moveSpeedMul);

        const EntityHandle projectileHandle = world.CreateEntityHandle();
        if (!projectileHandle.IsValid())
            return false;

        const EntityID projectileEntity = projectileHandle.GetIndex();
        world.AddComponent<SkillProjectileComponent>(
            projectileEntity,
            projectile);
        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);
        return true;
    }

    bool_t HandleProjectileHit(
        CWorld& world,
        const TickContext& /*tc*/,
        const SkillProjectileComponent& projectile,
        EntityID target,
        DamageRequest& outDamage,
        bool_t& outEnqueue)
    {
        if (projectile.kind != eProjectileKind::AsheBasicAttack)
            return false;

        outDamage = {};
        outEnqueue = false;
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return true;

        outDamage.source = projectile.sourceEntity;
        outDamage.target = target;
        outDamage.sourceTeam = projectile.sourceTeam;
        outDamage.type = projectile.damageType;
        outDamage.flatAmount = projectile.damage;
        outDamage.skillId = projectile.skillId;
        outDamage.rank = projectile.rank;
        outDamage.iSourceSlot = projectile.sourceSlot;
        outDamage.eSourceKind = projectile.damageSourceKind;
        outDamage.adRatioOverride = projectile.totalAdRatio;
        outDamage.bonusAdRatioOverride = projectile.bonusAdRatio;
        outDamage.apRatioOverride = projectile.apRatio;
        outDamage.flags = projectile.damageFlags;
        outEnqueue = projectile.bApplyDamageOnHit;
        return true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> expiredLedgers;
        world.ForEach<AsheVolleyHitLedgerComponent>(
            std::function<void(EntityID, AsheVolleyHitLedgerComponent&)>(
                [&](EntityID entity, AsheVolleyHitLedgerComponent& ledger)
                {
                    ledger.fRemainingSec =
                        std::max(0.f, ledger.fRemainingSec - tc.fDt);
                    if (ledger.fRemainingSec <= 0.f)
                        expiredLedgers.push_back(entity);
                }));
        for (EntityID entity : expiredLedgers)
        {
            if (world.IsAlive(entity))
                world.DestroyEntity(entity);
        }

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
