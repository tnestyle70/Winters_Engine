#include "Game/GameRoom.h"

#include "Game/ServerProjectileAuthority.h"
#include "GameRoomInternal.h"

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

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/TurretAISystem.h"

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
        DeterministicEntityIterator<TurretProjectileComponent>::CollectSorted(m_world);

    for (EntityID entity : projectiles)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<TurretProjectileComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& projectile = m_world.GetComponent<TurretProjectileComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);
        const Vec3 pos = projectile.currentPos;
        const NetEntityId currentProjectileNet = m_entityMap.ToNet(entity);
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
                const ReplicatedEventComponent hit =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        NULL_ENTITY,
                        entity,
                        CServerProjectileAuthority::kTurretProjectileKind,
                        pos,
                        tc.tickIndex);
                EnqueueReplicatedEvent(m_world, hit);
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

            Vec3 dir{ 0.f, 0.f, 1.f };
            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
            dir = NormalizeXZOrForward(
                Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z },
                eTeam::Neutral);

            const ReplicatedEventComponent spawn =
                CServerProjectileAuthority::BuildProjectileSpawnEvent(
                    projectile.sourceEntity,
                    projectile.targetEntity,
                    entity,
                    CServerProjectileAuthority::kTurretProjectileKind,
                    pos,
                    dir,
                    projectile.speed,
                    48.f,
                    tc.tickIndex);
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

        const Vec3 targetPos = m_world.GetComponent<TransformComponent>(
            projectile.targetEntity).GetPosition();
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

            const ReplicatedEventComponent hit =
                CServerProjectileAuthority::BuildProjectileHitEvent(
                    projectile.sourceEntity,
                    projectile.targetEntity,
                    entity,
                    CServerProjectileAuthority::kTurretProjectileKind,
                    targetAim,
                    tc.tickIndex);
            EnqueueReplicatedEvent(m_world, hit);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t dist = std::sqrt(distSq);
        if (dist <= std::numeric_limits<f32_t>::epsilon())
        {
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t step = projectile.speed * tc.fDt;
        const f32_t t = (step >= dist) ? 1.f : (step / dist);
        Vec3 next{
            pos.x + delta.x * t,
            pos.y + delta.y * t,
            pos.z + delta.z * t
        };
        projectile.currentPos = next;
        transform.SetPosition(next);
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

        if (!projectile.bSpawned)
        {
            const NetEntityId projectileNet = m_entityMap.IssueNew(entity);
            NetEntityIdComponent net{};
            net.netId = projectileNet;
            if (!m_world.HasComponent<NetEntityIdComponent>(entity))
                m_world.AddComponent<NetEntityIdComponent>(entity, net);

            projectile.bSpawned = true;

            const ReplicatedEventComponent spawn =
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

        if (!IsAliveHealth(m_world, projectile.sourceEntity) ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f)
        {
            const ReplicatedEventComponent hit =
                CServerProjectileAuthority::BuildProjectileHitEvent(
                    projectile.sourceEntity,
                    NULL_ENTITY,
                    entity,
                    static_cast<u16_t>(projectile.kind),
                    projectile.currentPos,
                    tc.tickIndex);
            EnqueueReplicatedEvent(m_world, hit);
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
                const ReplicatedEventComponent hit =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        NULL_ENTITY,
                        entity,
                        static_cast<u16_t>(projectile.kind),
                        projectile.currentPos,
                        tc.tickIndex);
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
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
            const f32_t hitRadiusSq = projectile.hitRadius * projectile.hitRadius;

            if (distSq <= hitRadiusSq)
            {
                const DamageRequest request =
                    CServerProjectileAuthority::BuildSkillProjectileDamageRequest(
                        projectile,
                        projectile.targetEntity,
                        eDamageType::Physical);
                EnqueueDamageRequest(m_world, request);

                const ReplicatedEventComponent hit =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        projectile.targetEntity,
                        entity,
                        static_cast<u16_t>(projectile.kind),
                        targetAim,
                        tc.tickIndex);
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t dist = std::sqrt(distSq);
            if (dist <= std::numeric_limits<f32_t>::epsilon())
            {
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
            if (remaining <= 0.f)
            {
                const ReplicatedEventComponent hit =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        NULL_ENTITY,
                        entity,
                        static_cast<u16_t>(projectile.kind),
                        projectile.currentPos,
                        tc.tickIndex);
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
            const f32_t actualStep = (step >= dist) ? dist : step;
            const f32_t t = actualStep / dist;
            const Vec3 next{
                projectile.currentPos.x + delta.x * t,
                projectile.currentPos.y + delta.y * t,
                projectile.currentPos.z + delta.z * t
            };
            projectile.currentPos = next;
            projectile.traveledDistance += actualStep;
            transform.SetPosition(next);

            if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
            {
                const ReplicatedEventComponent hit =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        NULL_ENTITY,
                        entity,
                        static_cast<u16_t>(projectile.kind),
                        next,
                        tc.tickIndex);
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
            }
            continue;
        }

        const Vec3 start = projectile.currentPos;
        const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
        const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
        const Vec3 end{
            start.x + projectile.direction.x * step,
            start.y + projectile.direction.y * step,
            start.z + projectile.direction.z * step
        };

        Vec3 hitPos = end;
        const EntityID target = CServerProjectileAuthority::FindSkillProjectileHitTarget(
            m_world,
            projectile,
            start,
            end,
            hitPos);

        if (target != NULL_ENTITY)
        {
            if (projectile.kind == eProjectileKind::Tornado)
                YasuoGameSim::ApplyTornadoAirborne(m_world, tc, projectile.sourceEntity, target);
            if (projectile.kind == eProjectileKind::LeeSinQ)
                LeeSinGameSim::ApplySonicWaveMark(m_world, tc, projectile.sourceEntity, target);
            if (projectile.kind == eProjectileKind::SylasChain)
                SylasGameSim::ApplyChainHit(m_world, tc, projectile.sourceEntity, target);
            if (projectile.bApplyOnHitStatus)
                GameplayStatus::ApplyStatusEffect(m_world, target, projectile.onHitStatus, tc);

            const DamageRequest request =
                CServerProjectileAuthority::BuildSkillProjectileDamageRequest(
                    projectile,
                    target,
                    projectile.kind == eProjectileKind::SylasChain
                        ? eDamageType::Magic
                        : eDamageType::Physical);
            EnqueueDamageRequest(m_world, request);

            const ReplicatedEventComponent hit =
                CServerProjectileAuthority::BuildProjectileHitEvent(
                    projectile.sourceEntity,
                    target,
                    entity,
                    static_cast<u16_t>(projectile.kind),
                    hitPos,
                    tc.tickIndex);
            EnqueueReplicatedEvent(m_world, hit);
            LogSkillProjectileEvent("hit", entity, projectile, target, hitPos);
            m_world.DestroyEntity(entity);
            continue;
        }

        projectile.currentPos = end;
        projectile.traveledDistance += step;
        transform.SetPosition(end);

        if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
        {
            const ReplicatedEventComponent hit =
                CServerProjectileAuthority::BuildProjectileHitEvent(
                    projectile.sourceEntity,
                    NULL_ENTITY,
                    entity,
                    static_cast<u16_t>(projectile.kind),
                    end,
                    tc.tickIndex);
            EnqueueReplicatedEvent(m_world, hit);
            LogSkillProjectileEvent("expire", entity, projectile, NULL_ENTITY, end);
            m_world.DestroyEntity(entity);
        }
    }
}
