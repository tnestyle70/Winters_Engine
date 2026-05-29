#pragma once

// [Phase T] R-4 비활성 — Phase T 에선 Scene_InGame::UpdateCombatInput 유지.
// 재개 시 #if 0 → #if 1 로 전환.
#if 0

#include "WintersTypes.h"
#include "ECS/ISystem.h"
//일단 추가해봄
#include <memory>

class CWorld;
class CDynamicCamera;

//클라이언트의 입력 시스템 -> 추후 네트워크 쪽으로 이전
//절대 클라이언트를 믿지 말 것!!!!!!!!!!!!!!!!!

namespace Systems
{
	//NetworkPlayer -> NetworkInputSystem으로 대체 
	//ISystem의 정체가 뭐지? -> ECS JobSystem으로 분리할 수 있게 해주는 인터패이스인가?
	class CInputSystem : public ISystem
	{
	public:
		//ISystem에 memory include가 없나?
		static std::unique_ptr<CInputSystem> Create();

		void Execute(CWorld& world, f32_t dt) override;
		uint32_t GetPhase() const override { return 0; }
		void Set_Camera(CDynamicCamera* pCamera) { m_pCamera = pCamera; }
	private:
		CInputSystem() = default;

		CDynamicCamera* m_pCamera = nullptr;
	};
}

#endif  // [Phase T] R-4 비활성