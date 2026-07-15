#pragma once
#include "Defines.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class IScene;

// CJobSystem / CJobCounter 는 Engine/Public/Core/*.h 에서 전역 네임스페이스에 정의됨.
// Engine namespace 안에서 forward-decl 하면 별도 타입이 되어 링크 불일치 → NS_BEGIN 밖 전역 선언.
class CJobSystem;
class CJobCounter;
class CFxSystem;

namespace Engine
{
	class CMapSurfaceSampler;
	class CFxStaticMeshRenderer;
}

NS_BEGIN(Client)

class CLoader final
{
public:
	using SceneFactory = std::function<std::unique_ptr<IScene>()>;
	//Factory로 생성? 근거가 뭐였지?
	static std::unique_ptr<CLoader> Create(eSceneID eNextSceneID,
		SceneFactory pFactory);
	~CLoader();

	void TickMainThreadLoad();
	bool IsFinished() const { return m_bFinished.load(); }
	bool HasFailed() const { return m_bLoadFailed.load(); }
	bool IsReadyToActivate() const;
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
	void PreloadChampionAssets(eChampion eChampionId);
	bool_t PreloadModel(const char* pPath, f32_t fProgressStep);
	bool_t PreloadTexture(const wchar_t* pPath, f32_t fProgressStep);
	void PrepareMainThreadInGameLoad();
	void StartInGameCpuLoad();
	void RunInGameCpuLoad();
	bool_t AreInGameCpuLoadsComplete() const;
	void TryFinishInGameLoad();
	void FailInGameLoad(const char* pStage);
	void SetProgress(f32_t fValue);

private:
	enum class eLoadStepType
	{
		FxDirectory,
		KalistaWSentinelTextures,
		FxMesh,
		Model,
		MapModel,
		Texture,
		MinionVisual,
		ChampionPortrait
	};

	struct LoadStep
	{
		eLoadStepType eType = eLoadStepType::Model;
		std::string strModelPath{};
		std::wstring strTexturePath{};
		eChampion champion = eChampion::END;
		f32_t fProgressAfter = 0.f;
	};

	eSceneID m_eNextSceneID = eSceneID::End;
	SceneFactory m_pFactory = { nullptr };
	
	std::atomic<bool> m_bFinished{ false };
	std::atomic<bool> m_bLoadFailed{ false };
	std::atomic<f32_t> m_fProgress{ 0.f };

	std::unique_ptr<CJobCounter> m_pCounter{};
	MatchContext m_LoadContext{};
	u64_t m_uRosterFingerprint = 0u;

	bool_t m_bMainThreadLoad = false;
	std::vector<LoadStep> m_LoadSteps{};
	u32_t m_iNextLoadStep = 0;
	std::unique_ptr<Engine::CMapSurfaceSampler> m_pPreparedMapSurfaceSampler{};
	std::unique_ptr<CFxSystem> m_pPreparedFxSystem{};
	std::unique_ptr<Engine::CFxStaticMeshRenderer> m_pPreparedFxMeshRenderer{};
	std::atomic_bool m_bCancelCpuLoad{ false };
	std::atomic_bool m_bCpuLoadSucceeded{ false };
};

NS_END
