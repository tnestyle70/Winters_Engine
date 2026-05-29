#include "Scene/InGameLifecycleBridge.h"

#include "GameInstance.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Structure_Manager.h"
#include "ECS/Systems/GameplayCollisionSystem.h"
#include "RHI/IRHIDevice.h"
#include "Scene/InGameNetworkBridge.h"
#include "Scene/Scene_InGame.h"

void CInGameLifecycleBridge::Shutdown(CScene_InGame& scene)
{
    CGameInstance::Get()->UI_Set_InGameBuyItemCallback(nullptr, nullptr);
    CGameInstance::Get()->UI_Set_LevelSkillCallback(nullptr, nullptr);
    CGameInstance::Get()->UI_Bind_World(nullptr);

    CInGameNetworkBridge::Shutdown(
        scene.m_pEntityIdMap,
        scene.m_pNetwork,
        scene.m_pNetworkView,
        scene.m_bUsingSharedNetwork,
        scene.m_pSnapshotApplier,
        scene.m_pEventApplier,
        scene.m_pCommandSerializer);
    scene.m_bNetworkAuthoritativeGameplay = false;

    scene.m_Map.Shutdown();
    scene.m_Cube.Shutdown();

    if (scene.m_pFxBeamSystem)   scene.m_pFxBeamSystem.reset();
    if (scene.m_pFxMeshSystem)   scene.m_pFxMeshSystem.reset();
    if (scene.m_pFxMeshRenderer) scene.m_pFxMeshRenderer.reset();

    scene.m_ChampionRenderers.clear();
    scene.m_NetworkChampionPrevPos.clear();
    scene.m_NetworkChampionMoveGraceSec.clear();
    scene.m_NetworkChampionMoving.clear();
    scene.m_NetworkActorInterpStates.clear();
    scene.m_uNetworkActorInterpSnapshotTick = 0;
    scene.m_NetworkActionAnimStates.clear();
    scene.m_pSSAOPass.reset();
    scene.m_pNormalPass.reset();
    scene.m_pFogOfWarRenderer.reset();
    scene.m_pVisionSystem = nullptr;
    scene.m_pGameplayCollisionSystem.reset();
    scene.m_pMinionSeparationSystem = nullptr;
    scene.m_BushIndex.Clear();
    scene.m_pWhiteTexture.reset();
    scene.m_pRHIUtilityPlaneRenderer.reset();
    if (IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice())
    {
        if (scene.m_hRHIAttackRangeTex.IsValid())
            pDevice->DestroyTexture(scene.m_hRHIAttackRangeTex);
    }
    scene.m_hRHIAttackRangeTex = {};
    scene.m_pAttackRangeTex.reset();
    scene.m_pAttackRangePlane.reset();

    CMinion_Manager::Get()->Set_Enabled(false);
    CMinion_Manager::Get()->Shutdown();
    CJungle_Manager::Get()->Shutdown();
    CStructure_Manager::Get()->Shutdown();
}
