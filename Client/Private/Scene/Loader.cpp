#include "Scene/Loader.h"
#include "GameInstance.h"
#include "ECS/Systems/EntityBlueprint.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

#include "Core/JobCounter.h"
#include "Core/JobSystem.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/LoLMatchContextRuntime.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/LobbyRosterHelpers.h"

#include <algorithm>
#include <chrono>
#include <thread>

NS_BEGIN(Client)

namespace
{
    constexpr const char* kSummonersRiftMapModelPath =
        "Texture/MAP/output/sr_base_flip.wmesh";

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
    pInstance->m_LoadContext = Client::CLoLMatchContextRuntime::Instance().Context();

    if (eNextSceneID == eSceneID::InGame)
    {
        Register_Blueprints_InGame();
        pInstance->PrepareMainThreadInGameLoad();
        return pInstance;
    }

    pInstance->m_pCounter = std::make_unique<CJobCounter>();
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

void CLoader::TickMainThreadLoad()
{
    if (!m_bMainThreadLoad || m_bFinished.load())
        return;

    if (m_iNextLoadStep >= m_LoadSteps.size())
    {
        SetProgress(1.f);
        m_bFinished.store(true);
        return;
    }

    const LoadStep& step = m_LoadSteps[m_iNextLoadStep++];
    switch (step.eType)
    {
    case eLoadStepType::FxDirectory:
        CFxCuePlayer::PreloadDirectory(step.strTexturePath.c_str());
        break;
    case eLoadStepType::Model:
        PreloadModel(step.strModelPath.c_str(), 0.f);
        break;
    case eLoadStepType::Texture:
        PreloadTexture(step.strTexturePath.c_str(), 0.f);
        break;
    default:
        break;
    }

    SetProgress(step.fProgressAfter);
    if (m_iNextLoadStep >= m_LoadSteps.size())
    {
        SetProgress(1.f);
        m_bFinished.store(true);
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
    PrepareMainThreadInGameLoad();
}

void CLoader::PrepareMainThreadInGameLoad()
{
    m_bMainThreadLoad = true;
    m_LoadSteps.clear();
    m_iNextLoadStep = 0;
    m_bFinished.store(false);
    SetProgress(0.05f);

    LoadStep mapStep{};
    mapStep.eType = eLoadStepType::Model;
    mapStep.strModelPath = kSummonersRiftMapModelPath;
    mapStep.fProgressAfter = 0.15f;
    m_LoadSteps.push_back(std::move(mapStep));

    LoadStep fxStep{};
    fxStep.eType = eLoadStepType::FxDirectory;
    fxStep.strTexturePath = L"Data/LoL/FX";
    fxStep.fProgressAfter = 0.30f;
    m_LoadSteps.push_back(std::move(fxStep));

    const size_t championStepBegin = m_LoadSteps.size();
    auto appendChampionAssets = [this](eChampion eChampionId)
    {
        const ChampionDef* pDef = FindLoaderChampionDef(eChampionId);
        if (!pDef)
            return;

        if (pDef->fbxPath && pDef->fbxPath[0] != '\0')
        {
            LoadStep step{};
            step.eType = eLoadStepType::Model;
            step.strModelPath = pDef->fbxPath;
            m_LoadSteps.push_back(std::move(step));
        }

        if (pDef->defaultTexturePath && pDef->defaultTexturePath[0] != L'\0')
        {
            LoadStep step{};
            step.eType = eLoadStepType::Texture;
            step.strTexturePath = pDef->defaultTexturePath;
            m_LoadSteps.push_back(std::move(step));
        }

        for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
        {
            if (!pDef->texturePath[i] || pDef->texturePath[i][0] == L'\0')
                continue;

            LoadStep step{};
            step.eType = eLoadStepType::Texture;
            step.strTexturePath = pDef->texturePath[i];
            m_LoadSteps.push_back(std::move(step));
        }
    };

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

        appendChampionAssets(eFallbackChampion);
        appendChampionAssets(CInGameRosterSpawner::ResolvePracticeBotChampion());
    }
    else
    {
        for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
        {
            const GameRosterSlot& slot = m_LoadContext.Roster[i];
            if (!IsSlotOccupied(slot) || !IsLoadableChampion(slot.champion))
                continue;

            appendChampionAssets(slot.champion);
        }
    }

    const size_t championStepCount = m_LoadSteps.size() - championStepBegin;
    if (championStepCount > 0)
    {
        for (size_t i = 0; i < championStepCount; ++i)
        {
            const f32_t t = static_cast<f32_t>(i + 1) /
                static_cast<f32_t>(championStepCount);
            m_LoadSteps[championStepBegin + i].fProgressAfter =
                (std::min)(0.95f, 0.30f + 0.65f * t);
        }
    }

    if (m_LoadSteps.empty())
        m_bFinished.store(true);
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

    //臾댁뒯 ?먮━濡?Entity媛 異붽??섍쾶 ?섎뒗 嫄곗??
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
	(void)hr;
}

NS_END
