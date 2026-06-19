#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

#include <unordered_map>

class CLobbyAuthority;
class CWorld;
class EntityIdMap;

class CSessionBinding final
{
public:
    void Bind(u32_t sessionId, EntityID entity);
    void Unbind(u32_t sessionId);
    bool HasBinding(u32_t sessionId) const;
    bool TryGet(u32_t sessionId, EntityID& outEntity) const;
    bool TryGetAlive(u32_t sessionId, const CWorld& world, EntityID& outEntity) const;

    EntityID ResolveControlledEntity(
        u32_t sessionId,
        const CWorld& world,
        const EntityIdMap& entityMap,
        const CLobbyAuthority* pLobbyAuthority);

private:
    std::unordered_map<u32_t, EntityID> m_sessionToEntity;
};
