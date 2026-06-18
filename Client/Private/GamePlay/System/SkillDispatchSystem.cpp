// [Phase T] R-4 비활성 — 헤더의 #if 0 와 쌍으로 본 cpp 전체도 무력화.
#if 0

#include "GamePlay/Systems/SkillDispatchSystem.h"
#include "GamePlay/CommandQueueComponent.h"
//싹 다 없는 헤더들 아님?
//#include "GamePlay/AbilitySetComponent.h"
//#include "GamePlay/AbilityRegistry.h"
//#include "GamePlay/CastCommand.h"
//#include "GamePlay/Effects/EffectHandlerRegistry.h"
//#include "GamePlay/Behaviors/AbilityBehaviorRegistry.h"
//#include "GamePlay/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"

namespace Systems
{
	std::unique_ptr<CSkillDispatchSystem> Systems::CSkillDispatchSystem::Create()
	{
		//이거도 동일하게 GetName 선언 안 되이서 생성 불가 오류!
		return std::unique_ptr<CSkillDispatchSystem>(new CSkillDispatchSystem());
	}

	bool CSkillDispatchSystem::BuildCastCommand(const PulseCommand& pulse, EntityID caster, 
		const AbilityDef& def, CWorld& world, CastCommand& outCommand)
	{
		outCommand.casterEntity = caster;
		outCommand.slot = pulse.iSlot;
		outCommand.targetEntity = pulse.target;
		
		switch (def.targetMode)
		{
		default:
			break;
		}

		return false;
	}
	void CSkillDispatchSystem::DispatchAbility(EntityID caster, const PulseCommand& pulse, 
		CWorld& world, CommandQueueComponent& cq)
	{

	}

	void Systems::CSkillDispatchSystem::Execute(CWorld& world, f32_t dt)
	{
		world.ForEach<LocalPlayerTag, CommandQueueComponent, AbilitySetComponent>(
			[&](EntityID entity, LocalPlayerTag&, CommandQueueComponent cq,
				AbilitySetComponent& set)
			{
				//1. Active Timer Decrease
				if (cq.fActiveRemaining > 0.f)
					cq.fActiveRemaining -= dt;
				if (cq.fActiveRemaining <= 0.f && cq.eActiveType != eCommandType::End &&
					cq.eActiveType != eCommandType::Move)
				{
					cq.eActiveType = eCommandType::End;
				}
				//슬롯 쿨다운 / Stage Window 감소
				for (int i = 0; i < 5; ++i)
				{
					if (set.slots[i].cooldownRemaining > 0.f)
						set.slots[i].cooldownRemaining -= dt;

				}

			}
		)
	}
}

#endif  // [Phase T] R-4 비활성
