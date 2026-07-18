// Scene_InGameLifecycle.cpp — CScene_InGame의 진입 부트스트랩(OnEnter)/종료(OnExit)/ECS 엔티티 조립 책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp에서 verbatim 이동. 동작/시그니처/호출순서 불변.
// 설계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md (OnEnter 부트스트랩→Installer는 Stage 2)
#define _CRT_SECURE_NO_WARNINGS

#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/EventApplier.h"
#include "Network/Client/GameSessionClient.h"
#include "Network/Client/SnapshotApplier.h"
#include "Replay/ReplayPlayer.h"

#include <Windows.h>
#include "Scene/GameplayQuery.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/LobbyRosterHelpers.h"
#include "Scene/RenderVisibilityFilter.h"
#include "Scene/Scene_InGame.h"
#include "UI/AttackSpeedLab.h"
#include "Scene/Scene_InGameInternal.h"
#include "Scene/Scene_Editor.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/AmbientProp_Manager.h"
#include "Manager/Bush_Manager.h"
#include "Map/MapDataIO.h"
#include "Core/CInput.h"
#include "WintersPaths.h"
#include "GameInstance.h"
#include "GamePlay/LoLMatchContextRuntime.h"
#include "GamePlay/LoLUIContentRegistry.h"
#include "ECS/Components/CoreComponents.h"   // ColliderComponent
#include "GamePlay/Systems/LocalUnitAISystem.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"
#include "Shared/GameSim/Systems/Turret/StructureProjectileSystem.h"
#include "ECS/Systems/NavigationThrottleSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/ConcealmentVolumeIndex.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ProfilerAPI.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "Manager/Navigation/Pathfinder.h"

// [Phase T] UI Panels + DebugDrawSystem
#include "UI/AIDebugPanel.h"
#include "UI/CombatDebugPanel.h"
#include "UI/MapTunerPanel.h"
#include "UI/RenderDebug.h"
#include "UI/DebugDrawSystem.h"
#include "UI/SkillTimingPanel.h"
#include "UI/ChampionTuner.h"
#include "UI/EffectTuner.h"
#include "UI/WfxEffectToolPanel.h"
#include "UI/MinimapPanel.h"
#include "Network/Client/NetworkEventTrace.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

#include "Resource/Animator.h"
#include "Resource/Animation.h"

#include "GameObject/ChampionDef.h"
#include "GameObject/Champion/Zed/ZedFxPresets.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Irelia/Irelia_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "GameObject/ChampionSpawnService.h"
#include "GameObject/Champion/Viego/Viego_FxPresets.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/SkillDefVisualDataAdapter.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/LobbyTypes_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

// [Phase T-8] FX / Status / Irelia Blade / Ult Wave
#include "GamePlay/Systems/LocalStatusEffectSystem.h"
#include "Shared/GameSim/Components/GameplayComponents.h"   // Stun/Slow/Disarm
#include "GameObject/FX/FxSystem.h"
#include "Renderer/RHISceneRenderer.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "Renderer/FxStaticMeshRenderer.h"

#include "ECS/Components/MeshGroupVisibilityComponent.h"

#include "RHI/IRHIDevice.h"
#include "RHI/RHITextureLoader.h"
#include "RHI/RHITypes.h"
#include "Renderer/FogOfWarRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <functional>
#include <vector>

namespace
{
    void UI_SendBuyItemCommand(void* pUser, u16_t itemId)
    {
        CScene_InGame* pScene = static_cast<CScene_InGame*>(pUser);
        if (!pScene || !pScene->GetCommandSerializer() || !pScene->GetNetworkView())
            return;

        pScene->GetCommandSerializer()->SendBuyItem(*pScene->GetNetworkView(), itemId);
    }

    void UI_SendLevelSkillCommand(void* pUser, u8_t slot)
    {
        CScene_InGame* pScene = static_cast<CScene_InGame*>(pUser);
        if (!pScene || !pScene->GetCommandSerializer() || !pScene->GetNetworkView())
            return;

        pScene->GetCommandSerializer()->SendLevelSkill(*pScene->GetNetworkView(), slot);
    }

    void UI_SendInventoryReorderCommand(
        void* pUser, u8_t sourceSlot, u8_t targetSlot, u16_t expectedItemId)
    {
        CScene_InGame* pScene = static_cast<CScene_InGame*>(pUser);
        if (!pScene || !pScene->GetCommandSerializer() || !pScene->GetNetworkView())
            return;

        pScene->GetCommandSerializer()->SendReorderItem(
            *pScene->GetNetworkView(),
            sourceSlot,
            targetSlot,
            expectedItemId);
    }

    RHITextureHandle CreateDefaultRHITexture(IRHIDevice* pDevice, const char* pszDebugName)
    {
        if (!pDevice)
            return {};

        const u32_t whitePixel = 0xFFFFFFFFu;
        RHITextureDesc desc{};
        desc.width = 1;
        desc.height = 1;
        desc.format = eRHIFormat::R8G8B8A8_UNorm;
        desc.usageFlags = static_cast<u32_t>(eRHITextureUsage::ShaderResource);
        desc.debugName = pszDebugName;
        return pDevice->CreateTexture(desc, &whitePixel, sizeof(whitePixel));
    }

    bool_t ShouldUseFastSmokeBootstrap()
    {
        return HasCommandLineToken(L"--banpick-smoke")
            && !HasCommandLineToken(L"--smoke-full-ingame");
    }

    bool_t ShouldSkipSmokeMap()
    {
        return ShouldUseFastSmokeBootstrap()
            && !HasCommandLineToken(L"--smoke-full-map");
    }

    bool_t ShouldUseFullLayerMap()
    {
        return HasCommandLineToken(L"--map-full-layers")
            || HasCommandLineToken(L"--map11-full-layers")
            || HasCommandLineToken(L"--map11-brush-test");
    }

    const char* SelectMapMeshPath()
    {
        const ClientData::MapRuntimeVisualDefinition& visual =
            ClientData::GetMapRuntimeVisualDefinition();
        return ShouldUseFullLayerMap()
            ? visual.fullLayerMapMesh.resourceRelativePath
            : visual.baseMapMesh.resourceRelativePath;
    }

    const wchar_t* SelectMapSurfacePath()
    {
        const ClientData::MapRuntimeVisualDefinition& visual =
            ClientData::GetMapRuntimeVisualDefinition();
        return ShouldUseFullLayerMap()
            ? visual.fullLayerMapSurface.resourceRelativePath
            : visual.baseMapSurface.resourceRelativePath;
    }

    void SendServerInGameReady()
    {
        MatchContext& context = Client::CLoLMatchContextRuntime::Instance().Context();
        CGameSessionClient& session = CGameSessionClient::Instance();

        if (!context.bUseNetworkRoster || !session.IsConnected())
            return;

        session.Pump();
        if (session.HasLobbyState())
            session.CopyLobbyToMatchContext(context);

        if (context.MySessionId == 0 || context.MySlotId == kInvalidGameRosterSlot)
        {
            return;
        }

		const bool_t bSent = session.SendLobbyCommand(
			Shared::Schema::LobbyCommandKind::SetReady,
			context.MySlotId,
			eChampion::END,
			0,
			1u);

		(void)bSent;
	}
}

const char* CScene_InGame::GetSelectedMapMeshPath()
{
    return SelectMapMeshPath();
}

const wchar_t* CScene_InGame::GetSelectedMapSurfacePath()
{
    return SelectMapSurfacePath();
}

void CScene_InGame::ConfigureDefaultMapTransform(CTransform& transform)
{
    transform.SetPosition(0.f, 0.f, 0.f);
    transform.SetScale({ -0.01f, 0.01f, 0.01f });
    transform.SetRotation({ 0.f, DirectX::XMConvertToRadians(-135.f), 0.f });
}

void CScene_InGame::AdoptPreparedFxMeshRenderer(
    unique_ptr<Engine::CFxStaticMeshRenderer> renderer)
{
    if (renderer)
        m_pFxMeshRenderer = std::move(renderer);
}

void CScene_InGame::AdoptPreparedFxSystem(unique_ptr<CFxSystem> fxSystem)
{
    if (fxSystem)
        m_pFxSystem = std::move(fxSystem);
}

bool CScene_InGame::OnEnter()
{
    UI::CAttackSpeedLab::ResetRuntime();
    const MatchContext& gameContext = Client::CLoLMatchContextRuntime::Instance().Context();
    Winters::DevSmoke::Log(
        "[InGameBootstrap] enter useNetworkRoster=%u selected=%u mySlot=%u sid=%u net=%u\n",
        gameContext.bUseNetworkRoster ? 1u : 0u,
        static_cast<u32_t>(gameContext.SelectedChampion),
        static_cast<u32_t>(gameContext.MySlotId),
        gameContext.MySessionId,
        gameContext.MyNetId);

    InitializeNetworkSession();
    m_bNetworkAuthoritativeGameplay =
        m_bUsingSharedNetwork || m_bReplayPlaybackMode;
    Winters::DevSmoke::Log("[InGameBootstrap] network initialized\n");
    Winters::DevSmoke::Log(
        "[InGameBootstrap] network authoritative gameplay=%u\n",
        m_bNetworkAuthoritativeGameplay ? 1u : 0u);

    CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem();

    m_pScheduler = std::unique_ptr<CSystemSchedular>(new CSystemSchedular());
    m_pScheduler->Initialize(pJS);
    m_World.Initialize_Spatial(DefaultSpatialGridDesc());

    {
        auto pTx = CTransformSystem::Create();
        pTx->Set_JobSystem(pJS);
        m_pScheduler->RegisterSystem(std::move(pTx));
    }

    {
        auto pSpatial = Engine::CSpatialHashSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pSpatial));
    }

    {
        auto pStatus = CLocalStatusEffectSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pStatus));
    }

    {
        auto pVision = Engine::CVisionSystem::Create(m_World.Get_SpatialIndex(), &m_ConcealmentIndex);
        const UI::MinimapProjection& MinimapProjection = UI::GetDefaultMinimapProjection();
        Engine::CVisionSystem::FowProjection FowProjection{};
        FowProjection.vWorldAtUv00 = MinimapProjection.vWorldAtUv00;
        FowProjection.vWorldAtUv10 = MinimapProjection.vWorldAtUv10;
        FowProjection.vWorldAtUv01 = MinimapProjection.vWorldAtUv01;
        pVision->SetFowProjection(FowProjection);
        m_pVisionSystem = pVision.get();
        m_pScheduler->RegisterSystem(std::move(pVision));
    }

    CStructure_Manager::Get()->Initialize(&m_World);
    CJungle_Manager::Get()->Initialize(&m_World);
    CMinion_Manager::Get()->Initialize(&m_World);
    CBush_Manager::Get()->Initialize(&m_World);
    CMinion_Manager::Get()->PrewarmNetworkVisualResources();

    wchar_t stagePath[MAX_PATH] = {};
    if (CMapDataIO::Get_StagePathW(1, stagePath, MAX_PATH))
    {
        const HRESULT hrStage = CMapDataIO::Load_Stage(stagePath);
        if (SUCCEEDED(hrStage))
        {
            Winters::DevSmoke::Log("[InGameBootstrap] Stage1 loaded\n");
        }
        else
        {
            char msg[384]{};
            sprintf_s(msg, "[InGameBootstrap] Stage1 load FAILED hr=0x%08X path=%ls\n",
                static_cast<unsigned>(hrStage), stagePath);
            OutputDebugStringA(msg);
        }
    }
    else
    {
        OutputDebugStringA("[InGameBootstrap] Stage1 path resolve FAILED\n");
    }

    BootstrapChampionModules();
    Winters::DevSmoke::Log("[InGameBootstrap] champion modules bootstrapped\n");

    m_pCamera = CDynamicCamera::Create(
        { 0.f, 10.f, -10.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f, 0.f });

    bool_t bMapInit = false;
    if (ShouldSkipSmokeMap())
    {
        Winters::DevSmoke::Log("[InGameBootstrap] map init skipped for smoke\n");
    }
    else
    {
        Winters::DevSmoke::Log("[InGameBootstrap] map init begin\n");
        bMapInit = m_Map.Initialize(GetSelectedMapMeshPath(), L"Shaders/Mesh3D.hlsl");
        if (bMapInit)
        {
            const bool_t bGrassTintReady = m_Map.SetGrassTintMaterialByName(
                "Maps/KitPieces/SRX/Base/Models/LevelProp/Materials/VertexDeform_inst",
                L"Texture/MAP/output/textures/assets/maps/info/map11/grasstint_srx.png");
            Winters::DevSmoke::Log(
                "[InGameBootstrap] map grass tint ready=%u\n",
                bGrassTintReady ? 1u : 0u);
        }
        Winters::DevSmoke::Log("[InGameBootstrap] map init done ok=%u\n", bMapInit ? 1u : 0u);
    }
    ConfigureDefaultMapTransform(m_MapTransform);
    m_vMapRotation = m_MapTransform.GetRotation();
    InitializeMapSurfaceSampler(bMapInit, GetSelectedMapSurfacePath());
    for (u32_t i = 0; i < CBush_Manager::Get()->Get_Count(); ++i)
    {
        Vec3 position = CBush_Manager::Get()->Get_Position(i);
        if (TryProjectToMapSurface(position, 0.02f))
            CBush_Manager::Get()->Set_Position(i, position);
    }
    CAmbientProp_Manager::Get()->Spawn(
        [this](Vec3& pos) { (void)TryProjectToMapSurface(pos, 0.02f); });

    Winters::DevSmoke::Log("[InGameBootstrap] CreateECSEntities begin\n");
    CreateECSEntities();
    Winters::DevSmoke::Log("[InGameBootstrap] CreateECSEntities done player=%u total=%u\n",
        static_cast<u32_t>(m_PlayerEntity),
        m_World.GetEntityCount());
    Winters::DevSmoke::Log("[InGameBootstrap] Stage bushes=%u\n",
        CBush_Manager::Get()->Get_Count());
    m_ConcealmentIndex.Build(m_World);
    if (m_pVisionSystem)
        m_pVisionSystem->ForceRebuildNextFrame();

    const eChampion selectedChampion = GetPlayerChampionId();
    if (Client::CLoLMatchContextRuntime::Instance().Context().bUseNetworkRoster
        || selectedChampion == eChampion::RIVEN
        || selectedChampion == eChampion::EZREAL
        || selectedChampion == eChampion::FIORA
        || selectedChampion == eChampion::JAX
        || selectedChampion == eChampion::ANNIE
        || selectedChampion == eChampion::ASHE
        || selectedChampion == eChampion::YONE)
    {
        BindPlayerToECSChampion(m_PlayerEntity);
    }

    CGameInstance::Get()->UI_Bind_World(&m_World);
    Client::RegisterLoLUIContent(*CGameInstance::Get());
    for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
    {
        const GameRosterSlot& slot = gameContext.Roster[i];
        if (IsSlotOccupied(slot))
            (void)UI::CMinimapPanel::PrewarmChampionPortrait(slot.champion);
    }
    CGameInstance::Get()->UI_Set_InGameBuyItemCallback(&UI_SendBuyItemCommand, this);
    CGameInstance::Get()->UI_Set_LevelSkillCallback(&UI_SendLevelSkillCommand, this);
    CGameInstance::Get()->UI_Set_InventoryReorderCallback(
        &UI_SendInventoryReorderCommand, this);

    CMinion_Manager::Get()->Set_Enabled(!m_bNetworkAuthoritativeGameplay);
    wchar_t navGridPath[MAX_PATH] = {};
    if (CMapDataIO::GetNavGridPathW(1, navGridPath, MAX_PATH))
    {
        m_pNavGrid = Engine::CNavGrid::LoadFromFile(navGridPath);
    }

    if (!m_pNavGrid)
    {
        m_pNavGrid = CreateMapNavGrid();
        m_pNavGrid->SetAllWalkable(true);
    }


    if (m_pNavGrid)
        Engine::CPathfinder::PrewarmReachabilityCache(m_pNavGrid.get());

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pNav = CNavigationSystem::Create();
        pNav->Set_Grid(m_pNavGrid.get());
        pNav->Set_JobSystem(pJS);
        m_pScheduler->RegisterSystem(std::move(pNav));
    }
    else
    {
        Winters::DevSmoke::Log("[InGameBootstrap] client navigation skipped for network authority\n");
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pAI = CLocalUnitAISystem::Create();
        pAI->Set_JobSystem(pJS);
        m_pScheduler->RegisterSystem(std::move(pAI));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pNavigationThrottle = Engine::CNavigationThrottleSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pNavigationThrottle));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pTurretAI = GameplayTurret::CTurretAISystem::Create();
        m_pScheduler->RegisterSystem(std::move(pTurretAI));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pStructureProjectiles = GameplayTurret::CStructureProjectileSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pStructureProjectiles));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pBT = Engine::CBehaviorTreeSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pBT));
    }

    {
        auto pYoneSoul = CYoneSoulSpawnSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pYoneSoul));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pMCTS = Engine::CMCTSSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pMCTS));
    }

    Mark_StructuresOnNavGrid();

    {
        CGameInstance* pGI = CGameInstance::Get();
        IRHIDevice* pRhiDevice = pGI->Get_RHIDevice();
        const bool_t bUseDX12RHI = pRhiDevice && pRhiDevice->GetBackend() == eRHIBackend::DX12;
        const ClientData::MapRuntimeVisualDefinition& mapVisual =
            ClientData::GetMapRuntimeVisualDefinition();

        m_pRHISceneRenderer = CRHISceneRenderer::Create(pRhiDevice);
        if (!m_pRHISceneRenderer)
            OutputDebugStringA("[InGameBootstrap] CRHISceneRenderer unavailable\n");

        if (bUseDX12RHI)
        {
            m_pRHIUtilityPlaneRenderer = CRHIFxSpriteRenderer::Create(pRhiDevice);
            m_hRHIAttackRangeTex = RHI_CreateTextureFromFile(
                pRhiDevice,
                mapVisual.attackRangeTexture.resourceRelativePath,
                "RHI_AttackRangeTexture");
            if (!m_hRHIAttackRangeTex.IsValid())
                m_hRHIAttackRangeTex = CreateDefaultRHITexture(pRhiDevice, "RHI_AttackRangeFallback");

        }
        else
        {
            m_pAttackRangePlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->Get_UIPlaneShader(),
                pGI->Get_UIPlanePipeline());

            m_pAttackRangeTex = CTexture::Create(
                pRhiDevice,
                mapVisual.attackRangeTexture.resourceRelativePath,
                eTexSamplerMode::Clamp);

            if (!m_pAttackRangeTex)
            {
                MSG_BOX("Attack Range Texture load failed");
                m_pAttackRangeTex = CTexture::CreateDefault(pRhiDevice);
            }

            if (m_pAttackRangePlane && m_pAttackRangeTex)
            {
                m_pAttackRangePlane->SetTexture(m_pAttackRangeTex.get());
                m_pAttackRangePlane->SetBlendCache(
                    pGI->Get_BlendStateCache(),
                    eBlendPreset::AlphaBlend);
            }

            m_pContactShadowPlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->Get_ContactShadowShader(),
                pGI->Get_ContactShadowPipeline());

            if (m_pContactShadowPlane)
            {
                m_pContactShadowPlane->SetBlendCache(
                    pGI->Get_BlendStateCache(),
                    eBlendPreset::AlphaBlend);
                m_pContactShadowPlane->SetFxParams(
                    { 0.015f, 0.018f, 0.020f, 0.44f },
                    { 0.f, 0.f, 1.f, 1.f },
                    { 0.f, 0.f },
                    0.003f,
                    0.f);
            }
        }

        if (!bUseDX12RHI)
        {
            m_pWhiteTexture = CTexture::CreateDefault(pRhiDevice);
            m_pNormalPass = Engine::CNormalPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            m_pSSAOPass = Engine::CSSAOPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            if (m_pSSAOPass)
            {
                m_pSSAOPass->SetEnabled(false);
                m_pSSAOPass->SetRadius(1.1f);
                m_pSSAOPass->SetIntensity(1.25f);
            }
            m_pPostFxPass = Engine::CPostFxPass::Create(
                pRhiDevice,
                g_iWinSizeX,
                g_iWinSizeY);
            if (m_pPostFxPass)
                m_pPostFxPass->SetEnabled(false);
            m_pFogOfWarRenderer = CFogOfWarRenderer::Create(
                pRhiDevice, Engine::CVisionSystem::FOW_TEX_DIM);
        }
    }
    Winters::DevSmoke::Log("[InGameBootstrap] render helpers ready\n");

    {
        CGameInstance* pGI = CGameInstance::Get();
        IRHIDevice* pRhiDevice = pGI->Get_RHIDevice();

        if (!m_pFxSystem)
        {
            m_pFxSystem = CFxSystem::Create(
                pRhiDevice,
                pGI->Get_BlendStateCache());
        }
        m_pFxBeamSystem = CFxBeamSystem::Create(
            pRhiDevice,
            pGI->Get_BlendStateCache());

        if (!m_pFxMeshRenderer)
        {
            m_pFxMeshRenderer = Engine::CFxStaticMeshRenderer::Create(
                pRhiDevice,
                pGI->Get_MeshShader(),
                pGI->Get_MeshPipeline(),
                pGI->Get_FxMeshShader(),
                pGI->Get_FxMeshPipeline(),
                pGI->Get_BlendStateCache());
        }
        if (m_pEventApplier)
            m_pEventApplier->SetFxMeshRenderer(m_pFxMeshRenderer.get());
        m_pFxMeshSystem = CFxMeshSystem::Create(m_pFxMeshRenderer.get());

        m_pIreliaBladeSystem = CIreliaBladeSystem::Create();
        m_pWindWallSystem = CWindWallSystem::Create();
        m_pYasuoProjectileSystem = CYasuoProjectileSystem::Create();
        m_pPendingHitSystem = CPendingHitSystem::Create();
        m_pKalistaProjectileSystem = CKalistaProjectileSystem::Create();
        m_pKalistaRendSystem = CKalistaRendSystem::Create();
    }
    Winters::DevSmoke::Log("[InGameBootstrap] skill fx systems ready\n");

    if (m_pFxMeshRenderer && ShouldUseFastSmokeBootstrap())
    {
        Winters::DevSmoke::Log("[InGameBootstrap] FX mesh preload skipped for smoke\n");
    }
    else if (m_pFxMeshRenderer)
    {
        const ClientData::FxMeshPreloadVisualPack& preloadPack =
            ClientData::GetFxMeshPreloadVisualPack();
        for (u32_t i = 0u; i < preloadPack.entryCount; ++i)
        {
            const ClientData::FxMeshPreloadVisualDefinition& preload =
                preloadPack.entries[i];
            if (!preload.mesh.resourceRelativePath || !preload.texture.resourceRelativePath)
                continue;
            m_pFxMeshRenderer->PreloadMesh(
                preload.mesh.resourceRelativePath,
                preload.texture.resourceRelativePath);
        }
    }

    CGameInstance::Get()->UI_Set_PlayerActorContent(static_cast<u8_t>(GetPlayerChampionId()));

    Winters::DevSmoke::Log("[InGameBootstrap] done player=%u champion=%u\n",
        static_cast<u32_t>(m_PlayerEntity),
        static_cast<u32_t>(GetPlayerChampionId()));

    if (m_bReplayPlaybackMode)
    {
        std::string error;
        m_pReplayPlayer = CReplayPlayer::LoadFromFile(m_strReplayPath, error);
        if (!m_pReplayPlayer)
        {
            m_strReplayStatus = error.empty() ? "Replay load failed" : error;
            CGameInstance::Get()->SetLoadingCursorMode(false);
            return true;
        }

        m_strReplayStatus = "Replay loaded";
        CGameInstance::Get()->SetLoadingCursorMode(false);
        return true;
    }

    SendServerInGameReady();
    // Loading scenes intentionally keep the responsive native cursor visible
    // through all synchronous bootstrap work. Switch back only after the client
    // has completed its final ready/replay step and can render the next frame.
    CGameInstance::Get()->SetLoadingCursorMode(false);
    return true;
}

void CScene_InGame::AssignPureECSChampionAlias(eChampion id, EntityID entity)
{
    switch (id)
    {
    case eChampion::SYLAS:
        m_SylasEntity = entity;
        break;
    case eChampion::FIORA:
        m_FioraEntity = entity;
        break;
    case eChampion::JAX:
        m_JaxEntity = entity;
        break;
    case eChampion::ANNIE:
        m_AnnieEntity = entity;
        break;
    case eChampion::ASHE:
        m_AsheEntity = entity;
        break;
    case eChampion::YONE:
        m_YoneEntity = entity;
        break;
    default:
        break;
    }
}

void CScene_InGame::ClearPureECSChampionAlias(EntityID entity)
{
    if (m_SylasEntity == entity)
        m_SylasEntity = NULL_ENTITY;
    if (m_FioraEntity == entity)
        m_FioraEntity = NULL_ENTITY;
    if (m_JaxEntity == entity)
        m_JaxEntity = NULL_ENTITY;
    if (m_AnnieEntity == entity)
        m_AnnieEntity = NULL_ENTITY;
    if (m_AsheEntity == entity)
        m_AsheEntity = NULL_ENTITY;
    if (m_YoneEntity == entity)
        m_YoneEntity = NULL_ENTITY;
}

void CScene_InGame::CreateMapEntity()
{
    if (m_MapEntity != NULL_ENTITY)
        return;

    m_MapEntity = m_World.CreateEntity();
    TransformComponent mapTf;
    mapTf.m_LocalPosition = m_MapTransform.GetPosition();
    mapTf.m_LocalRotation = m_MapTransform.GetRotation();
    mapTf.m_LocalScale = m_MapTransform.GetScale();
    m_World.AddComponent<TransformComponent>(m_MapEntity, mapTf);

    RenderComponent mapRc;
    mapRc.pRenderer = &m_Map;
    mapRc.bVisible = true;
    mapRc.bAnimated = false;
    m_World.AddComponent<RenderComponent>(m_MapEntity, mapRc);

    Winters::DevSmoke::Log(
        "[InGameMap] entity=%u pos=(%.2f,%.2f,%.2f) scale=(%.2f,%.2f,%.2f)\n",
        static_cast<u32_t>(m_MapEntity),
        mapTf.m_LocalPosition.x,
        mapTf.m_LocalPosition.y,
        mapTf.m_LocalPosition.z,
        mapTf.m_LocalScale.x,
        mapTf.m_LocalScale.y,
        mapTf.m_LocalScale.z);
}

void CScene_InGame::BindPlayerToECSChampion(EntityID entity)
{
    if (entity == NULL_ENTITY)
    {
        Winters::DevSmoke::Log("[InGameBind] skipped null entity\n");
        return;
    }
    if (!m_World.HasComponent<RenderComponent>(entity))
    {
        Winters::DevSmoke::Log("[InGameBind] entity=%u missing RenderComponent\n", static_cast<u32_t>(entity));
        return;
    }
    if (!m_World.HasComponent<TransformComponent>(entity))
    {
        Winters::DevSmoke::Log("[InGameBind] entity=%u missing TransformComponent\n", static_cast<u32_t>(entity));
        return;
    }

    auto& rc = m_World.GetComponent<RenderComponent>(entity);
    if (!rc.pRenderer)
    {
        Winters::DevSmoke::Log("[InGameBind] entity=%u missing renderer\n", static_cast<u32_t>(entity));
        return;
    }

    m_pPlayerRenderer = rc.pRenderer;
    m_pPlayerTransform = &m_PlayerEntityTransformCache;

    eChampion championId = eChampion::NONE;
    if (m_World.HasComponent<ChampionComponent>(entity))
    {
        const ChampionComponent& champion = m_World.GetComponent<ChampionComponent>(entity);
        championId = champion.id;
        // 네트워크 경로에서도 로컬 플레이어 팀이 채워지도록 설정한다.
        // (이전엔 로스터 경로에서만 대입되어 Red 진영 클라가 Blue 기본값에 머물렀다.)
        m_PlayerTeam = champion.team;
    }
    if (m_World.HasComponent<FormOverrideComponent>(entity))
    {
        const auto& form = m_World.GetComponent<FormOverrideComponent>(entity);
        if (form.bActive &&
            form.visualChampion != eChampion::NONE &&
            form.visualChampion != eChampion::END)
        {
            championId = form.visualChampion;
        }
    }

    const ChampionDef* cd = FindClientChampionDef(championId);

    if (cd && cd->animPrefix && cd->idleAnimKey && cd->runAnimKey)
    {
        m_PlayerIdleAnimStorage = std::string(cd->animPrefix) + cd->idleAnimKey;
        m_PlayerRunAnimStorage = std::string(cd->animPrefix) + cd->runAnimKey;
        m_pPlayerIdleAnim = m_PlayerIdleAnimStorage.c_str();
        m_pPlayerRunAnim = m_PlayerRunAnimStorage.c_str();
    }
    else
    {
        m_pPlayerIdleAnim = "riven_idle1";
        m_pPlayerRunAnim = "riven_run";
    }

    SyncPlayerEntityTransformFromECS();
    if (m_pCamera)
    {
        m_pCamera->SetFollowTarget(m_pPlayerTransform);
        m_pCamera->SetFollowMode(true);
        m_pCamera->SnapToTarget();
    }

    m_vPlayerDest = m_pPlayerTransform->GetPosition();
    m_pPlayerRenderer->PlayAnimationByName(m_pPlayerIdleAnim, true);

    Winters::DevSmoke::Log(
        "[InGameBind] entity=%u champion=%u idle=%s run=%s pos=(%.2f,%.2f,%.2f)\n",
        static_cast<u32_t>(entity),
        static_cast<u32_t>(championId),
        m_pPlayerIdleAnim ? m_pPlayerIdleAnim : "",
        m_pPlayerRunAnim ? m_pPlayerRunAnim : "",
        m_vPlayerDest.x,
        m_vPlayerDest.y,
        m_vPlayerDest.z);
}

void CScene_InGame::CreateECSEntities()
{
    MatchContext& context = Client::CLoLMatchContextRuntime::Instance().Context();
    CInGameRosterSpawner::EnsureLocalRosterFallback(context);
    m_PlayerEntity = NULL_ENTITY;

    InGameRosterSpawnDesc rosterDesc{
        m_World,
        m_pEntityIdMap.get(),
        m_bNetworkAuthoritativeGameplay,
        m_NetworkChampionPrevPos,
        [this](eChampion champion, eTeam team)
        {
            return SpawnChampionEntity(champion, team);
        },
        [this](eChampion champion, EntityID entity)
        {
            AssignPureECSChampionAlias(champion, entity);
        }
    };

    const InGameRosterSpawnResult rosterResult =
        CInGameRosterSpawner::SpawnFromContext(rosterDesc, context);
    m_PlayerEntity = rosterResult.playerEntity;

    CreateMapEntity();

    if (m_PlayerEntity == NULL_ENTITY)
    {
        Winters::DevSmoke::Log("[ECS:RosterOnly] no local player entity after roster creation\n");
        return;
    }

    BindPlayerToECSChampion(m_PlayerEntity);

    if (m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
        m_PlayerTeam = m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team;

    ReplayLastNetworkHelloIfShared();

    char dbg[192]{};
    sprintf_s(dbg, "[ECS:RosterOnly] created=%d total=%u player=%u champion=%u\n",
        rosterResult.bCreatedAny ? 1 : 0,
        m_World.GetEntityCount(),
        static_cast<u32_t>(m_PlayerEntity),
        static_cast<u32_t>(GetPlayerChampionId()));
    Winters::DevSmoke::Log("%s", dbg);
}

EntityID CScene_InGame::SpawnChampionEntity(eChampion champion, eTeam team)
{
    ChampionSpawnContext spawnContext{
        m_World,
        m_ChampionRenderers,
        m_NetworkChampionPrevPos,
        m_NetworkChampionMoveGraceSec,
        m_NetworkChampionMoving
    };

    ChampionSpawnRequest request{};
    request.champion = champion;
    request.team = team;

    return CChampionSpawnService::Spawn(spawnContext, request).entity;
}

void CScene_InGame::OnExit()
{
    Viego::Fx::StopAllSoulIdle(m_World);

    // ESC 강제 종료/메인 메뉴 이탈 포함 — 종료 산출물이 없으면 aborted로 저장
    // (S030 저장 보증 3/3; 정상 종료 시에는 이미 저장되어 no-op).
    SaveEndOfMatchArtifacts("aborted");

    CGameInstance::Get()->UI_Set_StatusPanelOpen(false);
    CGameInstance::Get()->UI_Set_PingWheel(false, 0.f, 0.f, 0.f, 0.f);
    m_bPingWheelActive = false;
    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);

    CGameInstance::Get()->UI_Set_InGameBuyItemCallback(nullptr, nullptr);
    CGameInstance::Get()->UI_Set_LevelSkillCallback(nullptr, nullptr);
    CGameInstance::Get()->UI_Set_InventoryReorderCallback(nullptr, nullptr);
    CGameInstance::Get()->UI_Clear_ActorHUDState();
    CGameInstance::Get()->UI_Clear_StatusPanelState();
    CGameInstance::Get()->UI_Clear_WorldHealthBars();
    CGameInstance::Get()->UI_Bind_World(nullptr);
    UI::CMinimapPanel::ShutdownRuntime();

    if (m_bUsingSharedNetwork)
    {
        CGameSessionClient::Instance().SetGameFrameCallback(nullptr);
    }
    else if (m_pNetwork)
    {
        m_pNetwork->Disconnect();
    }

    m_pCommandSerializer.reset();
    m_pEventApplier.reset();
    m_pSnapshotApplier.reset();
    m_pNetwork.reset();
    m_pNetworkView = nullptr;
    m_bUsingSharedNetwork = false;
    m_pEntityIdMap.reset();
    m_bNetworkAuthoritativeGameplay = false;

    m_pRHISceneRenderer.reset();
    m_Map.Shutdown();

    if (m_pFxBeamSystem)   m_pFxBeamSystem.reset();
    if (m_pFxMeshSystem)   m_pFxMeshSystem.reset();
    if (m_pFxMeshRenderer) m_pFxMeshRenderer.reset();

    CAmbientProp_Manager::Get()->Shutdown();
    m_ChampionRenderers.clear();
    m_NetworkChampionPrevPos.clear();
    m_NetworkChampionMoveGraceSec.clear();
    m_NetworkChampionMoving.clear();
    m_NetworkActorInterpStates.clear();
    m_uNetworkActorInterpSnapshotTick = 0;
    m_NetworkActionAnimStates.clear();
    m_pPostFxPass.reset();
    m_pSSAOPass.reset();
    m_pNormalPass.reset();
    m_pFogOfWarRenderer.reset();
    if (m_pVisionSystem)
        m_pVisionSystem->ClearFowLocalTeam();
    m_pVisionSystem = nullptr;
    m_ConcealmentIndex.Clear();
    m_pWhiteTexture.reset();
    m_pRHIUtilityPlaneRenderer.reset();
    if (IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice())
    {
        if (m_hRHIAttackRangeTex.IsValid())
            pDevice->DestroyTexture(m_hRHIAttackRangeTex);
    }
    m_hRHIAttackRangeTex = {};
    m_pAttackRangeTex.reset();
    m_pAttackRangePlane.reset();
    CMinion_Manager::Get()->Set_Enabled(false);
    CMinion_Manager::Get()->Shutdown();
    CBush_Manager::Get()->Shutdown();
    CJungle_Manager::Get()->Shutdown();
    CStructure_Manager::Get()->Shutdown();
}
