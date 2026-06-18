#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"

#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr f32_t kFioraQDistance = 3.0f;
    constexpr f32_t kFioraQRange = 4.0f;
    constexpr f32_t kFioraQRadius = 1.0f;
    constexpr f32_t kFioraQDamage = 70.f;
    constexpr f32_t kFioraWRange = 6.0f;
    constexpr f32_t kFioraWRadius = 0.8f;
    constexpr f32_t kFioraWSlowDurationSec = 1.5f;
    constexpr f32_t kFioraWSlowMoveSpeedMul = 0.50f;
    constexpr f32_t kFioraRDamage = 80.f;
    constexpr f32_t kFioraQDashDurationSec = 0.25f;

    struct FioraDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t durationSec = kFioraQDashDurationSec;
    };

    FioraSimComponent& EnsureFioraState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<FioraSimComponent>(caster))
            world.AddComponent<FioraSimComponent>(caster, FioraSimComponent{});

        return world.GetComponent<FioraSimComponent>(caster);
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    void EnqueuePhysicalDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        f32_t amount,
        u16_t skillId,
        u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = skillId;
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    EntityID FindEnemyInCone(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& origin,
        const Vec3& direction,
        f32_t range,
        f32_t radius)
    {
        const Vec3 fwd = WintersMath::NormalizeXZ(direction);
        EntityID best = NULL_ENTITY;
        f32_t bestDistSq = (range + radius) * (range + radius);

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == caster || champion.team == casterTeam)
                        return;

                    const Vec3 enemyPos = transform.GetPosition();
                    const f32_t distSq = WintersMath::DistanceSqXZ(origin, enemyPos);
                    if (distSq > (range + radius) * (range + radius))
                        return;

                    const f32_t dist = std::sqrt(std::max(0.0001f, distSq));
                    const f32_t dot =
                        fwd.x * ((enemyPos.x - origin.x) / dist) +
                        fwd.z * ((enemyPos.z - origin.z) / dist);
                    if (dot < 0.5f)
                        return;

                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        best = entity;
                    }
                }));

        return best;
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld ||
            !ctx.pCommand ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        auto& transform = world.GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 origin = transform.GetPosition();
        const Vec3 direction = WintersMath::NormalizeXZ(ctx.pCommand->direction);

        const Vec3 destination{
            origin.x + direction.x * kFioraQDistance,
            origin.y,
            origin.z + direction.z * kFioraQDistance
        };

        FioraDashComponent dash{};
        dash.start = origin;
        dash.end = destination;
        dash.durationSec = kFioraQDashDurationSec;
        if (world.HasComponent<FioraDashComponent>(ctx.casterEntity))
            world.GetComponent<FioraDashComponent>(ctx.casterEntity) = dash;
        else
            world.AddComponent<FioraDashComponent>(ctx.casterEntity, dash);

        const Vec3 rotation = transform.GetRotation();
        transform.SetRotation(Vec3{
            rotation.x,
            ResolveChampionVisualYawNear(eChampion::FIORA, direction, rotation.y),
            rotation.z
        });

        ClearMove(world, ctx.casterEntity);

        const EntityID hitTarget = FindEnemyInCone(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            direction,
            kFioraQRange,
            kFioraQRadius);

        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            hitTarget,
            ctx.casterTeam,
            kFioraQDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::FIORA) << 8) | 1u),
            ctx.skillRank);

        std::cout << "[FioraSim] Q lunge caster=" << ctx.casterEntity
            << " hit=" << hitTarget << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        FioraSimComponent& state = EnsureFioraState(world, ctx.casterEntity);
        state.bRiposteActive = true;
        state.riposteTimerSec = state.riposteWindowSec;
        ClearMove(world, ctx.casterEntity);

        const Vec3 origin =
            world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 direction = ctx.pCommand
            ? WintersMath::NormalizeXZ(ctx.pCommand->direction)
            : Vec3{ 0.f, 0.f, 1.f };
        const EntityID hitTarget = FindEnemyInCone(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            direction,
            kFioraWRange,
            kFioraWRadius);
        if (hitTarget != NULL_ENTITY)
        {
            GameplayStatus::ApplySlow(
                world,
                *ctx.pTickCtx,
                hitTarget,
                ctx.casterEntity,
                eChampion::FIORA,
                eSkillSlot::W,
                kFioraWSlowDurationSec,
                kFioraWSlowMoveSpeedMul);
        }

        std::cout << "[FioraSim] W riposte caster=" << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        FioraSimComponent& state = EnsureFioraState(*ctx.pWorld, ctx.casterEntity);
        state.bBladeworkActive = true;
        state.bladeworkTimerSec = 5.0f;
        state.bladeworkHitsRemaining = 2;

        std::cout << "[FioraSim] E bladework caster=" << ctx.casterEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY)
            return;

        FioraSimComponent& state = EnsureFioraState(world, ctx.casterEntity);
        state.bGrandChallengeActive = true;
        state.grandChallengeTimerSec = 8.0f;
        state.grandChallengeTarget = target;

        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            kFioraRDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::FIORA) << 8) | 4u),
            ctx.skillRank);

        std::cout << "[FioraSim] R mark caster=" << ctx.casterEntity
            << " target=" << target << "\n";
    }
}

namespace FioraGameSim
{
    f32_t ConsumeBasicAttackDamage(CWorld& world, EntityID caster, f32_t baseDamage)
    {
        if (!world.HasComponent<FioraSimComponent>(caster))
            return baseDamage;

        FioraSimComponent& state = world.GetComponent<FioraSimComponent>(caster);
        if (!state.bBladeworkActive || state.bladeworkHitsRemaining == 0)
            return baseDamage;

        --state.bladeworkHitsRemaining;
        if (state.bladeworkHitsRemaining == 0)
            state.bBladeworkActive = false;

        return baseDamage + state.bladeworkDamageBonus;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<FioraDashComponent, TransformComponent>(
            std::function<void(EntityID, FioraDashComponent&, TransformComponent&)>(
                [&](EntityID entity, FioraDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

                    dash.elapsedSec += tc.fDt;
                    f32_t t = dash.durationSec > 0.01f
                        ? dash.elapsedSec / dash.durationSec
                        : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }

                    const Vec3 position{
                        dash.start.x + (dash.end.x - dash.start.x) * t,
                        dash.start.y + (dash.end.y - dash.start.y) * t,
                        dash.start.z + (dash.end.z - dash.start.z) * t
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
            world.RemoveComponent<FioraDashComponent>(entity);

        world.ForEach<FioraSimComponent>(
            std::function<void(EntityID, FioraSimComponent&)>(
                [&](EntityID, FioraSimComponent& state)
                {
                    if (state.bBladeworkActive)
                    {
                        state.bladeworkTimerSec = std::max(0.f, state.bladeworkTimerSec - tc.fDt);
                        if (state.bladeworkTimerSec <= 0.f)
                        {
                            state.bBladeworkActive = false;
                            state.bladeworkHitsRemaining = 0;
                        }
                    }

                    if (state.bRiposteActive)
                    {
                        state.riposteTimerSec = std::max(0.f, state.riposteTimerSec - tc.fDt);
                        if (state.riposteTimerSec <= 0.f)
                            state.bRiposteActive = false;
                    }

                    if (state.bGrandChallengeActive)
                    {
                        state.grandChallengeTimerSec = std::max(
                            0.f,
                            state.grandChallengeTimerSec - tc.fDt);
                        if (state.grandChallengeTimerSec <= 0.f)
                        {
                            state.bGrandChallengeActive = false;
                            state.grandChallengeTarget = NULL_ENTITY;
                        }
                    }
                }));
    }

    void RegisterHooks()
    {
        //
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[FioraSim] hooks registered\n";
    }
}
