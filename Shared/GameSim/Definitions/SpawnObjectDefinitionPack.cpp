#include "Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h"

f32_t SpawnObjectDefinitionPack::ResolveStructureMaxHp(eStructureKind kind) const
{
    switch (kind)
    {
    case eStructureKind::Nexus:
        return structure.nexusMaxHp;
    case eStructureKind::Inhibitor:
        return structure.inhibitorMaxHp;
    case eStructureKind::Turret:
    default:
        return structure.turretMaxHp;
    }
}

const JungleCampGameDef* SpawnObjectDefinitionPack::FindJungleCamp(u8_t subKind) const
{
    for (std::size_t i = 0u; i < jungleCampCount; ++i)
    {
        if (jungleCamps[i].subKind == subKind)
            return &jungleCamps[i].value;
    }
    return nullptr;
}

JungleCampGameDef SpawnObjectDefinitionPack::ResolveJungleCamp(u8_t subKind) const
{
    const JungleCampGameDef* def = FindJungleCamp(subKind);
    return def ? *def : defaultJungleCamp;
}

const MinionCombatDef* SpawnObjectDefinitionPack::FindMinion(u8_t roleType) const
{
    for (std::size_t i = 0u; i < minionCount; ++i)
    {
        if (minions[i].roleType == roleType)
            return &minions[i].value;
    }
    return nullptr;
}

MinionCombatDef SpawnObjectDefinitionPack::ResolveMinion(u8_t roleType) const
{
    const MinionCombatDef* def = FindMinion(roleType);
    return def ? *def : defaultMinion;
}
