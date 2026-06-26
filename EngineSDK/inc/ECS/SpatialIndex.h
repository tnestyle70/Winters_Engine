#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/Components/SpatialAgentComponent.h"

#include <unordered_map>
#include <vector>

class CWorld;

struct SpatialGridDesc
{
    Vec3 worldOrigin = { 0.f, 0.f, 0.f };
    f32_t cellSize = 8.f;
    i32_t halfExtentX = 32;
    i32_t halfExtentZ = 32;
};

inline SpatialGridDesc DefaultSpatialGridDesc()
{
    return SpatialGridDesc{
        { 0.f, 0.f, 0.f },
        8.f,
        32,
        32
    };
}

#pragma warning(push)
#pragma warning(disable: 4251)

class WINTERS_ENGINE CSpatialIndex
{
public:
    struct DebugStats
    {
        u32_t totalEntities = 0;
        u32_t occupiedCells = 0;
        u32_t maxEntitiesInCell = 0;
    };

    CSpatialIndex() = default;
    ~CSpatialIndex() = default;

    CSpatialIndex(const CSpatialIndex&) = delete;
    CSpatialIndex& operator=(const CSpatialIndex&) = delete;

    void Initialize(const SpatialGridDesc& desc);
    const SpatialGridDesc& GetDesc() const { return m_Desc; }

    void Rebuild(CWorld& world);
    void QueryRadius(const Vec3& center, f32_t radius,
        u32_t kindMask, u32_t excludeTeamMask,
        std::vector<EntityID>& out) const;
    EntityID QueryClosest(const Vec3& center, f32_t maxRadius,
        u32_t kindMask, u8_t myTeam) const;

    DebugStats GetDebugStats() const;
    void GetOccupiedCellCenters(std::vector<Vec3>& out) const;

private:
    struct CellEntry
    {
        EntityID id = NULL_ENTITY;
        Vec3 pos{};
        eSpatialKind kind = eSpatialKind::None;
        u8_t team = 0;
        f32_t radius = 0.5f;
    };

    i32_t CellX(f32_t worldX) const;
    i32_t CellZ(f32_t worldZ) const;
    i64_t MakeKey(i32_t cx, i32_t cz) const;

    SpatialGridDesc m_Desc{};
    std::unordered_map<i64_t, std::vector<CellEntry>> m_mapCells{};
    u32_t m_uTotalEntities = 0;
};

#pragma warning(pop)
