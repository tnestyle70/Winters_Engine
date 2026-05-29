#pragma once

#include "GameContext.h"
#include "IScene.h"
#include "UI/ImageScenePresenter.h"

#include <memory>

class CScene_CustomMode final : public IScene
{
private:
	CScene_CustomMode() = default;

public:
	~CScene_CustomMode() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnLateUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

	static std::unique_ptr<CScene_CustomMode> Create();

private:
	void HandleInput();
	void HandleServerInput();
	void HandleLocalInput();
	void ChangeToChampionSelectScene();
	void StartMatchLoadingScene();

	u8_t ResolveClickedSlot(f32_t fSourceX, f32_t fSourceY) const;
	bool_t IsGamePlayClicked() const;
	bool_t AddBotToFirstEmptySlot(u32_t beginSlot, u32_t endSlot);
	bool_t JoinSlot(u8_t slotId);
	bool_t RemoveBotAndCompactTeam(u32_t beginSlot, u32_t endSlot, u8_t slotId);
	bool_t SendBotRemoval(u8_t slotId);
	bool_t CompactLocalBotRemoval(GameContext& context, u32_t beginSlot, u32_t endSlot, u8_t slotId);
	void SetBotLane(u8_t slotId, u8_t lane);
	bool_t SetBotChampion(u8_t slotId, eChampion champion);
	void RenderRosterOverlay();
	void RenderBotChampionButton(u8_t slotId, f32_t width, f32_t height);
	void RenderTeamRoster(const GameContext& context, u32_t beginSlot, u32_t endSlot,
		const ImageSourceRect& rect);

	CImageScenePresenter m_ImageUI{};
	bool_t m_bServerLobbyActive = false;
	bool_t m_bSceneTransitionStarted = false;
	u8_t m_SelectedSlotId = 0;
};
