#pragma once

#include "Shared/GameSim/Definitions/SkillScalingTable.h"

#include <cstddef>
#include <unordered_map>

class CSkillScalingRegistry
{
public:
    static CSkillScalingRegistry& Instance();

    void Add(u16_t scalingTableId, const SkillScalingTable& table);
    const SkillScalingTable* Find(u16_t scalingTableId) const;
    const SkillScalingTable* FindBySkillId(u16_t skillId) const;
    std::size_t Count() const { return m_ByScalingId.size(); }

private:
    CSkillScalingRegistry() = default;
    ~CSkillScalingRegistry() = default;
    CSkillScalingRegistry(const CSkillScalingRegistry&) = delete;
    CSkillScalingRegistry& operator=(const CSkillScalingRegistry&) = delete;

    std::unordered_map<u16_t, SkillScalingTable> m_ByScalingId{};
    std::unordered_map<u16_t, u16_t> m_SkillToScalingId{};
};
