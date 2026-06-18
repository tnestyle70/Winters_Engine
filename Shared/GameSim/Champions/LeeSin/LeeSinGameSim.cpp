#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr f32_t kLeeSinQMarkDurationSec = 3.f;
    constexpr f32_t kLeeSinQ2Damage = 95.f;
    constexpr f32_t kLeeSinQDashGap = 1.0f;
    constexpr f32_t kLeeSinQDashDurationSec = 0.18f;
    constexpr f32_t kLeeSinERadius = 3.5f;
    constexpr f32_t kLeeSinESlowDurationSec = 1.5f;
    constexpr f32_t kLeeSinESlowMoveSpeedMul = 0.60f;
    constexpr f32_t kLeeSinRDamage = 150.f;
    constexpr f32_t kLeeSinRAirborneDurationSec = 1.0f;

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
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
            ResolveChampionVisualYawNear(eChampion::LEESIN, dir, rot.y),
            rot.z });
    }

    void EnqueuePhysicalDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        f32_t amount,
        u8_t slot,
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
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::LEESIN) << 8) | slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    void StartTargetDash(CWorld& world, EntityID caster, EntityID target)
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
        const f32_t moveDist = std::max(0.f, dist - kLeeSinQDashGap);

        LeeSinDashComponent dash{};
        dash.vStart = start;
        dash.vEnd = Vec3{
            start.x + dir.x * moveDist,
            start.y,
            start.z + dir.z * moveDist
        };
        dash.fDurationSec = kLeeSinQDashDurationSec;

        if (world.HasComponent<LeeSinDashComponent>(caster))
            world.GetComponent<LeeSinDashComponent>(caster) = dash;
        else
            world.AddComponent<LeeSinDashComponent>(caster, dash);

        RotateToward(world, caster, dir);
        ClearMove(world, caster);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || ctx.pCommand->itemId != 2u)
            return;

        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY ||
            !ctx.pWorld->HasComponent<LeeSinQMarkComponent>(target))
        {
            std::cout << "[LeeSinSim] Q2 rejected missing mark caster="
                << ctx.casterEntity << " target=" << target << "\n";
            return;
        }

        const LeeSinQMarkComponent& mark =
            ctx.pWorld->GetComponent<LeeSinQMarkComponent>(target);
        if (mark.sourceEntity != ctx.casterEntity || mark.fRemainingSec <= 0.f)
        {
            std::cout << "[LeeSinSim] Q2 rejected stale mark caster="
                << ctx.casterEntity << " target=" << target << "\n";
            return;
        }

        StartTargetDash(*ctx.pWorld, ctx.casterEntity, target);
        EnqueuePhysicalDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            kLeeSinQ2Damage,
            static_cast<u8_t>(eSkillSlot::Q),
            ctx.skillRank);
        ctx.pWorld->RemoveComponent<LeeSinQMarkComponent>(target);

        std::cout << "[LeeSinSim] Q2 resonating strike caster="
            << ctx.casterEntity << " target=" << target << "\n";
    }

    void ApplyTempestCrippleSlow(CWorld& world, const TickContext& tc, EntityID caster, eTeam casterTeam)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        const f32_t radiusSq = kLeeSinERadius * kLeeSinERadius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == caster || champion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > radiusSq)
                        return;

                    GameplayStatus::ApplySlow(
                        world,
                        tc,
                        target,
                        caster,
                        eChampion::LEESIN,
                        eSkillSlot::E,
                        kLeeSinESlowDurationSec,
                        kLeeSinESlowMoveSpeedMul);
                }));
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        ApplyTempestCrippleSlow(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterTeam);

        std::cout << "[LeeSinSim] E slow caster="
            << ctx.casterEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY || !ctx.pWorld->IsAlive(target))
            return;

        EnqueuePhysicalDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            kLeeSinRDamage,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank);
        GameplayStatus::ApplyAirborne(
            *ctx.pWorld,
            *ctx.pTickCtx,
            target,
            ctx.casterEntity,
            eChampion::LEESIN,
            eSkillSlot::R,
            kLeeSinRAirborneDurationSec);

        std::cout << "[LeeSinSim] R dragon rage caster="
            << ctx.casterEntity << " target=" << target << "\n";
    }
}

namespace LeeSinGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[LeeSinSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<LeeSinDashComponent, TransformComponent>(
            std::function<void(EntityID, LeeSinDashComponent&, TransformComponent&)>(
                [&](EntityID entity, LeeSinDashComponent& dash, TransformComponent& transform)
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
            world.RemoveComponent<LeeSinDashComponent>(entity);

        std::vector<EntityID> expiredMarks;
        world.ForEach<LeeSinQMarkComponent>(
            std::function<void(EntityID, LeeSinQMarkComponent&)>(
                [&](EntityID entity, LeeSinQMarkComponent& mark)
                {
                    mark.fRemainingSec = std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f || !world.IsAlive(mark.sourceEntity))
                        expiredMarks.push_back(entity);
                }));

        for (EntityID entity : expiredMarks)
            world.RemoveComponent<LeeSinQMarkComponent>(entity);
    }

    void ApplySonicWaveMark(CWorld& world, EntityID source, EntityID target)
    {
        if (source == NULL_ENTITY || target == NULL_ENTITY || !world.IsAlive(target))
            return;

        LeeSinQMarkComponent mark{};
        mark.sourceEntity = source;
        mark.fRemainingSec = kLeeSinQMarkDurationSec;

        if (world.HasComponent<LeeSinQMarkComponent>(target))
            world.GetComponent<LeeSinQMarkComponent>(target) = mark;
        else
            world.AddComponent<LeeSinQMarkComponent>(target, mark);

        std::cout << "[LeeSinSim] Q1 sonic wave mark source="
            << source << " target=" << target << "\n";
    }
}
