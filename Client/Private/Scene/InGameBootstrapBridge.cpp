#include "Scene/InGameBootstrapBridge.h"

#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/GameplayCollisionSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/MinionAISystem.h"
#include "ECS/Systems/MinionPerformanceSystem.h"
#include "ECS/Systems/MinionSeparationSystem.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/StatusEffectSystem.h"
#include "ECS/Systems/TurretAISystem.h"
#include "ECS/Systems/TurretProjectileSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "GameInstance.h"
#include "GameObject/ChampionSpawnService.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "Network/Client/CommandSerializer.h"
#include "Dev/SmokeLog.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Structure_Manager.h"
#include "Map/MapDataIO.h"
#include "Renderer/FogOfWarRenderer.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITextureLoader.h"
#include "Scene/InGameNetworkBridge.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/Scene_InGame.h"
#include "WintersPaths.h"

#include <Windows.h>
#include <cstdio>
#include <cwchar>

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

    bool_t HasCommandToken(const wchar_t* pToken)
    {
        const wchar_t* pCommandLine = GetCommandLineW();
        return pCommandLine && pToken && wcsstr(pCommandLine, pToken);
    }

    bool_t ShouldUseFastSmokeBootstrap()
    {
        return HasCommandToken(L"--banpick-smoke")
            && !HasCommandToken(L"--smoke-full-ingame");
    }

    bool_t ShouldSkipSmokeMap()
    {
        return ShouldUseFastSmokeBootstrap()
            && !HasCommandToken(L"--smoke-full-map");
    }

    void SeedPracticeBushesForBootstrap(CWorld& world)
    {
        struct BushSeed
        {
            Vec3 center;
            f32_t radius;
            u32_t bushId;
        };

        static const BushSeed kBushes[] = {
            { { -45.f, 0.f,  60.f }, 5.f, 1 },
            { { -55.f, 0.f,  45.f }, 4.f, 1 },
            { { -30.f, 0.f,  90.f }, 5.f, 2 },
            { { -10.f, 0.f,  10.f }, 4.f, 3 },
            { {  10.f, 0.f, -10.f }, 4.f, 4 },
            { {  45.f, 0.f, -60.f }, 5.f, 5 },
            { {  55.f, 0.f, -45.f }, 4.f, 5 },
            { {  30.f, 0.f, -90.f }, 5.f, 6 },
            { { -30.f, 0.f,  30.f }, 4.f, 7 },
            { {  30.f, 0.f, -30.f }, 4.f, 8 },
            { { -60.f, 0.f,   0.f }, 4.f, 9 },
            { {  60.f, 0.f,   0.f }, 4.f, 10 },
        };

        for (const BushSeed& bush : kBushes)
        {
            const EntityID entity = world.CreateEntity();
            BushVolumeComponent component{};
            component.center = bush.center;
            component.radius = bush.radius;
            component.bushId = bush.bushId;
            world.AddComponent<BushVolumeComponent>(entity, component);
        }
    }
}

bool CInGameBootstrapBridge::Enter(CScene_InGame& scene)
{
    const GameContext& gameContext = CGameInstance::Get()->Get_GameContext();
    Winters::DevSmoke::Log(
        "[InGameBootstrap] enter useNetworkRoster=%u selected=%u mySlot=%u sid=%u net=%u\n",
        gameContext.bUseNetworkRoster ? 1u : 0u,
        static_cast<u32_t>(gameContext.SelectedChampion),
        static_cast<u32_t>(gameContext.MySlotId),
        gameContext.MySessionId,
        gameContext.MyNetId);

    InGameNetworkBridgeDesc networkDesc{
        scene.m_World,
        gameContext,
        scene.m_pEntityIdMap,
        scene.m_pNetwork,
        scene.m_pNetworkView,
        scene.m_bUsingSharedNetwork,
        scene.m_pSnapshotApplier,
        scene.m_pEventApplier,
        scene.m_pCommandSerializer,
        [&scene](eChampion champion, eTeam team)
        {
            ChampionSpawnContext spawnContext{
                scene.m_World,
                scene.m_ChampionRenderers,
                scene.m_NetworkChampionPrevPos,
                scene.m_NetworkChampionMoveGraceSec,
                scene.m_NetworkChampionMoving
            };

            ChampionSpawnRequest request{};
            request.champion = champion;
            request.team = team;

            return CChampionSpawnService::Spawn(spawnContext, request).entity;
        },
        [&scene](EntityID entity, eChampion champion, eTeam)
        {
            ChampionSpawnContext spawnContext{
                scene.m_World,
                scene.m_ChampionRenderers,
                scene.m_NetworkChampionPrevPos,
                scene.m_NetworkChampionMoveGraceSec,
                scene.m_NetworkChampionMoving
            };
            if (CChampionSpawnService::AttachVisual(spawnContext, entity, champion) &&
                entity == scene.m_PlayerEntity)
            {
                scene.BindPlayerToECSChampion(entity);
            }
        },
        [&scene](EntityID entity)
        {
            scene.m_ChampionRenderers.erase(entity);
            scene.m_NetworkChampionPrevPos.erase(entity);
            scene.m_NetworkChampionMoveGraceSec.erase(entity);
            scene.m_NetworkChampionMoving.erase(entity);
            scene.m_NetworkActorInterpStates.erase(entity);
        },
        [&scene](EntityID entity)
        {
            scene.m_PlayerEntity = entity;
            scene.BindPlayerToECSChampion(scene.m_PlayerEntity);
        }
    };
    CInGameNetworkBridge::Initialize(networkDesc);
    scene.m_bNetworkAuthoritativeGameplay = scene.m_bUsingSharedNetwork;
    Winters::DevSmoke::Log("[InGameBootstrap] network initialized\n");
    Winters::DevSmoke::Log(
        "[InGameBootstrap] network authoritative gameplay=%u\n",
        scene.m_bNetworkAuthoritativeGameplay ? 1u : 0u);

    CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem();

    scene.m_pScheduler = std::unique_ptr<CSystemSchedular>(new CSystemSchedular());
    scene.m_pScheduler->Initialize(pJS);
    scene.m_World.Initialize_Spatial(LoLSpatialGridDesc());

    {
        auto pTx = CTransformSystem::Create();
        pTx->Set_JobSystem(pJS);
        scene.m_pScheduler->RegisterSystem(std::move(pTx));
    }

    {
        auto pSpatial = Engine::CSpatialHashSystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pSpatial));
    }

    {
        auto pStatus = CStatusEffectSystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pStatus));
    }

    {
        auto pVision = Engine::CVisionSystem::Create(scene.m_World.Get_SpatialIndex(), &scene.m_BushIndex);
        scene.m_pVisionSystem = pVision.get();
        scene.m_pScheduler->RegisterSystem(std::move(pVision));
    }

    CStructure_Manager::Get()->Initialize(&scene.m_World);
    CJungle_Manager::Get()->Initialize(&scene.m_World);
    CMinion_Manager::Get()->Initialize(&scene.m_World);

    wchar_t stagePath[MAX_PATH] = {};
    if (CMapDataIO::Get_StagePathW(1, stagePath, MAX_PATH))
    {
        CMapDataIO::Load_Stage(stagePath);
        Winters::DevSmoke::Log("[InGameBootstrap] Stage1 loaded\n");
    }

    BootstrapChampionModules();
    Winters::DevSmoke::Log("[InGameBootstrap] champion modules bootstrapped\n");

    wchar_t shaderPath[MAX_PATH] = {};
    if (WintersResolveContentPath(L"Shaders/Default3D.hlsl", shaderPath, MAX_PATH))
        (void)scene.m_Cube.Init(shaderPath);

    scene.m_pCamera = CDynamicCamera::Create(
        { 0.f, 10.f, -10.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f, 0.f });

    bool_t bMapInit = false;
    if (ShouldSkipSmokeMap())
    {
        Winters::DevSmoke::Log("[InGameBootstrap] map init skipped for smoke\n");
    }
    else
    {
        Winters::DevSmoke::Log("[InGameBootstrap] map init begin\n");
        bMapInit = scene.m_Map.Initialize("Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh", L"Shaders/Mesh3D.hlsl");
        Winters::DevSmoke::Log("[InGameBootstrap] map init done ok=%u\n", bMapInit ? 1u : 0u);
    }
    scene.m_MapTransform.SetPosition(0.f, 0.f, 0.f);
    scene.m_MapTransform.SetScale({ -0.01f, 0.01f, 0.01f });
    scene.m_MapTransform.SetRotation(scene.m_vMapRotation);
    scene.InitializeMapSurfaceSampler(bMapInit);
    scene.m_CubeTransform.SetPosition(21.f, 3.f, 0.f);

    Winters::DevSmoke::Log("[InGameBootstrap] CreateECSEntities begin\n");
    scene.CreateECSEntities();
    Winters::DevSmoke::Log("[InGameBootstrap] CreateECSEntities done player=%u total=%u\n",
        static_cast<u32_t>(scene.m_PlayerEntity),
        scene.m_World.GetEntityCount());
    SeedPracticeBushesForBootstrap(scene.m_World);
    scene.m_BushIndex.Build(scene.m_World);
    if (scene.m_pVisionSystem)
        scene.m_pVisionSystem->ForceRebuildNextFrame();

    const eChampion selectedChampion = scene.GetPlayerChampionId();
    if (CGameInstance::Get()->Get_GameContext().bUseNetworkRoster
        || selectedChampion == eChampion::RIVEN
        || selectedChampion == eChampion::EZREAL
        || selectedChampion == eChampion::FIORA
        || selectedChampion == eChampion::JAX
        || selectedChampion == eChampion::ANNIE
        || selectedChampion == eChampion::ASHE
        || selectedChampion == eChampion::YONE)
    {
        scene.BindPlayerToECSChampion(scene.m_PlayerEntity);
    }

    CGameInstance::Get()->UI_Bind_World(&scene.m_World);
    CGameInstance::Get()->UI_Set_InGameBuyItemCallback(&UI_SendBuyItemCommand, &scene);
    CGameInstance::Get()->UI_Set_LevelSkillCallback(&UI_SendLevelSkillCommand, &scene);

    CMinion_Manager::Get()->Set_Enabled(!scene.m_bNetworkAuthoritativeGameplay);
    wchar_t navGridPath[MAX_PATH] = {};
    bool_t bLoadedAuthoredNavGrid = false;
    if (CMapDataIO::GetNavGridPathW(1, navGridPath, MAX_PATH))
    {
        scene.m_pNavGrid = Engine::CNavGrid::LoadFromFile(navGridPath);
        bLoadedAuthoredNavGrid = scene.m_pNavGrid != nullptr;
    }

    if (!scene.m_pNavGrid)
    {
        scene.m_pNavGrid = scene.CreateMapNavGrid();
        scene.m_pNavGrid->SetAllWalkable(true);
    }

    if (scene.m_pNavGrid)
    {
        char msg[256]{};
        sprintf_s(
            msg,
            "[ClientNav] %s origin=(%.2f,%.2f) walkable=%u hash=%08X\n",
            bLoadedAuthoredNavGrid ? "authored navgrid loaded" : "fallback navgrid created",
            scene.m_pNavGrid->Get_OriginX(),
            scene.m_pNavGrid->Get_OriginZ(),
            scene.m_pNavGrid->CountWalkableCells(),
            scene.m_pNavGrid->ComputeContentHash());
        OutputDebugStringA(msg);
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pNav = CNavigationSystem::Create();
        pNav->Set_Grid(scene.m_pNavGrid.get());
        pNav->Set_JobSystem(pJS);
        scene.m_pScheduler->RegisterSystem(std::move(pNav));
    }
    else
    {
        Winters::DevSmoke::Log("[InGameBootstrap] client navigation skipped for network authority\n");
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pAI = CMinionAISystem::Create();
        pAI->Set_JobSystem(pJS);
        scene.m_pScheduler->RegisterSystem(std::move(pAI));
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pMinionPerformance = Engine::CMinionPerformanceSystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pMinionPerformance));
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pSeparation = Engine::CMinionSeparationSystem::Create();
        scene.m_pMinionSeparationSystem = pSeparation.get();
        pSeparation->Set_Enabled(true);
        pSeparation->Set_SeparationRadius(1.15f);
        pSeparation->Set_SeparationWeight(0.65f);
        pSeparation->Set_JobSystem(pJS);
        scene.m_pScheduler->RegisterSystem(std::move(pSeparation));
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        scene.m_pGameplayCollisionSystem = Engine::CGameplayCollisionSystem::Create();
        if (scene.m_pGameplayCollisionSystem)
        {
            scene.m_pGameplayCollisionSystem->Set_Enabled(false);
            scene.m_pGameplayCollisionSystem->Set_PushStrength(0.f);
        }
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pTurretAI = Engine::CTurretAISystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pTurretAI));
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pTurretProjectiles = Engine::CTurretProjectileSystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pTurretProjectiles));
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pBT = Engine::CBehaviorTreeSystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pBT));
    }

    {
        auto pYoneSoul = CYoneSoulSpawnSystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pYoneSoul));
    }

    if (!scene.m_bNetworkAuthoritativeGameplay)
    {
        auto pMCTS = Engine::CMCTSSystem::Create();
        scene.m_pScheduler->RegisterSystem(std::move(pMCTS));
    }

    scene.Mark_StructuresOnNavGrid();

    {
        CGameInstance* pGI = CGameInstance::Get();
        IRHIDevice* pRhiDevice = pGI->Get_RHIDevice();
        const bool_t bUseDX12RHI = pRhiDevice && pRhiDevice->GetBackend() == eRHIBackend::DX12;

        if (bUseDX12RHI)
        {
            scene.m_pRHIUtilityPlaneRenderer = CRHIFxSpriteRenderer::Create(pRhiDevice);
            scene.m_hRHIAttackRangeTex = RHI_CreateTextureFromFile(
                pRhiDevice,
                L"Client/Bin/Resource/Texture/UI/UI_AttackRange.png",
                "RHI_AttackRangeTexture");
            if (!scene.m_hRHIAttackRangeTex.IsValid())
                scene.m_hRHIAttackRangeTex = CreateDefaultRHITexture(pRhiDevice, "RHI_AttackRangeFallback");

        }
        else
        {
            scene.m_pAttackRangePlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->Get_UIPlaneShader(),
                pGI->Get_UIPlanePipeline());

            scene.m_pAttackRangeTex = CTexture::Create(
                pRhiDevice,
                L"Client/Bin/Resource/Texture/UI/UI_AttackRange.png",
                eTexSamplerMode::Clamp);

            if (!scene.m_pAttackRangeTex)
            {
                wchar_t cwd[MAX_PATH] = L"";
                ::GetCurrentDirectoryW(MAX_PATH, cwd);
                wchar_t msg[MAX_PATH * 3];
                swprintf_s(msg, L"[Scene_InGame] UI_AttackRange.png load failed - fallback to 1x1 white (cwd=%ls)\n", cwd);
                ::OutputDebugStringW(msg);
                MSG_BOX("Attack Range Texture load failed");
                scene.m_pAttackRangeTex = CTexture::CreateDefault(pRhiDevice);
            }

            if (scene.m_pAttackRangePlane && scene.m_pAttackRangeTex)
            {
                scene.m_pAttackRangePlane->SetTexture(scene.m_pAttackRangeTex.get());
                scene.m_pAttackRangePlane->SetBlendCache(
                    pGI->Get_BlendStateCache(),
                    eBlendPreset::AlphaBlend);
            }

            scene.m_pContactShadowPlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->Get_ContactShadowShader(),
                pGI->Get_ContactShadowPipeline());

            if (scene.m_pContactShadowPlane)
            {
                scene.m_pContactShadowPlane->SetBlendCache(
                    pGI->Get_BlendStateCache(),
                    eBlendPreset::AlphaBlend);
                scene.m_pContactShadowPlane->SetFxParams(
                    { 0.015f, 0.018f, 0.020f, 0.44f },
                    { 0.f, 0.f, 1.f, 1.f },
                    { 0.f, 0.f },
                    0.003f,
                    0.f);
            }
        }

        if (!bUseDX12RHI)
        {
            scene.m_pWhiteTexture = CTexture::CreateDefault(pRhiDevice);
            scene.m_pNormalPass = Engine::CNormalPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            scene.m_pSSAOPass = Engine::CSSAOPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            if (scene.m_pSSAOPass)
            {
                scene.m_pSSAOPass->SetEnabled(true);
                scene.m_pSSAOPass->SetRadius(1.1f);
                scene.m_pSSAOPass->SetIntensity(1.25f);
            }
            scene.m_pFogOfWarRenderer = CFogOfWarRenderer::Create(
                pRhiDevice, Engine::CVisionSystem::FOW_TEX_DIM);
        }
    }
    Winters::DevSmoke::Log("[InGameBootstrap] render helpers ready\n");

    {
        CGameInstance* pGI = CGameInstance::Get();
        IRHIDevice* pRhiDevice = pGI->Get_RHIDevice();

        scene.m_pFxSystem = CFxSystem::Create(
            pRhiDevice,
            pGI->Get_FxSpriteShader(),
            pGI->Get_FxSpritePipeline(),
            pGI->Get_BlendStateCache());
        scene.m_pFxBeamSystem = CFxBeamSystem::Create(
            pRhiDevice,
            pGI->Get_FxSpriteShader(),
            pGI->Get_FxSpritePipeline(),
            pGI->Get_BlendStateCache());

        scene.m_pFxMeshRenderer = Engine::CFxStaticMeshRenderer::Create(
            pRhiDevice,
            pGI->Get_MeshShader(),
            pGI->Get_MeshPipeline(),
            pGI->Get_FxMeshShader(),
            pGI->Get_FxMeshPipeline(),
            pGI->Get_BlendStateCache());
        if (scene.m_pEventApplier)
            scene.m_pEventApplier->SetFxMeshRenderer(scene.m_pFxMeshRenderer.get());
        scene.m_pFxMeshSystem = CFxMeshSystem::Create(scene.m_pFxMeshRenderer.get());

        scene.m_pIreliaBladeSystem = CIreliaBladeSystem::Create();
        scene.m_pUltWaveSystem = CUltWaveSystem::Create();
        scene.m_pWindWallSystem = CWindWallSystem::Create();
        scene.m_pYasuoProjectileSystem = CYasuoProjectileSystem::Create();
        scene.m_pPendingHitSystem = CPendingHitSystem::Create();
        scene.m_pKalistaProjectileSystem = CKalistaProjectileSystem::Create();
        scene.m_pKalistaRendSystem = CKalistaRendSystem::Create();
    }
    Winters::DevSmoke::Log("[InGameBootstrap] skill fx systems ready\n");

    if (scene.m_pFxMeshRenderer && ShouldUseFastSmokeBootstrap())
    {
        Winters::DevSmoke::Log("[InGameBootstrap] FX mesh preload skipped for smoke\n");
    }
    else if (scene.m_pFxMeshRenderer)
    {
        static const struct { const char* fbx; const wchar_t* tex; } kIreliaFx[] = {
            { "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_blade.fbx",
              L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_blades_passive_4_texture.png" },
            { "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
              L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_beam_mult.png" },
        };
        for (const auto& it : kIreliaFx)
            scene.m_pFxMeshRenderer->PreloadMesh(it.fbx, it.tex);

        static const struct { const char* fbx; const wchar_t* tex; } kYasuoFx[] = {
            { "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_base_q_tornado_blade_cas.fbx",
              L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_base_e_tonado_blend.png" },
            { "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_w_windwall_mesh.fbx",
              L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_w_windwall_dust.png" },
            { "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_base_r_sword_wind2.fbx",
              L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_r_sword_glow.png" },
        };
        for (const auto& it : kYasuoFx)
            scene.m_pFxMeshRenderer->PreloadMesh(it.fbx, it.tex);

        static const struct { const char* fbx; const wchar_t* tex; } kKalistaFx[] = {
            { "Client/Bin/Resource/Texture/FX/Kalista/fbx/kalista_base_q_mis_spear.fbx",
              L"Client/Bin/Resource/Texture/FX/Kalista/kalista_base_q_mis_glow_color.png" },
            { "Client/Bin/Resource/Texture/FX/Kalista/fbx/kalista_base_e_spear_hold.fbx",
              L"Client/Bin/Resource/Texture/FX/Kalista/kalista_base_e_spear_glow.png" },
        };
        for (const auto& it : kKalistaFx)
            scene.m_pFxMeshRenderer->PreloadMesh(it.fbx, it.tex);
    }

    CGameInstance::Get()->UI_Set_PlayerChampion(scene.GetPlayerChampionId());

    Winters::DevSmoke::Log("[InGameBootstrap] done player=%u champion=%u\n",
        static_cast<u32_t>(scene.m_PlayerEntity),
        static_cast<u32_t>(scene.GetPlayerChampionId()));

    return true;
}
