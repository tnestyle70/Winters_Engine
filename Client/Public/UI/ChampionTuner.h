#pragma once

class CScene_InGame;

namespace UI
{
	//ChampionTunerPanel
	//Attack Speed Multiplier - 공격 / 스킬 애니 재생속도 계수
	//Global Anim Speed : 모든 애니 재생속도 계수
	//Basic Attack Range : A키 사거리 + 평타 발동 기준 거리
	//    차후 확장:
	//      Dash Distance / Duration (Q)
	//      W Channel Duration
	//      E Sword Place Range / Bind Speed / Stun Duration
	//      R Wave Length / Width / Speed / Damage
	class CChampionTuner
	{
	public:
		static void Render(CScene_InGame* pScene);
	};
}