#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"

#include "Shared/GameSim/Core/Ecs/CoreComponents.h"
#include "Shared/GameSim/Core/Ecs/SpatialAgentComponent.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/Ecs/VisionComponents.h"
#include "Shared/GameSim/Core/Ecs/SpatialIndex.h"
#include "Shared/GameSim/Core/World/World.h"
#include "ProfilerAPI.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

#include <cmath>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    u8_t TeamOf(eTeam team)
    {
        return static_cast<u8_t>(team);
    }

    bool_t TryGetTeam(CWorld& world, EntityID entity, u8_t& outTeam)
    {
        if (world.HasComponent<SpatialAgentComponent>(entity))
        {
            outTeam = world.GetComponent<SpatialAgentComponent>(entity).team;
            return true;
        }
        if (world.HasComponent<ChampionComponent>(entity))
        {
            outTeam = TeamOf(world.GetComponent<ChampionComponent>(entity).team);
            return true;
        }
        if (world.HasComponent<MinionComponent>(entity))
        {
            outTeam = TeamOf(world.GetComponent<MinionComponent>(entity).team);
            return true;
        }
        if (world.HasComponent<MinionStateComponent>(entity))
        {
            outTeam = TeamOf(world.GetComponent<MinionStateComponent>(entity).team);
            return true;
        }
        if (world.HasComponent<TurretComponent>(entity))
        {
            outTeam = TeamOf(world.GetComponent<TurretComponent>(entity).team);
            return true;
        }
        if (world.HasComponent<StructureComponent>(entity))
        {
            outTeam = TeamOf(world.GetComponent<StructureComponent>(entity).team);
            return true;
        }
        return false;
    }

    bool_t IsHealthDead(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<HealthComponent>(entity))
            return false;

        const HealthComponent& hp = world.GetComponent<HealthComponent>(entity);
        return hp.bIsDead || hp.fCurrent <= 0.f;
    }
}

namespace GameplayTurret
{
    void CTurretAISystem::Execute(CWorld& world, f32_t fTimeDelta)
    {
        WINTERS_PROFILE_SCOPE("TurretAI::Execute");

        ConsumeAggroNotifications(world);

        m_fActivationAccum += fTimeDelta;
        if (m_fActivationAccum >= ACTIVATION_INTERVAL)
        {
            m_fActivationAccum = 0.f;
            UpdateActivation(world);
        }

        TickTurrets(world, fTimeDelta);
    }

    void CTurretAISystem::ConsumeAggroNotifications(CWorld& world)
    {
        std::vector<TowerAggroNotifyComponent> vecNotifications;
        std::vector<EntityID> vecRemove;

        world.ForEach<TowerAggroNotifyComponent>(
            std::function<void(EntityID, TowerAggroNotifyComponent&)>(
                [&](EntityID id, TowerAggroNotifyComponent& notify)
                {
                    vecNotifications.push_back(notify);
                    vecRemove.push_back(id);
                }));

        for (const TowerAggroNotifyComponent& notify : vecNotifications)
            ApplyAggro(world, notify.attackerEntity, notify.victimEntity, notify.priorityDuration);

        for (EntityID id : vecRemove)
        {
            if (world.IsAlive(id) && world.HasComponent<TowerAggroNotifyComponent>(id))
                world.DestroyEntity(id);
        }
    }

    void CTurretAISystem::UpdateActivation(CWorld& world)
    {
        world.ForEach<TurretAIComponent, TurretComponent, StructureComponent>(
            std::function<void(EntityID, TurretAIComponent&, TurretComponent&, StructureComponent&)>(
                [&](EntityID id, TurretAIComponent& ai, TurretComponent&, StructureComponent& structure)
                {
                    bool_t bActive = true;

                    // 파괴된 포탑은 재활성/재타겟 대상이 아니다.
                    if (world.HasComponent<HealthComponent>(id))
                    {
                        const auto& selfHp = world.GetComponent<HealthComponent>(id);
                        if (selfHp.bIsDead || selfHp.fCurrent <= 0.f)
                            bActive = false;
                    }

                    const u32_t lane = structure.lane;
                    const u32_t tier = structure.tier;
                    const u8_t team = TeamOf(structure.team);

                    world.ForEach<StructureComponent, HealthComponent>(
                        std::function<void(EntityID, StructureComponent&, HealthComponent&)>(
                            [&](EntityID other, StructureComponent& prev, HealthComponent& hp)
                            {
                                if (other == id || hp.bIsDead || hp.fCurrent <= 0.f)
                                    return;
                                if (TeamOf(prev.team) != team || prev.lane != lane)
                                    return;
                                if (prev.tier < tier)
                                    bActive = false;
                            }));

                    ai.bActive = bActive;
                    if (bActive)
                    {
                        if (!world.HasComponent<TargetableTag>(id))
                            world.AddComponent<TargetableTag>(id);
                    }
                    else
                    {
                        ai.attackTargetId = NULL_ENTITY;
                        ai.aggroTargetId = NULL_ENTITY;
                        if (world.HasComponent<TargetableTag>(id))
                            world.RemoveComponent<TargetableTag>(id);
                    }
                }));
    }

    void CTurretAISystem::TickTurrets(CWorld& world, f32_t fTimeDelta)
    {
        std::vector<std::pair<EntityID, EntityID>> vecShots;

        world.ForEach<TurretAIComponent, TurretComponent, TransformComponent>(
            std::function<void(EntityID, TurretAIComponent&, TurretComponent&, TransformComponent&)>(
                [&](EntityID id, TurretAIComponent& ai, TurretComponent& turret, TransformComponent&)
                {
                    if (!ai.bActive)
                        return;

                    // 파괴된 포탑은 발사하지 않는다.
                    if (world.HasComponent<HealthComponent>(id))
                    {
                        const auto& selfHp = world.GetComponent<HealthComponent>(id);
                        if (selfHp.bIsDead || selfHp.fCurrent <= 0.f)
                            return;
                    }

                    if (ai.attackCooldown > 0.f)
                    {
                        ai.attackCooldown -= fTimeDelta;
                        if (ai.attackCooldown < 0.f)
                            ai.attackCooldown = 0.f;
                    }

                    if (ai.aggroLockTimer > 0.f)
                    {
                        ai.aggroLockTimer -= fTimeDelta;
                        if (ai.aggroLockTimer <= 0.f)
                        {
                            ai.aggroLockTimer = 0.f;
                            ai.aggroTargetId = NULL_ENTITY;
                        }
                    }

                    const u8_t turretTeam = TeamOf(turret.team);
                    if (ai.aggroTargetId != NULL_ENTITY &&
                        !IsValidTarget(world, ai.aggroTargetId, turretTeam))
                    {
                        ai.aggroTargetId = NULL_ENTITY;
                        ai.aggroLockTimer = 0.f;
                    }

                    EntityID target = ai.aggroTargetId;
                    if (target == NULL_ENTITY)
                        target = SelectTarget(world, id);

                    ai.attackTargetId = target;
                    turret.targetId = target;

                    if (target == NULL_ENTITY || ai.attackCooldown > 0.f)
                        return;

                    vecShots.push_back({ id, target });
                    ai.attackCooldown = ai.attackCooldownMax;
                }));

        for (const auto& shot : vecShots)
            SpawnProjectile(world, shot.first, shot.second);

        WINTERS_PROFILE_COUNT("TurretAI::Shots", static_cast<i32_t>(vecShots.size()));
    }

    void CTurretAISystem::ApplyAggro(CWorld& world, EntityID attacker, EntityID victim,
        f32_t priorityDuration)
    {
        if (attacker == NULL_ENTITY || victim == NULL_ENTITY)
            return;
        if (!world.IsAlive(attacker) || !world.IsAlive(victim))
            return;
        if (world.HasComponent<PracticeDummyTag>(victim))
            return;
        if (!world.HasComponent<ChampionComponent>(attacker) ||
            !world.HasComponent<ChampionComponent>(victim) ||
            !world.HasComponent<TransformComponent>(victim))
        {
            return;
        }

        const u8_t attackerTeam = TeamOf(world.GetComponent<ChampionComponent>(attacker).team);
        const u8_t victimTeam = TeamOf(world.GetComponent<ChampionComponent>(victim).team);
        if (attackerTeam == victimTeam)
            return;

        const Vec3 victimPos = world.GetComponent<TransformComponent>(victim).GetPosition();

        world.ForEach<TurretAIComponent, TurretComponent, TransformComponent>(
            std::function<void(EntityID, TurretAIComponent&, TurretComponent&, TransformComponent&)>(
                [&](EntityID, TurretAIComponent& ai, TurretComponent& turret, TransformComponent& xf)
                {
                    if (!ai.bActive || TeamOf(turret.team) != victimTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(xf.GetPosition(), victimPos) > ai.attackRange * ai.attackRange)
                        return;

                    ai.aggroTargetId = attacker;
                    ai.aggroLockTimer = priorityDuration;
                }));
    }

    EntityID CTurretAISystem::SelectTarget(CWorld& world, EntityID turretEntity) const
    {
        if (!world.HasComponent<TurretAIComponent>(turretEntity) ||
            !world.HasComponent<TurretComponent>(turretEntity) ||
            !world.HasComponent<TransformComponent>(turretEntity))
        {
            return NULL_ENTITY;
        }

        const TurretAIComponent& ai = world.GetComponent<TurretAIComponent>(turretEntity);
        const TurretComponent& turret = world.GetComponent<TurretComponent>(turretEntity);
        const Vec3 pos = world.GetComponent<TransformComponent>(turretEntity).GetPosition();
        const u8_t turretTeam = TeamOf(turret.team);

        std::vector<EntityID> candidates;
        if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
        {
            const u32_t mask = SpatialMask(eSpatialKind::Character)
                | SpatialMask(eSpatialKind::Unit);
            pSpatial->QueryRadius(pos, ai.attackRange, mask, (1u << turretTeam), candidates);
        }

        EntityID best = NULL_ENTITY;
        i32_t bestPriority = std::numeric_limits<i32_t>::max();
        f32_t bestDistSq = ai.attackRange * ai.attackRange;

        for (EntityID candidate : candidates)
        {
            if (!IsValidTarget(world, candidate, turretTeam))
                continue;

            const i32_t priority = TargetPriority(world, candidate);
            if (priority >= 1000)
                continue;

            const Vec3 targetPos = world.GetComponent<TransformComponent>(candidate).GetPosition();
            const f32_t distSq = WintersMath::DistanceSqXZ(pos, targetPos);
            if (priority < bestPriority || (priority == bestPriority && distSq < bestDistSq))
            {
                bestPriority = priority;
                bestDistSq = distSq;
                best = candidate;
            }
        }

        return best;
    }

    void CTurretAISystem::SpawnProjectile(CWorld& world, EntityID turretEntity,
        EntityID targetEntity) const
    {
        if (!world.IsAlive(turretEntity) || !world.IsAlive(targetEntity))
            return;
        if (!world.HasComponent<TurretAIComponent>(turretEntity) ||
            !world.HasComponent<TurretComponent>(turretEntity) ||
            !world.HasComponent<TransformComponent>(turretEntity))
        {
            return;
        }

        const TurretAIComponent& ai = world.GetComponent<TurretAIComponent>(turretEntity);
        const Vec3 turretPos = world.GetComponent<TransformComponent>(turretEntity).GetPosition();

        const EntityID projectile = world.CreateEntity();

        TransformComponent xf{};
        xf.SetPosition({ turretPos.x, turretPos.y + 4.0f, turretPos.z });
        world.AddComponent<TransformComponent>(projectile, xf);

        StructureProjectileComponent pc{};
        pc.sourceEntity = turretEntity;
        pc.targetEntity = targetEntity;
        pc.sourceHandle = world.GetEntityHandle(turretEntity);
        pc.targetHandle = world.GetEntityHandle(targetEntity);
        pc.currentPos = xf.GetPosition();
        pc.speed = ai.projectileSpeed;
        pc.damage = ai.attackDamage;
        world.AddComponent<StructureProjectileComponent>(projectile, pc);

        SpatialAgentComponent agent{};
        agent.kind = eSpatialKind::Projectile;
        agent.team = TeamOf(world.GetComponent<TurretComponent>(turretEntity).team);
        agent.radius = 0.2f;
        world.AddComponent<SpatialAgentComponent>(projectile, agent);

        ColliderComponent collider{};
        collider.vHalfExtents = { agent.radius, agent.radius, agent.radius };
        collider.bIsTrigger = true;
        world.AddComponent<ColliderComponent>(projectile, collider);

        world.AddComponent<VisibilityComponent>(projectile);
    }

    bool_t CTurretAISystem::IsValidTarget(CWorld& world, EntityID entity, u8_t turretTeam) const
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;
        if (!world.HasComponent<TransformComponent>(entity))
            return false;
        if (IsHealthDead(world, entity))
            return false;
        if (world.HasComponent<PracticeDummyTag>(entity))
            return false;

        u8_t targetTeam = 0;
        if (!TryGetTeam(world, entity, targetTeam) || targetTeam == turretTeam)
            return false;

        if (world.HasComponent<VisibilityComponent>(entity))
        {
            const VisibilityComponent& vis = world.GetComponent<VisibilityComponent>(entity);
            if ((vis.teamVisibilityMask & static_cast<u8_t>(1u << turretTeam)) == 0)
                return false;
        }

        if (world.HasComponent<TargetableTag>(entity))
            return true;

        return world.HasComponent<ChampionComponent>(entity)
            || world.HasComponent<MinionComponent>(entity)
            || world.HasComponent<MinionStateComponent>(entity);
    }

    i32_t CTurretAISystem::TargetPriority(CWorld& world, EntityID entity) const
    {
        if (world.HasComponent<MinionComponent>(entity))
        {
            const MinionComponent& minion = world.GetComponent<MinionComponent>(entity);
            switch (minion.roleType)
            {
            case 3: return 0;
            case 2: return 1;
            case 0: return 2;
            case 1: return 3;
            default: return 4;
            }
        }
        if (world.HasComponent<MinionStateComponent>(entity))
        {
            const MinionStateComponent& minion = world.GetComponent<MinionStateComponent>(entity);
            switch (minion.type)
            {
            case 3: return 0;
            case 2: return 1;
            case 0: return 2;
            case 1: return 3;
            default: return 4;
            }
        }
        if (world.HasComponent<JungleComponent>(entity))
            return 5;
        if (world.HasComponent<ChampionComponent>(entity))
            return 10;
        return 1000;
    }
}
