#include "Scene/Loader.h"
#include "GameInstance.h"

#include "Core/JobCounter.h"
#include "Core/JobSystem.h"
#include "GameObject/Champion/Kalista/KalistaFxPresets.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxSystem.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/LoLMatchContextRuntime.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "Manager/Minion_Manager.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/LobbyRosterHelpers.h"
#include "Scene/Scene_InGame.h"
#include "UI/MinimapPanel.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Renderer/FxStaticMeshRenderer.h"
#include "Core/CTransform.h"
#include "WintersPaths.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_set>

NS_BEGIN(Client)

namespace
{
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

    u64_t BuildRosterAssetFingerprint(const MatchContext& context)
    {
        constexpr u64_t kFnvOffset = 1469598103934665603ull;
        constexpr u64_t kFnvPrime = 1099511628211ull;
        u64_t hash = kFnvOffset;
        for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
        {
            const GameRosterSlot& slot = context.Roster[i];
            const u32_t assetId = IsSlotOccupied(slot)
                ? static_cast<u32_t>(slot.champion) + 1u
                : 0u;
            hash ^= static_cast<u64_t>(assetId);
            hash *= kFnvPrime;
        }
        hash ^= static_cast<u64_t>(context.SelectedChampion);
        hash *= kFnvPrime;
        return hash;
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
        pInstance->PrepareMainThreadInGameLoad();
        pInstance->StartInGameCpuLoad();
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
    m_bCancelCpuLoad.store(true, std::memory_order_release);

    if (m_pCounter && !m_pCounter->IsComplete())
    {
        if (CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem())
            pJS->WaitForCounter(m_pCounter.get(), 0);
        else
        {
            while (!m_pCounter->IsComplete())
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void CLoader::TickMainThreadLoad()
{
    if (!m_bMainThreadLoad || m_bLoadFailed.load())
        return;

    if (m_bFinished.load(std::memory_order_acquire))
    {
        const MatchContext& currentContext =
            Client::CLoLMatchContextRuntime::Instance().Context();
        if (BuildRosterAssetFingerprint(currentContext) == m_uRosterFingerprint)
            return;

        m_LoadContext = currentContext;
        PrepareMainThreadInGameLoad();
    }

    if (m_iNextLoadStep < m_LoadSteps.size())
    {
        const LoadStep& step = m_LoadSteps[m_iNextLoadStep];
        bool_t bStepComplete = true;
        bool_t bStepSucceeded = true;
        switch (step.eType)
        {
        case eLoadStepType::FxDirectory:
            // A missing directory or a directory whose WFX files all fail to
            // parse must not satisfy the activation barrier.
            bStepSucceeded =
                CFxCuePlayer::PreloadDirectory(step.strTexturePath.c_str()) > 0u;
            break;
        case eLoadStepType::KalistaWSentinelTextures:
        {
            CGameInstance* pGameInstance = CGameInstance::Get();
            if (!m_pPreparedFxSystem)
            {
                m_pPreparedFxSystem = CFxSystem::Create(
                    pGameInstance->Get_RHIDevice(),
                    pGameInstance->Get_BlendStateCache());
            }
            bStepSucceeded = m_pPreparedFxSystem &&
                KalistaFx::PreloadWSentinelTextures(*m_pPreparedFxSystem);
            break;
        }
        case eLoadStepType::FxMesh:
        {
            CGameInstance* pGameInstance = CGameInstance::Get();
            if (!m_pPreparedFxMeshRenderer)
            {
                m_pPreparedFxMeshRenderer = Engine::CFxStaticMeshRenderer::Create(
                    pGameInstance->Get_RHIDevice(),
                    pGameInstance->Get_MeshShader(),
                    pGameInstance->Get_MeshPipeline(),
                    pGameInstance->Get_FxMeshShader(),
                    pGameInstance->Get_FxMeshPipeline(),
                    pGameInstance->Get_BlendStateCache());
            }
            bStepSucceeded = m_pPreparedFxMeshRenderer &&
                m_pPreparedFxMeshRenderer->PreloadMeshStrict(
                    step.strModelPath,
                    step.strTexturePath);
            break;
        }
        case eLoadStepType::Model:
            bStepSucceeded = PreloadModel(step.strModelPath.c_str(), 0.f);
            break;
        case eLoadStepType::MapModel:
            if (!AreInGameCpuLoadsComplete())
                return;
            if (!m_bCpuLoadSucceeded.load(std::memory_order_acquire) ||
                !m_pPreparedMapSurfaceSampler ||
                !m_pPreparedMapSurfaceSampler->IsReady())
            {
                FailInGameLoad("map surface prepare");
                return;
            }
            bStepSucceeded = PreloadModel(step.strModelPath.c_str(), 0.f);
            break;
        case eLoadStepType::Texture:
            bStepSucceeded = PreloadTexture(step.strTexturePath.c_str(), 0.f);
            break;
        case eLoadStepType::MinionVisual:
            bStepSucceeded = CMinion_Manager::Get()->
                PrewarmNextNetworkVisualResource(bStepComplete);
            break;
        case eLoadStepType::ChampionPortrait:
            bStepSucceeded = UI::CMinimapPanel::PrewarmChampionPortrait(step.champion);
            break;
        default:
            break;
        }

        if (!bStepSucceeded)
        {
            FailInGameLoad("required main-thread preload");
            return;
        }

        if (bStepComplete)
        {
            ++m_iNextLoadStep;
            SetProgress(step.fProgressAfter);
        }
    }

    TryFinishInGameLoad();
}

unique_ptr<IScene> CLoader::Build_NextScene()
{
    if (!m_pFactory || !IsReadyToActivate())
    {
        return nullptr;
    }

    unique_ptr<IScene> pNextScene = m_pFactory();
    if (!pNextScene)
    {
        FailInGameLoad("scene factory");
        return nullptr;
    }

    if (m_eNextSceneID == eSceneID::InGame)
    {
        if (CScene_InGame* pInGame = dynamic_cast<CScene_InGame*>(pNextScene.get()))
        {
            pInGame->AdoptPreparedMapSurfaceSampler(std::move(m_pPreparedMapSurfaceSampler));
            pInGame->AdoptPreparedFxSystem(std::move(m_pPreparedFxSystem));
            pInGame->AdoptPreparedFxMeshRenderer(std::move(m_pPreparedFxMeshRenderer));
        }
        else
        {
            FailInGameLoad("in-game scene type");
            return nullptr;
        }
    }

    return pNextScene;
}

bool CLoader::IsReadyToActivate() const
{
    if (!m_bFinished.load(std::memory_order_acquire) ||
        m_bLoadFailed.load(std::memory_order_acquire))
    {
        return false;
    }

    if (!m_bMainThreadLoad || m_eNextSceneID != eSceneID::InGame)
        return true;

    return BuildRosterAssetFingerprint(
        Client::CLoLMatchContextRuntime::Instance().Context()) ==
        m_uRosterFingerprint;
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
    m_bLoadFailed.store(false);
    m_uRosterFingerprint = BuildRosterAssetFingerprint(m_LoadContext);
    SetProgress(0.05f);

    std::unordered_set<std::string> modelPaths;
    std::unordered_set<std::wstring> texturePaths;
    std::unordered_set<u32_t> portraitChampions;
    bool_t bKalistaWSentinelTexturesQueued = false;

    auto appendModel = [this, &modelPaths](
        const char* pPath,
        eLoadStepType eType = eLoadStepType::Model)
    {
        if (!pPath || pPath[0] == '\0' || !modelPaths.emplace(pPath).second)
            return;

        LoadStep step{};
        step.eType = eType;
        step.strModelPath = pPath;
        m_LoadSteps.push_back(std::move(step));
    };

    auto appendTexture = [this, &texturePaths](const wchar_t* pPath)
    {
        if (!pPath || pPath[0] == L'\0' || !texturePaths.emplace(pPath).second)
            return;

        LoadStep step{};
        step.eType = eLoadStepType::Texture;
        step.strTexturePath = pPath;
        m_LoadSteps.push_back(std::move(step));
    };

    auto appendPortrait = [this, &portraitChampions](eChampion champion)
    {
        if (!IsLoadableChampion(champion) ||
            !portraitChampions.emplace(static_cast<u32_t>(champion)).second)
        {
            return;
        }

        LoadStep step{};
        step.eType = eLoadStepType::ChampionPortrait;
        step.champion = champion;
        m_LoadSteps.push_back(std::move(step));
    };

    LoadStep fxStep{};
    fxStep.eType = eLoadStepType::FxDirectory;
    fxStep.strTexturePath = L"Data/LoL/FX";
    m_LoadSteps.push_back(std::move(fxStep));

    auto appendChampionAssets = [&](eChampion eChampionId)
    {
        const ChampionDef* pDef = FindLoaderChampionDef(eChampionId);
        if (!pDef)
            return;

        appendModel(pDef->fbxPath);
        appendTexture(pDef->defaultTexturePath);

        for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
            appendTexture(pDef->texturePath[i]);

        appendPortrait(eChampionId);

        if (eChampionId == eChampion::KALISTA &&
            !bKalistaWSentinelTexturesQueued)
        {
            LoadStep step{};
            step.eType = eLoadStepType::KalistaWSentinelTextures;
            m_LoadSteps.push_back(std::move(step));
            bKalistaWSentinelTexturesQueued = true;
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

    constexpr Winters::Map::eObjectKind kStructureKinds[] =
    {
        Winters::Map::eObjectKind::Structure_Turret,
        Winters::Map::eObjectKind::Structure_Inhibitor,
        Winters::Map::eObjectKind::Structure_Nexus
    };
    constexpr eTeam kStructureTeams[] = { eTeam::Blue, eTeam::Red };
    for (const Winters::Map::eObjectKind kind : kStructureKinds)
    {
        for (const eTeam team : kStructureTeams)
        {
            const ClientData::StructureVisualDefinition* pVisual =
                ClientData::FindStructureVisualDefinition(kind, team);
            if (pVisual)
                appendModel(pVisual->mesh.resourceRelativePath);
        }
    }

    constexpr u32_t kJungleVisualCount = 11u;
    for (u32_t subKind = 0u; subKind < kJungleVisualCount; ++subKind)
    {
        const ClientData::JungleVisualDefinition* pVisual =
            ClientData::FindJungleVisualDefinition(subKind);
        if (!pVisual)
            continue;

        appendModel(pVisual->mesh.resourceRelativePath);
        for (u8_t i = 0u; i < pVisual->textureOverrideCount; ++i)
            appendTexture(pVisual->textureOverrides[i].resourceRelativePath);
    }

    constexpr u32_t kMinionTeamCount = 2u;
    constexpr u32_t kMinionTypeCount = 5u;
    for (u32_t team = 0u; team < kMinionTeamCount; ++team)
    {
        for (u32_t type = 0u; type < kMinionTypeCount; ++type)
        {
            const ClientData::MinionVisualDefinition* pVisual =
                ClientData::FindMinionVisualDefinition(type, team);
            if (!pVisual)
                continue;
            appendModel(pVisual->mesh.resourceRelativePath);
            appendTexture(pVisual->textureAllMeshes.resourceRelativePath);
        }
    }

    const ClientData::AmbientPropVisualPack& ambientPack =
        ClientData::GetAmbientPropVisualPack();
    for (u32_t i = 0u; i < ambientPack.propCount; ++i)
        appendModel(ambientPack.props[i].mesh.resourceRelativePath);

    const ClientData::FxMeshPreloadVisualPack& fxMeshPack =
        ClientData::GetFxMeshPreloadVisualPack();
    for (u32_t i = 0u; i < fxMeshPack.entryCount; ++i)
    {
        const ClientData::FxMeshPreloadVisualDefinition& preload =
            fxMeshPack.entries[i];
        if (!preload.mesh.resourceRelativePath ||
            !preload.texture.resourceRelativePath)
        {
            continue;
        }

        LoadStep fxMeshStep{};
        fxMeshStep.eType = eLoadStepType::FxMesh;
        fxMeshStep.strModelPath = preload.mesh.resourceRelativePath;
        fxMeshStep.strTexturePath = preload.texture.resourceRelativePath;
        m_LoadSteps.push_back(std::move(fxMeshStep));
    }

    appendTexture(ClientData::GetMapRuntimeVisualDefinition().
        attackRangeTexture.resourceRelativePath);

    // Let the CPU surface worker finish first. This avoids reading/parsing the
    // same 42 MB WMesh concurrently and leaves the file hot for GPU finalization.
    appendModel(CScene_InGame::GetSelectedMapMeshPath(), eLoadStepType::MapModel);

    LoadStep minionVisualStep{};
    minionVisualStep.eType = eLoadStepType::MinionVisual;
    m_LoadSteps.push_back(std::move(minionVisualStep));

    const f32_t stepCount = static_cast<f32_t>(m_LoadSteps.size());
    for (size_t i = 0; i < m_LoadSteps.size(); ++i)
    {
        const f32_t t = static_cast<f32_t>(i + 1u) / stepCount;
        m_LoadSteps[i].fProgressAfter = 0.05f + 0.94f * t;
    }
}

void CLoader::StartInGameCpuLoad()
{
    m_bCancelCpuLoad.store(false, std::memory_order_release);
    m_bCpuLoadSucceeded.store(false, std::memory_order_release);
    m_pCounter = std::make_unique<CJobCounter>();
    CLoader* pRaw = this;
    if (CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem())
        pJS->Submit([pRaw]() { pRaw->RunInGameCpuLoad(); }, m_pCounter.get());
    else
        RunInGameCpuLoad();
}

void CLoader::RunInGameCpuLoad()
{
    wchar_t resolvedPath[MAX_PATH]{};
    const wchar_t* pSurfacePath = CScene_InGame::GetSelectedMapSurfacePath();
    if (!WintersResolveContentPath(pSurfacePath, resolvedPath, MAX_PATH))
        return;

    CTransform mapTransform{};
    CScene_InGame::ConfigureDefaultMapTransform(mapTransform);

    auto sampler = std::make_unique<Engine::CMapSurfaceSampler>();
    if (!sampler->LoadFromWMesh(
        resolvedPath,
        mapTransform.GetWorldMatrix(),
        &m_bCancelCpuLoad))
    {
        return;
    }

    if (!m_bCancelCpuLoad.load(std::memory_order_acquire))
    {
        m_pPreparedMapSurfaceSampler = std::move(sampler);
        m_bCpuLoadSucceeded.store(true, std::memory_order_release);
    }
}

bool_t CLoader::AreInGameCpuLoadsComplete() const
{
    return !m_pCounter || m_pCounter->IsComplete();
}

void CLoader::TryFinishInGameLoad()
{
    if (m_bLoadFailed.load(std::memory_order_acquire) ||
        m_iNextLoadStep < m_LoadSteps.size() ||
        !AreInGameCpuLoadsComplete())
        return;

    if (!m_bCpuLoadSucceeded.load(std::memory_order_acquire) ||
        !m_pPreparedMapSurfaceSampler ||
        !m_pPreparedMapSurfaceSampler->IsReady())
    {
        FailInGameLoad("ready barrier");
        return;
    }

    const MatchContext& currentContext =
        Client::CLoLMatchContextRuntime::Instance().Context();
    const u64_t currentFingerprint = BuildRosterAssetFingerprint(currentContext);
    if (currentFingerprint != m_uRosterFingerprint)
    {
        OutputDebugStringA(
            "[Loading] authoritative roster changed; rebuilding required asset queue\n");
        m_LoadContext = currentContext;
        PrepareMainThreadInGameLoad();
        return;
    }

    SetProgress(1.f);
    m_bFinished.store(true, std::memory_order_release);
}

void CLoader::FailInGameLoad(const char* pStage)
{
    if (m_bLoadFailed.exchange(true, std::memory_order_acq_rel))
        return;

    char message[256]{};
    sprintf_s(message,
        "[Loading] required preparation failed stage=%s step=%u/%zu\n",
        pStage ? pStage : "unknown",
        m_iNextLoadStep,
        m_LoadSteps.size());
    OutputDebugStringA(message);
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

bool_t CLoader::PreloadModel(const char* pPath, f32_t fProgressStep)
{
    bool_t bLoaded = false;
    if (pPath && pPath[0] != '\0')
        bLoaded = CGameInstance::Get()->Preload_ModelResource(pPath);

    if (fProgressStep > 0.f)
        SetProgress(m_fProgress.load() + fProgressStep);

    return bLoaded;
}

bool_t CLoader::PreloadTexture(const wchar_t* pPath, f32_t fProgressStep)
{
    bool_t bLoaded = false;
    if (pPath && pPath[0] != L'\0')
        bLoaded = CGameInstance::Get()->Preload_TextureResource(pPath);

    if (fProgressStep > 0.f)
        SetProgress(m_fProgress.load() + fProgressStep);

    return bLoaded;
}

void CLoader::SetProgress(f32_t fValue)
{
    if (fValue < 0.f)
        fValue = 0.f;
    else if (fValue > 1.f)
        fValue = 1.f;

    m_fProgress.store(fValue);
}

NS_END
