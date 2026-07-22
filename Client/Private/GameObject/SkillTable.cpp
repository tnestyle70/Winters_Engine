#include "GameObject/SkillDef.h"

#include "GamePlay/SkillRegistry.h"

const SkillDef* FindSkillDef(eChampion champ, uint8_t slot)
{
    return CSkillRegistry::Instance().Find(champ, slot);
}
