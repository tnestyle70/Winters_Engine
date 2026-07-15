#include "Game/GameRoom.h"

#include "Game/ServerProjectileAuthority.h"
#include "GameRoomInternal.h"

#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"
#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Systems/SpatialHashSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
    void LogSkillProjectileEvent(
        const char* state,
        EntityID projectileEntity,
        const SkillProjectileComponent& projectile,
        EntityID targetEntity,
        const Vec3& pos)
    {
        static u32_t s_logCount = 0;
        if (s_logCount >= 128u)
            return;

        char msg[256]{};
        sprintf_s(msg,
            "[SkillProjectile] %s kind=%u source=%u projectile=%u target=%u pos=(%.2f,%.2f,%.2f) traveled=%.2f\n",
            state ? state : "-",
            static_cast<u32_t>(projectile.kind),
            static_cast<u32_t>(projectile.sourceEntity),
            static_cast<u32_t>(projectileEntity),
            static_cast<u32_t>(targetEntity),
            pos.x,
            pos.y,
            pos.z,
            projectile.traveledDistance);
        OutputServerAITrace(msg);
        ++s_logCount;
    }
}

void CGameRoom::Phase_ServerTurretAI(TickContext& tc)
{
    if (!IsInGamePhase())
        return;

    if (m_pSpatialSystem)
        m_pSpatialSystem->Execute(m_world, tc.fDt);
    if (m_pTurretAI)
        m_pTurretAI->Execute(m_world, tc.fDt);
}

void CGameRoom::Phase_ServerProjectiles(TickContext& tc)
{
    if (!IsInGamePhase())
        return;

    const auto projectiles =
        DeterministicEntityIterator<StructureProjectileComponent>::CollectSorted(m_world);

    for (EntityID entity : projectiles)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<StructureProjectileComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& projectile = m_world.GetComponent<StructureProjectileComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);
        const Vec3 pos = projectile.currentPos;
        const NetEntityId currentProjectileNet = m_entityMap.ToNet(entity);
        auto EnqueueStructureContact =
            [&](EntityID target,
                const Vec3& position,
                ProjectileContactReason eReason,
                bool_t bDestroyed)
            {
                ReplicatedEventComponent event =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        target,
                        entity,
                        CServerProjectileAuthority::kStructureProjectileKind,
                        position,
                        tc.tickIndex,
                        eReason,
                        projectile.uContactOrdinal++,
                        bDestroyed);
                event.projectileNetOverride = projectile.uProjectileNetAtSpawn;
                event.sourceNetOverride = projectile.uSourceNetAtSpawn;
                event.targetNetOverride = target != NULL_ENTITY
                    ? m_entityMap.ToNet(target)
                    : (eReason == ProjectileContactReason::TargetInvalid
                        ? projectile.uTargetNetAtSpawn
                        : NULL_NET_ENTITY);
                EnqueueReplicatedEvent(m_world, event);
                if (bDestroyed &&
                    event.projectileNetOverride != NULL_NET_ENTITY)
                {
                    m_entityMap.Unbind(event.projectileNetOverride);
                }
            };
        if (projectile.sourceHandle.IsValid())
        {
            projectile.sourceEntity =
                m_world.ResolveEntity(projectile.sourceHandle);
        }
        if (projectile.targetHandle.IsValid())
        {
            projectile.targetEntity =
                m_world.ResolveEntity(projectile.targetHandle);
        }
        const bool_t bTargetAlive =
            projectile.targetEntity != NULL_ENTITY &&
            m_world.IsAlive(projectile.targetEntity) &&
            m_world.HasComponent<TransformComponent>(projectile.targetEntity) &&
            IsAliveHealth(m_world, projectile.targetEntity) &&
            GameplayStateQuery::CanReceiveProjectileHit(
                m_world,
                projectile.sourceEntity,
                projectile.targetEntity);

        if (!bTargetAlive)
        {
            if (currentProjectileNet != NULL_NET_ENTITY)
            {
                if (projectile.uProjectileNetAtSpawn == NULL_NET_ENTITY)
                    projectile.uProjectileNetAtSpawn = currentProjectileNet;
                if (projectile.uSourceNetAtSpawn == NULL_NET_ENTITY)
                    projectile.uSourceNetAtSpawn =
                        m_entityMap.ToNet(projectile.sourceEntity);
                if (projectile.uTargetNetAtSpawn == NULL_NET_ENTITY)
                    projectile.uTargetNetAtSpawn =
                        m_entityMap.ToNet(projectile.targetEntity);
                EnqueueStructureContact(
                    NULL_ENTITY,
                    pos,
                    ProjectileContactReason::TargetInvalid,
                    true);
            }

            m_world.DestroyEntity(entity);
            continue;
        }

        if (currentProjectileNet == NULL_NET_ENTITY)
        {
            const NetEntityId projectileNet = m_entityMap.IssueNew(entity);
            NetEntityIdComponent net{};
            net.netId = projectileNet;
            if (!m_world.HasComponent<NetEntityIdComponent>(entity))
                m_world.AddComponent<NetEntityIdComponent>(entity, net);

            projectile.uProjectileNetAtSpawn = projectileNet;
            projectile.uSourceNetAtSpawn =
                m_entityMap.ToNet(projectile.sourceEntity);
            projectile.uTargetNetAtSpawn =
                m_entityMap.ToNet(projectile.targetEntity);

            Vec3 dir{ 0.f, 0.f, 1.f };
            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
            dir = NormalizeXZOrForward(
                Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z },
                eTeam::Neutral);
            projectile.direction = dir;

            ReplicatedEventComponent spawn =
                CServerProjectileAuthority::BuildProjectileSpawnEvent(
                    projectile.sourceEntity,
                    projectile.targetEntity,
                    entity,
                    CServerProjectileAuthority::kStructureProjectileKind,
                    pos,
                    dir,
                    projectile.speed,
                    projectile.maxDistance,
                    tc.tickIndex);
            spawn.projectileNetOverride = projectile.uProjectileNetAtSpawn;
            spawn.sourceNetOverride = projectile.uSourceNetAtSpawn;
            spawn.targetNetOverride = projectile.uTargetNetAtSpawn;
            EnqueueReplicatedEvent(m_world, spawn);

            static u32_t s_turretProjectileLogCount = 0;
            if (s_turretProjectileLogCount < 64u)
            {
                char msg[192]{};
                sprintf_s(msg,
                    "[TurretAI] projectile tick=%llu source=%u target=%u projectile=%u pos=(%.2f,%.2f,%.2f)\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(projectile.sourceEntity),
                    static_cast<u32_t>(projectile.targetEntity),
                    static_cast<u32_t>(entity),
                    pos.x,
                    pos.y,
                    pos.z);
                OutputServerAITrace(msg);
                ++s_turretProjectileLogCount;
            }
        }

        const f32_t fDirectionLengthSq =
            projectile.direction.x * projectile.direction.x +
            projectile.direction.y * projectile.direction.y +
            projectile.direction.z * projectile.direction.z;
        if (!std::isfinite(projectile.currentPos.x) ||
            !std::isfinite(projectile.currentPos.y) ||
            !std::isfinite(projectile.currentPos.z) ||
            !std::isfinite(projectile.direction.x) ||
            !std::isfinite(projectile.direction.y) ||
            !std::isfinite(projectile.direction.z) ||
            !std::isfinite(projectile.speed) ||
            !std::isfinite(projectile.maxDistance) ||
            !std::isfinite(projectile.traveledDistance) ||
            !std::isfinite(projectile.hitRadius) ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f ||
            projectile.traveledDistance < 0.f ||
            projectile.hitRadius < 0.f ||
            fDirectionLengthSq <= std::numeric_limits<f32_t>::epsilon())
        {
            EnqueueStructureContact(
                NULL_ENTITY,
                projectile.currentPos,
                ProjectileContactReason::InvalidTrajectory,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }

        const Vec3 targetPos = m_world.GetComponent<TransformComponent>(
            projectile.targetEntity).GetPosition();
        if (!std::isfinite(targetPos.x) ||
            !std::isfinite(targetPos.y) ||
            !std::isfinite(targetPos.z))
        {
            EnqueueStructureContact(
                NULL_ENTITY,
                projectile.currentPos,
                ProjectileContactReason::TargetInvalid,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }
        const Vec3 targetAim{ targetPos.x, targetPos.y + 1.2f, targetPos.z };
        const Vec3 delta{
            targetAim.x - pos.x,
            targetAim.y - pos.y,
            targetAim.z - pos.z
        };
        const f32_t distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        const f32_t hitRadiusSq = projectile.hitRadius * projectile.hitRadius;

        if (distSq <= hitRadiusSq)
        {
            eTeam sourceTeam = eTeam::Neutral;
            (void)TryResolveCombatTeam(m_world, projectile.sourceEntity, sourceTeam);

            const DamageRequest request =
                CServerProjectileAuthority::BuildTurretDamageRequest(
                    projectile.sourceEntity,
                    projectile.targetEntity,
                    sourceTeam,
                    projectile.damage);
            EnqueueDamageRequest(m_world, request);

            EnqueueStructureContact(
                projectile.targetEntity,
                targetAim,
                ProjectileContactReason::UnitHit,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t dist = std::sqrt(distSq);
        const f32_t remaining =
            projectile.maxDistance - projectile.traveledDistance;
        if (dist <= std::numeric_limits<f32_t>::epsilon() ||
            remaining <= 0.f)
        {
            EnqueueStructureContact(
                NULL_ENTITY,
                pos,
                ProjectileContactReason::RangeExpired,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t fInvDistance = 1.f / dist;
        projectile.direction = Vec3{
            delta.x * fInvDistance,
            delta.y * fInvDistance,
            delta.z * fInvDistance };
        const f32_t step = (std::min)(projectile.speed * tc.fDt, remaining);
        const f32_t actualStep = (std::min)(step, dist);
        const f32_t t = actualStep / dist;
        const Vec3 next{
            pos.x + delta.x * t,
            pos.y + delta.y * t,
            pos.z + delta.z * t };
        projectile.currentPos = next;
        projectile.traveledDistance += actualStep;
        transform.SetPosition(next);

        if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
        {
            EnqueueStructureContact(
                NULL_ENTITY,
                next,
                ProjectileContactReason::RangeExpired,
                true);
            m_world.DestroyEntity(entity);
        }
    }

    const auto skillProjectiles =
        DeterministicEntityIterator<SkillProjectileComponent>::CollectSorted(m_world);

    for (EntityID entity : skillProjectiles)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<SkillProjectileComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& projectile = m_world.GetComponent<SkillProjectileComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);
        auto EnqueueProjectileContact =
            [&](EntityID target,
                const Vec3& position,
                ProjectileContactReason eReason,
                bool_t bDestroyed)
            {
                ReplicatedEventComponent event =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        target,
                        entity,
                        static_cast<u16_t>(projectile.kind),
                        position,
                        tc.tickIndex,
                        eReason,
                        projectile.uContactOrdinal++,
                        bDestroyed);
                event.sourceNetOverride = projectile.uSourceNetAtSpawn;
                event.projectileNetOverride = projectile.uProjectileNetAtSpawn;
                event.targetNetOverride = target != NULL_ENTITY
                    ? m_entityMap.ToNet(target)
                    : (eReason == ProjectileContactReason::TargetInvalid
                        ? projectile.uTargetNetAtSpawn
                        : NULL_NET_ENTITY);
                EnqueueReplicatedEvent(m_world, event);
                if (bDestroyed &&
                    event.projectileNetOverride != NULL_NET_ENTITY)
                {
                    m_entityMap.Unbind(event.projectileNetOverride);
                }
            };

        if (projectile.sourceHandle.IsValid())
        {
            const EntityID resolvedSource =
                m_world.ResolveEntity(projectile.sourceHandle);
            if (resolvedSource == NULL_ENTITY)
            {
                projectile.sourceEntity = NULL_ENTITY;
                if (!projectile.bPersistAfterSourceDeath)
                {
                    if (projectile.bSpawned)
                    {
                        EnqueueProjectileContact(
                            NULL_ENTITY,
                            projectile.currentPos,
                            ProjectileContactReason::SourceInvalid,
                            true);
                    }
                    m_world.DestroyEntity(entity);
                    continue;
                }
            }
            else
            {
                projectile.sourceEntity = resolvedSource;
            }
        }
        if (projectile.targetHandle.IsValid())
        {
            const EntityID resolvedTarget =
                m_world.ResolveEntity(projectile.targetHandle);
            if (resolvedTarget == NULL_ENTITY)
            {
                if (projectile.bSpawned)
                {
                    EnqueueProjectileContact(
                        NULL_ENTITY,
                        projectile.currentPos,
                        ProjectileContactReason::TargetInvalid,
                        true);
                }
                m_world.DestroyEntity(entity);
                continue;
            }
            projectile.targetEntity = resolvedTarget;
        }

        if (!projectile.bSpawned)
        {
            const NetEntityId projectileNet = m_entityMap.IssueNew(entity);
            NetEntityIdComponent net{};
            net.netId = projectileNet;
            if (!m_world.HasComponent<NetEntityIdComponent>(entity))
                m_world.AddComponent<NetEntityIdComponent>(entity, net);

            projectile.bSpawned = true;
            projectile.uProjectileNetAtSpawn = projectileNet;
            projectile.uSourceNetAtSpawn = m_entityMap.ToNet(projectile.sourceEntity);
            projectile.uTargetNetAtSpawn = m_entityMap.ToNet(projectile.targetEntity);

            ReplicatedEventComponent spawn =
                CServerProjectileAuthority::BuildProjectileSpawnEvent(
                    projectile.sourceEntity,
                    projectile.targetEntity,
                    entity,
                    static_cast<u16_t>(projectile.kind),
                    projectile.currentPos,
                    projectile.direction,
                    projectile.speed,
                    projectile.maxDistance,
                    tc.tickIndex);
            spawn.projectileNetOverride = projectile.uProjectileNetAtSpawn;
            spawn.sourceNetOverride = projectile.uSourceNetAtSpawn;
            spawn.targetNetOverride = projectile.uTargetNetAtSpawn;
            EnqueueReplicatedEvent(m_world, spawn);
            if (!CServerProjectileAuthority::IsMinionRangedProjectileKind(projectile.kind))
                LogSkillProjectileEvent(
                    "spawn",
                    entity,
                    projectile,
                    projectile.targetEntity,
                    projectile.currentPos);
            continue;
        }

        if (!projectile.bPersistAfterSourceDeath &&
            !IsAliveHealth(m_world, projectile.sourceEntity))
        {
            EnqueueProjectileContact(
                NULL_ENTITY,
                projectile.currentPos,
                ProjectileContactReason::SourceInvalid,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t fDirectionLengthSq =
            projectile.direction.x * projectile.direction.x +
            projectile.direction.y * projectile.direction.y +
            projectile.direction.z * projectile.direction.z;
        const bool_t bFiniteTrajectory =
            std::isfinite(projectile.currentPos.x) &&
            std::isfinite(projectile.currentPos.y) &&
            std::isfinite(projectile.currentPos.z) &&
            std::isfinite(projectile.direction.x) &&
            std::isfinite(projectile.direction.y) &&
            std::isfinite(projectile.direction.z) &&
            std::isfinite(projectile.speed) &&
            std::isfinite(projectile.maxDistance) &&
            std::isfinite(projectile.traveledDistance) &&
            std::isfinite(projectile.hitRadius);
        const bool_t bLinearDirectionInvalid =
            projectile.targetEntity == NULL_ENTITY &&
            fDirectionLengthSq <= std::numeric_limits<f32_t>::epsilon();
        if (!bFiniteTrajectory ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f ||
            projectile.traveledDistance < 0.f ||
            projectile.hitRadius < 0.f ||
            bLinearDirectionInvalid)
        {
            EnqueueProjectileContact(
                NULL_ENTITY,
                projectile.currentPos,
                ProjectileContactReason::InvalidTrajectory,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }

        if (projectile.targetEntity != NULL_ENTITY)
        {
            const bool_t bTargetAlive =
                m_world.IsAlive(projectile.targetEntity) &&
                m_world.HasComponent<TransformComponent>(projectile.targetEntity) &&
                IsAliveHealth(m_world, projectile.targetEntity) &&
                GameplayStateQuery::CanReceiveProjectileHit(
                    m_world,
                    projectile.sourceEntity,
                    projectile.targetEntity);

            if (!bTargetAlive)
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    projectile.currentPos,
                    ProjectileContactReason::TargetInvalid,
                    true);
                m_world.DestroyEntity(entity);
                continue;
            }

            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
            if (!std::isfinite(targetPos.x) ||
                !std::isfinite(targetPos.y) ||
                !std::isfinite(targetPos.z))
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    projectile.currentPos,
                    ProjectileContactReason::TargetInvalid,
                    true);
                m_world.DestroyEntity(entity);
                continue;
            }
            const Vec3 targetAim{
                targetPos.x,
                targetPos.y + CServerProjectileAuthority::kMinionRangedProjectileTargetHeight,
                targetPos.z
            };
            const Vec3 delta{
                targetAim.x - projectile.currentPos.x,
                targetAim.y - projectile.currentPos.y,
                targetAim.z - projectile.currentPos.z
            };
            const f32_t distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            const f32_t dist = std::sqrt(distSq);
            const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
            f32_t actualStep = 0.f;
            Vec3 next = projectile.currentPos;
            if (remaining > 0.f && dist > std::numeric_limits<f32_t>::epsilon())
            {
                const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
                actualStep = (step >= dist) ? dist : step;
                const f32_t t = actualStep / dist;
                next = Vec3{
                    projectile.currentPos.x + delta.x * t,
                    projectile.currentPos.y + delta.y * t,
                    projectile.currentPos.z + delta.z * t };
            }
            if (actualStep > 0.f &&
                dist > std::numeric_limits<f32_t>::epsilon())
            {
                const f32_t fInvDistance = 1.f / dist;
                projectile.direction = Vec3{
                    delta.x * fInvDistance,
                    delta.y * fInvDistance,
                    delta.z * fInvDistance };
            }

            Vec3 targetHitPos = next;
            f32_t targetHitT = 1.f;
            const bool_t bTargetHit =
                CServerProjectileAuthority::FindTargetedProjectileHit(
                    m_world,
                    projectile,
                    tc.pLagCompensation,
                    tc.tickIndex,
                    projectile.targetEntity,
                    projectile.currentPos,
                    next,
                    targetHitPos,
                    targetHitT);
            Vec3 barrierHitPos = next;
            f32_t barrierHitT = 1.f;
            const bool_t bBarrierHit =
                CServerProjectileAuthority::FindProjectileBarrierHit(
                    m_world,
                    projectile,
                    projectile.currentPos,
                    next,
                    barrierHitPos,
                    barrierHitT);
            const bool_t bBarrierFirst = bBarrierHit &&
                (!bTargetHit ||
                    CServerProjectileAuthority::QuantizeContactT(barrierHitT) <=
                    CServerProjectileAuthority::QuantizeContactT(targetHitT));
            if (bBarrierFirst)
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    barrierHitPos,
                    ProjectileContactReason::Barrier,
                    true);
                LogSkillProjectileEvent(
                    "barrier",
                    entity,
                    projectile,
                    NULL_ENTITY,
                    barrierHitPos);
                m_world.DestroyEntity(entity);
                continue;
            }

            if (bTargetHit)
            {
                if (CServerProjectileAuthority::IsMinionRangedProjectileKind(projectile.kind) &&
                    !GameplayStateQuery::IsAttackSegmentGateExemptTarget(
                        m_world, projectile.targetEntity) &&
                    !SegmentWalkableXZ(projectile.currentPos, targetHitPos, 0.f))
                {
                    EnqueueProjectileContact(
                        NULL_ENTITY,
                        projectile.currentPos,
                        ProjectileContactReason::Terrain,
                        true);
                    m_world.DestroyEntity(entity);
                    continue;
                }

                if (projectile.bApplyOnHitStatus)
                {
                    GameplayStatus::ApplyStatusEffect(
                        m_world,
                        projectile.targetEntity,
                        projectile.onHitStatus,
                        tc);
                }

                // 칼리스타 평타(타겟 투사체) 적중 = 서버 권위 Rend 스택 +1.
                if (projectile.kind == eProjectileKind::KalistaBasicAttack ||
                    projectile.kind == eProjectileKind::KalistaPierce)
                {
                    KalistaGameSim::ApplyRendStackOnHit(
                        m_world, tc, projectile.sourceEntity, projectile.targetEntity);
                }

                DamageRequest request{};
                bool_t bEnqueueDamage = projectile.bApplyDamageOnHit;
                const bool_t bAsheHandled = AsheGameSim::HandleProjectileHit(
                    m_world,
                    tc,
                    projectile,
                    projectile.targetEntity,
                    request,
                    bEnqueueDamage);
                const bool_t bEzrealHandled = !bAsheHandled &&
                    EzrealGameSim::HandleProjectileHit(
                        m_world,
                        tc,
                        projectile,
                        projectile.targetEntity,
                        request,
                        bEnqueueDamage);
                if (!bAsheHandled && !bEzrealHandled)
                {
                    request = CServerProjectileAuthority::BuildSkillProjectileDamageRequest(
                        projectile,
                        projectile.targetEntity,
                        projectile.damageType);
                }
                if (bEnqueueDamage)
                    EnqueueDamageRequest(m_world, request);

                EnqueueProjectileContact(
                    projectile.targetEntity,
                    targetHitPos,
                    ProjectileContactReason::UnitHit,
                    true);
                m_world.DestroyEntity(entity);
                continue;
            }

            if (remaining <= 0.f ||
                dist <= std::numeric_limits<f32_t>::epsilon())
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    projectile.currentPos,
                    ProjectileContactReason::RangeExpired,
                    true);
                m_world.DestroyEntity(entity);
                continue;
            }

            if (CServerProjectileAuthority::IsMinionRangedProjectileKind(projectile.kind) &&
                !GameplayStateQuery::IsAttackSegmentGateExemptTarget(
                    m_world, projectile.targetEntity) &&
                !SegmentWalkableXZ(projectile.currentPos, next, 0.f))
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    projectile.currentPos,
                    ProjectileContactReason::Terrain,
                    true);
                m_world.DestroyEntity(entity);
                continue;
            }
            projectile.currentPos = next;
            projectile.traveledDistance += actualStep;
            transform.SetPosition(next);

            if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    next,
                    ProjectileContactReason::RangeExpired,
                    true);
                m_world.DestroyEntity(entity);
            }
            continue;
        }

        const Vec3 start = projectile.currentPos;
        const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
        if (remaining <= 0.f)
        {
            EnqueueProjectileContact(
                NULL_ENTITY,
                start,
                ProjectileContactReason::RangeExpired,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }
        const f32_t step = (std::min)(projectile.speed * tc.fDt, remaining);
        Vec3 end{
            start.x + projectile.direction.x * step,
            start.y + projectile.direction.y * step,
            start.z + projectile.direction.z * step
        };
        bool_t bBlockedByNavigation = false;
        if (projectile.bCollidesWithTerrain)
        {
            Vec3 guardedEnd = end;
            if (!TryClampMoveSegmentXZ(start, end, 0.f, guardedEnd))
            {
                guardedEnd = start;
                bBlockedByNavigation = true;
            }
            else if (WintersMath::DistanceSqXZ(guardedEnd, end) > 0.0001f)
            {
                bBlockedByNavigation = true;
            }
            end = guardedEnd;
        }

        bool_t bProjectileDestroyed = false;
        for (;;)
        {
            Vec3 hitPos = end;
            f32_t targetHitT = 1.f;
            const EntityID target =
                CServerProjectileAuthority::FindSkillProjectileHitTarget(
                    m_world,
                    projectile,
                    tc.pLagCompensation,
                    tc.tickIndex,
                    start,
                    end,
                    hitPos,
                    targetHitT);
            Vec3 barrierHitPos = end;
            f32_t barrierHitT = 1.f;
            const bool_t bBarrierHit =
                CServerProjectileAuthority::FindProjectileBarrierHit(
                    m_world,
                    projectile,
                    start,
                    end,
                    barrierHitPos,
                    barrierHitT);
            const bool_t bBarrierFirst = bBarrierHit &&
                (target == NULL_ENTITY ||
                    CServerProjectileAuthority::QuantizeContactT(barrierHitT) <=
                    CServerProjectileAuthority::QuantizeContactT(targetHitT));
            if (bBarrierFirst)
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    barrierHitPos,
                    ProjectileContactReason::Barrier,
                    true);
                LogSkillProjectileEvent(
                    "barrier",
                    entity,
                    projectile,
                    NULL_ENTITY,
                    barrierHitPos);
                m_world.DestroyEntity(entity);
                bProjectileDestroyed = true;
                break;
            }
            if (target == NULL_ENTITY)
                break;

            const bool_t bPiercingProjectile =
                projectile.unitHitPolicy == eProjectileUnitHitPolicy::Pierce ||
                projectile.kind == eProjectileKind::Tornado;
            const u16_t maxUniqueHits = (std::min)(
                projectile.kind == eProjectileKind::Tornado ||
                    projectile.maxUniqueHits == 0u
                    ? kMaxPiercingProjectileHits
                    : projectile.maxUniqueHits,
                kMaxPiercingProjectileHits);
            if (bPiercingProjectile &&
                projectile.hitEntityCount >= maxUniqueHits)
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    hitPos,
                    ProjectileContactReason::HitLimit,
                    true);
                m_world.DestroyEntity(entity);
                bProjectileDestroyed = true;
                break;
            }

            const bool_t bApplyGameplayHit =
                projectile.kind != eProjectileKind::AsheVolleyArrow ||
                AsheGameSim::TryRegisterVolleyHit(
                    m_world,
                    projectile.sharedHitLedgerEntity,
                    target);

            if (bApplyGameplayHit && projectile.kind == eProjectileKind::Wind)
            {
                if (projectile.hitEntityCount == 0u)
                {
                    YasuoGameSim::RegisterQHit(
                        m_world,
                        tc,
                        projectile.sourceEntity,
                        projectile.kind);
                }
            }
            if (bApplyGameplayHit && bPiercingProjectile)
            {
                projectile.hitEntities[projectile.hitEntityCount++] =
                    m_world.GetEntityHandle(target);
                if (projectile.kind == eProjectileKind::Tornado)
                {
                    YasuoGameSim::ApplyTornadoAirborne(
                        m_world,
                        tc,
                        projectile.sourceEntity,
                        target);
                }
            }
            if (bApplyGameplayHit && projectile.kind == eProjectileKind::LeeSinQ)
                LeeSinGameSim::ApplySonicWaveMark(m_world, tc, projectile.sourceEntity, target);
            if (bApplyGameplayHit && projectile.kind == eProjectileKind::SylasChain)
                SylasGameSim::ApplyChainHit(m_world, tc, projectile.sourceEntity, target);
            // 칼리스타 Q(비타겟 투사체) 적중 = 서버 권위 Rend 스택 +1.
            if (bApplyGameplayHit &&
                (projectile.kind == eProjectileKind::KalistaBasicAttack ||
                 projectile.kind == eProjectileKind::KalistaPierce))
            {
                KalistaGameSim::ApplyRendStackOnHit(
                    m_world, tc, projectile.sourceEntity, target);
            }
            if (bApplyGameplayHit && projectile.bApplyOnHitStatus)
                GameplayStatus::ApplyStatusEffect(m_world, target, projectile.onHitStatus, tc);

            if (bApplyGameplayHit)
            {
                DamageRequest request{};
                bool_t bEnqueueDamage = projectile.bApplyDamageOnHit;
                const bool_t bAsheHandled = AsheGameSim::HandleProjectileHit(
                    m_world,
                    tc,
                    projectile,
                    target,
                    request,
                    bEnqueueDamage);
                const bool_t bEzrealHandled = !bAsheHandled &&
                    EzrealGameSim::HandleProjectileHit(
                        m_world,
                        tc,
                        projectile,
                        target,
                        request,
                        bEnqueueDamage);
                if (!bAsheHandled && !bEzrealHandled)
                {
                    request = CServerProjectileAuthority::BuildSkillProjectileDamageRequest(
                        projectile,
                        target,
                        projectile.kind == eProjectileKind::SylasChain
                            ? eDamageType::Magic
                            : projectile.damageType);
                    if ((projectile.kind == eProjectileKind::Wind ||
                            projectile.kind == eProjectileKind::Tornado) &&
                        projectile.hitEntityCount > 1u)
                    {
                        request.flags &= ~DamageFlag_OnHit;
                    }
                }
                if (bEnqueueDamage)
                    EnqueueDamageRequest(m_world, request);
            }

            EnqueueProjectileContact(
                target,
                hitPos,
                ProjectileContactReason::UnitHit,
                !bPiercingProjectile);
            LogSkillProjectileEvent("hit", entity, projectile, target, hitPos);

            if (!bPiercingProjectile)
            {
                m_world.DestroyEntity(entity);
                bProjectileDestroyed = true;
                break;
            }
        }

        if (bProjectileDestroyed)
            continue;

        projectile.currentPos = end;
        projectile.traveledDistance +=
            std::sqrt(WintersMath::DistanceSqXZ(start, end));
        transform.SetPosition(end);

        if (bBlockedByNavigation ||
            projectile.traveledDistance >= projectile.maxDistance - 0.001f)
        {
            EnqueueProjectileContact(
                NULL_ENTITY,
                end,
                bBlockedByNavigation
                    ? ProjectileContactReason::Terrain
                    : ProjectileContactReason::RangeExpired,
                true);
            LogSkillProjectileEvent("expire", entity, projectile, NULL_ENTITY, end);
            m_world.DestroyEntity(entity);
        }
    }
}
