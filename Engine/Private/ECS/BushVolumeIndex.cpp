#include "WintersPCH.h"
#include "ECS/BushVolumeIndex.h"
#include "ECS/World.h"
#include "ECS/Components/VisionComponents.h"

void CBushVolumeIndex::Build(CWorld& world)
{
    m_vecBushes.clear();
    world.ForEach<BushVolumeComponent>(
        function<void(EntityID, BushVolumeComponent&)>(
            [&](EntityID ID, BushVolumeComponent& bv)
            {
                BushEntry entry{};
                entry.ID = ID;
                entry.center = bv.center;
                entry.radius = bv.radius;
                entry.bushId = bv.bushId;
                m_vecBushes.push_back(entry);
            }));
}

EntityID CBushVolumeIndex::QueryBushAt(const Vec3& pos) const
{
    for (const BushEntry& bush : m_vecBushes)
    {
        const f32_t dx = pos.x - bush.center.x;
        const f32_t dz = pos.z - bush.center.z;
        if (dx * dx + dz * dz <= bush.radius * bush.radius)
            return bush.ID;
    }
    return NULL_ENTITY;
}
