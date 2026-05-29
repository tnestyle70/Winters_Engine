#pragma once

#include "Shared/GameSim/Definitions/SkinDef.h"

#include <cstddef>
#include <functional>
#include <unordered_map>

class CSkinRegistry
{
public:
    using IterFn = std::function<void(const SkinDef&)>;

    static CSkinRegistry& Instance();

    void Add(u32_t skinId, const SkinDef& def);
    const SkinDef* Find(u32_t skinId) const;
    void ForEachByChampion(eChampion championId, const IterFn& fn) const;
    std::size_t Count() const { return m_Map.size(); }

private:
    CSkinRegistry() = default;
    ~CSkinRegistry() = default;
    CSkinRegistry(const CSkinRegistry&) = delete;
    CSkinRegistry& operator=(const CSkinRegistry&) = delete;

    std::unordered_map<u32_t, SkinDef> m_Map{};
};
