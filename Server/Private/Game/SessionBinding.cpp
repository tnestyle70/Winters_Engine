#include "Game/SessionBinding.h"

#include "ECS/World.h"
#include "Game/LobbyAuthority.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

void CSessionBinding::Bind(u32_t sessionId, EntityID entity)
{
    if (sessionId == 0 || entity == NULL_ENTITY)
        return;

    m_sessionToEntity[sessionId] = entity;
}

void CSessionBinding::Unbind(u32_t sessionId)
{
    m_sessionToEntity.erase(sessionId);
}

bool CSessionBinding::HasBinding(u32_t sessionId) const
{
    return m_sessionToEntity.find(sessionId) != m_sessionToEntity.end();
}

bool CSessionBinding::TryGet(u32_t sessionId, EntityID& outEntity) const
{
    outEntity = NULL_ENTITY;

    const auto it = m_sessionToEntity.find(sessionId);
    if (it == m_sessionToEntity.end())
        return false;

    outEntity = it->second;
    return true;
}

bool CSessionBinding::TryGetAlive(
    u32_t sessionId,
    const CWorld& world,
    EntityID& outEntity) const
{
    if (!TryGet(sessionId, outEntity))
        return false;

    if (outEntity == NULL_ENTITY || !world.IsAlive(outEntity))
    {
        outEntity = NULL_ENTITY;
        return false;
    }

    return true;
}

EntityID CSessionBinding::ResolveControlledEntity(
    u32_t sessionId,
    const CWorld& world,
    const EntityIdMap& entityMap,
    const CLobbyAuthority* pLobbyAuthority)
{
    EntityID entity = NULL_ENTITY;
    if (TryGetAlive(sessionId, world, entity))
        return entity;

    if (!pLobbyAuthority)
        return NULL_ENTITY;

    auto resolveFromSlot = [&](const LobbySlotState& slot) -> EntityID
        {
            if (!slot.bHuman || slot.sessionId != sessionId || slot.netId == NULL_NET_ENTITY)
                return NULL_ENTITY;

            const EntityID resolvedEntity = entityMap.FromNet(slot.netId);
            if (resolvedEntity == NULL_ENTITY || !world.IsAlive(resolvedEntity))
                return NULL_ENTITY;

            Bind(sessionId, resolvedEntity);
            return resolvedEntity;
        };

    u8_t slotIndex = 0;
    if (pLobbyAuthority->TryGetSessionSlot(sessionId, slotIndex))
    {
        const LobbySlotState* pSlots = pLobbyAuthority->GetSlots();
        if (pSlots && slotIndex < pLobbyAuthority->GetSlotCount())
        {
            if (EntityID resolvedEntity = resolveFromSlot(pSlots[slotIndex]);
                resolvedEntity != NULL_ENTITY)
            {
                return resolvedEntity;
            }
        }
    }

    const LobbySlotState* pSlots = pLobbyAuthority->GetSlots();
    const u32_t slotCount = pLobbyAuthority->GetSlotCount();
    for (u32_t i = 0; pSlots && i < slotCount; ++i)
    {
        if (EntityID resolvedEntity = resolveFromSlot(pSlots[i]);
            resolvedEntity != NULL_ENTITY)
        {
            return resolvedEntity;
        }
    }

    return NULL_ENTITY;
}
