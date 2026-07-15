#include "WintersPCH.h"
#include "ECS/ConcealmentVolumeIndex.h"
#include "ECS/World.h"
#include "ECS/Components/VisionComponents.h"

void CConcealmentVolumeIndex::Build(CWorld& world)
{
    m_vecVolumes.clear();
    world.ForEach<ConcealmentVolumeComponent>(
        function<void(EntityID, ConcealmentVolumeComponent&)>(
            [&](EntityID ID, ConcealmentVolumeComponent& volume)
            {
                VolumeEntry entry{};
                entry.ID = ID;
                entry.center = volume.center;
                entry.radius = volume.radius;
                entry.volumeId = volume.volumeId;
                m_vecVolumes.push_back(entry);
            }));
}

EntityID CConcealmentVolumeIndex::QueryVolumeAt(const Vec3& pos) const
{
    for (const VolumeEntry& volume : m_vecVolumes)
    {
        const f32_t dx = pos.x - volume.center.x;
        const f32_t dz = pos.z - volume.center.z;
        if (dx * dx + dz * dz <= volume.radius * volume.radius)
        {
            return volume.volumeId != 0u
                ? static_cast<EntityID>(volume.volumeId)
                : volume.ID;
        }
    }
    return NULL_ENTITY;
}
