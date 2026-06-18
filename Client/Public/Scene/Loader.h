#pragma once
#include "Defines.h"
#include "GameContext.h"
#include <atomic>
#include <functional>
#include <memory>

class IScene;

// CJobSystem / CJobCounter 는 Engine/Public/Core/*.h 에서 전역 네임스페이스에 정의됨.
// Engine namespace 안에서 forward-decl 하면 별도 타입이 되어 링크 불일치 → NS_BEGIN 밖 전역 선언.
class CJobSystem;
class CJobCounter;

NS_BEGIN(Client)

class CLoader final
{
public:
	using SceneFactory = std::function<std::unique_ptr<IScene>()>;
	//Factory로 생성? 근거가 뭐였지?
	static std::unique_ptr<CLoader> Create(eSceneID eNextSceneID,
		SceneFactory pFactory);
	~CLoader();

	bool IsFinished() const { return m_bFinished.load(); }
	f32_t Get_Progress() const { return m_fProgress.load(); }
	eSceneID Get_NextSceneID() const { return m_eNextSceneID; }

	unique_ptr<IScene> Build_NextScene();
	static void Register_Blueprints_InGame();
private:
	CLoader() = default;

	void Ready_For_MainMenu();
	void Ready_For_BanPick();
	void Ready_For_InGame();

	void RunLoadJob();
	void PreloadInGameAssets();
	void PreloadChampionAssets(eChampion eChampionId);
	void PreloadModel(const char* pPath, f32_t fProgressStep);
	void PreloadTexture(const wchar_t* pPath, f32_t fProgressStep);
	void SetProgress(f32_t fValue);

private:
	eSceneID m_eNextSceneID = eSceneID::End;
	SceneFactory m_pFactory = { nullptr };
	
	std::atomic<bool> m_bFinished{ false };
	std::atomic<f32_t> m_fProgress{ 0.f };

	std::unique_ptr<CJobCounter> m_pCounter{};
	GameContext m_LoadContext{};
};

NS_END
