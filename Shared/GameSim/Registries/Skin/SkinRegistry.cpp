#include "Shared/GameSim/Registries/Skin/SkinRegistry.h"

CSkinRegistry& CSkinRegistry::Instance()
{
    static CSkinRegistry s_inst;
    return s_inst;
}

void CSkinRegistry::Add(u32_t skinId, const SkinDef& def)
{
    SkinDef copy = def;
    copy.skinId = skinId;
    m_Map[skinId] = copy;
}

const SkinDef* CSkinRegistry::Find(u32_t skinId) const
{
    auto it = m_Map.find(skinId);
    return (it != m_Map.end()) ? &it->second : nullptr;
}

void CSkinRegistry::ForEachByChampion(eChampion championId, const IterFn& fn) const
{
    for (const auto& [id, def] : m_Map)
    {
        (void)id;
        if (def.championId == championId)
            fn(def);
    }
}
