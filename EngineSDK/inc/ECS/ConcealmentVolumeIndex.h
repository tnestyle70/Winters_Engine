#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

#include <vector>

class CWorld;

#pragma warning(push)
#pragma warning(disable: 4251)

class WINTERS_ENGINE CConcealmentVolumeIndex
{
public:
    CConcealmentVolumeIndex() = default;
    ~CConcealmentVolumeIndex() = default;

    CConcealmentVolumeIndex(const CConcealmentVolumeIndex&) = delete;
    CConcealmentVolumeIndex& operator=(const CConcealmentVolumeIndex&) = delete;

    void Build(CWorld& world);
    EntityID QueryVolumeAt(const Vec3& pos) const;
    void Clear() { m_vecVolumes.clear(); }

private:
    struct VolumeEntry
    {
        EntityID ID = NULL_ENTITY;
        Vec3 center{};
        f32_t radius = 0.f;
        u32_t volumeId = 0;
    };

    std::vector<VolumeEntry> m_vecVolumes{};
};

#pragma warning(pop)
