#pragma once
#include "IScene.h"
#include "Replay/ReplayLibrary.h"
#include "UI/ImageScenePresenter.h"

#include <memory>
#include <string>
#include <vector>

// 나의 정보 씬 — MainMenu 우상단 챔피언 초상화 버튼으로 진입.
// 전적(백엔드 /profile/me/history + 로컬 경기 기록)과 리플레이 재생을 함께 제공한다.
class CScene_MyInfo final : public IScene
{
public:
	static std::unique_ptr<CScene_MyInfo> Create();
	~CScene_MyInfo() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnLateUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CScene_MyInfo() = default;
	CScene_MyInfo(const CScene_MyInfo&) = delete;
	CScene_MyInfo& operator=(const CScene_MyInfo&) = delete;

	void ReloadLocalMatchRecords();
	void ReloadReplayItems();
	void DrawCloudReplayItems();
	void DrawReplayItems(const std::vector<ReplayListItem>& items, const char* pEmptyText);
	void OpenReplay(const wstring_t& path, u32_t perspectiveNetId);
	void ChangeToMainMenu();
	bool_t HasExpectedCloudReplay() const;
	void UpdateExpectedReplayPolling(f32_t dt);

	CImageScenePresenter m_ImageUI{};
	std::vector<ReplayListItem> m_vAccountReplayItems{};
	std::vector<ReplayListItem> m_vDebugReplayItems{};
	std::vector<std::string> m_vLocalMatchRecords{};
	bool_t m_bBackRequested = false;
	bool_t m_bSceneTransitionStarted = false;
	u32_t m_uObservedReplayLibraryRevision = 0u;
	std::string m_strExpectedReplayMatchID{};
	f32_t m_fReplayReadyPollRemainingSec = 0.f;
	f32_t m_fReplayReadyPollCooldownSec = 0.f;
};
