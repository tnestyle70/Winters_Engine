#include "WintersPCH.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/BushVolumeIndex.h"
#include "ECS/SpatialIndex.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <cmath>

namespace
{
    Vec3 FowTexelToWorld(
        const Engine::CVisionSystem::FowProjection& Projection,
        u32_t TexelX,
        u32_t TexelZ,
        u32_t Dim)
    {
        //texel을 월드 좌표에 투영 시킨다
        const f32_t u = (static_cast<f32_t>(TexelX) + 0.5f) / static_cast<f32_t>(Dim);
        const f32_t v = (static_cast<f32_t>(TexelZ) + 0.5f) / static_cast<f32_t>(Dim);

        const f32_t ux = Projection.vWorldAtUv10.x - Projection.vWorldAtUv00.x;
        const f32_t uz = Projection.vWorldAtUv10.y - Projection.vWorldAtUv00.y;
        const f32_t vx = Projection.vWorldAtUv01.x - Projection.vWorldAtUv00.x;
        const f32_t vz = Projection.vWorldAtUv01.y - Projection.vWorldAtUv00.y;

        return Vec3{
            Projection.vWorldAtUv00.x + ux * u + vx * v,
            0.f,
            Projection.vWorldAtUv00.y + uz * u + vz * v
        };
    }

    f32_t ResolveFowTexelWorldSize(
        const Engine::CVisionSystem::FowProjection& Projection,
        u32_t Dim)
    {
        const f32_t ux = Projection.vWorldAtUv10.x - Projection.vWorldAtUv00.x;
        const f32_t uz = Projection.vWorldAtUv10.y - Projection.vWorldAtUv00.y;
        const f32_t vx = Projection.vWorldAtUv01.x - Projection.vWorldAtUv00.x;
        const f32_t vz = Projection.vWorldAtUv01.y - Projection.vWorldAtUv00.y;

        const f32_t uLen = std::sqrt(ux * ux + uz * uz);
        const f32_t vLen = std::sqrt(vx * vx + vz * vz);
        return (std::min)(uLen, vLen) / static_cast<f32_t>(Dim);
    }

    bool_t WorldToFowUv(
        const Engine::CVisionSystem::FowProjection& Projection,
        const Vec3& vWorldPos,
        f32_t& fOutU,
        f32_t& fOutV)
    {
        const f32_t ux = Projection.vWorldAtUv10.x - Projection.vWorldAtUv00.x;
        const f32_t uz = Projection.vWorldAtUv10.y - Projection.vWorldAtUv00.y;
        const f32_t vx = Projection.vWorldAtUv01.x - Projection.vWorldAtUv00.x;
        const f32_t vz = Projection.vWorldAtUv01.y - Projection.vWorldAtUv00.y;
        const f32_t det = ux * vz - uz * vx;
        if (std::fabs(det) <= 0.0001f)
            return false;

        const f32_t wx = vWorldPos.x - Projection.vWorldAtUv00.x;
        const f32_t wz = vWorldPos.z - Projection.vWorldAtUv00.y;

        fOutU = (wx * vz - wz * vx) / det;
        fOutV = (ux * wz - uz * wx) / det;
        return true;
    }
}

NS_BEGIN(Engine)

std::unique_ptr<CVisionSystem> CVisionSystem::Create(CSpatialIndex* pIndex,
    CBushVolumeIndex* pBushIndex)
{
    std::unique_ptr<CVisionSystem> p(new CVisionSystem());
    p->m_pIndex = pIndex;
    p->m_pBushIndex = pBushIndex;
    p->m_vecFowTexture.assign(FOW_TEX_DIM * FOW_TEX_DIM, 0);
    p->m_vecDebugRecords.reserve(128);
    p->m_vecVisibilityCandidates.reserve(256);
    p->m_vecMinionVisionCells.reserve(128);
    return p;
}

void CVisionSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("Vision::Execute");

    m_fAccumDt += fTimeDelta;
    if (!m_bForceRebuild && m_fAccumDt < TICK_INTERVAL)
        return;

    m_fAccumDt = 0.f;
    m_bForceRebuild = false;

    UpdateBushOccupancy(world);
    TickVisibility(world);
    UpdateFowTexture(world);
}

void CVisionSystem::SetFowProjection(const FowProjection& Projection)
{
    if (!Projection.IsValid())
        return;

    m_FowProjection = Projection;
    m_bForceRebuild = true;
    m_bFowTextureDirty = true;
}

void CVisionSystem::UpdateBushOccupancy(CWorld& world)
{
    if (!m_pBushIndex)
        return;

    world.ForEach<TransformComponent, VisibilityComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, VisibilityComponent&, SpatialAgentComponent&)>(
            [&](EntityID, TransformComponent& xf, VisibilityComponent& vis, SpatialAgentComponent&)
            {
                const EntityID prevBush = vis.bushId;
                const EntityID nowBush = m_pBushIndex->QueryBushAt(xf.GetPosition());

                vis.bInBush = (nowBush != NULL_ENTITY);
                vis.bushId = nowBush;

                if (prevBush != nowBush)
                    m_bForceRebuild = true;
            }));
}

void CVisionSystem::TickVisibility(CWorld& world)
{
    WINTERS_PROFILE_SCOPE("Vision::TickVisibility");

    m_vecDebugRecords.clear();
    m_vecMinionVisionCells.clear();
    u64_t sourceCount = 0;
    u64_t skippedSourceCount = 0;
    u64_t candidateCount = 0;
    u64_t visibleCount = 0;

    world.ForEach<VisibilityComponent>(
        function<void(EntityID, VisibilityComponent&)>(
            [](EntityID, VisibilityComponent& vis)
            {
                vis.teamVisibilityMask = 0;
            }));

    world.ForEach<VisibilityComponent, SpatialAgentComponent>(
        function<void(EntityID, VisibilityComponent&, SpatialAgentComponent&)>(
            [](EntityID, VisibilityComponent& vis, SpatialAgentComponent& agent)
            {
                vis.teamVisibilityMask |= static_cast<u8_t>(1u << agent.team);
            }));

    world.ForEach<TransformComponent, VisionSourceComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, VisionSourceComponent&, SpatialAgentComponent&)>(
            [&](EntityID srcId, TransformComponent& srcXf, VisionSourceComponent& vs, SpatialAgentComponent& srcAgent)
            {
                if (srcAgent.kind == eSpatialKind::Minion)
                {
                    const i64_t cellXKey =
                        static_cast<i64_t>(static_cast<u32_t>(srcAgent.cachedCellX) & 0xFFFFFu);
                    const i64_t cellZKey =
                        static_cast<i64_t>(static_cast<u32_t>(srcAgent.cachedCellZ) & 0xFFFFFu);
                    const i64_t cellKey =
                        (static_cast<i64_t>(srcAgent.team) << 40) |
                        (cellXKey << 20) |
                        cellZKey;
                    if (std::find(m_vecMinionVisionCells.begin(),
                        m_vecMinionVisionCells.end(), cellKey) != m_vecMinionVisionCells.end())
                    {
                        ++skippedSourceCount;
                        return;
                    }
                    m_vecMinionVisionCells.push_back(cellKey);
                }

                ++sourceCount;

                const Vec3 srcPos = srcXf.GetPosition();
                const f32_t sightRangeSq = vs.sightRange * vs.sightRange;
                const u32_t sourceTeamMask = 1u << srcAgent.team;
                const VisibilityComponent* pSourceVis = nullptr;
                if (world.HasComponent<VisibilityComponent>(srcId))
                    pSourceVis = &world.GetComponent<VisibilityComponent>(srcId);

                m_vecVisibilityCandidates.clear();
                if (m_pIndex)
                {
                    const u32_t mask = SpatialMask(eSpatialKind::Champion)
                        | SpatialMask(eSpatialKind::Minion)
                        | SpatialMask(eSpatialKind::Turret)
                        | SpatialMask(eSpatialKind::JungleMob)
                        | SpatialMask(eSpatialKind::Projectile)
                        | SpatialMask(eSpatialKind::Inhibitor)
                        | SpatialMask(eSpatialKind::Nexus)
                        | SpatialMask(eSpatialKind::Ward);
                    m_pIndex->QueryRadius(srcPos, vs.sightRange, mask,
                        sourceTeamMask, m_vecVisibilityCandidates);
                }

                candidateCount += m_vecVisibilityCandidates.size();
                for (EntityID target : m_vecVisibilityCandidates)
                {
                    if (target == srcId)
                        continue;
                    if (!world.HasComponent<VisibilityComponent>(target))
                        continue;
                    if (!world.HasComponent<TransformComponent>(target))
                        continue;

                    VisibilityComponent& targetVis = world.GetComponent<VisibilityComponent>(target);
                    const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
                    if (!IsTargetVisibleFast(pSourceVis, srcPos, targetVis, targetPos,
                        vs.bTrueSight, sightRangeSq))
                    {
                        continue;
                    }

                    targetVis.teamVisibilityMask |= static_cast<u8_t>(sourceTeamMask);
                    ++visibleCount;

#if defined(WINTERS_VISION_DEBUG_RECORDS)
                    const f32_t dx = targetPos.x - srcPos.x;
                    const f32_t dz = targetPos.z - srcPos.z;
                    m_vecDebugRecords.push_back({ srcId, target, std::sqrt(dx * dx + dz * dz) });
#endif
                }
            }));

    WINTERS_PROFILE_COUNT("Vision::Sources", sourceCount);
    WINTERS_PROFILE_COUNT("Vision::SkippedSources", skippedSourceCount);
    WINTERS_PROFILE_COUNT("Vision::Candidates", candidateCount);
    WINTERS_PROFILE_COUNT("Vision::Visible", visibleCount);
}

bool CVisionSystem::IsTargetVisible(CWorld& world, EntityID source, EntityID target,
    f32_t sightRange) const
{
    if (!world.HasComponent<TransformComponent>(source) ||
        !world.HasComponent<TransformComponent>(target))
        return false;

    const Vec3 srcPos = world.GetComponent<TransformComponent>(source).GetPosition();
    const Vec3 tgtPos = world.GetComponent<TransformComponent>(target).GetPosition();
    const f32_t dx = tgtPos.x - srcPos.x;
    const f32_t dz = tgtPos.z - srcPos.z;
    if (dx * dx + dz * dz > sightRange * sightRange)
        return false;

    if (world.HasComponent<VisibilityComponent>(target))
    {
        const VisibilityComponent& tgtVis = world.GetComponent<VisibilityComponent>(target);
        if (tgtVis.bInBush)
        {
            if (world.HasComponent<VisibilityComponent>(source))
            {
                const VisibilityComponent& srcVis = world.GetComponent<VisibilityComponent>(source);
                if (srcVis.bInBush && srcVis.bushId == tgtVis.bushId)
                    return true;
            }

            if (world.HasComponent<VisionSourceComponent>(source) &&
                world.GetComponent<VisionSourceComponent>(source).bTrueSight)
            {
                return true;
            }

            return false;
        }
    }

    return true;
}

bool CVisionSystem::IsTargetVisibleFast(const VisibilityComponent* pSourceVis,
    const Vec3& sourcePos, const VisibilityComponent& targetVis,
    const Vec3& targetPos, bool_t bSourceTrueSight,
    f32_t sightRangeSq) const
{
    const f32_t dx = targetPos.x - sourcePos.x;
    const f32_t dz = targetPos.z - sourcePos.z;
    if (dx * dx + dz * dz > sightRangeSq)
        return false;

    if (!targetVis.bInBush)
        return true;

    if (pSourceVis && pSourceVis->bInBush && pSourceVis->bushId == targetVis.bushId)
        return true;

    return bSourceTrueSight;
}

void CVisionSystem::UpdateFowTexture(CWorld& world)
{
    WINTERS_PROFILE_SCOPE("Vision::UpdateFow");

    u8_t localTeam = 0;
    bool_t bLocalFound = false;
    world.ForEach<LocalPlayerTag, SpatialAgentComponent>(
        function<void(EntityID, LocalPlayerTag&, SpatialAgentComponent&)>(
            [&](EntityID, LocalPlayerTag&, SpatialAgentComponent& agent)
            {
                localTeam = agent.team;
                bLocalFound = true;
            }));
    if (!bLocalFound)
        return;
    constexpr u8_t ExploredValue = 127;
    constexpr u8_t VisibleValue = 255;

    for (u8_t& value : m_vecFowTexture)
    {
        if (value > ExploredValue)
            value = ExploredValue;
    }

    const FowProjection Projection = m_FowProjection;
    if (!Projection.IsValid())
        return;

    const f32_t cellWorld = ResolveFowTexelWorldSize(Projection, FOW_TEX_DIM);
    if (cellWorld <= 0.f)
        return;

    world.ForEach<TransformComponent, VisionSourceComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, VisionSourceComponent&, SpatialAgentComponent&)>(
            [&](EntityID, TransformComponent& xf, VisionSourceComponent& vs, SpatialAgentComponent& agent)
            {
                if (agent.team != localTeam)
                    return;

                const Vec3 srcPos = xf.GetPosition();
                const f32_t r = vs.sightRange;
                if (r <= 0.f)
                    return;
                const f32_t r2 = r * r;
                const f32_t feather = std::clamp(r * 0.18f, cellWorld * 2.f, cellWorld * 7.f);
                const f32_t coreRadius = std::max(0.f, r - feather);

                f32_t srcU = 0.f;
                f32_t srcV = 0.f;
                if (!WorldToFowUv(Projection, srcPos, srcU, srcV))
                    return;

                const f32_t texelRadius = r / cellWorld;
                const i32_t minX = (std::max)(0,
                    static_cast<i32_t>(std::floor(srcU * FOW_TEX_DIM - texelRadius - 2.f)));
                const i32_t maxX = (std::min)(static_cast<i32_t>(FOW_TEX_DIM) - 1,
                    static_cast<i32_t>(std::ceil(srcU * FOW_TEX_DIM + texelRadius + 2.f)));
                const i32_t minZ = (std::max)(0,
                    static_cast<i32_t>(std::floor(srcV * FOW_TEX_DIM - texelRadius - 2.f)));
                const i32_t maxZ = (std::min)(static_cast<i32_t>(FOW_TEX_DIM) - 1,
                    static_cast<i32_t>(std::ceil(srcV * FOW_TEX_DIM + texelRadius + 2.f)));

                for (i32_t qz = minZ; qz <= maxZ; ++qz)
                {
                    for (i32_t qx = minX; qx <= maxX; ++qx)
                    {
                        const Vec3 samplePos = FowTexelToWorld(
                            Projection,
                            static_cast<u32_t>(qx),
                            static_cast<u32_t>(qz),
                            FOW_TEX_DIM);

                        const f32_t tx = samplePos.x - srcPos.x;
                        const f32_t tz = samplePos.z - srcPos.z;
                        const f32_t distSq = tx * tx + tz * tz;
                        if (distSq > r2)
                            continue;

                        u8_t visibleValue = VisibleValue;
                        const f32_t dist = std::sqrt(distSq);
                        if (dist > coreRadius)
                        {
                            f32_t t = (dist - coreRadius) / std::max(feather, 0.001f);
                            t = std::clamp(t, 0.f, 1.f);
                            t = t * t * (3.f - 2.f * t);
                            visibleValue = static_cast<u8_t>(
                                std::clamp(static_cast<f32_t>(ExploredValue) +
                                    (1.f - t) * static_cast<f32_t>(VisibleValue - ExploredValue),
                                    static_cast<f32_t>(ExploredValue),
                                    static_cast<f32_t>(VisibleValue)));
                        }

                        u8_t& dst = m_vecFowTexture[qz * FOW_TEX_DIM + qx];
                        dst = std::max(dst, visibleValue);
                    }
                }
            }));

    m_bFowTextureDirty = true;
}

NS_END
