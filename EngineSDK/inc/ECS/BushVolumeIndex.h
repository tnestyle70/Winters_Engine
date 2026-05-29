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

class WINTERS_ENGINE CBushVolumeIndex
{
public:
    CBushVolumeIndex() = default;
    ~CBushVolumeIndex() = default;

    CBushVolumeIndex(const CBushVolumeIndex&) = delete;
    CBushVolumeIndex& operator=(const CBushVolumeIndex&) = delete;

    void Build(CWorld& world);
    EntityID QueryBushAt(const Vec3& pos) const;
    void Clear() { m_vecBushes.clear(); }

private:
    struct BushEntry
    {
        EntityID ID = NULL_ENTITY;
        Vec3 center{};
        f32_t radius = 0.f;
        u32_t bushId = 0;
    };

    std::vector<BushEntry> m_vecBushes{};
};

#pragma warning(pop)
