#pragma once

#include "AI/BehaviorTree.h"

#include <memory>
#include <string>

NS_BEGIN(Engine)

namespace BTNodes
{
    std::unique_ptr<CBTNode> Cond_HpBelow(f32_t pct);
    std::unique_ptr<CBTNode> Cond_ManaBelow(f32_t pct);
    std::unique_ptr<CBTNode> Cond_EnemyCharacterInSight(f32_t range);
    std::unique_ptr<CBTNode> Cond_EnemyUnitInRange(f32_t range);
    std::unique_ptr<CBTNode> Cond_AllyCharacterInSight(f32_t range);
    std::unique_ptr<CBTNode> Cond_InStructureRange(f32_t range);
    std::unique_ptr<CBTNode> Cond_BBKeySet(const std::string& key);
    std::unique_ptr<CBTNode> Cond_SkillReady(u8_t slot);

    std::unique_ptr<CBTNode> Act_MoveTo(const std::string& bbKeyTargetPos);
    std::unique_ptr<CBTNode> Act_AttackTarget(const std::string& bbKeyTargetEntity);
    std::unique_ptr<CBTNode> Act_CastSkill(u8_t slot, const std::string& bbKeyTargetPos);
    std::unique_ptr<CBTNode> Act_Recall();
    std::unique_ptr<CBTNode> Act_Retreat();
    std::unique_ptr<CBTNode> Act_SetBBValue(const std::string& key, f32_t value);
    std::unique_ptr<CBTNode> Act_LogDebug(const std::string& message);
}

WINTERS_ENGINE std::unique_ptr<CBehaviorTree> BuildStandardCombatAgentBT();
WINTERS_ENGINE std::unique_ptr<CBehaviorTree> BuildAdvancedCombatAgentBT();

NS_END
