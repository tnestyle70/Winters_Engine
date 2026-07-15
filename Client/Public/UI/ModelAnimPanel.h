#pragma once

class CScene_InGame;

namespace UI
{
	// 디자이너용 인게임 모델/애니메이션 랩 ('7' 키 — F5 는 Attack Speed Lab 전용).
	// 챔피언 선택 -> 클립 목록 재생(루프/역재생/배속) -> Pause 스크럽 -> 서브메시 분해.
	class CModelAnimPanel final
	{
	public:
		static void Render(CScene_InGame* pScene);

	private:
		CModelAnimPanel() = delete;
	};
}
