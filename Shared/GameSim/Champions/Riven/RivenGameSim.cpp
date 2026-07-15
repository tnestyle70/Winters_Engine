#include "Shared/GameSim/Champions/Riven/RivenGameSim.h"

#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/Shield/ShieldSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace
{
    constexpr u8_t kRivenRSlot = static_cast<u8_t>(eSkillSlot::R);
    constexpr u32_t kBladeOfTheExileBuffId =
        (static_cast<u32_t>(eChampion::RIVEN) << 16) |
        GameplayHookVariant::R_CastFrame;

    f32_t ResolveRivenSkillEffectParam(
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
            eChampion::RIVEN,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    RivenStateComponent& EnsureRivenState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<RivenStateComponent>(caster))
            world.AddComponent<RivenStateComponent>(caster, RivenStateComponent{});

        return world.GetComponent<RivenStateComponent>(caster);
    }

    Vec3 ResolveCommandDirection(const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 commandDirection = WintersMath::NormalizeXZ(
                ctx.pCommand->direction,
                Vec3{},
                0.0001f);
            if (commandDirection.x != 0.f || commandDirection.z != 0.f)
                return commandDirection;
        }

        if (ctx.pWorld &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return WintersMath::DirectionFromYawXZ(
                ctx.pWorld->GetComponent<TransformComponent>(
                    ctx.casterEntity).GetRotation().y);
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    void RemoveBladeOfTheExileBuff(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<BuffComponent>(caster))
            return;

        auto& buffs = world.GetComponent<BuffComponent>(caster);
        u8_t write = 0u;
        for (u8_t read = 0u;
            read < buffs.count && read < BuffComponent::kMaxBuffs;
            ++read)
        {
            const BuffInstance& buff = buffs.buffs[read];
            if (buff.buffDefId == kBladeOfTheExileBuffId &&
                buff.source == caster)
            {
                continue;
            }
            buffs.buffs[write++] = buff;
        }
        for (u8_t index = write;
            index < buffs.count && index < BuffComponent::kMaxBuffs;
            ++index)
        {
            buffs.buffs[index] = {};
        }
        if (write != buffs.count)
        {
            buffs.count = write;
            if (world.HasComponent<StatComponent>(caster))
                world.GetComponent<StatComponent>(caster).bDirty = true;
        }
    }

    void ClearBladeOfTheExile(
        CWorld& world,
        EntityID caster,
        RivenStateComponent& state)
    {
        state.bUlted = false;
        state.bWindSlashAvailable = false;
        state.fUltTimer = 0.f;
        RemoveBladeOfTheExileBuff(world, caster);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        RivenStateComponent& state = EnsureRivenState(world, ctx.casterEntity);
        const u8_t stage = RivenGameSim::ResolveQVariantStage(
            world,
            ctx.casterEntity);
        const f32_t q3Radius = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Radius);
        const f32_t q3AirborneDurationSec = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::AirborneDurationSec);
        const f32_t qStackWindowSec = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::StackWindowSec);

        if (stage >= 3u)
        {
            const Vec3 origin =
                world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const std::vector<EntityID> targets =
                GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                    world,
                    ctx.casterEntity,
                    origin,
                    q3Radius);
            for (EntityID target : targets)
            {
                GameplayStatus::ApplyAirborne(
                    world,
                    *ctx.pTickCtx,
                    target,
                    ctx.casterEntity,
                    eChampion::RIVEN,
                    eSkillSlot::Q,
                    q3AirborneDurationSec);
            }

            state.qStackCount = 0;
            state.qStackTimer = 0.f;
        }
        else
        {
            state.qStackCount = stage;
            state.qStackTimer = qStackWindowSec;
        }

        std::cout << "[RivenSim] Q caster=" << ctx.casterEntity
            << " stage=" << static_cast<u32_t>(stage)
            << " stack=" << static_cast<u32_t>(state.qStackCount) << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const f32_t wRadius = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Radius);
        const f32_t wStunDurationSec = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::StunDurationSec);
        const Vec3 origin =
            world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                world,
                ctx.casterEntity,
                origin,
                wRadius);
        for (EntityID target : targets)
        {
            GameplayStatus::ApplyStun(
                world,
                *ctx.pTickCtx,
                target,
                ctx.casterEntity,
                eChampion::RIVEN,
                eSkillSlot::W,
                wStunDurationSec);
        }

        std::cout << "[RivenSim] W stun caster=" << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        const f32_t shieldAmount = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::ShieldBaseAmount,
            70.f);
        const f32_t shieldDurationSec = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::ShieldDurationSec,
            3.f);
        CShieldSystem::Grant(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            shieldAmount,
            shieldDurationSec);

        std::cout << "[RivenSim] E shield caster=" << ctx.casterEntity
            << " amount=" << shieldAmount << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld ||
            !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        RivenStateComponent& state = EnsureRivenState(world, ctx.casterEntity);
        const bool_t bWindSlash = ctx.pCommand && ctx.pCommand->itemId >= 2u;
        if (!bWindSlash)
        {
            const f32_t durationSec = ResolveRivenSkillEffectParam(
                ctx,
                eSkillSlot::R,
                eSkillEffectParamId::EffectDurationSec,
                15.f);
            const f32_t bonusAd = ResolveRivenSkillEffectParam(
                ctx,
                eSkillSlot::R,
                eSkillEffectParamId::BonusAd,
                20.f);
            state.bUlted = true;
            state.bWindSlashAvailable = true;
            state.fUltTimer = durationSec;

            BuffComponent& buffs = world.HasComponent<BuffComponent>(ctx.casterEntity)
                ? world.GetComponent<BuffComponent>(ctx.casterEntity)
                : world.AddComponent<BuffComponent>(ctx.casterEntity, BuffComponent{});
            BuffInstance buff{};
            buff.buffDefId = kBladeOfTheExileBuffId;
            buff.source = ctx.casterEntity;
            buff.fDurationRemaining = durationSec;
            buff.stackCount = 1u;
            buff.flatAdPerStack = bonusAd;
            if (CBuffSystem::AddOrRefresh(buffs, buff) &&
                world.HasComponent<StatComponent>(ctx.casterEntity))
            {
                world.GetComponent<StatComponent>(ctx.casterEntity).bDirty = true;
            }
            return;
        }

        if (!state.bUlted ||
            !state.bWindSlashAvailable ||
            state.fUltTimer <= 0.f)
            return;

        const f32_t range = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Range,
            10.5f);
        const f32_t halfAngleCos = std::clamp(
            ResolveRivenSkillEffectParam(
                ctx,
                eSkillSlot::R,
                eSkillEffectParamId::HalfAngleCos,
                0.70710678f),
            -1.f,
            1.f);
        const f32_t baseDamage = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage,
            100.f);
        const f32_t damagePerRank = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::DamagePerRank,
            50.f);
        const u8_t rank = ctx.skillRank > 0u ? ctx.skillRank : 1u;
        const Vec3 origin = world.GetComponent<TransformComponent>(
            ctx.casterEntity).GetPosition();
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCone(
                world,
                ctx.casterEntity,
                origin,
                ResolveCommandDirection(ctx),
                range,
                std::acos(halfAngleCos));
        for (EntityID target : targets)
        {
            DamageRequest request{};
            request.source = ctx.casterEntity;
            request.target = target;
            request.sourceTeam = ctx.casterTeam;
            request.type = eDamageType::Physical;
            request.flatAmount = baseDamage +
                damagePerRank * static_cast<f32_t>(rank - 1u);
            request.skillId = static_cast<u16_t>(
                (static_cast<u32_t>(eChampion::RIVEN) << 8) | kRivenRSlot);
            request.rank = rank;
            request.iSourceSlot = kRivenRSlot;
            request.eSourceKind = eDamageSourceKind::Skill;
            EnqueueDamageRequest(world, request);
        }

        state.bWindSlashAvailable = false;
    }
}

namespace RivenGameSim
{
    u8_t ResolveQVariantStage(CWorld& world, EntityID caster)
    {
        if (caster == NULL_ENTITY || !world.HasComponent<RivenStateComponent>(caster))
            return 1u;

        const auto& state = world.GetComponent<RivenStateComponent>(caster);
        return static_cast<u8_t>(std::min<u32_t>(
            3u,
            static_cast<u32_t>(state.qStackCount) + 1u));
    }

    bool_t CanCastBladeOfTheExile(CWorld& world, EntityID caster, u8_t stage)
    {
        if (caster == NULL_ENTITY || !world.IsAlive(caster))
            return false;

        const RivenStateComponent* state =
            world.HasComponent<RivenStateComponent>(caster)
            ? &world.GetComponent<RivenStateComponent>(caster)
            : nullptr;
        if (stage >= 2u)
            return state &&
                state->bUlted &&
                state->bWindSlashAvailable &&
                state->fUltTimer > 0.f;

        return !state || !state->bUlted || state->fUltTimer <= 0.f;
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::RIVEN, GameplayHookVariant::Q_OnCastAccepted), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::RIVEN, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::RIVEN, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::RIVEN, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[RivenSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<RivenStateComponent>(
            std::function<void(EntityID, RivenStateComponent&)>(
                [&](EntityID entity, RivenStateComponent& state)
                {
                    if (state.qStackTimer > 0.f)
                    {
                        state.qStackTimer = std::max(0.f, state.qStackTimer - tc.fDt);
                        if (state.qStackTimer <= 0.f)
                            state.qStackCount = 0;
                    }

                    if (state.fUltTimer > 0.f)
                    {
                        state.fUltTimer = std::max(0.f, state.fUltTimer - tc.fDt);
                        if (state.fUltTimer <= 0.f)
                            ClearBladeOfTheExile(world, entity, state);
                    }
                }));
    }
}
