#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"

#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include "Shared/GameSim/Systems/Move/DashArrival.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr u8_t kSylasPassiveMaxStacks = 3u;
    constexpr f32_t kSylasPassiveWindowSec = 4.0f;

    f32_t ResolveSylasSkillEffectParam(
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
            eChampion::SYLAS,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveSylasSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return ResolveSylasSkillEffectParam(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            param,
            fallbackValue);
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<MoveTargetComponent>(entity))
            return;

        auto& move = world.GetComponent<MoveTargetComponent>(entity);
        move.bHasTarget = false;
        move.pathCount = 0;
        move.pathIndex = 0;
    }

    Vec3 ResolveCommandDirection(const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction, Vec3{}, 0.0001f);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }
        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            const f32_t yaw = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetRotation().y;
            return WintersMath::DirectionFromYawXZ(yaw);
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    void RotateToward(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;

        const Vec3 dir = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::SYLAS, dir, rot.y),
            rot.z
            });
    }

    void StartDash(CWorld& world, EntityID caster, const Vec3& end, f32_t duration)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();

        SylasDashComponent dash{};
        dash.vStart = start;
        dash.vEnd = end;
        dash.fDurationSec = duration;

        if (world.HasComponent<SylasDashComponent>(caster))
            world.GetComponent<SylasDashComponent>(caster) = dash;
        else
            world.AddComponent<SylasDashComponent>(caster, dash);

        RotateToward(world, caster, Vec3{ end.x - start.x, 0.f, end.z - start.z });
        ClearMove(world, caster);
    }

    void StartDirectionalDash(
        CWorld& world,
        EntityID caster,
        const Vec3& direction,
        f32_t distance,
        f32_t durationSec)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        const Vec3 dir = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 end{
            start.x + dir.x * distance,
            start.y,
            start.z + dir.z * distance
        };

        StartDash(world, caster, end, durationSec);
    }

    void StartTargetDash(
        CWorld& world,
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
            }, Vec3{}, 0.0001f);

        if (dir.x == 0.f && dir.z == 0.f)
            return;

        const f32_t dx = targetPos.x - start.x;
        const f32_t dz = targetPos.z - start.z;
        const f32_t dist = std::sqrt(dx * dx + dz * dz);
        const f32_t moveDist = std::max(0.f, dist - gap);

        const Vec3 end{
            start.x + dir.x * moveDist,
            start.y,
            start.z + dir.z * moveDist
        };

        StartDash(world, caster, end, durationSec);
    }

    void SpawnChainProjectile(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand ||
            ctx.casterEntity == NULL_ENTITY ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        origin.y += 1.f;

        const Vec3 dir = ResolveCommandDirection(ctx);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        const TickContext* pTickCtx = ctx.pTickCtx;
        TickContext fallbackTick{};
        const TickContext& tc = pTickCtx ? *pTickCtx : fallbackTick;
        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            *ctx.pWorld,
            ctx.casterEntity,
            tc,
            eChampion::SYLAS,
            static_cast<u8_t>(eSkillSlot::E));
        if (range <= 0.f)
            range = 6.f;

        const u8_t rank = ctx.skillRank > 0u ? ctx.skillRank : static_cast<u8_t>(1u);
        const f32_t rankBonus = static_cast<f32_t>(rank - 1u);
        const f32_t chainSpeed = ResolveSylasSkillEffectParam(
            *ctx.pWorld,
            tc,
            ctx.casterEntity,
            eSkillSlot::E,
            eSkillEffectParamId::Speed);
        const f32_t chainHitRadius = ResolveSylasSkillEffectParam(
            *ctx.pWorld,
            tc,
            ctx.casterEntity,
            eSkillSlot::E,
            eSkillEffectParamId::Radius);
        const f32_t baseDamage = ResolveSylasSkillEffectParam(
            *ctx.pWorld,
            tc,
            ctx.casterEntity,
            eSkillSlot::E,
            eSkillEffectParamId::BaseDamage);
        const f32_t damagePerRank = ResolveSylasSkillEffectParam(
            *ctx.pWorld,
            tc,
            ctx.casterEntity,
            eSkillSlot::E,
            eSkillEffectParamId::DamagePerRank);

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = ctx.casterEntity;
        projectile.sourceHandle = ctx.pWorld->GetEntityHandle(ctx.casterEntity);
        projectile.sourceTeam = ctx.casterTeam;
        projectile.kind = eProjectileKind::SylasChain;
        projectile.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::SYLAS) << 8) |
            static_cast<u8_t>(eSkillSlot::E));
        projectile.rank = rank;
        projectile.currentPos = origin;
        projectile.direction = dir;
        projectile.speed = chainSpeed;
        projectile.maxDistance = range;
        projectile.hitRadius = chainHitRadius;
        projectile.damage = baseDamage + rankBonus * damagePerRank;

        const EntityID projectileEntity = ctx.pWorld->CreateEntity();
        ctx.pWorld->AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        ctx.pWorld->AddComponent<TransformComponent>(projectileEntity, transform);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        if (ctx.pCommand->itemId == 2u)
            SpawnChainProjectile(ctx);
        else
        {
            const f32_t dashDistance = ResolveSylasSkillEffectParam(
                ctx,
                eSkillSlot::E,
                eSkillEffectParamId::DashDistance);
            const f32_t dashDurationSec = ResolveSylasSkillEffectParam(
                ctx,
                eSkillSlot::E,
                eSkillEffectParamId::DashDurationSec);
            StartDirectionalDash(
                *ctx.pWorld,
                ctx.casterEntity,
                ResolveCommandDirection(ctx),
                dashDistance,
                dashDurationSec);
        }
    }

    bool_t IsValidChampion(eChampion champion)
    {
        return champion != eChampion::NONE && champion != eChampion::END;
    }

    bool_t IsAliveChampion(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<ChampionComponent>(entity))
        {
            return false;
        }

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }

        return true;
    }

    eChampion ResolveHijackSourceChampion(CWorld& world, EntityID target)
    {
        if (!world.HasComponent<ChampionComponent>(target))
            return eChampion::END;

        return world.GetComponent<ChampionComponent>(target).id;
    }

    u8_t ResolveHijackRank(CWorld& world, EntityID caster, EntityID target)
    {
        const u8_t rSlot = static_cast<u8_t>(eSkillSlot::R);
        if (world.HasComponent<SkillRankComponent>(caster))
        {
            const auto& casterRanks = world.GetComponent<SkillRankComponent>(caster);
            if (casterRanks.ranks[rSlot] > 0u)
                return casterRanks.ranks[rSlot];
        }
        if (world.HasComponent<SkillRankComponent>(target))
        {
            const auto& targetRanks = world.GetComponent<SkillRankComponent>(target);
            if (targetRanks.ranks[rSlot] > 0u)
                return targetRanks.ranks[rSlot];
        }
        return 1u;
    }

    bool_t CanHijackUltimateInternal(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (!IsAliveChampion(world, caster) || !IsAliveChampion(world, target))
            return false;
        if (!world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
        if (!GameplayStateQuery::CanBeTargetedBy(world, caster, target))
            return false;

        const auto& casterChampion = world.GetComponent<ChampionComponent>(caster);
        const auto& targetChampion = world.GetComponent<ChampionComponent>(target);
        if (casterChampion.id != eChampion::SYLAS)
            return false;
        if (casterChampion.team == targetChampion.team &&
            casterChampion.team != eTeam::Neutral)
        {
            return false;
        }

        const eChampion stolenChampion = ResolveHijackSourceChampion(world, target);
        if (!IsValidChampion(stolenChampion) || stolenChampion == eChampion::SYLAS)
            return false;
        if (!CSpellbookFormOverrideSystem::CanDispatchCapturedUltimate(stolenChampion))
            return false;

        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::SYLAS,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            range = 10.f;
        const f32_t effectiveRange =
            range +
            GameplayStateQuery::ResolveGameplayRadius(world, caster) +
            GameplayStateQuery::ResolveGameplayRadius(world, target);
        return WintersMath::DistanceSqXZ(
            world.GetComponent<TransformComponent>(caster).GetPosition(),
            world.GetComponent<TransformComponent>(target).GetPosition()) <=
            effectiveRange * effectiveRange;
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID caster = ctx.casterEntity;
        const EntityID target = ctx.pCommand->targetEntity;

        if (world.HasComponent<SpellbookOverrideComponent>(caster))
            return;
        TickContext fallbackTick{};
        const TickContext& tc = ctx.pTickCtx ? *ctx.pTickCtx : fallbackTick;
        if (!CanHijackUltimateInternal(world, tc, caster, target))
            return;

        SpellbookOverrideComponent spellbook{};
        spellbook.sourceChampion = ResolveHijackSourceChampion(world, target);
        spellbook.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.localSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.sourceRank = ResolveHijackRank(world, caster, target);
        spellbook.fRemainingSec = ResolveSylasSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSkillEffectParamId::EffectDurationSec,
            45.f);
        spellbook.bActive = true;

        world.AddComponent<SpellbookOverrideComponent>(caster, spellbook);

#if defined(_DEBUG)
        char msg[192]{};
        sprintf_s(msg,
            "[SylasHijack] caster=%u target=%u stolenChampion=%u rank=%u\n",
            static_cast<u32_t>(caster),
            static_cast<u32_t>(target),
            static_cast<u32_t>(spellbook.sourceChampion),
            static_cast<u32_t>(spellbook.sourceRank));
        WintersOutputAIDebugStringA(msg);
#endif
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
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<SylasSimComponent>(
            std::function<void(EntityID, SylasSimComponent&)>(
                [&](EntityID, SylasSimComponent& sylas)
                {
                    if (sylas.passiveRemainingSec <= 0.f)
                    {
                        sylas.passiveStacks = 0u;
                        sylas.passiveRemainingSec = 0.f;
                        return;
                    }

                    sylas.passiveRemainingSec =
                        std::max(0.f, sylas.passiveRemainingSec - tc.fDt);
                    if (sylas.passiveRemainingSec <= 0.f)
                        sylas.passiveStacks = 0u;
                }));

        std::vector<EntityID> finishedDashes;
        world.ForEach<SylasDashComponent, TransformComponent>(
            std::function<void(EntityID, SylasDashComponent&, TransformComponent&)>(
                [&](EntityID entity, SylasDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

                    if (!GameplayStateQuery::CanMove(world, entity))
                    {
                        finishedDashes.push_back(entity);
                        return;
                    }

                    dash.fElapsedSec += tc.fDt;
                    f32_t t = dash.fDurationSec > 0.01f
                        ? dash.fElapsedSec / dash.fDurationSec
                        : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }

                    const Vec3 desired{
                        dash.vStart.x + (dash.vEnd.x - dash.vStart.x) * t,
                        dash.vStart.y + (dash.vEnd.y - dash.vStart.y) * t,
                        dash.vStart.z + (dash.vEnd.z - dash.vStart.z) * t
                    };

                    Vec3 guarded = desired;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable)
                    {
                        const Vec3 current = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(current, desired, 0.5f, guarded))
                        {
                            guarded = current;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(guarded, desired) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }

                        f32_t surfaceY = 0.f;
                        if (tc.pWalkable->TrySampleHeight(guarded.x, guarded.z, surfaceY))
                            guarded.y = surfaceY;
                    }

                    transform.SetPosition(guarded);
                    if (bDashBlocked && t < 1.f)
                        finishedDashes.push_back(entity);
                }));

        for (EntityID entity : finishedDashes)
        {
            if (world.HasComponent<SylasDashComponent>(entity))
                SnapDashArrivalToWalkable(world, tc, entity,
                    world.GetComponent<SylasDashComponent>(entity).vStart);
            world.RemoveComponent<SylasDashComponent>(entity);
        }
    }

    bool_t CanHijackUltimate(CWorld& world, const TickContext& tc, EntityID caster, EntityID target)
    {
        return CanHijackUltimateInternal(world, tc, caster, target);
    }

    void ArmPassiveOnSkillCast(CWorld& world, EntityID caster)
    {
        if (caster == NULL_ENTITY || !world.IsAlive(caster))
            return;

        SylasSimComponent& sylas = world.HasComponent<SylasSimComponent>(caster)
            ? world.GetComponent<SylasSimComponent>(caster)
            : world.AddComponent<SylasSimComponent>(caster, SylasSimComponent{});
        sylas.passiveStacks = static_cast<u8_t>(
            std::min<u32_t>(kSylasPassiveMaxStacks, sylas.passiveStacks + 1u));
        sylas.passiveRemainingSec = kSylasPassiveWindowSec;
    }

    bool_t TryConsumePassiveBasicAttack(CWorld& world, EntityID caster)
    {
        if (caster == NULL_ENTITY ||
            !world.IsAlive(caster) ||
            !world.HasComponent<SylasSimComponent>(caster))
        {
            return false;
        }

        auto& sylas = world.GetComponent<SylasSimComponent>(caster);
        if (sylas.passiveStacks == 0u || sylas.passiveRemainingSec <= 0.f)
            return false;

        --sylas.passiveStacks;
        if (sylas.passiveStacks == 0u)
            sylas.passiveRemainingSec = 0.f;
        else
            sylas.passiveRemainingSec = kSylasPassiveWindowSec;
        return true;
    }

    void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
    {
        if (source == NULL_ENTITY ||
            target == NULL_ENTITY ||
            !world.IsAlive(source) ||
            !world.IsAlive(target))
        {
            return;
        }

        const f32_t targetDashGap = ResolveSylasSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::E,
            eSkillEffectParamId::Gap);
        const f32_t targetDashDurationSec = ResolveSylasSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::E,
            eSkillEffectParamId::TargetDashDurationSec);
        const f32_t airborneDurationSec = ResolveSylasSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::E,
            eSkillEffectParamId::AirborneDurationSec,
            0.75f);
        const f32_t slowDurationSec = ResolveSylasSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::E,
            eSkillEffectParamId::SlowDurationSec,
            1.5f);
        const f32_t slowMoveSpeedMul = ResolveSylasSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::E,
            eSkillEffectParamId::MoveSpeedMul,
            0.6f);

        StartTargetDash(world, source, target, targetDashGap, targetDashDurationSec);
        GameplayStatus::ApplyAirborne(
            world,
            tc,
            target,
            source,
            eChampion::SYLAS,
            eSkillSlot::E,
            airborneDurationSec,
            2.1f);
        GameplayStatus::ApplySlow(
            world,
            tc,
            target,
            source,
            eChampion::SYLAS,
            eSkillSlot::E,
            slowDurationSec,
            slowMoveSpeedMul);
    }
}
