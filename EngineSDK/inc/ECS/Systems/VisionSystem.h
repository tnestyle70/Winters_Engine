#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/ISystem.h"

#include <cmath>
#include <memory>
#include <vector>

class CWorld;
class CSpatialIndex;
class CConcealmentVolumeIndex;
struct VisibilityComponent;

NS_BEGIN(Engine)

#pragma warning(push)
#pragma warning(disable: 4251)

class WINTERS_ENGINE CVisionSystem final : public ISystem
{
public:
    static constexpr u32_t FOW_TEX_DIM = 256;
    static constexpr f32_t FOW_TEX_WORLD_SIZE = 280.f;

    struct FowProjection
    {
        Vec2 vWorldAtUv00{ -140.f, -140.f };
        Vec2 vWorldAtUv10{ 140.f, -140.f };
        Vec2 vWorldAtUv01{ -140.f, 140.f };

        bool_t IsValid() const
        {
            const f32_t ux = vWorldAtUv10.x - vWorldAtUv00.x;
            const f32_t uz = vWorldAtUv10.y - vWorldAtUv00.y;
            const f32_t vx = vWorldAtUv01.x - vWorldAtUv00.x;
            const f32_t vz = vWorldAtUv01.y - vWorldAtUv00.y;
            return std::fabs(ux * vz - uz * vx) > 0.0001f;
        }
    };

    struct VisRecord
    {
        EntityID source = NULL_ENTITY;
        EntityID target = NULL_ENTITY;
        f32_t distance = 0.f;
    };

    ~CVisionSystem() override = default;

    static std::unique_ptr<CVisionSystem> Create(CSpatialIndex* pIndex,
        CConcealmentVolumeIndex* pConcealmentIndex);

    u32_t GetPhase() const override { return 5; }
    const char* GetName() const override { return "VisionSystem"; }
    void Execute(CWorld& world, f32_t fTimeDelta) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;

    void ForceRebuildNextFrame() { m_bForceRebuild = true; }
    void SetFowProjection(const FowProjection& Projection);
    const FowProjection& GetFowProjection() const { return m_FowProjection; }
    void SetFowLocalTeam(u8_t iTeam);
    void ClearFowLocalTeam();

    const u8_t* GetFowTextureData() const { return m_vecFowTexture.data(); }
    u32_t GetFowTextureDim() const { return FOW_TEX_DIM; }
    bool_t IsFowTextureDirty() const { return m_bFowTextureDirty; }
    void ClearFowTextureDirty() { m_bFowTextureDirty = false; }

    const std::vector<VisRecord>& GetDebugRecords() const { return m_vecDebugRecords; }

private:
    CVisionSystem() = default;

    void TickVisibility(CWorld& world);
    void UpdateConcealmentOccupancy(CWorld& world);
    void UpdateFowTexture(CWorld& world);
    bool IsTargetVisible(CWorld& world, EntityID source, EntityID target,
        f32_t sightRange) const;
    bool IsTargetVisibleFast(const VisibilityComponent* pSourceVis,
        const Vec3& sourcePos, const VisibilityComponent& targetVis,
        const Vec3& targetPos, bool_t bSourceTrueSight,
        f32_t sightRangeSq) const;

    CSpatialIndex* m_pIndex = nullptr;
    CConcealmentVolumeIndex* m_pConcealmentIndex = nullptr;

    f32_t m_fAccumDt = 0.f;
    bool_t m_bForceRebuild = true;
    bool_t m_bFowTextureDirty = false;
    u8_t m_iFowLocalTeam = 255u;
    bool_t m_bHasFowLocalTeam = false;
    FowProjection m_FowProjection{};

    std::vector<u8_t> m_vecFowTexture{};
    std::vector<VisRecord> m_vecDebugRecords{};
    std::vector<EntityID> m_vecVisibilityCandidates{};
    std::vector<i64_t> m_vecUnitVisionCells{};

    static constexpr f32_t TICK_INTERVAL = 0.1f;
};

#pragma warning(pop)

NS_END
