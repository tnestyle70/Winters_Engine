#include "WintersPCH.h"
#include "ECS/SpatialIndex.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

void CSpatialIndex::Initialize(const SpatialGridDesc& desc)
{
    m_Desc = desc;
    m_mapCells.clear();
    m_uTotalEntities = 0;
}

void CSpatialIndex::Rebuild(CWorld& world)
{
    WINTERS_PROFILE_SCOPE("SpatialIndex::Rebuild");

    m_mapCells.clear();
    m_uTotalEntities = 0;

    world.ForEach<TransformComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, SpatialAgentComponent&)>(
            [&](EntityID id, TransformComponent& xf, SpatialAgentComponent& agent)
            {
                if (agent.kind == eSpatialKind::None)
                    return;

                const Vec3 pos = xf.GetPosition();
                const i32_t cx = CellX(pos.x);
                const i32_t cz = CellZ(pos.z);

                if (std::abs(cx) > m_Desc.halfExtentX ||
                    std::abs(cz) > m_Desc.halfExtentZ)
                    return;

                agent.cachedCellX = cx;
                agent.cachedCellZ = cz;

                CellEntry entry{};
                entry.id = id;
                entry.pos = pos;
                entry.kind = agent.kind;
                entry.team = agent.team;
                entry.radius = agent.radius;

                m_mapCells[MakeKey(cx, cz)].push_back(entry);
                ++m_uTotalEntities;
            }));

    WINTERS_PROFILE_COUNT("SpatialIndex::TotalEntities",
        static_cast<i32_t>(m_uTotalEntities));
    WINTERS_PROFILE_COUNT("SpatialIndex::OccupiedCells",
        static_cast<i32_t>(m_mapCells.size()));
}

void CSpatialIndex::QueryRadius(const Vec3& center, f32_t radius,
    u32_t kindMask, u32_t excludeTeamMask,
    std::vector<EntityID>& out) const
{
    WINTERS_PROFILE_SCOPE("SpatialIndex::QueryRadius");

    const i32_t cx = CellX(center.x);
    const i32_t cz = CellZ(center.z);
    const i32_t cellRadius = static_cast<i32_t>(std::ceil(radius / m_Desc.cellSize)) + 1;
    const f32_t radiusSq = radius * radius;

    for (i32_t dz = -cellRadius; dz <= cellRadius; ++dz)
    {
        for (i32_t dx = -cellRadius; dx <= cellRadius; ++dx)
        {
            const i32_t qx = cx + dx;
            const i32_t qz = cz + dz;
            if (std::abs(qx) > m_Desc.halfExtentX ||
                std::abs(qz) > m_Desc.halfExtentZ)
                continue;

            const auto it = m_mapCells.find(MakeKey(qx, qz));
            if (it == m_mapCells.end())
                continue;

            for (const CellEntry& entry : it->second)
            {
                if ((static_cast<u32_t>(entry.kind) & kindMask) == 0u)
                    continue;
                if (((1u << entry.team) & excludeTeamMask) != 0u)
                    continue;

                const f32_t ex = entry.pos.x - center.x;
                const f32_t ez = entry.pos.z - center.z;
                const f32_t expandedRadius = radius + entry.radius;
                if (ex * ex + ez * ez <= std::max(radiusSq, expandedRadius * expandedRadius))
                    out.push_back(entry.id);
            }
        }
    }
}

EntityID CSpatialIndex::QueryClosest(const Vec3& center, f32_t maxRadius,
    u32_t kindMask, u8_t myTeam) const
{
    WINTERS_PROFILE_SCOPE("SpatialIndex::QueryClosest");

    const i32_t cx = CellX(center.x);
    const i32_t cz = CellZ(center.z);
    const i32_t cellRadius = static_cast<i32_t>(std::ceil(maxRadius / m_Desc.cellSize));
    const f32_t maxRadiusSq = maxRadius * maxRadius;

    EntityID best = NULL_ENTITY;
    f32_t bestDistSq = maxRadiusSq;

    for (i32_t dz = -cellRadius; dz <= cellRadius; ++dz)
    {
        for (i32_t dx = -cellRadius; dx <= cellRadius; ++dx)
        {
            const i32_t qx = cx + dx;
            const i32_t qz = cz + dz;
            if (std::abs(qx) > m_Desc.halfExtentX ||
                std::abs(qz) > m_Desc.halfExtentZ)
                continue;

            const auto it = m_mapCells.find(MakeKey(qx, qz));
            if (it == m_mapCells.end())
                continue;

            for (const CellEntry& entry : it->second)
            {
                if ((static_cast<u32_t>(entry.kind) & kindMask) == 0u)
                    continue;
                if (entry.team == myTeam)
                    continue;

                const f32_t ex = entry.pos.x - center.x;
                const f32_t ez = entry.pos.z - center.z;
                const f32_t distSq = ex * ex + ez * ez;
                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    best = entry.id;
                }
            }
        }
    }

    return best;
}

CSpatialIndex::DebugStats CSpatialIndex::GetDebugStats() const
{
    DebugStats stats{};
    stats.totalEntities = m_uTotalEntities;
    stats.occupiedCells = static_cast<u32_t>(m_mapCells.size());
    for (const auto& kv : m_mapCells)
    {
        stats.maxEntitiesInCell = std::max(stats.maxEntitiesInCell,
            static_cast<u32_t>(kv.second.size()));
    }
    return stats;
}

void CSpatialIndex::GetOccupiedCellCenters(std::vector<Vec3>& out) const
{
    out.clear();
    out.reserve(m_mapCells.size());

    for (const auto& kv : m_mapCells)
    {
        const i32_t cx = static_cast<i32_t>(kv.first >> 32);
        const i32_t cz = static_cast<i32_t>(static_cast<u32_t>(kv.first & 0xFFFFFFFFull));
        out.push_back({
            (static_cast<f32_t>(cx) + 0.5f) * m_Desc.cellSize + m_Desc.worldOrigin.x,
            0.f,
            (static_cast<f32_t>(cz) + 0.5f) * m_Desc.cellSize + m_Desc.worldOrigin.z
        });
    }
}

i32_t CSpatialIndex::CellX(f32_t worldX) const
{
    return static_cast<i32_t>(std::floor((worldX - m_Desc.worldOrigin.x) / m_Desc.cellSize));
}

i32_t CSpatialIndex::CellZ(f32_t worldZ) const
{
    return static_cast<i32_t>(std::floor((worldZ - m_Desc.worldOrigin.z) / m_Desc.cellSize));
}

i64_t CSpatialIndex::MakeKey(i32_t cx, i32_t cz) const
{
    return (static_cast<i64_t>(cx) << 32) | static_cast<u32_t>(cz);
}
