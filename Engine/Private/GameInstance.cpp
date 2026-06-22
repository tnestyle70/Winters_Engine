#include "GameInstance.h"
#include "Core/Timer_Manager.h"
#include "Scene/Scene_Manager.h"
#include "Sound/Sound_Manager.h"
#include "Manager/UI/UI_Manager.h"
#include "Manager/Profiler/ProfilerOverlay.h"
#include "ECS/Systems/EntityBlueprintRegistry.h"
#include "Core/JobSystem.h"
#include "Framework/CEngineApp.h"   // RHI 게터 포워딩 대상
#include "Core/Profiler/CPUProfiler.h"
#include "FX/FxAsset.h"

// ─────────────────────────────────────────────────────────────────
//  생성자/소멸자는 반드시 이 .cpp 에서 정의한다.
//  이유: m_pTimer_Manager 가 unique_ptr<class CTimer_Manager> 이고
//       GameInstance.h 에서는 CTimer_Manager 가 전방 선언만 되어 있다.
//       헤더에 = default 로 inline 정의하면,
//       GameInstance.h 만 include 한 TU 가 소멸자 바디를 실체화할 때
//       default_delete<CTimer_Manager> 가 sizeof 를 요구 →
//       static_assert "can't delete an incomplete type" 발동.
//       out-of-line 으로 내려서 이 .cpp (Timer_Manager.h 완전 타입 보임) 에서만 생성.
// ─────────────────────────────────────────────────────────────────
CGameInstance::CGameInstance()  = default;
CGameInstance::~CGameInstance()
{
	// Worker 스레드를 반드시 JobSystem 소멸 전에 join.
	// (unique_ptr reset 자체가 CJobSystem::~CJobSystem()→Shutdown 을 호출하므로
	//  명시적 Shutdown 은 선택이지만, 매니저 간 종료 순서 명확화를 위해 호출.)
	Shutdown_Engine();
	// 이후 멤버들이 순차적으로 reset 됨 (unique_ptr 역순).
}

// ───────────────── RHI 포워딩 게터 ─────────────────
// CEngineApp::Get() 은 엔진 내부에서만 유효. Client 는 CGameInstance 경유로 접근.
IRHIDevice* CGameInstance::Get_RHIDevice()
{
    return &CEngineApp::Get().GetDevice();
}
DX11Shader* CGameInstance::Get_MeshShader()
{
    return CEngineApp::Get().GetMeshShader();
}
DX11Pipeline* CGameInstance::Get_MeshPipeline()
{
    return CEngineApp::Get().GetMeshPipeline();
}
CBlendStateCache* CGameInstance::Get_BlendStateCache()
{
    return CEngineApp::Get().GetBlendStateCache();
}

DX11Shader* CGameInstance::Get_FxSpriteShader()
{
	return CEngineApp::Get().GetFxSpriteShader();
}

DX11Pipeline* CGameInstance::Get_FxSpritePipeline()
{
	return CEngineApp::Get().GetFxSpritePipeline();
}

DX11Shader* CGameInstance::Get_FxMeshShader()
{
	return CEngineApp::Get().GetFxMeshShader();
}

DX11Pipeline* CGameInstance::Get_FxMeshPipeline()
{
	return CEngineApp::Get().GetFxMeshPipeline();
}

DX11Shader* CGameInstance::Get_UIPlaneShader()
{
    return CEngineApp::Get().GetUIPlaneShader();
}

DX11Pipeline* CGameInstance::Get_UIPlanePipeline()
{
    return CEngineApp::Get().GetUIPlanePipeline();
}

DX11Shader* CGameInstance::Get_ContactShadowShader()
{
	return CEngineApp::Get().GetContactShadowShader();
}

DX11Pipeline* CGameInstance::Get_ContactShadowPipeline()
{
	return CEngineApp::Get().GetContactShadowPipeline();
}

bool_t CGameInstance::Preload_ModelResource(const char* pPath)
{
	if (!pPath || pPath[0] == '\0')
		return false;

	return CEngineApp::Get().GetResourceCache().LoadModel(Get_RHIDevice(), pPath) != nullptr;
}

bool_t CGameInstance::Preload_TextureResource(const wchar_t* pPath)
{
	if (!pPath || pPath[0] == L'\0')
		return false;

	return CEngineApp::Get().GetResourceCache().LoadTexture(
		pPath,
		Engine::eTexColorSpace::ShaderLocalSRGB) != nullptr;
}

HRESULT CGameInstance::Initialize_Engine(uint32_t iNumScenes)
{
	m_pTimer_Manager = CTimer_Manager::Create();
	if (m_pTimer_Manager == nullptr)
		return E_FAIL;

	m_pScene_Manager = CScene_Manager::Create();
	if (m_pScene_Manager == nullptr)
		return E_FAIL;

	m_pSound_Manager = CSound_Manager::Create();
	if (m_pSound_Manager == nullptr)
		return E_FAIL;

	m_pUI_Manager = CUI_Manager::Create();
	if (m_pUI_Manager == nullptr)
		return E_FAIL;
	//왜 불완전 형식? 
	m_pBlueprintRegistry = CEntityBlueprintRegistry::Create(iNumScenes);
	if (m_pBlueprintRegistry == nullptr)
		return E_FAIL;

	m_pJobSystem = std::unique_ptr<CJobSystem>(new CJobSystem());
	m_pJobSystem->Initialize(0);

	m_pFxAssetRegistry = std::unique_ptr<::CFxAssetRegistry>(new ::CFxAssetRegistry());

	//항상 Profiler생성! 
	m_pProfiler = CCPUProfiler::Create();
	if (!m_pProfiler) return E_FAIL;
	m_pProfilerOverlay = CProfilerOverlay::Create(m_pProfiler.get());
	if (!m_pProfilerOverlay) return E_FAIL;

	return S_OK;
}

void CGameInstance::Shutdown_Engine()
{
	if (m_pJobSystem)
		m_pJobSystem->Shutdown();

	if (m_pSound_Manager)
		m_pSound_Manager->StopAll();

	if (m_pUI_Manager)
		m_pUI_Manager->Shutdown();

	m_pProfilerOverlay.reset();
	m_pProfiler.reset();
	m_pFxAssetRegistry.reset();
	m_pScene_Manager.reset();
	m_pBlueprintRegistry.reset();
	m_pUI_Manager.reset();
	m_pSound_Manager.reset();
	m_pTimer_Manager.reset();
	m_pJobSystem.reset();
}

void CGameInstance::Tick_Engine()
{
	if (m_pSound_Manager) m_pSound_Manager->Tick();
}

f32_t CGameInstance::Get_TimeDelta(const wstring_t& strTimerTag)
{
	return m_pTimer_Manager->Get_TimeDelta(strTimerTag);
}

HRESULT CGameInstance::Add_Timer(const wstring_t& strTimerTag)
{
	return m_pTimer_Manager->Add_Timer(strTimerTag);
}

void CGameInstance::Update_TimeDelta(const wstring_t& strTimerTag)
{
	m_pTimer_Manager->UpdateTimeDelta(strTimerTag);
}
//=====Scene Manager======
HRESULT CGameInstance::Change_Scene(uint32_t iNextSceneID, unique_ptr<IScene> pNewScene)
{
	if (m_pScene_Manager == nullptr)
		return E_FAIL;

	m_pScene_Manager->Change_Scene(iNextSceneID, std::move(pNewScene));
	return S_OK;
}

HRESULT CGameInstance::Set_StaticScene(unique_ptr<IScene> pScene)
{
	return m_pScene_Manager->Set_StaticScene(std::move(pScene));
}

void CGameInstance::Clear_Resources(uint32_t iPrevSceneID)
{
	// 씬 종료 시 해당 씬의 Blueprint 전부 파기 (수업 Clear_Resources 관례 흡수).
	// 추후 ResourceCache::Unload_Scene 연계 예정.
	if (m_pBlueprintRegistry)
		m_pBlueprintRegistry->Clear_Scene(iPrevSceneID);
}

// ── Sound Manager ────────────────────────────────────────────────
void CGameInstance::PlaySoundOn(const wstring_t& strSoundKey, eSoundChannel eChannel, f32_t fVolume)
{
	if (m_pSound_Manager) m_pSound_Manager->PlaySoundOn(strSoundKey, eChannel, fVolume);
}

void CGameInstance::PlayEffect(const wstring_t& strSoundKey, f32_t fVolume)
{
	if (m_pSound_Manager) m_pSound_Manager->PlayEffect(strSoundKey, fVolume);
}

void CGameInstance::PlayBGM(const wstring_t& strSoundKey, f32_t fVolume)
{
	if (m_pSound_Manager) m_pSound_Manager->PlayBGM(strSoundKey, fVolume);
}

void CGameInstance::StopChannel(eSoundChannel eChannel)
{
	if (m_pSound_Manager) m_pSound_Manager->StopChannel(eChannel);
}

void CGameInstance::StopAllSounds()
{
	if (m_pSound_Manager) m_pSound_Manager->StopAll();
}

void CGameInstance::SetChannelVolume(eSoundChannel eChannel, f32_t fVolume)
{
	if (m_pSound_Manager) m_pSound_Manager->SetChannelVolume(eChannel, fVolume);
}

void CGameInstance::SetMasterVolume(f32_t fVolume)
{
	if (m_pSound_Manager) m_pSound_Manager->SetMasterVolume(fVolume);
}

// ── UI Manager ───────────────────────────────────────────────────
HRESULT CGameInstance::UI_Initialize(CWorld* pWorld, IRHIDevice* pDevice,
	uint32_t iWinSizeX, uint32_t iWinSizeY)
{
	if (!m_pUI_Manager) return E_FAIL;
	return m_pUI_Manager->Initialize(pWorld, pDevice, iWinSizeX, iWinSizeY);
}

void CGameInstance::UI_Bind_World(CWorld* pWorld)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Bind_World(pWorld);
}

void CGameInstance::UI_Shutdown()
{
	if (m_pUI_Manager) m_pUI_Manager->Shutdown();
}

void CGameInstance::UI_Render_Overlay(const Mat4& matVP)
{
	if (m_pUI_Manager) m_pUI_Manager->Render_Overlay(matVP);
}

void CGameInstance::UI_Render_Cursor()
{
	if (m_pUI_Manager)
		m_pUI_Manager->Render_Cursor();
}

bool_t CGameInstance::UI_Begin_RawImagePass(uint32_t iScreenWidth, uint32_t iScreenHeight, bool_t bPointSample)
{
	if (!m_pUI_Manager)
		return false;
	return m_pUI_Manager->Begin_RawImagePass(iScreenWidth, iScreenHeight, bPointSample);
}

void CGameInstance::UI_Draw_RawImage(void* pTextureSRV,
	f32_t fX, f32_t fY, f32_t fW, f32_t fH,
	const Vec4& vUVRect,
	const Vec4& vColor)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Draw_RawImage(pTextureSRV, fX, fY, fW, fH, vUVRect, vColor);
}

void CGameInstance::UI_End_RawImagePass()
{
	if (m_pUI_Manager)
		m_pUI_Manager->End_RawImagePass();
}

void CGameInstance::UI_Draw_RawImageCircle(
	void* pTextureSRV,
	f32_t fX, f32_t fY, f32_t fW, f32_t fH,
	const Vec4& vUVRect, const Vec4& vColor, u32_t iSegmentCount)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Draw_RawImageCircle(
			pTextureSRV, fX, fY, fW, fH,
			vUVRect, vColor, iSegmentCount);
}

void CGameInstance::UI_OnImGui_Tuner()
{
	if (m_pUI_Manager) m_pUI_Manager->OnImGui_Tuner();
}

void CGameInstance::UI_Set_ActiveLuaScreen(const char* pScreenID)
{
	if (m_pUI_Manager)
		m_pUI_Manager->SetActiveLuaUIScreen(pScreenID);
}

void CGameInstance::UI_Reload_Lua()
{
	if (m_pUI_Manager)
		m_pUI_Manager->ReloadLuaUI();
}

void CGameInstance::UI_Toggle_InGameShop()
{
	if (m_pUI_Manager)
		m_pUI_Manager->ToggleInGameShop();
}

void CGameInstance::UI_Set_InGameShopOpen(bool_t bOpen)
{
	if (m_pUI_Manager)
		m_pUI_Manager->SetInGameShopOpen(bOpen);
}

void CGameInstance::UI_Toggle_StatusPanel()
{
	if (m_pUI_Manager)
		m_pUI_Manager->ToggleStatusPanel();
}

void CGameInstance::UI_Set_StatusPanelOpen(bool_t bOpen)
{
	if (m_pUI_Manager)
		m_pUI_Manager->SetStatusPanelOpen(bOpen);
}

void CGameInstance::UI_Set_ShowHealthBars(bool_t b)
{
	if (m_pUI_Manager) m_pUI_Manager->Set_ShowHealthBars(b);
}

void CGameInstance::UI_Set_PlayerChampion(eChampion champ)
{
	if (m_pUI_Manager) m_pUI_Manager->Set_PlayerChampion(champ);
}

void CGameInstance::UI_Set_EnemyHoverCursor(bool_t bEnemyHover)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Set_EnemyHoverCursor(bEnemyHover);
}

void CGameInstance::UI_Set_AttackMode(bool_t bAttackMode)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Set_AttackMode(bAttackMode);
}

void CGameInstance::UI_Set_PingWheel(bool_t bVisible,
	f32_t fCenterX, f32_t fCenterY,
	f32_t fMouseX, f32_t fMouseY)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Set_PingWheel(bVisible, fCenterX, fCenterY, fMouseX, fMouseY);
}

void CGameInstance::UI_Push_MapPing(const Vec3& vWorldPos, u8_t iDirection)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_MapPing(vWorldPos, iDirection);
}

void CGameInstance::UI_Set_InGameBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser)
{
	if (m_pUI_Manager)
		m_pUI_Manager->SetInGameBuyItemCallback(pfn, pUser);
}

void CGameInstance::UI_Set_LevelSkillCallback(void(*pfn)(void*, u8_t), void* pUser)
{
	if (m_pUI_Manager)
		m_pUI_Manager->SetLevelSkillCallback(pfn, pUser);
}

void CGameInstance::UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
	u8_t iDamageType, bool_t bWasCrit, bool_t bKilled)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_DamageNumber(vWorldPos, fAmount, iDamageType, bWasCrit, bKilled);
}

void CGameInstance::UI_Push_WorldText(const Vec3& vWorldPos, const char* pText,
	const Vec4& vColor, f32_t fLifetime)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_WorldText(vWorldPos, pText, vColor, fLifetime);
}

void CGameInstance::UI_Push_GoldText(const Vec3& vWorldPos, u32_t iGoldAmount,
	f32_t fLifetime)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_GoldText(vWorldPos, iGoldAmount, fLifetime);
}

void CGameInstance::UI_Push_KillFeedBanner(eChampion eSourceChampion,
	eChampion eTargetChampion, u8_t iObjectKind, bool_t bSourceAlly, const char* pMessage)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_KillFeedBanner(
			eSourceChampion,
			eTargetChampion,
			iObjectKind,
			bSourceAlly,
			pMessage
		);
}

void CGameInstance::UI_RecordGameContextChampionKill(u8_t iSourceTeam, u8_t iTargetTeam,
	bool_t bLocalSource, bool_t bLocalTarget)
{
	if (m_pUI_Manager)
		m_pUI_Manager->RecordGameContextChampionKill(
			iSourceTeam,
			iTargetTeam,
			bLocalSource,
			bLocalTarget);
}

void CGameInstance::UI_RecordGameContextMinionKill()
{
	if (m_pUI_Manager)
		m_pUI_Manager->RecordGameContextMinionKill();
}

void CGameInstance::UI_SetGameContextServerTimeMs(u64_t iServerTimeMs)
{
	if (m_pUI_Manager)
		m_pUI_Manager->SetGameContextServerTimeMs(iServerTimeMs);
}

HRESULT CGameInstance::Add_Blueprint(uint32_t ISceneID, const std::wstring& strKey, CEntityBlueprint blueprint)
{
	return m_pBlueprintRegistry->Add_Blueprint(ISceneID, strKey, move(blueprint));
}

EntityID CGameInstance::Clone_Entity(uint32_t ISceneID, const std::wstring& strKey, CWorld& world)
{
	return m_pBlueprintRegistry->Clone_Entity(ISceneID, strKey, world);
}

// 수업 Clone(void* pArg) 관례 — per-instance 초기화용. Client 가 struct 포인터 전달.
EntityID CGameInstance::Clone_Entity(uint32_t ISceneID, const std::wstring& strKey,
	CWorld& world, const void* pArg)
{
	return m_pBlueprintRegistry->Clone_Entity(ISceneID, strKey, world, pArg);
}

void CGameInstance::Profiler_Begin()
{
	if (m_pProfiler)
		m_pProfiler->BeginFrame();
}

void CGameInstance::Profiler_End()
{
	if (m_pProfiler)
		m_pProfiler->EndFrame();
}

void CGameInstance::Profiler_Toggle()
{
	if (m_pProfilerOverlay)
		m_pProfilerOverlay->Toggle();
}

bool_t CGameInstance::Profiler_IsOverlayVisible() const
{
	return m_pProfilerOverlay && m_pProfilerOverlay->IsVisible();
}

void CGameInstance::Profiler_DrawOverlay()
{
	if (m_pProfilerOverlay)
		m_pProfilerOverlay->Draw();
}

bool_t CGameInstance::Profiler_SaveJson(const char* pPath)
{
	return m_pProfilerOverlay && m_pProfilerOverlay->CaptureToJson(pPath, true);
}
