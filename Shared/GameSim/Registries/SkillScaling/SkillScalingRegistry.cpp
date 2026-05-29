#include "Shared/GameSim/Registries/SkillScaling/SkillScalingRegistry.h"

CSkillScalingRegistry& CSkillScalingRegistry::Instance()
{
    static CSkillScalingRegistry s_inst;
    return s_inst;
}

void CSkillScalingRegistry::Add(u16_t scalingTableId, const SkillScalingTable& table)
{
    SkillScalingTable copy = table;
    copy.scalingTableId = scalingTableId;
    m_ByScalingId[scalingTableId] = copy;
    if (copy.skillId != 0)
        m_SkillToScalingId[copy.skillId] = scalingTableId;
}

const SkillScalingTable* CSkillScalingRegistry::Find(u16_t scalingTableId) const
{
    auto it = m_ByScalingId.find(scalingTableId);
    return (it != m_ByScalingId.end()) ? &it->second : nullptr;
}

const SkillScalingTable* CSkillScalingRegistry::FindBySkillId(u16_t skillId) const
{
    auto skillIt = m_SkillToScalingId.find(skillId);
    if (skillIt == m_SkillToScalingId.end())
        return nullptr;
    return Find(skillIt->second);
}
