#pragma once

#include "ECS/Entity.h"

#include <cstdint>
#include <unordered_map>

using NetEntityId = uint32_t;
constexpr NetEntityId NULL_NET_ENTITY = 0;

class EntityIdMap
{
public:
    EntityID FromNet(NetEntityId netId) const
    {
        auto it = m_NetToLocal.find(netId);
        return (it != m_NetToLocal.end()) ? it->second : NULL_ENTITY;
    }

    NetEntityId ToNet(EntityID entity) const
    {
        auto it = m_LocalToNet.find(entity);
        return (it != m_LocalToNet.end()) ? it->second : NULL_NET_ENTITY;
    }

    NetEntityId IssueNew(EntityID entity)
    {
        if (NetEntityId existing = ToNet(entity); existing != NULL_NET_ENTITY)
            return existing;

        const NetEntityId netId = m_NextNetId++;
        Bind(netId, entity);
        return netId;
    }

    void Bind(NetEntityId netId, EntityID entity)
    {
        if (netId == NULL_NET_ENTITY || entity == NULL_ENTITY)
            return;

        m_NetToLocal[netId] = entity;
        m_LocalToNet[entity] = netId;
    }

    void Unbind(NetEntityId netId)
    {
        auto it = m_NetToLocal.find(netId);
        if (it == m_NetToLocal.end())
            return;

        m_LocalToNet.erase(it->second);
        m_NetToLocal.erase(it);
    }

private:
    std::unordered_map<NetEntityId, EntityID> m_NetToLocal;
    std::unordered_map<EntityID, NetEntityId> m_LocalToNet;
    NetEntityId m_NextNetId = 1;
};
