#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

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

        const auto existingNet = m_NetToLocal.find(netId);
        if (existingNet != m_NetToLocal.end() && existingNet->second != entity)
        {
            const auto reverse = m_LocalToNet.find(existingNet->second);
            if (reverse != m_LocalToNet.end() && reverse->second == netId)
                m_LocalToNet.erase(reverse);
        }

        const auto existingEntity = m_LocalToNet.find(entity);
        if (existingEntity != m_LocalToNet.end() && existingEntity->second != netId)
        {
            const auto forward = m_NetToLocal.find(existingEntity->second);
            if (forward != m_NetToLocal.end() && forward->second == entity)
                m_NetToLocal.erase(forward);
        }

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

    // Chrono Break: keyframe 저장/복원용. m_NextNetId는 max+1로 재유도하면 안 된다
    // (와드 만료 등 Unbind가 이미 배포된 id를 지울 수 있음) — 그대로 왕복시킨다.
    template<typename Fn> void ForEachBinding(Fn&& fn) const
    {
        for (const auto& [netId, entity] : m_NetToLocal)
            fn(netId, entity);
    }
    NetEntityId GetNextNetId() const { return m_NextNetId; }
    void RestoreState(const std::vector<std::pair<NetEntityId, EntityID>>& bindings,
        NetEntityId nextNetId)
    {
        m_NetToLocal.clear();
        m_LocalToNet.clear();
        for (const auto& [netId, entity] : bindings)
        {
            m_NetToLocal[netId] = entity;
            m_LocalToNet[entity] = netId;
        }
        m_NextNetId = nextNetId;
    }
    void SwapState(EntityIdMap& other) noexcept
    {
        m_NetToLocal.swap(other.m_NetToLocal);
        m_LocalToNet.swap(other.m_LocalToNet);
        const NetEntityId nextNetId = m_NextNetId;
        m_NextNetId = other.m_NextNetId;
        other.m_NextNetId = nextNetId;
    }

private:
    std::unordered_map<NetEntityId, EntityID> m_NetToLocal;
    std::unordered_map<EntityID, NetEntityId> m_LocalToNet;
    NetEntityId m_NextNetId = 1;
};
