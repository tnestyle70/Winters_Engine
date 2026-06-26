#pragma once
#include "IScene.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Resource/Texture.h"
#include "UI/ImageScenePresenter.h"
#include <memory>
#include <vector>

class CScene_BanPick : public IScene
{
private:
	CScene_BanPick() = default;

public:
	~CScene_BanPick() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnLateUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

	static std::unique_ptr<CScene_BanPick> Create();

private:
	void UpdateServerSmokeAutomation(f32_t dt);
	void HandleChampionSelectInput();
	void HandleServerChampionSelectInput();
	void HandleLocalChampionSelectInput();
	void RenderChampionGridAndRosterOverlay();
	void RenderChampionSelectSlots(const MatchContext& context, u8_t selectedSlotId);
	void BuildChampionCells();
	void StartMatchLoadingScene();

	eChampion ResolveClickedChampion(f32_t fSourceX, f32_t fSourceY) const;
	u8_t ResolveClickedChampionSlot(f32_t fSourceX, f32_t fSourceY, const MatchContext& context) const;
	Engine::CTexture* FindChampionTexture(eChampion champion) const;
	bool_t IsReadyButtonClicked() const;
	bool_t IsLocalPlayerChampionPicked() const;

	struct ChampionCell
	{
		eChampion champion = eChampion::END;
		ImageSourceRect rect{};
		std::unique_ptr<Engine::CTexture> pTexture{};
	};

	struct ServerSmokeAutomation
	{
		bool_t bEnabled = false;
		bool_t bStartGameWhenReady = false;
		bool_t bStartCommandSent = false;
		u8_t slotId = 0;
		u8_t minHumansToStart = 1;
		eChampion champion = eChampion::EZREAL;
		f32_t commandRetryTimerSec = 0.f;
	};

	u8_t m_SelectedSlotId = 0;
	eChampion m_SelectedChampion = eChampion::END;
	CImageScenePresenter m_ImageUI{};
	std::vector<ChampionCell> m_ChampionCells{};
	bool_t m_bServerLobbyActive = false;
	bool_t m_bSceneTransitionStarted = false;
	ServerSmokeAutomation m_ServerSmoke{};
};
