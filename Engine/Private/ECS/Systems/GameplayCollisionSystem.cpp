#include "WintersPCH.h"
#include "ECS/Systems/GameplayCollisionSystem.h"

#include "ECS/SystemAccess.h"
#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ProfilerAPI.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

NS_BEGIN(Engine)

namespace
{
    struct CollisionBody
    {
        EntityID id = NULL_ENTITY;
        Vec3 pos{};
        f32_t radius = 0.5f;
        f32_t moveWeight = 1.f;
    };

    bool_t IsStaticBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::Turret
            || kind == eSpatialKind::Inhibitor
            || kind == eSpatialKind::Nexus
            || kind == eSpatialKind::JungleMob;
    }

    bool_t IsSolidBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::Champion
            || kind == eSpatialKind::Minion
            || IsStaticBlockingKind(kind);
    }

    bool_t IsDead(CWorld& world, EntityID id)
    {
        if (world.HasComponent<HealthComponent>(id))
        {
            const HealthComponent& hp = world.GetComponent<HealthComponent>(id);
            if (hp.bIsDead || hp.fCurrent <= 0.f)
                return true;
        }

        if (world.HasComponent<MinionStateComponent>(id))
        {
            const MinionStateComponent& ms = world.GetComponent<MinionStateComponent>(id);
            if (ms.current == MinionStateComponent::Dead)
                return true;
        }

        return false;
    }

    eSpatialKind ResolveBodyKind(CWorld& world, EntityID id, f32_t& radius)
    {
        if (world.HasComponent<SpatialAgentComponent>(id))
        {
            const SpatialAgentComponent& spatial = world.GetComponent<SpatialAgentComponent>(id);
            radius = std::max(radius, spatial.radius);
            return spatial.kind;
        }

        if (world.HasComponent<ChampionComponent>(id))
            return eSpatialKind::Champion;
        if (world.HasComponent<MinionComponent>(id) || world.HasComponent<MinionStateComponent>(id))
            return eSpatialKind::Minion;
        if (world.HasComponent<JungleComponent>(id))
            return eSpatialKind::JungleMob;
        if (world.HasComponent<TurretComponent>(id))
            return eSpatialKind::Turret;
        if (world.HasComponent<NexusTag>(id))
            return eSpatialKind::Nexus;
        if (world.HasComponent<InhibitorTag>(id))
            return eSpatialKind::Inhibitor;

        return eSpatialKind::None;
    }

    Vec3 ExactOverlapDir(EntityID a, EntityID b)
    {
        const u32_t seed = (a * 73856093u) ^ (b * 19349663u);
        const f32_t angle =
            static_cast<f32_t>(seed & 1023u) * (WintersMath::kTwoPi / 1024.f);
        return {
            static_cast<f32_t>(std::cos(angle)),
            0.f,
            static_cast<f32_t>(std::sin(angle))
        };
    }

    i32_t CollisionCellCoord(f32_t v, f32_t cellSize)
    {
        return static_cast<i32_t>(std::floor(v / cellSize));
    }

    i64_t CollisionCellKey(i32_t cx, i32_t cz)
    {
        return (static_cast<i64_t>(cx) << 32) | static_cast<u32_t>(cz);
    }

    void ResolveCollisionPair(CollisionBody& a, CollisionBody& b,
        f32_t pushStrength, u64_t& pairCount, u64_t& resolvedCount)
    {
        if (a.moveWeight <= 0.f && b.moveWeight <= 0.f)
            return;

        ++pairCount;

        f32_t dx = a.pos.x - b.pos.x;
        f32_t dz = a.pos.z - b.pos.z;
        const f32_t distSq = dx * dx + dz * dz;
        const f32_t minDist = a.radius + b.radius;
        if (distSq >= minDist * minDist)
            return;

        f32_t nx = 0.f;
        f32_t nz = 0.f;
        f32_t dist = 0.f;
        if (distSq <= 0.000001f)
        {
            const Vec3 dir = ExactOverlapDir(a.id, b.id);
            nx = dir.x;
            nz = dir.z;
        }
        else
        {
            dist = std::sqrt(distSq);
            const f32_t invDist = 1.f / dist;
            nx = dx * invDist;
            nz = dz * invDist;
        }

        const f32_t overlap = (minDist - dist) * pushStrength;
        const f32_t totalWeight = a.moveWeight + b.moveWeight;
        if (totalWeight <= 0.f || overlap <= 0.f)
            return;

        const f32_t aPush = overlap * (a.moveWeight / totalWeight);
        const f32_t bPush = overlap * (b.moveWeight / totalWeight);
        a.pos.x += nx * aPush;
        a.pos.z += nz * aPush;
        b.pos.x -= nx * bPush;
        b.pos.z -= nz * bPush;
        ++resolvedCount;
    }
}

void CGameplayCollisionSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.Read<ColliderComponent>()
        .Read<SpatialAgentComponent>()
        .Read<HealthComponent>()
        .Read<MinionStateComponent>()
        .Write<TransformComponent>();
}

void CGameplayCollisionSystem::Execute(CWorld& world, f32_t /*fTimeDelta*/)
{
    // Gameplay bodies should steer around each other, never push another actor's transform.
    // This legacy resolver remains compiled for diagnostics, but the active path is disabled.
    return;

    if (!m_bEnabled || m_fPushStrength <= 0.f)
        return;

    WINTERS_PROFILE_SCOPE("GameplayCollision::Execute");

    std::vector<CollisionBody> bodies;
    bodies.reserve(256);
    f32_t fMaxRadius = 0.5f;

    world.ForEach<TransformComponent, ColliderComponent>(
        [&](EntityID id, TransformComponent& tf, ColliderComponent& collider)
        {
            if (collider.bIsTrigger)
                return;
            if (IsDead(world, id))
                return;

            const f32_t colliderRadius = std::max(collider.vHalfExtents.x, collider.vHalfExtents.z);
            f32_t radius = colliderRadius;
            const eSpatialKind kind = ResolveBodyKind(world, id, radius);
            if (!IsSolidBlockingKind(kind))
                return;

            CollisionBody body{};
            body.id = id;
            body.pos = tf.GetPosition();
            body.radius = radius;
            body.moveWeight = IsStaticBlockingKind(kind) ? 0.f : 1.f;
            fMaxRadius = std::max(fMaxRadius, radius);
            bodies.push_back(body);
        });

    if (bodies.size() < 2)
        return;

    u64_t pairCount = 0;
    u64_t resolvedCount = 0;
    u64_t cellCount = 0;
    u64_t maxCellOccupancy = 0;
    const i32_t iterations = std::max(1, m_iIterations);
    const f32_t pushStrength = m_fPushStrength;
    const f32_t cellSize = std::max(1.f, fMaxRadius * 2.f);
    const i32_t queryCellRadius = std::max(
        1,
        static_cast<i32_t>(std::ceil((fMaxRadius * 2.f) / cellSize)));

    for (i32_t iteration = 0; iteration < iterations; ++iteration)
    {
        std::unordered_map<i64_t, std::vector<u32_t>> cellMap;
        cellMap.reserve(bodies.size() * 2);

        for (u32_t i = 0; i < static_cast<u32_t>(bodies.size()); ++i)
        {
            const i32_t cx = CollisionCellCoord(bodies[i].pos.x, cellSize);
            const i32_t cz = CollisionCellCoord(bodies[i].pos.z, cellSize);
            auto& cell = cellMap[CollisionCellKey(cx, cz)];
            cell.push_back(i);
            maxCellOccupancy = std::max(maxCellOccupancy, static_cast<u64_t>(cell.size()));
        }

        cellCount += static_cast<u64_t>(cellMap.size());

        for (u32_t i = 0; i < static_cast<u32_t>(bodies.size()); ++i)
        {
            const i32_t cx = CollisionCellCoord(bodies[i].pos.x, cellSize);
            const i32_t cz = CollisionCellCoord(bodies[i].pos.z, cellSize);

            for (i32_t dz = -queryCellRadius; dz <= queryCellRadius; ++dz)
            for (i32_t dx = -queryCellRadius; dx <= queryCellRadius; ++dx)
            {
                const auto it = cellMap.find(CollisionCellKey(cx + dx, cz + dz));
                if (it == cellMap.end())
                    continue;

                for (const u32_t j : it->second)
                {
                    if (j <= i)
                        continue;
                    ResolveCollisionPair(
                        bodies[i],
                        bodies[j],
                        pushStrength,
                        pairCount,
                        resolvedCount);
                }
            }
        }
    }

    for (const CollisionBody& body : bodies)
    {
        if (body.moveWeight <= 0.f)
            continue;
        if (!world.IsAlive(body.id) || !world.HasComponent<TransformComponent>(body.id))
            continue;

        TransformComponent& tf = world.GetComponent<TransformComponent>(body.id);
        Vec3 pos = tf.GetPosition();
        pos.x = body.pos.x;
        pos.z = body.pos.z;
        tf.SetPosition(pos);
    }

    WINTERS_PROFILE_COUNT("GameplayCollision::Pairs", pairCount);
    WINTERS_PROFILE_COUNT("GameplayCollision::Resolved", resolvedCount);
    WINTERS_PROFILE_COUNT("GameplayCollision::Bodies", static_cast<u64_t>(bodies.size()));
    WINTERS_PROFILE_COUNT("GameplayCollision::Cells", cellCount);
    WINTERS_PROFILE_COUNT("GameplayCollision::MaxCellOccupancy", maxCellOccupancy);
}

NS_END
