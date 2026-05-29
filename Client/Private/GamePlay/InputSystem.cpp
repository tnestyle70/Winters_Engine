// [Phase T] R-4 비활성 — 전체 감쌈. Phase T 에선 Scene_InGame::UpdateCombatInput 유지.
#if 0

#include "GamePlay/InputSystem.h"
#include "GamePlay/CommandQueueComponent.h"
#include "GamePlay/GameplayComponents.h"
#include "GamePlay/HoveredTargetComponent.h"
#include "ECS/World.h"
#include "Core/CInput.h"
#include "DynamicCamera.h"
#include "EngineConfig.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace Systems
{
	std::unique_ptr<CInputSystem> Systems::CInputSystem::Create()
	{
		//순수 가상함수 GetName에 재정의자가 없다!
		return std::unique_ptr<CInputSystem>(new CInputSystem());
	}

	void Systems::CInputSystem::Execute(CWorld& world, f32_t dt)
	{
		(void)dt;

		const bool_t bImGuiKbd = ImGui::GetIO().WantCaptureKeyboard();
		const bool_t bImGuiMouse = ImGui::GetIO().WantCaptureMouse();

		//Hover Target - Targeting System이 세팅한 싱글턴? -> 어떻게 리팩토링?
		EntityID hovered = NULL_ENTITY;
		if (world.HasSingleton<HoveredTargetComponent>())
			hovered = world.GetSingleton<HoveredTargetComponent>().entity;

		//로컬 플레이어만 스킬 입력해서 사용 가능하도록 설정! 
		world.ForEach<LocalPlayerTag, CommandQueueComponent>(
			[&](EntityID entity, LocalPlayerTag&, CommandQueueComponent& cq)
			{
				//Skill Pulse Q W E R 
				if (!bImGuiKbd)
				{
					auto pushSkillPulse = [&](uint8_t slot, char vKey)
						{
							if (!in.IsKeyPressed((uint8_t)vKey))
								return;

							PulseCommand pulse{};
							pulse.eType = eCommandType::Skill;
							pulse.iSlot = slot;
							pulse.target = hovered;
							pulse.bHasCursor = (m_pCamera != nullptr);
							if (pulse.bHasCursor)
							{
								pulse.vGroundPos = CInput::Get().GetMouseGroundPos(*m_pCamera,
									(int32_t)g_iWinSizeX, (int32_t)g_iWinSizeY);
								cq.PushPulse(pulse);
							};
							pushSkillPulse(1, 'Q');
							pushSkillPulse(2, 'W');
							pushSkillPulse(3, 'E');
							pushSkillPulse(4, 'R');
						};
					//Right Click - Basic Attack(Target Exist) or Move(ground)
					if (!bImGuiMouse && m_pCamera)
					{
						if (CInput::Get().IsRButtonPressed())
						{
							if (hovered != NULL_ENTITY)
							{
								//Basic Attack Pulse
								PulseCommand pulse{};
								pulse.eType = eCommandType::BasicAttack;
								pulse.iSlot = 0;
								pulse.target = hovered;
								pulse.bHasCursor = true;
								pulse.vGroundPos = CInput::Get().GetMouseGroundPos(
									*m_pCamera, (int32_t)g_iWinSizeX, (int32_t)g_iWinSizeY);
								cq.PushPulse(pulse);
							}
							else
							{
								//Move Pulse + State
								Vec3 vGroundPos = CInput::Get().GetMouseGroundPos(
									*m_pCamera, (int32_t)g_iWinSizeX, (int32_t)g_iWinSizeY);

								if (fabsf(vGroundPos.x) + fabsf(vGroundPos.z) > 0.001f)
								{
									PulseCommand pulse{};
									pulse.eType = eCommandType::Move;
									pulse.vGroundPos = vGroundPos;
									cq.PushPulse(pulse);
									cq.tState.bMoveActive = true;
									cq.tState.vMoveDest = vGroundPos;
								}
							}
						}
						else if (CInput::Get().IsRButtonDown() && 
							cq.eActiveType == eCommandType::Move)
						{
							//우클릭 홀드 -> state 갱신 active Move 덮어쓰기
							Vec3 vGroundPos = CInput::Get().GetMouseGroundPos(
								*m_pCamera, (int32_t)g_iWinSizeX, (int32_t)g_iWinSizeY);
							if (fabsf(vGroundPos.x) + fabsf(vGroundPos.z) > 0.001f)
								cq.tState.vMoveDest = vGroundPos;
						}
					}//여기 검토좀!! 
				}
			});
	}
}
#endif  // [Phase T] R-4 비활성
