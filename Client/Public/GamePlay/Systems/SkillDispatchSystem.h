#pragma once

// [Phase T] R-4 (CommandQueue + AbilityDispatch) 폐기로 본 헤더 비활성.
// AbilityDef / CastCommand / EffectHandlerRegistry 헤더 부재로 파싱 실패 → 전체 감쌈.
// 재개 시 #if 0 → #if 1 로 전환 + 누락 헤더 작성.
#if 0

#include "WintersTypes.h"
#include "ECS/ISystem.h"
//일단 추가
#include <memory>

class CWorld;

namespace Systems
{
	//Pulse 소비 -> AbilityDef 조회 -> Execution 분기 
	//Active 전환(Move -> Skill Cancle, Skill Complete -> Pending Move)
	//Phase2 InputSystem = 0, TargetingSystem = 1, SkillDispatchSystem = 2
	class CSkillDispatchSystem : public ISystem
	{
	public:
		static std::unique_ptr<CSkillDispatchSystem> Create();
		//ISystem Jobsystem의 분기를 위한 거 맞지?
		void Execute(CWorld& world, f32_t dt) override;
		//왜 2??
		uint32_t GetPhase() const override { return 2; }
	private:
		CSkillDispatchSystem() = default;
		//AbilityDef랑 CastCommand!!!!!!!!!!!없음!
		static bool BuildCastCommand(const PulseCommand& pulse, EntityID caster,
			const AbilityDef& def, CWorld& world, CastCommand& outCommand);
		static void DispatchAbility(EntityID caster, const PulseCommand& pulse,
			CWorld& world, CommandQueueComponent& cq);
	};
}

#endif  // [Phase T] R-4 비활성