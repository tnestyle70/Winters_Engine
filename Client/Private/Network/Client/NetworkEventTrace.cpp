#include "Network/Client/NetworkEventTrace.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#include <flatbuffers/flatbuffers.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#pragma push_macro("new")
#undef new
#include "imgui.h"
#pragma pop_macro("new")

#include <cstddef>

CNetworkEventTrace& CNetworkEventTrace::Instance()
{
    static CNetworkEventTrace instance;
    return instance;
}

void CNetworkEventTrace::Clear()
{
    m_entries = {};
    m_counts = {};
    m_writeIndex = 0;
    m_size = 0;
}

u32_t CNetworkEventTrace::GetCount(eTraceKind kind) const
{
    const std::size_t index = static_cast<std::size_t>(kind);
    return index < m_counts.size() ? m_counts[index] : 0;
}

const CNetworkEventTrace::Entry* CNetworkEventTrace::GetLatest() const
{
    if (m_size == 0)
        return nullptr;

    const u32_t index = (m_writeIndex + kCapacity - 1) % kCapacity;
    return &m_entries[index];
}

void CNetworkEventTrace::RecordEventPacket(
    const u8_t* payload,
    u32_t len,
    u32_t sequence)
{
    if (!m_bEnabled || !payload || len == 0)
        return;

    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyEventPacketBuffer(verifier))
        return;

    const auto* packet = Shared::Schema::GetEventPacket(payload);
    if (!packet)
        return;

    Entry entry{};
    entry.serverTick = packet->serverTick();
    entry.sequence = sequence;

    switch (packet->kind())
    {
    case Shared::Schema::EventKind::Damage:
        if (const auto* ev = packet->damage())
        {
            entry.kind = eTraceKind::Damage;
            entry.sourceNet = ev->sourceNet();
            entry.targetNet = ev->targetNet();
            entry.idA = ev->skillId();
            entry.idB = ev->type();
            entry.value = ev->amount();
        }
        break;
    case Shared::Schema::EventKind::SkillCast:
        if (const auto* ev = packet->skillCast())
        {
            entry.kind = eTraceKind::SkillCast;
            entry.sourceNet = ev->casterNet();
            entry.targetNet = ev->targetNet();
            entry.idA = ev->slot();
            entry.idB = ev->rank();
        }
        break;
    case Shared::Schema::EventKind::ProjectileSpawn:
        if (const auto* ev = packet->projectile())
        {
            entry.kind = eTraceKind::ProjectileSpawn;
            entry.sourceNet = ev->ownerNet();
            entry.projectileNet = ev->netId();
            entry.idA = ev->kind();
            entry.value = ev->speed();
        }
        break;
    case Shared::Schema::EventKind::ProjectileHit:
        if (const auto* ev = packet->projectileHit())
        {
            entry.kind = eTraceKind::ProjectileHit;
            entry.sourceNet = ev->ownerNet();
            entry.targetNet = ev->targetNet();
            entry.projectileNet = ev->netId();
            entry.idA = ev->kind();
            entry.idB = ev->bDestroyed() ? 1u : 0u;
        }
        break;
    case Shared::Schema::EventKind::ActionStart:
        if (const auto* ev = packet->actionStart())
        {
            entry.kind = eTraceKind::ActionStart;
            entry.sourceNet = ev->netId();
            entry.idA = ev->actionId();
            entry.idB = ev->actionSeq();
            entry.value = static_cast<f32_t>(ev->actionStage());
        }
        break;
    case Shared::Schema::EventKind::EffectTrigger:
        if (const auto* ev = packet->effect())
        {
            entry.kind = eTraceKind::EffectTrigger;
            entry.sourceNet = ev->sourceNet();
            entry.targetNet = ev->targetNet();
            entry.idA = ev->effectId();
            entry.idB = ev->flags();
            entry.value = static_cast<f32_t>(ev->durationMs()) / 1000.f;
        }
        break;
    default:
        break;
    }

    if (entry.kind != eTraceKind::None)
        Push(entry);
}

void CNetworkEventTrace::DrawImGui()
{
    if (!ImGui::Begin("Network Event Trace"))
    {
        ImGui::End();
        return;
    }

    bool enabled = m_bEnabled;
    if (ImGui::Checkbox("Enabled", &enabled))
        m_bEnabled = enabled;

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        Clear();

    ImGui::Separator();

    ImGui::Text("Damage: %u", GetCount(eTraceKind::Damage));
    ImGui::Text("SkillCast: %u", GetCount(eTraceKind::SkillCast));
    ImGui::Text("ProjectileSpawn: %u", GetCount(eTraceKind::ProjectileSpawn));
    ImGui::Text("ProjectileHit: %u", GetCount(eTraceKind::ProjectileHit));
    ImGui::Text("ActionStart: %u", GetCount(eTraceKind::ActionStart));
    ImGui::Text("EffectTrigger: %u", GetCount(eTraceKind::EffectTrigger));

    ImGui::Separator();

    const u32_t rows = (m_size < 32) ? m_size : 32;
    for (u32_t i = 0; i < rows; ++i)
    {
        const u32_t index =
            (m_writeIndex + kCapacity - 1u - i) % kCapacity;
        const Entry& e = m_entries[index];

        ImGui::Text(
            "[%llu] %s seq=%u src=%u tgt=%u proj=%u a=%u b=%u v=%.2f",
            static_cast<unsigned long long>(e.serverTick),
            ToString(e.kind),
            e.sequence,
            e.sourceNet,
            e.targetNet,
            e.projectileNet,
            e.idA,
            e.idB,
            e.value);
    }

    ImGui::End();
}

void CNetworkEventTrace::Push(const Entry& entry)
{
    m_entries[m_writeIndex] = entry;
    m_writeIndex = (m_writeIndex + 1u) % kCapacity;
    if (m_size < kCapacity)
        ++m_size;

    const std::size_t index = static_cast<std::size_t>(entry.kind);
    if (index < m_counts.size())
        ++m_counts[index];
}

const char* CNetworkEventTrace::ToString(eTraceKind kind)
{
    switch (kind)
    {
    case eTraceKind::Damage: return "Damage";
    case eTraceKind::SkillCast: return "SkillCast";
    case eTraceKind::ProjectileSpawn: return "ProjectileSpawn";
    case eTraceKind::ProjectileHit: return "ProjectileHit";
    case eTraceKind::ActionStart: return "ActionStart";
    case eTraceKind::EffectTrigger: return "EffectTrigger";
    default: return "None";
    }
}
