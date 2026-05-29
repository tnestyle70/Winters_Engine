#include "WintersPCH.h"
#include "ECS/Systems/MinionSeparationSystem.h"

#include "ECS/SystemAccess.h"
#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ProfilerAPI.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <vector>

NS_BEGIN(Engine)

namespace
{
    struct MinionSeparationSnap
    {
        EntityID id = NULL_ENTITY;
        Vec3 pos{};
        u8_t team = 0;
    };

    Vec3 ExactOverlapPush(EntityID self, EntityID other)
    {
        const u32_t seed = (self * 73856093u) ^ (other * 19349663u);
        const f32_t angle =
            static_cast<f32_t>(seed & 1023u) * (WintersMath::kTwoPi / 1024.f);
        return {
            static_cast<f32_t>(std::cos(angle)),
            0.f,
            static_cast<f32_t>(std::sin(angle))
        };
    }
}

void CMinionSeparationSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.Read<TransformComponent>()
        .Read<MinionStateComponent>()
        .Write<VelocityComponent>();
}

void CMinionSeparationSystem::Execute(CWorld& world, f32_t /*fTimeDelta*/)
{
    if (!m_bEnabled)
        return;
    if (m_fSeparationRadius <= 0.001f || m_fSeparationWeight <= 0.f)
        return;

    WINTERS_PROFILE_SCOPE("MinionSeparation::Execute");

    std::vector<MinionSeparationSnap> vecSnaps;
    vecSnaps.reserve(128);

    world.ForEach<MinionStateComponent, TransformComponent>(
        function<void(EntityID, MinionStateComponent&, TransformComponent&)>(
            [&](EntityID id, MinionStateComponent& ms, TransformComponent& tf)
            {
                if (ms.current == MinionStateComponent::Dead)
                    return;

                MinionSeparationSnap snap{};
                snap.id = id;
                snap.pos = tf.GetPosition();
                snap.team = static_cast<u8_t>(ms.team);
                vecSnaps.push_back(snap);
            }));

    if (vecSnaps.size() < 2)
        return;

    const f32_t radius = m_fSeparationRadius;
    const f32_t radiusSq = radius * radius;
    const f32_t weight = m_fSeparationWeight;
    const i32_t maxNeighbors = m_iMaxNeighbors;

    u64_t appliedCount = 0;

    world.ForEach<MinionStateComponent, VelocityComponent>(
        function<void(EntityID, MinionStateComponent&, VelocityComponent&)>(
            [&](EntityID id, MinionStateComponent& ms, VelocityComponent& vel)
            {
                if (ms.current == MinionStateComponent::Dead)
                    return;
                if (ms.current != MinionStateComponent::Chase)
                    return;
                if (vel.fSpeed <= 0.001f)
                    return;

                auto itSelf = std::find_if(
                    vecSnaps.begin(),
                    vecSnaps.end(),
                    [id](const MinionSeparationSnap& snap)
                    {
                        return snap.id == id;
                    });
                if (itSelf == vecSnaps.end())
                    return;

                Vec3 force{ 0.f, 0.f, 0.f };
                i32_t neighborCount = 0;

                for (const MinionSeparationSnap& other : vecSnaps)
                {
                    if (other.id == id)
                        continue;
                    if (other.team != itSelf->team)
                        continue;
                    if (neighborCount >= maxNeighbors)
                        break;

                    const f32_t dx = itSelf->pos.x - other.pos.x;
                    const f32_t dz = itSelf->pos.z - other.pos.z;
                    const f32_t distSq = dx * dx + dz * dz;
                    if (distSq >= radiusSq)
                        continue;

                    if (distSq < 0.0001f)
                    {
                        const Vec3 push = ExactOverlapPush(id, other.id);
                        force.x += push.x;
                        force.z += push.z;
                        ++neighborCount;
                        continue;
                    }

                    const f32_t dist = std::sqrt(distSq);
                    const f32_t invDist = 1.f / dist;
                    const f32_t strength = (radius - dist) / radius;
                    force.x += dx * invDist * strength;
                    force.z += dz * invDist * strength;
                    ++neighborCount;
                }

                if (neighborCount == 0)
                    return;

                force.x /= static_cast<f32_t>(neighborCount);
                force.z /= static_cast<f32_t>(neighborCount);

                vel.vDirection.x += force.x * weight;
                vel.vDirection.z += force.z * weight;

                const f32_t mag = std::sqrt(
                    vel.vDirection.x * vel.vDirection.x +
                    vel.vDirection.z * vel.vDirection.z);
                if (mag > 0.001f)
                {
                    vel.vDirection.x /= mag;
                    vel.vDirection.y = 0.f;
                    vel.vDirection.z /= mag;
                    ++appliedCount;
                }
            }));

    WINTERS_PROFILE_COUNT("MinionSeparation::Applied", appliedCount);
    WINTERS_PROFILE_COUNT("MinionSeparation::Snaps", static_cast<u64_t>(vecSnaps.size()));
}

NS_END
