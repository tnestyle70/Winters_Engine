#include "WintersPCH.h"
#include "ECS/Systems/MinionAISystem.h"
#include "ECS/World.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/NavAgentComponent.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

NS_BEGIN(Engine)

static constexpr uint32_t kParallelThreshold = 16;
static constexpr bool_t kMinionAIDebugOutput = false;

static void FaceToward(TransformComponent& transform, const Vec3& targetPos)
{
    const Vec3 pos = transform.GetPosition();
    const Vec3 dir = WintersMath::DirectionXZ(pos, targetPos, Vec3{}, 0.0001f);
    if (dir.x == 0.f && dir.z == 0.f)
        return;

    Vec3 rot = transform.GetRotation();
    rot.y = static_cast<f32_t>(std::atan2(-dir.x, -dir.z));
    transform.SetRotation(rot);
}

void CMinionAISystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.Write<MinionStateComponent>()
        .Read<MinionComponent>()
        .Write<TransformComponent>()
        .Write<NavAgentComponent>()
        .Write<HealthComponent>();
}

void CMinionAISystem::Set_JobSystem(CJobSystem* pJS)
{
    m_pJobSystem = pJS;
    Ensure_SlotBuffers();
}

void CMinionAISystem::Ensure_SlotBuffers()
{
    const uint32_t need = (m_pJobSystem ? m_pJobSystem->GetWorkerCount() : 0u) + 1u;
    if (m_vecDecisionsPerSlot.size() == need && m_vecDamagesPerSlot.size() == need)
        return;

    m_vecDecisionsPerSlot.resize(need);
    m_vecDamagesPerSlot.resize(need);
}

void CMinionAISystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("MinionAI::Execute");
    Ensure_SlotBuffers();

    std::vector<EntityID> vecMinions;
    world.ForEach<MinionStateComponent, MinionComponent, TransformComponent>(
        function<void(EntityID, MinionStateComponent&, MinionComponent&, TransformComponent&)>(
            [&](EntityID id, MinionStateComponent& ms, MinionComponent&, TransformComponent&)
            {
                if (ms.current == MinionStateComponent::Dead)
                    return;
                vecMinions.push_back(id);
            }));

    WINTERS_PROFILE_COUNT("MinionAI::Candidates",
        static_cast<i32_t>(vecMinions.size()));

    if (vecMinions.size() < kParallelThreshold || m_pJobSystem == nullptr)
    {
        WINTERS_PROFILE_SCOPE("MinionAI::DecisionPass");
        for (EntityID id : vecMinions)
            DecisionPass(world, id, dt);
    }
    else
    {
        WINTERS_PROFILE_SCOPE("MinionAI::DecisionPassParallel");
        CJobCounter counter;
        CWorld* pWorld = &world;
        CMinionAISystem* pThis = this;
        for (EntityID id : vecMinions)
        {
            m_pJobSystem->Submit(
                [pThis, pWorld, id, dt]() { pThis->DecisionPass(*pWorld, id, dt); },
                &counter);
        }
        m_pJobSystem->WaitForCounter(&counter);
    }

    {
        WINTERS_PROFILE_SCOPE("MinionAI::ApplyPass");
        ApplyPass(world, dt);
    }
}

void CMinionAISystem::DecisionPass(CWorld& world, EntityID id, f32_t dt)
{
    if (!world.HasComponent<MinionStateComponent>(id)) return;
    if (!world.HasComponent<MinionComponent>(id))      return;
    if (!world.HasComponent<TransformComponent>(id))   return;

    const auto& ms = world.GetComponent<MinionStateComponent>(id);
    const auto& minion = world.GetComponent<MinionComponent>(id);
    const auto& xform = world.GetComponent<TransformComponent>(id);

    if (ms.current == MinionStateComponent::Dead)
        return;

    MinionDecision dec{};
    dec.self = id;
    dec.cooldownAfterTick = std::max(0.f, ms.attackCooldown - dt);
    dec.targetScanCooldownAfterTick = std::max(0.f, ms.targetScanCooldown - dt);

    EntityID curTarget = ms.attackTargetId;
    if (curTarget != NULL_ENTITY)
    {
        bool_t bValid = world.IsAlive(curTarget)
            && world.HasComponent<MinionStateComponent>(curTarget)
            && world.HasComponent<HealthComponent>(curTarget)
            && world.HasComponent<TransformComponent>(curTarget);

        if (bValid)
        {
            const auto& tgtHp = world.GetComponent<HealthComponent>(curTarget);
            if (tgtHp.bIsDead || tgtHp.fCurrent <= 0.f)
                bValid = false;
        }

        if (!bValid)
            curTarget = NULL_ENTITY;
    }

    const Vec3 myPos = xform.GetPosition();
    if (curTarget == NULL_ENTITY)
    {
        if (dec.targetScanCooldownAfterTick <= 0.f)
        {
            curTarget = FindClosestEnemy(
                world, id, myPos, static_cast<uint8_t>(minion.team), ms.sightRange);
            dec.targetScanCooldownAfterTick = ms.targetScanInterval;
        }
        else
        {
            dec.target = NULL_ENTITY;
            dec.desiredState = MinionStateComponent::LaneMove;
            dec.bStopMovement = true;
            Push_Decision(dec);
            return;
        }
    }

    if (curTarget == NULL_ENTITY)
    {
        dec.target = NULL_ENTITY;
        dec.desiredState = MinionStateComponent::LaneMove;
        dec.bStopMovement = true;
        Push_Decision(dec);
        return;
    }

    dec.target = curTarget;

    const auto& tgtXform = world.GetComponent<TransformComponent>(curTarget);
    const Vec3 tgtPos = tgtXform.GetPosition();
    const f32_t dx = tgtPos.x - myPos.x;
    const f32_t dz = tgtPos.z - myPos.z;
    const f32_t distSq = dx * dx + dz * dz;
    const f32_t rangeSq = ms.attackRange * ms.attackRange;
    const f32_t sightSq = ms.sightRange * ms.sightRange;

    if (distSq <= rangeSq)
    {
        dec.desiredState = MinionStateComponent::Attack;
        dec.bStopMovement = true;
        dec.bStartAttack = (dec.cooldownAfterTick <= 0.f && ms.attackTimer <= 0.f);
    }
    else if (distSq <= sightSq)
    {
        dec.desiredState = MinionStateComponent::Chase;
        dec.navTarget = tgtPos;
        dec.bSetNavTarget = true;
    }
    else
    {
        // sight 밖 → 타겟 버리고 LaneMove 복귀.
        // bStopMovement=true 로 ApplyPass 가 NavAgent path/goal 정리 → 다음 Chase 첫 프레임 stale path 방지.
        dec.target = NULL_ENTITY;
        dec.desiredState = MinionStateComponent::LaneMove;
        dec.bStopMovement = true;
    }

    if (dec.desiredState == MinionStateComponent::Attack)
        WINTERS_PROFILE_COUNT("MinionAI::Attack", 1);
    else if (dec.desiredState == MinionStateComponent::Chase)
        WINTERS_PROFILE_COUNT("MinionAI::Chase", 1);

#if defined(_DEBUG)
    if constexpr (kMinionAIDebugOutput)
    {
        static i32_t s_aiLogCount = 0;
        if (s_aiLogCount < 120 &&
            (dec.desiredState == MinionStateComponent::Chase ||
             dec.desiredState == MinionStateComponent::Attack))
        {
            char dbg[512];
            sprintf_s(dbg,
                "[MinionAI] #%d self=%u team=%u state=%u target=%u targetDist=%.2f range=%.2f sight=%.2f setNav=%d navTarget=(%.2f,%.2f,%.2f) stop=%d startAtk=%d cd=%.2f atkTimer=%.2f\n",
                s_aiLogCount,
                static_cast<u32_t>(id),
                static_cast<u32_t>(minion.team),
                static_cast<u32_t>(dec.desiredState),
                static_cast<u32_t>(dec.target),
                sqrtf(distSq),
                ms.attackRange,
                ms.sightRange,
                dec.bSetNavTarget ? 1 : 0,
                dec.navTarget.x, dec.navTarget.y, dec.navTarget.z,
                dec.bStopMovement ? 1 : 0,
                dec.bStartAttack ? 1 : 0,
                dec.cooldownAfterTick,
                ms.attackTimer);
            WintersOutputAIDebugStringA(dbg);
            ++s_aiLogCount;
        }
    }
#endif

    Push_Decision(dec);
}

void CMinionAISystem::ApplyPass(CWorld& world, f32_t dt)
{
    for (auto& vecBuf : m_vecDecisionsPerSlot)
    {
        for (const MinionDecision& dec : vecBuf)
        {
            if (!world.IsAlive(dec.self)) continue;
            if (!world.HasComponent<MinionStateComponent>(dec.self)) continue;

            auto& ms = world.GetComponent<MinionStateComponent>(dec.self);
            if (ms.current == MinionStateComponent::Dead)
                continue;

            ms.attackCooldown = dec.cooldownAfterTick;
            ms.targetScanCooldown = dec.targetScanCooldownAfterTick;
            ms.attackTargetId = dec.target;
            ms.current = dec.desiredState;

            if (world.HasComponent<TransformComponent>(dec.self))
            {
                auto& selfTf = world.GetComponent<TransformComponent>(dec.self);
                if ((dec.desiredState == MinionStateComponent::Attack ||
                     dec.desiredState == MinionStateComponent::Chase) &&
                    dec.target != NULL_ENTITY &&
                    world.HasComponent<TransformComponent>(dec.target))
                {
                    FaceToward(selfTf, world.GetComponent<TransformComponent>(dec.target).GetPosition());
                }
                else if (dec.bSetNavTarget)
                {
                    FaceToward(selfTf, dec.navTarget);
                }
            }

            if (dec.bStopMovement)
            {
                if (dec.desiredState != MinionStateComponent::Attack)
                {
                    ms.attackTimer = 0.f;
                    ms.bHitFired = false;
                    ms.bAttackAnimRequested = false;
                }

                if (world.HasComponent<NavAgentComponent>(dec.self))
                {
                    auto& nav = world.GetComponent<NavAgentComponent>(dec.self);
                    nav.bHasGoal = false;
                    // 잔존 path 정리 — 다음 Chase 트리거 시 bPathDirty=true 로 재계산되지만
                    // 명시적으로 비워야 stale waypoint 인덱스로 따라가는 사고 방지.
                    nav.pathCellsX.clear();
                    nav.pathCellsY.clear();
                    nav.iPathIndex = 0;
                }

                if (world.HasComponent<VelocityComponent>(dec.self))
                {
                    auto& vel = world.GetComponent<VelocityComponent>(dec.self);
                    vel.vDirection = { 0.f, 0.f, 0.f };
                    vel.fSpeed = 0.f;
                }
            }

            if (dec.bSetNavTarget && world.HasComponent<NavAgentComponent>(dec.self))
            {
                auto& nav = world.GetComponent<NavAgentComponent>(dec.self);
                nav.vTarget = dec.navTarget;
                nav.fSpeed = ms.moveSpeed;
                nav.bHasGoal = true;
                nav.bPathDirty = true;
            }

            if (dec.bStartAttack)
            {
                ms.attackTimer = ms.attackWindup + ms.attackRecovery;
                ms.bHitFired = false;
                ms.bAttackAnimRequested = true;
                ms.attackCooldown = ms.attackCooldownMax;
            }
        }
        vecBuf.clear();
    }

    world.ForEach<MinionStateComponent>(
        function<void(EntityID, MinionStateComponent&)>(
            [this, &world, dt](EntityID id, MinionStateComponent& ms)
            {
                if (ms.current != MinionStateComponent::Attack)
                    return;
                if (ms.attackTimer <= 0.f)
                    return;

                const f32_t prev = ms.attackTimer;
                ms.attackTimer = std::max(0.f, ms.attackTimer - dt);
                const f32_t hitTime = ms.attackRecovery;

                if (!ms.bHitFired && prev > hitTime && ms.attackTimer <= hitTime)
                {
                    if (world.HasComponent<TransformComponent>(id) &&
                        world.HasComponent<TransformComponent>(ms.attackTargetId))
                    {
                        auto& selfTf = world.GetComponent<TransformComponent>(id);
                        const Vec3 targetPos = world.GetComponent<TransformComponent>(ms.attackTargetId).GetPosition();
                        FaceToward(selfTf, targetPos);
                    }
                    QueueDamage(id, ms.attackTargetId, ms.attackDamage, true);
                    ms.bHitFired = true;
                }
            }));

    for (auto& vecBuf : m_vecDamagesPerSlot)
    {
        for (const DamageEvent& evt : vecBuf)
            Apply_Damage(world, evt);
        vecBuf.clear();
    }
}

void CMinionAISystem::Push_Decision(const MinionDecision& dec)
{
    const uint32_t slot = CJobSystem::Get_WorkerSlot();
    if (slot >= m_vecDecisionsPerSlot.size())
        return;
    m_vecDecisionsPerSlot[slot].push_back(dec);
}

void CMinionAISystem::QueueDamage(EntityID source, EntityID target, f32_t amount, bool_t bKill)
{
    if (m_vecDamagesPerSlot.empty())
        return;
    m_vecDamagesPerSlot[0].push_back(DamageEvent{ source, target, amount, bKill });
}

void CMinionAISystem::Apply_Damage(CWorld& world, const DamageEvent& evt)
{
    if (evt.target == NULL_ENTITY) return;
    if (!world.IsAlive(evt.target)) return;
    if (!world.HasComponent<HealthComponent>(evt.target)) return;

    auto& hp = world.GetComponent<HealthComponent>(evt.target);
    if (hp.bIsDead)
        return;

    hp.fCurrent -= evt.amount;
    if (hp.fCurrent > 0.f || !evt.bKill)
        return;

    hp.fCurrent = 0.f;
    hp.bIsDead = true;

    if (world.HasComponent<MinionStateComponent>(evt.target))
    {
        auto& tgtMs = world.GetComponent<MinionStateComponent>(evt.target);
        tgtMs.current = MinionStateComponent::Dead;
        tgtMs.deathTimer = 1.5f;
        tgtMs.attackTargetId = NULL_ENTITY;
        tgtMs.attackTimer = 0.f;
        tgtMs.bHitFired = false;
        tgtMs.bAttackAnimRequested = false;
    }

    if (world.HasComponent<VelocityComponent>(evt.target))
    {
        auto& vel = world.GetComponent<VelocityComponent>(evt.target);
        vel.vDirection = { 0.f, 0.f, 0.f };
        vel.fSpeed = 0.f;
    }

    if (world.HasComponent<NavAgentComponent>(evt.target))
        world.GetComponent<NavAgentComponent>(evt.target).bHasGoal = false;
}

EntityID CMinionAISystem::FindClosestEnemy(
    CWorld& world, EntityID self, const Vec3& myPos, uint8_t myTeamRaw, f32_t searchRange)
{
    WINTERS_PROFILE_SCOPE("MinionAI::FindClosest");

    if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
    {
        EntityID candidate = pSpatial->QueryClosest(
            myPos,
            searchRange,
            SpatialMask(eSpatialKind::Minion),
            myTeamRaw);

        if (candidate == NULL_ENTITY || candidate == self)
            return NULL_ENTITY;
        if (!world.IsAlive(candidate))
            return NULL_ENTITY;
        if (!world.HasComponent<MinionStateComponent>(candidate))
            return NULL_ENTITY;
        if (!world.HasComponent<HealthComponent>(candidate))
            return NULL_ENTITY;
        if (!world.HasComponent<TransformComponent>(candidate))
            return NULL_ENTITY;

        const auto& ms = world.GetComponent<MinionStateComponent>(candidate);
        const auto& hp = world.GetComponent<HealthComponent>(candidate);
        if (static_cast<u8_t>(ms.team) == myTeamRaw)
            return NULL_ENTITY;
        if (ms.current == MinionStateComponent::Dead || hp.bIsDead || hp.fCurrent <= 0.f)
            return NULL_ENTITY;

        return candidate;
    }

    EntityID best = NULL_ENTITY;
    f32_t bestDistSq = searchRange * searchRange;
#if defined(_DEBUG)
    EntityID nearestAny = NULL_ENTITY;
    f32_t nearestAnyDistSq = std::numeric_limits<f32_t>::max();
    i32_t enemyCandidateCount = 0;
    EntityID nearestMinion = NULL_ENTITY;
    f32_t nearestMinionDistSq = std::numeric_limits<f32_t>::max();
    i32_t enemyMinionCandidateCount = 0;
#endif

    auto eval = [&](EntityID id, uint8_t otherTeamRaw, bool_t bDead, const Vec3& tp, bool_t bSelectable)
    {
        if (id == self || bDead || otherTeamRaw == myTeamRaw) return;
        const f32_t dx = tp.x - myPos.x;
        const f32_t dz = tp.z - myPos.z;
        const f32_t distSq = dx * dx + dz * dz;
#if defined(_DEBUG)
        ++enemyCandidateCount;
        if (distSq < nearestAnyDistSq)
        {
            nearestAnyDistSq = distSq;
            nearestAny = id;
        }
#endif
        if (!bSelectable)
            return;
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            best = id;
        }
    };

    {
        WINTERS_PROFILE_SCOPE("FindClosest::Minions");
        world.ForEach<MinionStateComponent, TransformComponent, HealthComponent>(
            function<void(EntityID, MinionStateComponent&, TransformComponent&, HealthComponent&)>(
                [&](EntityID id, MinionStateComponent& ms, TransformComponent& t, HealthComponent& hp)
                {
                    const bool_t bDead = (ms.current == MinionStateComponent::Dead) || hp.bIsDead;
#if defined(_DEBUG)
                    if (id != self && !bDead && static_cast<uint8_t>(ms.team) != myTeamRaw)
                    {
                        const Vec3 tp = t.GetPosition();
                        const f32_t dx = tp.x - myPos.x;
                        const f32_t dz = tp.z - myPos.z;
                        const f32_t distSq = dx * dx + dz * dz;
                        ++enemyMinionCandidateCount;
                        if (distSq < nearestMinionDistSq)
                        {
                            nearestMinionDistSq = distSq;
                            nearestMinion = id;
                        }
                    }
#endif
                    eval(id, static_cast<uint8_t>(ms.team), bDead, t.GetPosition(), true);
                }));
    }

    // Lane minion AI only targets enemy minions for now. Champions and
    // structures are ignored so first aggro cannot pull minions off wave combat.
    if (best != NULL_ENTITY)
        return best;

#if defined(_DEBUG)
    if constexpr (kMinionAIDebugOutput)
    {
        if (best == NULL_ENTITY)
        {
            static i32_t s_noTargetLogCount = 0;
            if (s_noTargetLogCount < 120)
            {
                const f32_t nearestDist = (nearestAny != NULL_ENTITY)
                    ? sqrtf(nearestAnyDistSq)
                    : -1.f;
                const f32_t nearestMinionDist = (nearestMinion != NULL_ENTITY)
                    ? sqrtf(nearestMinionDistSq)
                    : -1.f;

                char dbg[512];
                sprintf_s(dbg,
                    "[MinionAI-NoTarget] #%d self=%u team=%u pos=(%.2f,%.2f,%.2f) sight=%.2f candidates=%d nearest=%u nearestDist=%.2f minionCandidates=%d nearestMinion=%u nearestMinionDist=%.2f\n",
                    s_noTargetLogCount,
                    static_cast<u32_t>(self),
                    static_cast<u32_t>(myTeamRaw),
                    myPos.x, myPos.y, myPos.z,
                    searchRange,
                    enemyCandidateCount,
                    static_cast<u32_t>(nearestAny),
                    nearestDist,
                    enemyMinionCandidateCount,
                    static_cast<u32_t>(nearestMinion),
                    nearestMinionDist);
                WintersOutputAIDebugStringA(dbg);
                ++s_noTargetLogCount;
            }
        }
    }
#endif

    return best;
}

NS_END
