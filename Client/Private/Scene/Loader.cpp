#include "Scene/Loader.h"
#include "GameInstance.h"
#include "ECS/Systems/EntityBlueprint.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"

#include "Core/JobCounter.h"
#include "Core/JobSystem.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GamePlay/ChampionCatalog.h"
#include "Scene/LobbyRosterHelpers.h"

#include <chrono>
#include <thread>

NS_BEGIN(Client)

namespace
{
    constexpr const char* kSummonersRiftMapModelPath =
        "Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh";

    bool_t IsLoadableChampion(eChampion eChampionId)
    {
        return eChampionId != eChampion::END && eChampionId != eChampion::NONE;
    }

    const ChampionDef* FindLoaderChampionDef(eChampion eChampionId)
    {
        const ChampionCatalogEntry* pEntry = CChampionCatalog::Instance().Find(eChampionId);
        if (pEntry && pEntry->pDef)
            return pEntry->pDef;

        return FindChampionDef(eChampionId);
    }

}

std::unique_ptr<CLoader> CLoader::Create(eSceneID eNextSceneID, SceneFactory pFactory)
{
    auto pInstance = std::unique_ptr<CLoader>(new CLoader());
    pInstance->m_eNextSceneID = eNextSceneID;
    pInstance->m_pFactory = std::move(pFactory);
    pInstance->m_LoadContext = CGameInstance::Get()->Get_GameContext();
    pInstance->m_pCounter = std::make_unique<CJobCounter>();

    if (eNextSceneID == eSceneID::InGame)
        Register_Blueprints_InGame();

    CLoader* pRaw = pInstance.get();
    if (CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem())
        pJS->Submit([pRaw]() {pRaw->RunLoadJob(); }, pInstance->m_pCounter.get());
    else
        pRaw->RunLoadJob();

    return pInstance;
}

CLoader::~CLoader()
{
    if (m_pCounter && !m_pCounter->IsComplete())
    {
        if (CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem())
            pJS->WaitForCounter(m_pCounter.get(), 0);
        else
        {
            while (!m_bFinished.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

unique_ptr<IScene> CLoader::Build_NextScene()
{
    if (!m_pFactory) return nullptr;
    return m_pFactory();
}

void CLoader::RunLoadJob()
{
    switch (m_eNextSceneID)
    {
    case Client::eSceneID::MainMenu: Ready_For_MainMenu();  break;
    case Client::eSceneID::BanPick: Ready_For_BanPick(); break;
    case Client::eSceneID::InGame: Ready_For_InGame(); break;
    default: break;
    }
    SetProgress(1.f);
    m_bFinished.store(true);
}

void CLoader::Ready_For_MainMenu()
{
    for (int i = 0; i < 5; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m_fProgress = static_cast<f32_t>(i + 1) / 5.f;
    }
}

void CLoader::Ready_For_BanPick()
{
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        m_fProgress = static_cast<f32_t>(i + 1) / 10.f;
    }
}

void CLoader::Ready_For_InGame()
{
    PreloadInGameAssets();
}

void CLoader::PreloadInGameAssets()
{
    SetProgress(0.05f);
    PreloadModel(kSummonersRiftMapModelPath, 0.15f);

    CFxCuePlayer::PreloadDirectory(L"Data/LoL/FX");
    SetProgress(0.30f);

    u32_t iLoadableSlotCount = 0;
    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        const GameRosterSlot& slot = m_LoadContext.Roster[i];
        if (IsSlotOccupied(slot) && IsLoadableChampion(slot.champion))
            ++iLoadableSlotCount;
    }

    if (iLoadableSlotCount == 0)
    {
        const eChampion eFallbackChampion = IsLoadableChampion(m_LoadContext.SelectedChampion)
            ? m_LoadContext.SelectedChampion
            : eChampion::EZREAL;

        PreloadChampionAssets(eFallbackChampion);
        PreloadChampionAssets(eChampion::SYLAS);
        SetProgress(0.95f);
        return;
    }

    const f32_t fSlotStep = 0.65f / static_cast<f32_t>(iLoadableSlotCount);
    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        const GameRosterSlot& slot = m_LoadContext.Roster[i];
        if (!IsSlotOccupied(slot) || !IsLoadableChampion(slot.champion))
            continue;

        PreloadChampionAssets(slot.champion);
        SetProgress(m_fProgress.load() + fSlotStep);
    }
}

void CLoader::PreloadChampionAssets(eChampion eChampionId)
{
    const ChampionDef* pDef = FindLoaderChampionDef(eChampionId);
    if (!pDef)
        return;

    PreloadModel(pDef->fbxPath, 0.f);

    if (pDef->defaultTexturePath)
        PreloadTexture(pDef->defaultTexturePath, 0.f);

    for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
    {
        if (pDef->texturePath[i])
            PreloadTexture(pDef->texturePath[i], 0.f);
    }
}

void CLoader::PreloadModel(const char* pPath, f32_t fProgressStep)
{
    if (pPath && pPath[0] != '\0')
        CGameInstance::Get()->Preload_ModelResource(pPath);

    if (fProgressStep > 0.f)
        SetProgress(m_fProgress.load() + fProgressStep);
}

void CLoader::PreloadTexture(const wchar_t* pPath, f32_t fProgressStep)
{
    if (pPath && pPath[0] != L'\0')
        CGameInstance::Get()->Preload_TextureResource(pPath);

    if (fProgressStep > 0.f)
        SetProgress(m_fProgress.load() + fProgressStep);
}

void CLoader::SetProgress(f32_t fValue)
{
    if (fValue < 0.f)
        fValue = 0.f;
    else if (fValue > 1.f)
        fValue = 1.f;

    m_fProgress.store(fValue);
}

void CLoader::Register_Blueprints_InGame()
{
    const uint32_t SCENE = static_cast<uint32_t>(eSceneID::InGame);

    //무슨 원리로 Entity가 추가되게 되는 거지?
    CEntityBlueprint blueprint;
    blueprint.Add([](CWorld& world, EntityID entity)
        {
            world.AddComponent<TransformComponent>(entity);
        })
        .Add([](CWorld& world, EntityID entity)
            {
                ChampionComponent cc;
                cc.id = eChampion::SYLAS;
                cc.team = eTeam::Red;
                cc.hp = 600.f; cc.maxHp = 600.f;
                cc.mana = 300.f; cc.maxMana = 300.f;
                cc.moveSpeed = 5.f;
                world.AddComponent<ChampionComponent>(entity, cc);
            })
        .Add([](CWorld& world, EntityID entity)
            {
                world.AddComponent<ServerIdComponent>(entity);
            });
    const HRESULT hr = CGameInstance::Get()->Add_Blueprint(SCENE, L"Sylas", std::move(blueprint));
    if (SUCCEEDED(hr))
        OutputDebugStringA("[Loader] Sylas Blueprint registered\n");
    else
        OutputDebugStringA("[Loader] Sylas Blueprint already registered or registration failed\n");
}

NS_END
