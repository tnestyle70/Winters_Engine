#pragma once

#include "Defines.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "WintersTypes.h"

#include <array>
#include <cstddef>

class CNetworkEventTrace final
{
public:
    enum class eTraceKind : u8_t
    {
        None = 0,
        Damage,
        SkillCast,
        ProjectileSpawn,
        ProjectileHit,
        ActionStart,
        EffectTrigger,
        Count
    };

    struct Entry
    {
        eTraceKind kind = eTraceKind::None;
        u64_t serverTick = 0;
        u32_t sequence = 0;

        NetEntityId sourceNet = NULL_NET_ENTITY;
        NetEntityId targetNet = NULL_NET_ENTITY;
        NetEntityId projectileNet = NULL_NET_ENTITY;

        u32_t idA = 0;
        u32_t idB = 0;
        f32_t value = 0.f;
    };

    static CNetworkEventTrace& Instance();

    void Clear();
    void SetEnabled(bool_t enabled) { m_bEnabled = enabled; }
    bool_t IsEnabled() const { return m_bEnabled; }

    void RecordEventPacket(const u8_t* payload, u32_t len, u32_t sequence = 0);
    void DrawImGui();

    u32_t GetCount(eTraceKind kind) const;
    const Entry* GetLatest() const;

private:
    CNetworkEventTrace() = default;

    void Push(const Entry& entry);
    static const char* ToString(eTraceKind kind);

    static constexpr u32_t kCapacity = 256;

    std::array<Entry, kCapacity> m_entries{};
    std::array<u32_t, static_cast<std::size_t>(eTraceKind::Count)> m_counts{};

    u32_t m_writeIndex = 0;
    u32_t m_size = 0;
    bool_t m_bEnabled = true;
};
