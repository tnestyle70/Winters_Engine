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
#include "Scene/RenderVisibilityFilter.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_Editor.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Map/MapDataIO.h"
#include "Core/CInput.h"
#include "WintersPaths.h"
#include "GameInstance.h"
#include "ECS/Components/CoreComponents.h"   // ColliderComponent
#include "ECS/Systems/MinionAISystem.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/TurretAISystem.h"
#include "ECS/Systems/TurretProjectileSystem.h"
#include "ECS/Systems/MinionPerformanceSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/BushVolumeIndex.h"
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
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "GameObject/ChampionSpawnService.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/SkillDefVisualDataAdapter.h"
#include "GameContext.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
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
#include "ECS/Systems/StatusEffectSystem.h"
#include "ECS/Components/GameplayComponents.h"   // Stun/Slow/Disarm
#include "GameObject/FX/FxSystem.h"
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

CScene_InGame::CScene_InGame() = default;

CScene_InGame::CScene_InGame(const wstring_t& replayPath)
    : m_bReplayPlaybackMode(true)
    , m_strReplayPath(replayPath)
{
}

CScene_InGame::~CScene_InGame() = default;

namespace
{
    constexpr f32_t kMoveTargetMaxSurfaceDeltaY = 3.f;
    constexpr f32_t kNetworkActorInterpDurationSec = 0.055f;
    constexpr f32_t kNetworkActorInterpTeleportSq = 9.f;
    constexpr f32_t kNetworkActorInterpMinMoveSq = 0.0001f;
    constexpr f32_t kNetworkActorInterpMinYaw = 0.0005f;

    u8_t GetSkillStageIndex(u8_t skillStage)
    {
        return skillStage >= 2u ? 1u : 0u;
    }

    eTargetShape GetTargetShape(const SkillTargetSpec& target, u8_t skillStage)
    {
        return target.shape[GetSkillStageIndex(skillStage)];
    }

    eSkillFacingMode GetFacingMode(const SkillFacingSpec& facing, u8_t skillStage)
    {
        return facing.mode[GetSkillStageIndex(skillStage)];
    }

    SkillVisualStageData GetVisualStage(const SkillVisualData& visual, u8_t skillStage)
    {
        return visual.stages[GetSkillStageIndex(skillStage)];
    }

    const VisualEventData* FindVisualEvent(
        const SkillVisualStageData& stage,
        eVisualEventKind kind)
    {
        for (u8_t i = 0; i < stage.eventCount; ++i)
        {
            if (stage.events[i].kind == static_cast<u8_t>(kind))
            {
                return &stage.events[i];
            }
        }

        return nullptr;
    }

    eTargetMode ToLegacyTargetMode(eTargetShape shape)
    {
        switch (shape)
        {
        case eTargetShape::Unit:
            return eTargetMode::UnitTarget;
        case eTargetShape::Ground:
            return eTargetMode::GroundTarget;
        case eTargetShape::Direction:
            return eTargetMode::Direction;
        case eTargetShape::Self:
        default:
            return eTargetMode::Self;
        }
    }

    eRotateMode ToLegacyRotateMode(eSkillFacingMode mode)
    {
        switch (mode)
        {
        case eSkillFacingMode::TowardsTarget:
            return eRotateMode::TowardsTarget;
        case eSkillFacingMode::TowardsCommandDirection:
            return eRotateMode::TowardsCursor;
        case eSkillFacingMode::None:
        default:
            return eRotateMode::None;
        }
    }

    f32_t ResolveSkillStageLockSec(
        const SkillGameAtomBundle& gameData,
        const SkillDef& legacyDef,
        u8_t skillStage)
    {
        const f32_t stageLock =
            gameData.stage.lockDurationSec[GetSkillStageIndex(skillStage)];
        if (stageLock > 0.f)
        {
            return stageLock;
        }

        if (skillStage >= 2u && legacyDef.stage2LockSec > 0.f)
        {
            return legacyDef.stage2LockSec;
        }

        return legacyDef.lockDurationSec;
    }

    SkillDef BuildLegacyHookBridge(
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage)
    {
        SkillDef bridge = legacyDef;
        const SkillVisualStageData visualStage = GetVisualStage(visualData, skillStage);

        bridge.champ = gameData.slot.champion;
        bridge.slot = gameData.slot.slot;
        bridge.targetMode = ToLegacyTargetMode(GetTargetShape(gameData.target, skillStage));
        bridge.cooldownSec = gameData.cooldown.cooldownSec;
        bridge.rangeMax = gameData.range.rangeMax;
        bridge.manaCost = gameData.cost.manaCost;
        bridge.lockDurationSec = ResolveSkillStageLockSec(gameData, legacyDef, skillStage);
        bridge.stageCount = gameData.stage.stageCount;
        bridge.stageWindowSec = gameData.stage.stageWindowSec;
        bridge.rotate = ToLegacyRotateMode(GetFacingMode(gameData.facing, skillStage));
        bridge.animKey = visualStage.animationKey ? visualStage.animationKey : legacyDef.animKey;
        bridge.animPlaySpeed = visualStage.playbackSpeed > 0.f
            ? visualStage.playbackSpeed
            : legacyDef.animPlaySpeed;

        if (const VisualEventData* eventData =
            FindVisualEvent(visualStage, eVisualEventKind::KeySwap))
        {
            bridge.keySwapHookId = eventData->hookId;
        }
        if (const VisualEventData* eventData =
            FindVisualEvent(visualStage, eVisualEventKind::CastAccepted))
        {
            bridge.onCastAcceptedHookId = eventData->hookId;
        }
        if (const VisualEventData* eventData =
            FindVisualEvent(visualStage, eVisualEventKind::Cast))
        {
            bridge.castFrame = eventData->frame;
            bridge.castFrameHookId = eventData->hookId;
        }
        if (const VisualEventData* eventData =
            FindVisualEvent(visualStage, eVisualEventKind::Recovery))
        {
            bridge.recoveryFrame = eventData->frame;
            bridge.recoveryHookId = eventData->hookId;
        }

        bridge.endTransitionIdleAnim = visualData.endTransitionIdleAnim;
        bridge.endTransitionRunAnim = visualData.endTransitionRunAnim;
        bridge.endTransitionDuration = visualData.endTransitionDuration;
        bridge.skillId = gameData.slot.skillId;
        bridge.scalingTableId = gameData.effect.scalingTableId;
        return bridge;
    }

    f32_t SmoothStep01(f32_t t)
    {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    Vec3 LerpVec3(const Vec3& from, const Vec3& to, f32_t t)
    {
        return Vec3{
            from.x + (to.x - from.x) * t,
            from.y + (to.y - from.y) * t,
            from.z + (to.z - from.z) * t
        };
    }

    Vec3 LerpRotationNear(const Vec3& from, const Vec3& to, f32_t t)
    {
        Vec3 result = from;
        result.x = from.x + (to.x - from.x) * t;
        result.z = from.z + (to.z - from.z) * t;

        const f32_t targetYaw = MakeChampionVisualYawNear(to.y, from.y);
        result.y = from.y + NormalizeChampionVisualYaw(targetYaw - from.y) * t;
        return result;
    }

    bool_t HasCommandLineToken(const wchar_t* token)
    {
        const wchar_t* cmd = GetCommandLineW();
        return cmd != nullptr && token != nullptr && std::wcsstr(cmd, token) != nullptr;
    }

    bool_t ShouldRunInGameSkillSmoke()
    {
        return HasCommandLineToken(L"--banpick-smoke")
            && !HasCommandLineToken(L"--smoke-no-skill");
    }

    bool_t IsValidChampionId(eChampion champion)
    {
        return champion != eChampion::END && champion != eChampion::NONE;
    }

    const ChampionDef* FindClientChampionDef(eChampion champion)
    {
        const ChampionCatalogEntry* pEntry = CChampionCatalog::Instance().Find(champion);
        if (pEntry && pEntry->pDef)
            return pEntry->pDef;

        const ChampionDef* pDef = CChampionRegistry::Instance().Find(champion);
        if (pDef)
            return pDef;

        return FindChampionDef(champion);
    }

    bool_t IsNetworkAction(u16_t actionId)
    {
        const auto id = static_cast<eActionStateId>(actionId);
        return id == eActionStateId::BasicAttack ||
            id == eActionStateId::SkillQ ||
            id == eActionStateId::SkillW ||
            id == eActionStateId::SkillE ||
            id == eActionStateId::SkillR ||
            id == eActionStateId::Recall ||
            id == eActionStateId::DeathStart ||
            id == eActionStateId::ViegoConsumeSoul;
    }

    std::vector<CNavGrid::Cell> SmoothClientMovePathCells(
        const CNavGrid& navGrid,
        const std::vector<CNavGrid::Cell>& path)
    {
        if (path.size() <= 2)
            return path;

        std::vector<CNavGrid::Cell> smoothed{};
        smoothed.reserve(path.size());
        smoothed.push_back(path.front());

        size_t anchor = 0;
        while (anchor + 1u < path.size())
        {
            size_t best = anchor + 1u;
            for (size_t probe = path.size() - 1u; probe > anchor + 1u; --probe)
            {
                if (navGrid.LineCellsWalkableForRadius(path[anchor], path[probe], 0.f))
                {
                    best = probe;
                    break;
                }
            }

            smoothed.push_back(path[best]);
            anchor = best;
        }

        return smoothed;
    }

    bool_t IsFacingCandidateOpposedToIntent(
        const Vec3& origin,
        const Vec3& intentTarget,
        const Vec3& candidate)
    {
        const Vec3 intent{
            intentTarget.x - origin.x,
            0.f,
            intentTarget.z - origin.z
        };
        const Vec3 candidateDir{
            candidate.x - origin.x,
            0.f,
            candidate.z - origin.z
        };
        const f32_t intentLenSq = intent.x * intent.x + intent.z * intent.z;
        const f32_t candidateLenSq =
            candidateDir.x * candidateDir.x + candidateDir.z * candidateDir.z;
        if (intentLenSq <= 0.0001f || candidateLenSq <= 0.0001f)
            return false;

        const f32_t dot = intent.x * candidateDir.x + intent.z * candidateDir.z;
        const f32_t minDot = -0.10f * std::sqrt(intentLenSq * candidateLenSq);
        return dot < minDot;
    }

    u8_t NetworkActionToSkillSlot(u16_t actionId)
    {
        switch (static_cast<eActionStateId>(actionId))
        {
        case eActionStateId::SkillQ:
            return static_cast<u8_t>(eSkillSlot::Q);
        case eActionStateId::SkillW:
            return static_cast<u8_t>(eSkillSlot::W);
        case eActionStateId::SkillE:
            return static_cast<u8_t>(eSkillSlot::E);
        case eActionStateId::SkillR:
            return static_cast<u8_t>(eSkillSlot::R);
        case eActionStateId::BasicAttack:
        default:
            return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
    }

    const SkillDef* FindNetworkSkillDef(eChampion champion, u16_t actionId)
    {
        if (static_cast<eActionStateId>(actionId) == eActionStateId::Recall)
            return nullptr;

        const u8_t slot = NetworkActionToSkillSlot(actionId);
        const SkillDef* pDef = CSkillRegistry::Instance().Find(champion, slot);
        if (!pDef)
            pDef = FindSkillDef(champion, slot);
        return pDef;
    }

    f32_t ResolveNetworkActionDurationSec(
        eChampion champion,
        const SkillDef* pDef,
        u16_t actionId,
        u8_t stage)
    {
        if (static_cast<eActionStateId>(actionId) == eActionStateId::Recall)
            return kRecallDurationSec;

        const u8_t slot = NetworkActionToSkillSlot(actionId);
        const ChampionSkillTimingDefaults timing =
            ChampionGameDataDB::ResolveSkillTiming(champion, slot, stage);

        f32_t durationSec = timing.lockDurationSec;
        if (durationSec <= 0.01f && stage >= 2u && pDef && pDef->stage2LockSec > 0.01f)
            durationSec = pDef->stage2LockSec;
        if (durationSec <= 0.01f && pDef && pDef->lockDurationSec > 0.01f)
            durationSec = pDef->lockDurationSec;
        if (durationSec <= 0.01f)
            durationSec = static_cast<eActionStateId>(actionId) == eActionStateId::BasicAttack ? 0.45f : 0.6f;

        return durationSec;
    }

    std::string ResolveNetworkAnimName(const ChampionDef& championDef, const char* pAnimKey)
    {
        if (!pAnimKey || pAnimKey[0] == 0)
            return {};

        if (championDef.animPrefix && championDef.animPrefix[0] != 0)
        {
            const size_t prefixLen = std::strlen(championDef.animPrefix);
            if (std::strncmp(pAnimKey, championDef.animPrefix, prefixLen) == 0)
                return std::string(pAnimKey);
            return std::string(championDef.animPrefix) + pAnimKey;
        }

        return std::string(pAnimKey);
    }

    std::string ResolveNetworkBaseAnimName(
        const ChampionDef& championDef,
        bool_t bMoving)
    {
        return ResolveNetworkAnimName(
            championDef,
            bMoving ? championDef.runAnimKey : championDef.idleAnimKey);
    }

    std::string ResolveNetworkDeathAnimName(const ChampionDef& championDef)
    {
        return ResolveNetworkAnimName(championDef, "death");
    }

    bool_t ShouldLoopNetworkAction(
        eChampion champion,
        u16_t actionId,
        u8_t stage)
    {
        return champion == eChampion::JAX &&
            static_cast<eActionStateId>(actionId) == eActionStateId::SkillE &&
            stage <= 1u;
    }

    std::string ResolveNetworkActionAnimName(
        eChampion champion,
        const SkillDef* pSkillDef,
        const ActionStateComponent& action)
    {
        if (!pSkillDef)
        {
            return {};
        }

        const ChampionDef* pChampionDef = FindClientChampionDef(champion);
        if (!pChampionDef)
            return {};

        const u8_t stage = action.stage == 0u ? 1u : action.stage;
        const bool_t bStage2 = stage >= 2u;
        const char* pAnimKey = (bStage2 && pSkillDef->stage2AnimKey)
            ? pSkillDef->stage2AnimKey
            : pSkillDef->animKey;

        return ResolveNetworkAnimName(*pChampionDef, pAnimKey);
    }

    void PlayLoopNetworkActionIfNeeded(
        eChampion champion,
        const SkillDef* pSkillDef,
        const ActionStateComponent& action,
        RenderComponent& render)
    {
        if (!ShouldLoopNetworkAction(champion, action.actionId, action.stage) ||
            !render.pRenderer)
            return;

        const std::string animName =
            ResolveNetworkActionAnimName(champion, pSkillDef, action);
        if (animName.empty())
            return;

        const Engine::CAnimator* pAnimator = render.pRenderer->GetAnimator();
        const Engine::CAnimation* pCurrentAnim = pAnimator
            ? pAnimator->GetCurrentAnimation()
            : nullptr;
        if (pAnimator &&
            pAnimator->IsPlaying() &&
            pCurrentAnim &&
            pCurrentAnim->GetName() == animName)
        {
            return;
        }

        render.pRenderer->PlayAnimationByNameAdvanced(
            animName,
            true,
            false,
            1.f);
    }

    void LogNetworkEndTransition(
        EntityID entity,
        const char* pAnimName,
        bool_t bMoving,
        f32_t durationSec)
    {
        static u32_t s_logCount = 0;
        if (s_logCount >= 96u)
            return;

        char msg[192]{};
        sprintf_s(msg,
            "[NetworkEndTransition] entity=%u anim=%s moving=%u duration=%.2f\n",
            static_cast<u32_t>(entity),
            pAnimName ? pAnimName : "(null)",
            bMoving ? 1u : 0u,
            durationSec);
        Winters::DevSmoke::Log("%s", msg);
        ++s_logCount;
    }

    void ResetNetworkAnimatorSpeed(RenderComponent& render)
    {
        if (!render.pRenderer)
            return;

        if (Engine::CAnimator* pAnimator = render.pRenderer->GetAnimator())
            pAnimator->SetPlaySpeed(1.f);
    }

    eChampion ResolveLocalRosterChampion(const GameContext& context)
    {
        if (context.bUseNetworkRoster)
        {
            for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
            {
                const GameRosterSlot& slot = context.Roster[i];
                if (!slot.bHuman || !IsValidChampionId(slot.champion))
                    continue;
                if (context.MySessionId != 0 && slot.sessionId == context.MySessionId)
                    return slot.champion;
            }

            for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
            {
                const GameRosterSlot& slot = context.Roster[i];
                if (!slot.bHuman || !IsValidChampionId(slot.champion))
                    continue;
                if (context.MyNetId != 0 && slot.netId == context.MyNetId)
                    return slot.champion;
            }

            if (context.MySlotId != kInvalidGameRosterSlot
                && context.MySlotId < kGameRosterSlotCount)
            {
                const GameRosterSlot& slot = context.Roster[context.MySlotId];
                if ((slot.bHuman || slot.bBot) && IsValidChampionId(slot.champion))
                    return slot.champion;
            }
        }

        return context.SelectedChampion;
    }

    constexpr char kBaseMapMeshPath[] =
        "Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh";
    constexpr wchar_t kBaseMapMeshPathW[] =
        L"Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh";
    constexpr char kFullLayerMapMeshPath[] =
        "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/sr_base_flip_full_layers.wmesh";
    constexpr wchar_t kFullLayerMapMeshPathW[] =
        L"Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/sr_base_flip_full_layers.wmesh";
    constexpr wchar_t kMapBrushVolumeCsvPath[] =
        L"Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv";

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
        return ShouldUseFullLayerMap() ? kFullLayerMapMeshPath : kBaseMapMeshPath;
    }

    const wchar_t* SelectMapSurfacePath()
    {
        return ShouldUseFullLayerMap() ? kFullLayerMapMeshPathW : kBaseMapMeshPathW;
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

    constexpr wchar_t kMapBrushVolumeBinPath[] =
        L"Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_brush_volumes.wbrush";

    // map11 부쉬 시야 볼륨 바이너리(.wbrush v1, Tools/cook_map11_brush_volumes.py로 쿡).
    // TODO(wmap): Stage 데이터 .wmap 통합 시 BushEntry(07_STAGE6_WMAP.md)로 승격.
    struct MapBrushVolumeRecord
    {
        u32_t bushId = 0;
        f32_t worldX = 0.f;
        f32_t worldZ = 0.f;
        f32_t radius = 0.f;
    };

    bool_t SeedMap11BrushesFromBinaryForBootstrap(CWorld& world)
    {
        wchar_t resolvedPath[MAX_PATH]{};
        if (!WintersResolveContentPath(kMapBrushVolumeBinPath, resolvedPath, MAX_PATH))
            return false;

        FILE* fp = nullptr;
        if (_wfopen_s(&fp, resolvedPath, L"rb") != 0 || !fp)
            return false;

        constexpr u32_t kBrushVolumeMagic = 0x48534257u; // 'WBSH'
        u32_t header[4]{};
        if (fread(header, sizeof(header), 1, fp) != 1 ||
            header[0] != kBrushVolumeMagic ||
            header[1] != 1u ||
            header[2] == 0u ||
            header[2] > 4096u)
        {
            fclose(fp);
            return false;
        }

        u32_t iSeeded = 0;
        for (u32_t i = 0; i < header[2]; ++i)
        {
            MapBrushVolumeRecord record{};
            if (fread(&record, sizeof(record), 1, fp) != 1)
                break;
            if (record.radius <= 0.f)
                continue;

            const EntityID entity = world.CreateEntity();
            BushVolumeComponent component{};
            component.center = { record.worldX, 0.f, record.worldZ };
            component.radius = record.radius;
            component.bushId = record.bushId;
            world.AddComponent<BushVolumeComponent>(entity, component);
            ++iSeeded;
        }

        fclose(fp);
        return iSeeded > 0;
    }

    bool_t SeedMap11BrushesFromResourceForBootstrap(CWorld& world)
    {
        wchar_t resolvedPath[MAX_PATH]{};
        if (!WintersResolveContentPath(kMapBrushVolumeCsvPath, resolvedPath, MAX_PATH))
        {
            return false;
        }

        FILE* fp = nullptr;
        if (_wfopen_s(&fp, resolvedPath, L"rt") != 0 || !fp)
        {
            return false;
        }

        u32_t iSeeded = 0;
        char line[256]{};
        while (fgets(line, static_cast<int>(sizeof(line)), fp))
        {
            if (line[0] == '\0' || line[0] == '#' || line[0] == '\n' || line[0] == '\r')
                continue;

            u32_t bushId = 0;
            f32_t x = 0.f;
            f32_t z = 0.f;
            f32_t radius = 0.f;
            if (sscanf_s(line, " %u , %f , %f , %f", &bushId, &x, &z, &radius) != 4)
                continue;

            if (radius <= 0.f)
                continue;

            const EntityID entity = world.CreateEntity();
            BushVolumeComponent component{};
            component.center = { x, 0.f, z };
            component.radius = radius;
            component.bushId = bushId;
            world.AddComponent<BushVolumeComponent>(entity, component);
            ++iSeeded;
        }

        fclose(fp);

        return iSeeded > 0;
    }

    bool_t s_bWReleasePending = false;
    EntityID s_NetworkAttackTarget = NULL_ENTITY;
    u32_t s_uNetworkAttackCommandFrame = 0u;
    u32_t s_uNetworkAttackMissLogCount = 0u;
    constexpr u32_t kNetworkAttackCommandIntervalFrames = 6u;

    void ClearNetworkAttackIntent()
    {
        s_NetworkAttackTarget = NULL_ENTITY;
        s_uNetworkAttackCommandFrame = 0u;
    }

    bool_t HasPendingSkillStage(CScene_InGame& scene, u8_t slot)
    {
        CWorld& world = scene.GetWorld();
        const EntityID player = scene.GetPlayerEntity();
        if (player == NULL_ENTITY ||
            !world.HasComponent<SkillStateComponent>(player) ||
            slot >= 5u)
        {
            return false;
        }

        const auto& skillSlot =
            world.GetComponent<SkillStateComponent>(player).slots[slot];
        return skillSlot.currentStage == 1 && skillSlot.stageWindow > 0.f;
    }

    void ProtectNetworkBasicAttackYaw(CScene_InGame& scene, u32_t commandSeq)
    {
        CSnapshotApplier* pSnapshotApplier = scene.GetSnapshotApplier();
        CClientNetwork* pNetworkView = scene.GetNetworkView();
        CTransform* pPlayerTransform = scene.GetPlayerTransformPtr();
        if (commandSeq == 0 ||
            !pSnapshotApplier ||
            !pNetworkView ||
            !pPlayerTransform)
        {
            return;
        }

        pSnapshotApplier->ProtectLocalMoveYaw(
            pNetworkView->GetMyNetEntityId(),
            commandSeq,
            pPlayerTransform->GetRotation().y);
    }

    bool_t IsLocalSkillLearned(CScene_InGame& scene, uint8_t slot)
    {
        if (slot == static_cast<uint8_t>(eSkillSlot::BasicAttack))
            return true;

        CWorld& world = scene.GetWorld();
        const EntityID player = scene.GetPlayerEntity();
        if (!world.HasComponent<SkillRankComponent>(player) ||
            slot >= SkillRankComponent::kSlotCount)
        {
            return false;
        }

        return world.GetComponent<SkillRankComponent>(player).ranks[slot] > 0;
    }

    bool_t ShouldLoopLocalSkillAnimation(const SkillDef& def, u8_t skillStage)
    {
        if (def.champ == eChampion::JAX &&
            def.slot == static_cast<u8_t>(eSkillSlot::E) &&
            skillStage == 1u)
        {
            return true;
        }

        return !def.bOneShot;
    }

    f32_t& LocalKalistaPassiveDashDurationSec()
    {
        static f32_t s_fDurationSec =
            ChampionGameDataDB::ResolvePassiveDashDurationSec(eChampion::KALISTA);
        return s_fDurationSec;
    }

    void NotifyTowerAggroOnChampionHit(CWorld& world, EntityID attacker, EntityID victim)
    {
        if (attacker == NULL_ENTITY || victim == NULL_ENTITY)
            return;
        if (!world.IsAlive(attacker) || !world.IsAlive(victim))
            return;
        if (!world.HasComponent<ChampionComponent>(attacker) ||
            !world.HasComponent<ChampionComponent>(victim))
        {
            return;
        }

        TowerAggroNotifyComponent notify{};
        notify.attackerEntity = attacker;
        notify.victimEntity = victim;
        notify.priorityDuration = 2.0f;

        if (world.HasComponent<TowerAggroNotifyComponent>(attacker))
            world.GetComponent<TowerAggroNotifyComponent>(attacker) = notify;
        else
            world.AddComponent<TowerAggroNotifyComponent>(attacker, notify);
    }

    void SetLocalPassiveDashDuration(f32_t duration)
    {
        LocalKalistaPassiveDashDurationSec() = (duration < 0.03f) ? 0.03f : duration;
    }

    f32_t GetLocalPassiveDashDuration()
    {
        return LocalKalistaPassiveDashDurationSec();
    }

    constexpr f32_t kPlayerAvoidancePadding = 0.05f;

    bool_t IsPlayerMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::JungleMob;
    }

    f32_t ResolveAgentRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return (std::max)(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        return 0.5f;
    }

    ChampionStatsDef ResolvePlayerStatsDef(CWorld& world, EntityID playerEntity)
    {
        eChampion champion = eChampion::NONE;
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<ChampionComponent>(playerEntity))
        {
            champion = world.GetComponent<ChampionComponent>(playerEntity).id;
        }

        return CChampionStatsRegistry::Instance().Resolve(champion);
    }

    f32_t ResolvePlayerMoveSpeed(CWorld& world, EntityID playerEntity)
    {
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<StatComponent>(playerEntity))
        {
            const f32_t moveSpeed =
                world.GetComponent<StatComponent>(playerEntity).moveSpeed;
            if (moveSpeed > 0.f)
                return moveSpeed;
        }

        return ResolvePlayerStatsDef(world, playerEntity).baseMoveSpeed;
    }

    f32_t ResolvePlayerArriveRadius(CWorld& world, EntityID playerEntity)
    {
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<NavAgentComponent>(playerEntity))
        {
            const f32_t arriveRadius =
                world.GetComponent<NavAgentComponent>(playerEntity).fArriveRadius;
            if (arriveRadius > 0.f)
                return arriveRadius;
        }

        return ResolvePlayerStatsDef(world, playerEntity).navArriveRadius;
    }

    bool_t IsSeparatingCandidate(
        const Vec3& vCurrent, const Vec3& vCandidate, const Vec3& vBlockerPos, f32_t minDistSq)
    {
        const f32_t currentDistSq = WintersMath::DistanceSqXZ(vCurrent, vBlockerPos);

        if (currentDistSq >= minDistSq)
            return false;

        const f32_t candidateDistSq = WintersMath::DistanceSqXZ(vCandidate, vBlockerPos);
        return candidateDistSq > currentDistSq + 0.0001f;
    }

    bool_t IsCandidateClear(
        CWorld& world,
        EntityID self,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius)
    {
        bool_t bClear = true;
        world.ForEach<SpatialAgentComponent, TransformComponent>(
            std::function<void(EntityID, SpatialAgentComponent&, TransformComponent&)>(
                [&](EntityID other, SpatialAgentComponent& agent, TransformComponent& tf)
                {
                    if (!bClear || other == self)
                        return;
                    if (!IsPlayerMoveBlockingKind(agent.kind))
                        return;

                    if (world.HasComponent<HealthComponent>(other))
                    {
                        const auto& health = world.GetComponent<HealthComponent>(other);
                        if (health.bIsDead || health.fCurrent <= 0.f)
                            return;
                    }

                    const f32_t minDist =
                        radius + (std::max)(0.2f, agent.radius) + kPlayerAvoidancePadding;
                    const Vec3 otherPos = tf.GetPosition();
                    const f32_t minDistSq = minDist * minDist;
                    if (WintersMath::DistanceSqXZ(candidate, otherPos) < minDistSq &&
                        !IsSeparatingCandidate(current, candidate, otherPos, minDistSq))
                        bClear = false;
                }));

        return bClear;
    }

    Vec3 ResolveAvoidedMoveDirection(
        CWorld& world,
        EntityID self,
        const Vec3& pos,
        const Vec3& desired,
        f32_t step,
        const std::function<bool_t(const Vec3&)>& isStepWalkable)
    {
        static constexpr f32_t kAngles[] = {
            0.f,
            0.610865f, -0.610865f,
            1.22173f, -1.22173f,
            1.570796f, -1.570796f
        };

        const f32_t radius = ResolveAgentRadius(world, self);
        for (const f32_t angle : kAngles)
        {
            const Vec3 dir = WintersMath::RotateXZ(desired, angle);
            const Vec3 candidate{
                pos.x + dir.x * step,
                pos.y,
                pos.z + dir.z * step
            };

            if (!IsCandidateClear(world, self, pos, candidate, radius))
                continue;

            if (isStepWalkable && !isStepWalkable(candidate))
                continue;

            return dir;
        }

        return Vec3{};
    }
    // Mouse pick indicator arrows converge toward the accepted move target.
    void SpawnMovementIndicator(CScene_InGame& scene, const Vec3& center)
    {
        static constexpr const wchar_t* kTexturePath =
            L"Client/Bin/Resource/Texture/UI/movement_indicator.png";

        static constexpr f32_t kLifetime = 0.32f;
        static constexpr f32_t kStartRadius = 0.95f;
        static constexpr f32_t kEndRadius = 0.18f;
        static constexpr f32_t kInwardSpeed = (kStartRadius - kEndRadius) / kLifetime;
        static constexpr f32_t kYOffset = 0.05f;
        static constexpr f32_t kWidth = 0.55f;
        static constexpr f32_t kHeight = 0.90f;
        static constexpr f32_t kYawOffset = WintersMath::kPi;

        const Vec3 radialDirs[4] = {
            { 1.f, 0.f, 0.f },
            { -1.f, 0.f, 0.f },
            { 0.f, 0.f, 1.f },
            { 0.f, 0.f, -1.f },
        };

        for (const Vec3& radial : radialDirs)
        {
            const Vec3 inward{ -radial.x, 0.f, -radial.z };

            FxBillboardComponent fx{};
            fx.renderType = eFxRenderType::GroundDecal;
            fx.texturePath = kTexturePath;
            fx.vWorldPos = {
                center.x + radial.x * kStartRadius,
                center.y + kYOffset,
                center.z + radial.z * kStartRadius
            };
            fx.vVelocity = { inward.x * kInwardSpeed, 0.f, inward.z * kInwardSpeed };
            fx.fWidth = kWidth;
            fx.fHeight = kHeight;
            fx.fYaw = std::atan2f(inward.x, inward.z) + kYawOffset;
            fx.vColor = { 1.f, 1.f, 1.f, 0.95f };
            fx.fLifetime = kLifetime;
            fx.fFadeIn = 0.02f;
            fx.fFadeOut = 0.22f;
            fx.fAlphaClip = 0.02f;
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.depthMode = eFxDepthMode::DepthTestWriteOff;
            fx.bBillboard = false;

            CFxSystem::Spawn(scene.GetWorld(), fx);
        }
    }

    constexpr f32_t kFallbackScreenWidth = 1280.f;
    constexpr f32_t kFallbackScreenHeight = 720.f;

    Vec3 GameplayForwardFromVisualYaw(eChampion champion, f32_t yaw)
    {
        const f32_t gameplayYaw = yaw - ChampionGameDataDB::ResolveVisualYawOffset(champion);
        return Vec3{ std::sinf(gameplayYaw), 0.f, std::cosf(gameplayYaw) };
    }

    void FlushTransformForRender(TransformComponent& tf)
    {
        if (tf.m_bLocalDirty)
        {
            DirectX::XMVECTOR scale = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalScale));
            DirectX::XMVECTOR rot = DirectX::XMQuaternionRotationRollPitchYaw(
                tf.m_LocalRotation.x,
                tf.m_LocalRotation.y,
                tf.m_LocalRotation.z);
            DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalPosition));
            DirectX::XMMATRIX local = DirectX::XMMatrixAffineTransformation(scale, DirectX::XMVectorZero(), rot, pos);
            DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&tf.m_LocalMatrix), local);
            tf.m_bLocalDirty = false;
            tf.m_bWorldDirty = true;
        }

        if (tf.m_bWorldDirty)
        {
            tf.m_WorldMatrix = tf.m_LocalMatrix;
            tf.m_bWorldDirty = false;
        }
    }

    Mat4 BuildContactShadowWorld(const TransformComponent& tf,
        f32_t fSize,
        f32_t fYOffset)
    {
        const Vec3 vPos = tf.GetPosition();
        const DirectX::XMMATRIX matScale =
            DirectX::XMMatrixScaling(fSize, 1.f, fSize * 0.72f);
        const DirectX::XMMATRIX matTrans =
            DirectX::XMMatrixTranslation(vPos.x, vPos.y + fYOffset, vPos.z);

        Mat4 matWorld{};
        DirectX::XMStoreFloat4x4(
            reinterpret_cast<DirectX::XMFLOAT4X4*>(&matWorld.m),
            matScale * matTrans);

        return matWorld;
    }

    void RenderAttackRangePreview(
        CScene_InGame& scene,
        const Mat4& matViewProjection,
        IRHIDevice* pDevice,
        bool_t bUseDX12RHI)
    {
        CTransform* pPlayerTransform = scene.GetPlayerTransformPtr();
        if (!scene.IsShowAttackRange() || !pPlayerTransform)
            return;

        const f32_t fRadius = GameplayQuery::ResolveAttackRangePreviewRadius(
            scene.GetWorld(),
            scene.GetPlayerEntity(),
            scene.GetPlayerChampionId(),
            scene.GetBasicAttackRange(),
            scene.IsNetworkAuthoritativeGameplay());

        const Vec3& vPos = pPlayerTransform->GetPosition();
        const DirectX::XMMATRIX matScale =
            DirectX::XMMatrixScaling(fRadius * 2.f, 1.f, fRadius * 2.f);
        const DirectX::XMMATRIX matTrans =
            DirectX::XMMatrixTranslation(vPos.x, vPos.y + 0.02f, vPos.z);

        Mat4 matWorld{};
        DirectX::XMStoreFloat4x4(
            reinterpret_cast<DirectX::XMFLOAT4X4*>(&matWorld.m),
            matScale * matTrans);

        if (bUseDX12RHI &&
            scene.GetRHIUtilityPlaneRenderer() &&
            scene.GetRHIAttackRangeTexture().IsValid() &&
            pDevice)
        {
            scene.GetRHIUtilityPlaneRenderer()->Draw(
                pDevice,
                scene.GetRHIAttackRangeTexture(),
                matWorld,
                matViewProjection,
                { 1.f, 1.f, 1.f, 1.f },
                { 0.f, 0.f, 1.f, 1.f },
                { 0.f, 0.f },
                0.02f,
                0.f,
                eBlendPreset::AlphaBlend);
            return;
        }

        if (scene.GetAttackRangePlane() && scene.GetAttackRangeTexture())
        {
            scene.GetAttackRangePlane()->SetWorld(matWorld);
            scene.GetAttackRangePlane()->Render(pDevice, matViewProjection);
        }
    }

    constexpr wchar_t kMapAmbientPropBinPath[] =
        L"Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_ambient_props.wamb";

    // map11 앰비언트 프롭 배치(.wamb v1, Tools/cook_map11_ambient_props.py로 쿡).
    // 좌표는 LoL 공간 그대로이며 스폰 시 맵 메시와 동일한 m_MapTransform으로 변환한다.
    struct MapAmbientPropRecord
    {
        u32_t kind = 0;
        f32_t lolX = 0.f;
        f32_t lolY = 0.f;
        f32_t lolZ = 0.f;
        f32_t lolYaw = 0.f;
        f32_t scale = 1.f;
    };

    struct MapAmbientAssetDesc
    {
        const char* pMeshPath;
        const char* pIdleAnim;
    };

    constexpr MapAmbientAssetDesc kMapAmbientAssets[] = {
        { "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/ambient/sru_bird/sru_bird.wmesh",
          "sru_bird_idle_tree1" },
        { "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/ambient/sru_duck/sru_duck.wmesh",
          "sru_duck_idle1" },
    };

    void SendServerInGameReady()
    {
        GameContext& context = CGameInstance::Get()->Get_GameContext();
        CGameSessionClient& session = CGameSessionClient::Instance();

        if (!context.bUseNetworkRoster || !session.IsConnected())
            return;

        session.Pump();
        if (session.HasLobbyState())
            session.CopyLobbyToGameContext(context);

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

bool CScene_InGame::OnEnter()
{
    const GameContext& gameContext = CGameInstance::Get()->Get_GameContext();
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
    m_World.Initialize_Spatial(LoLSpatialGridDesc());

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
        auto pStatus = CStatusEffectSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pStatus));
    }

    {
        auto pVision = Engine::CVisionSystem::Create(m_World.Get_SpatialIndex(), &m_BushIndex);
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
    CMinion_Manager::Get()->PrewarmNetworkVisualResources();

    wchar_t stagePath[MAX_PATH] = {};
    if (CMapDataIO::Get_StagePathW(1, stagePath, MAX_PATH))
    {
        CMapDataIO::Load_Stage(stagePath);
        Winters::DevSmoke::Log("[InGameBootstrap] Stage1 loaded\n");
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
        bMapInit = m_Map.Initialize(SelectMapMeshPath(), L"Shaders/Mesh3D.hlsl");
        Winters::DevSmoke::Log("[InGameBootstrap] map init done ok=%u\n", bMapInit ? 1u : 0u);
    }
    m_MapTransform.SetPosition(0.f, 0.f, 0.f);
    m_MapTransform.SetScale({ -0.01f, 0.01f, 0.01f });
    m_MapTransform.SetRotation(m_vMapRotation);
    InitializeMapSurfaceSampler(bMapInit, SelectMapSurfacePath());
    SpawnMapAmbientProps();

    Winters::DevSmoke::Log("[InGameBootstrap] CreateECSEntities begin\n");
    CreateECSEntities();
    Winters::DevSmoke::Log("[InGameBootstrap] CreateECSEntities done player=%u total=%u\n",
        static_cast<u32_t>(m_PlayerEntity),
        m_World.GetEntityCount());
    if (!SeedMap11BrushesFromBinaryForBootstrap(m_World) &&
        !SeedMap11BrushesFromResourceForBootstrap(m_World))
    {
        SeedPracticeBushesForBootstrap(m_World);
    }
    m_BushIndex.Build(m_World);
    if (m_pVisionSystem)
        m_pVisionSystem->ForceRebuildNextFrame();

    const eChampion selectedChampion = GetPlayerChampionId();
    if (CGameInstance::Get()->Get_GameContext().bUseNetworkRoster
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
    CGameInstance::Get()->UI_Set_InGameBuyItemCallback(&UI_SendBuyItemCommand, this);
    CGameInstance::Get()->UI_Set_LevelSkillCallback(&UI_SendLevelSkillCommand, this);

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
        auto pAI = CMinionAISystem::Create();
        pAI->Set_JobSystem(pJS);
        m_pScheduler->RegisterSystem(std::move(pAI));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pMinionPerformance = Engine::CMinionPerformanceSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pMinionPerformance));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pTurretAI = Engine::CTurretAISystem::Create();
        m_pScheduler->RegisterSystem(std::move(pTurretAI));
    }

    if (!m_bNetworkAuthoritativeGameplay)
    {
        auto pTurretProjectiles = Engine::CTurretProjectileSystem::Create();
        m_pScheduler->RegisterSystem(std::move(pTurretProjectiles));
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

        if (bUseDX12RHI)
        {
            m_pRHIUtilityPlaneRenderer = CRHIFxSpriteRenderer::Create(pRhiDevice);
            m_hRHIAttackRangeTex = RHI_CreateTextureFromFile(
                pRhiDevice,
                L"Client/Bin/Resource/Texture/UI/UI_AttackRange.png",
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
                L"Client/Bin/Resource/Texture/UI/UI_AttackRange.png",
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
            m_pFogOfWarRenderer = CFogOfWarRenderer::Create(
                pRhiDevice, Engine::CVisionSystem::FOW_TEX_DIM);
        }
    }
    Winters::DevSmoke::Log("[InGameBootstrap] render helpers ready\n");

    {
        CGameInstance* pGI = CGameInstance::Get();
        IRHIDevice* pRhiDevice = pGI->Get_RHIDevice();

        m_pFxSystem = CFxSystem::Create(
            pRhiDevice,
            pGI->Get_FxSpriteShader(),
            pGI->Get_FxSpritePipeline(),
            pGI->Get_BlendStateCache());
        m_pFxBeamSystem = CFxBeamSystem::Create(
            pRhiDevice,
            pGI->Get_FxSpriteShader(),
            pGI->Get_FxSpritePipeline(),
            pGI->Get_BlendStateCache());

        m_pFxMeshRenderer = Engine::CFxStaticMeshRenderer::Create(
            pRhiDevice,
            pGI->Get_MeshShader(),
            pGI->Get_MeshPipeline(),
            pGI->Get_FxMeshShader(),
            pGI->Get_FxMeshPipeline(),
            pGI->Get_BlendStateCache());
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
        static const struct { const char* fbx; const wchar_t* tex; } kIreliaFx[] = {
            { "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_blade.fbx",
              L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_blades_passive_4_texture.png" },
            { "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
              L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_beam_mult.png" },
        };
        for (const auto& it : kIreliaFx)
            m_pFxMeshRenderer->PreloadMesh(it.fbx, it.tex);

        static const struct { const char* fbx; const wchar_t* tex; } kYasuoFx[] = {
            { "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_base_q_tornado_blade_cas.fbx",
              L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_base_e_tonado_blend.png" },
            { "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_w_windwall_mesh.fbx",
              L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_w_windwall_dust.png" },
            { "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_base_r_sword_wind2.fbx",
              L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_r_sword_glow.png" },
        };
        for (const auto& it : kYasuoFx)
            m_pFxMeshRenderer->PreloadMesh(it.fbx, it.tex);

        static const struct { const char* fbx; const wchar_t* tex; } kKalistaFx[] = {
            { "Client/Bin/Resource/Texture/FX/Kalista/fbx/kalista_base_q_mis_spear.fbx",
              L"Client/Bin/Resource/Texture/FX/Kalista/kalista_base_q_mis_glow_color.png" },
            { "Client/Bin/Resource/Texture/FX/Kalista/fbx/kalista_base_e_spear_hold.fbx",
              L"Client/Bin/Resource/Texture/FX/Kalista/kalista_base_e_spear_glow.png" },
        };
        for (const auto& it : kKalistaFx)
            m_pFxMeshRenderer->PreloadMesh(it.fbx, it.tex);
    }

    CGameInstance::Get()->UI_Set_PlayerChampion(GetPlayerChampionId());

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
            return true;
        }

        m_strReplayStatus = "Replay loaded";
        return true;
    }

    SendServerInGameReady();
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

eChampion CScene_InGame::GetPlayerChampionId() const
{
    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        return m_World.GetComponent<ChampionComponent>(m_PlayerEntity).id;
    }

    return ResolveLocalRosterChampion(CGameInstance::Get()->Get_GameContext());
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
        championId = m_World.GetComponent<ChampionComponent>(entity).id;

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
    GameContext& context = CGameInstance::Get()->Get_GameContext();
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

void CScene_InGame::SpawnMapAmbientProps()
{
    m_AmbientProps.clear();

    wchar_t resolvedPath[MAX_PATH]{};
    if (!WintersResolveContentPath(kMapAmbientPropBinPath, resolvedPath, MAX_PATH))
        return;

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, resolvedPath, L"rb") != 0 || !fp)
        return;

    constexpr u32_t kAmbientPropMagic = 0x424D4157u; // 'WAMB'
    u32_t header[4]{};
    if (fread(header, sizeof(header), 1, fp) != 1 ||
        header[0] != kAmbientPropMagic ||
        header[1] != 1u ||
        header[2] == 0u ||
        header[2] > 1024u)
    {
        fclose(fp);
        return;
    }

    const Mat4 mapWorld = m_MapTransform.GetWorldMatrix();
    const DirectX::XMMATRIX xmMapWorld =
        DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&mapWorld.m));

    constexpr u32_t kAssetCount =
        static_cast<u32_t>(sizeof(kMapAmbientAssets) / sizeof(kMapAmbientAssets[0]));

    for (u32_t i = 0; i < header[2]; ++i)
    {
        MapAmbientPropRecord record{};
        if (fread(&record, sizeof(record), 1, fp) != 1)
            break;
        if (record.kind >= kAssetCount)
            continue;

        const MapAmbientAssetDesc& asset = kMapAmbientAssets[record.kind];
        auto pRenderer = std::make_unique<ModelRenderer>();
        if (!pRenderer->Initialize(asset.pMeshPath, L"Shaders/Mesh3D.hlsl"))
        {
            Winters::DevSmoke::Log("[MapAmbient] init failed kind=%u\n", record.kind);
            continue;
        }
        pRenderer->PlayAnimationByName(asset.pIdleAnim, true);

        const DirectX::XMFLOAT3 lolPos{ record.lolX, record.lolY, record.lolZ };
        const DirectX::XMVECTOR vWorld = DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&lolPos), xmMapWorld);
        Vec3 worldPos{
            DirectX::XMVectorGetX(vWorld),
            DirectX::XMVectorGetY(vWorld),
            DirectX::XMVectorGetZ(vWorld)
        };
        (void)TryProjectToMapSurface(worldPos, 0.02f);

        MapAmbientProp prop{};
        prop.pRenderer = std::move(pRenderer);
        prop.transform.SetPosition(worldPos);
        // X-flip 맵이라 LoL yaw는 부호가 반전된 채 맵 회전에 더해진다.
        prop.transform.SetRotation({ 0.f, m_vMapRotation.y - record.lolYaw, 0.f });
        const f32_t fScale = 0.01f * record.scale;
        prop.transform.SetScale({ fScale, fScale, fScale });
        m_AmbientProps.push_back(std::move(prop));
    }

    fclose(fp);

    Winters::DevSmoke::Log(
        "[MapAmbient] spawned %zu ambient props\n", m_AmbientProps.size());
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

void CScene_InGame::InitializeNetworkSession()
{
    const GameContext& gameContext = CGameInstance::Get()->Get_GameContext();
    const bool_t bUseNetworkRoster = gameContext.bUseNetworkRoster;
    const bool_t bDisableLiveNetwork = m_bReplayPlaybackMode;

    m_pEntityIdMap = std::make_unique<EntityIdMap>();
    m_pNetworkView = nullptr;
    m_bUsingSharedNetwork = false;

    if (!bDisableLiveNetwork)
    {
        if (bUseNetworkRoster && CGameSessionClient::Instance().IsConnected())
        {
            m_bUsingSharedNetwork = true;
            m_pNetworkView = CGameSessionClient::Instance().GetNetwork();
        }
        else
        {
            m_pNetwork = CClientNetwork::Create();
            m_pNetworkView = m_pNetwork.get();
        }
    }

    m_pSnapshotApplier = CSnapshotApplier::Create();
    m_pEventApplier = CEventApplier::Create();
    m_pCommandSerializer = CCommandSerializer::Create();

    if (m_pSnapshotApplier)
    {
        m_pSnapshotApplier->SetOnNewEntityCallback(
            [this](u32_t netId, u8_t championId, u8_t team) -> EntityID
            {
                (void)netId;
                return SpawnChampionEntity(
                    static_cast<eChampion>(championId),
                    static_cast<eTeam>(team));
            });
        //viego soul callback function
        m_pSnapshotApplier->SetOnChampionVisualChangedCallback(
            [this](EntityID entity, u8_t championId, u8_t)
            {
                ChampionSpawnContext spawnContext{
                    m_World,
                    m_ChampionRenderers,
                    m_NetworkChampionPrevPos,
                    m_NetworkChampionMoveGraceSec,
                    m_NetworkChampionMoving
                };
                if (CChampionSpawnService::AttachVisual(
                        spawnContext,
                        entity,
                        static_cast<eChampion>(championId)) &&
                    entity == m_PlayerEntity)
                {
                    BindPlayerToECSChampion(entity);
                }
            });
        m_pSnapshotApplier->SetOnRemoveEntityCallback(
            [this](EntityID entity)
            {
                m_ChampionRenderers.erase(entity);
                m_NetworkChampionPrevPos.erase(entity);
                m_NetworkChampionMoveGraceSec.erase(entity);
                m_NetworkChampionMoving.erase(entity);
                m_NetworkActorInterpStates.erase(entity);
            });
        m_pSnapshotApplier->SetOnAuthoritativeSnapshot(
            [this](
                u64_t serverTick,
                u64_t iServerTimeMs,
                u32_t lastAckedCommandSeq,
                u32_t localNetId)
            {
                CGameInstance::Get()->UI_SetGameContextServerTimeMs(iServerTimeMs);
                OnAuthoritativeSnapshot(
                    serverTick,
                    iServerTimeMs,
                    lastAckedCommandSeq,
                    localNetId);
            });
    }

    if (bDisableLiveNetwork)
        return;

    if (!m_pNetworkView || !m_pSnapshotApplier || !m_pEventApplier || !m_pEntityIdMap)
        return;

    CGameSessionClient::FrameCallback frameHandler =
        [this](ePacketType type, u32_t sequence, const u8_t* payload, u32_t len)
        {
            (void)sequence;

            if (!m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
                return;

            EntityIdMap& entityMap = *m_pEntityIdMap;
            CSnapshotApplier& snapshotApplier = *m_pSnapshotApplier;
            CEventApplier& eventApplier = *m_pEventApplier;

            if (type == ePacketType::Hello)
            {
                u32_t myNetId = 0;
                u32_t mySessionId = 0;
                snapshotApplier.OnHello(
                    m_World,
                    entityMap,
                    payload,
                    len,
                    &myNetId,
                    &mySessionId);

                const GameContext& context = CGameInstance::Get()->Get_GameContext();
                const u32_t bindNetId = myNetId != 0
                    ? myNetId
                    : (context.bUseNetworkRoster ? context.MyNetId : 0);
                const u32_t bindSessionId = mySessionId != 0
                    ? mySessionId
                    : (context.bUseNetworkRoster ? context.MySessionId : 0);

                if (m_pNetworkView)
                {
                    m_pNetworkView->SetMyNetEntityId(bindNetId);
                    m_pNetworkView->SetMySessionId(bindSessionId);
                }

                const EntityID localNetEntity = bindNetId != 0
                    ? entityMap.FromNet(bindNetId)
                    : NULL_ENTITY;
                Winters::DevSmoke::Log(
                    "[InGameNetwork] hello myNet=%u mySid=%u bindNet=%u bindSid=%u entity=%u\n",
                    myNetId,
                    mySessionId,
                    bindNetId,
                    bindSessionId,
                    static_cast<u32_t>(localNetEntity));
                if (localNetEntity != NULL_ENTITY)
                {
                    m_PlayerEntity = localNetEntity;
                    BindPlayerToECSChampion(m_PlayerEntity);
                }
            }
            else if (type == ePacketType::Snapshot)
            {
                static u32_t s_snapshotLogCount = 0;
                if (s_snapshotLogCount < 3u)
                {
                    Winters::DevSmoke::Log(
                        "[InGameNetwork] snapshot len=%u index=%u\n",
                        len,
                        s_snapshotLogCount);
                    ++s_snapshotLogCount;
                }
                snapshotApplier.OnSnapshot(m_World, entityMap, payload, len);
            }
            else if (type == ePacketType::Event)
            {
                eventApplier.OnEvent(m_World, entityMap, payload, len);
            }
        };

    if (m_bUsingSharedNetwork)
        CGameSessionClient::Instance().SetGameFrameCallback(std::move(frameHandler));
    else
        m_pNetworkView->SetFrameCallback(std::move(frameHandler));
    Winters::DevSmoke::Log("[Scene] callbacks registered (snapshot/event/cmd/network)\n");

    if (bUseNetworkRoster)
    {
        Winters::DevSmoke::Log(m_bUsingSharedNetwork
            ? "[Scene_InGame] Reusing BanPick TCP session.\n"
            : "[Scene_InGame] Network roster active without shared session; local roster only.\n");
    }
    else if (m_pNetworkView->Connect("127.0.0.1", 9000))
    {
        Winters::DevSmoke::Log("[Scene_InGame] Connected to local Winters server.\n");
    }
    else
    {
        Winters::DevSmoke::Log("[Scene_InGame] Server not reachable; running local-only mode.\n");
    }
}

bool_t CScene_InGame::PumpNetwork()
{
    const bool_t bNetworkActive = (m_pNetworkView && m_pNetworkView->IsConnected());
    if (!bNetworkActive)
        return false;

    if (m_bUsingSharedNetwork)
        CGameSessionClient::Instance().Pump();
    else
        m_pNetworkView->PumpReceivedFrames();

    return true;
}

void CScene_InGame::ReplayLastNetworkHelloIfShared()
{
    if (m_bUsingSharedNetwork)
        CGameSessionClient::Instance().ReplayLastHelloToGameFrameCallback();
}

bool CScene_InGame::HasPlayerTransform() const
{
    return m_pPlayerTransform != nullptr;
}

Vec3 CScene_InGame::GetPlayerPosition() const
{
    if (m_pPlayerTransform)
        return m_pPlayerTransform->GetPosition();
    return Vec3{};
}

void CScene_InGame::SetPlayerPosition(const Vec3& v)
{
    if (!m_pPlayerTransform)
        return;

    m_pPlayerTransform->SetPosition(v);
    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;
    if (m_pPlayerTransform != &m_PlayerEntityTransformCache)
        return;

    m_World.GetComponent<TransformComponent>(m_PlayerEntity).SetPosition(v);
}

f32_t CScene_InGame::GetPlayerYaw() const
{
    return m_pPlayerTransform ? m_pPlayerTransform->GetRotation().y : 0.f;
}

void CScene_InGame::SetPlayerYaw(f32_t yaw)
{
    if (!m_pPlayerTransform)
        return;

    Vec3 rot = m_pPlayerTransform->GetRotation();
    const f32_t previousYaw = rot.y;
    const f32_t normalizedYaw = NormalizeChampionVisualYaw(yaw);
    const f32_t resolvedYaw = MakeChampionVisualYawNear(normalizedYaw, previousYaw);
    m_pPlayerTransform->SetRotation({ rot.x, resolvedYaw, rot.z });

    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;
    if (m_pPlayerTransform != &m_PlayerEntityTransformCache)
        return;

    auto& tf = m_World.GetComponent<TransformComponent>(m_PlayerEntity);
    Vec3 ecsRot = tf.GetRotation();
    ecsRot.y = resolvedYaw;
    tf.SetRotation(ecsRot);
}

void CScene_InGame::SyncPlayerEntityTransformFromECS()
{
    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;

    if (m_bNetworkAuthoritativeGameplay &&
        m_bKalistaPassiveDashActive)
    {
        SyncPlayerEntityTransformToECS();
        return;
    }

    auto& tf = m_World.GetComponent<TransformComponent>(m_PlayerEntity);
    m_PlayerEntityTransformCache.SetPosition(tf.GetPosition());
    m_PlayerEntityTransformCache.SetRotation(tf.GetRotation());
    m_PlayerEntityTransformCache.SetScale(tf.GetScale());
}

void CScene_InGame::SyncPlayerEntityTransformToECS()
{
    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;
    if (m_pPlayerTransform != &m_PlayerEntityTransformCache)
        return;

    auto& tf = m_World.GetComponent<TransformComponent>(m_PlayerEntity);
    tf.SetPosition(m_PlayerEntityTransformCache.GetPosition());
    tf.SetRotation(m_PlayerEntityTransformCache.GetRotation());
    tf.SetScale(m_PlayerEntityTransformCache.GetScale());
}

void CScene_InGame::CaptureNetworkActorInterpolationStarts()
{
    if (!m_bNetworkAuthoritativeGameplay || !m_bNetworkActorInterpolationEnabled)
        return;

    auto capture = [this](EntityID e, TransformComponent& tf)
        {
            auto& state = m_NetworkActorInterpStates[e];
            state.vPendingStartPos = tf.GetPosition();
            state.vPendingStartRot = tf.GetRotation();
            state.bHasPendingStart = true;
        };

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            capture(e, tf);
        });

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID e, MinionStateComponent& ms, TransformComponent& tf)
        {
            if (ms.current == MinionStateComponent::Dead)
                return;

            capture(e, tf);
        });
}

void CScene_InGame::BeginNetworkActorInterpolationForSnapshot(u64_t serverTick)
{
    if (!m_bNetworkAuthoritativeGameplay || !m_bNetworkActorInterpolationEnabled)
        return;
    if (serverTick == 0)
        return;

    auto begin = [this, serverTick](
        EntityID e,
        TransformComponent& tf,
        bool_t bLocalDashProtected)
        {
            auto& state = m_NetworkActorInterpStates[e];
            const Vec3 targetPos = tf.GetPosition();
            const Vec3 targetRot = tf.GetRotation();
            const Vec3 startPos = state.bHasPendingStart ? state.vPendingStartPos : targetPos;
            const Vec3 startRot = state.bHasPendingStart ? state.vPendingStartRot : targetRot;

            const f32_t dx = targetPos.x - startPos.x;
            const f32_t dz = targetPos.z - startPos.z;
            const f32_t distSq = dx * dx + dz * dz;
            const f32_t yawDelta = std::fabs(NormalizeChampionVisualYaw(targetRot.y - startRot.y));
            const bool_t bTinyChange =
                distSq <= kNetworkActorInterpMinMoveSq &&
                yawDelta <= kNetworkActorInterpMinYaw;
            const bool_t bTeleport = distSq >= kNetworkActorInterpTeleportSq;

            state.vStartPos = startPos;
            state.vStartRot = startRot;
            state.vTargetPos = targetPos;
            state.vTargetRot = targetRot;
            state.fElapsedSec = 0.f;
            state.fDurationSec = kNetworkActorInterpDurationSec;
            state.uSourceServerTick = serverTick;
            state.bActive = !bTinyChange && !bTeleport && !bLocalDashProtected;
            state.bHasPendingStart = false;

            if (state.bActive)
            {
                tf.SetPosition(startPos);
                tf.SetRotation(startRot);
            }
            else
            {
                tf.SetPosition(targetPos);
                tf.SetRotation(targetRot);
            }
        };

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            const bool_t bLocalDashProtected =
                e == m_PlayerEntity && m_bKalistaPassiveDashActive;
            begin(e, tf, bLocalDashProtected);
        });

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID e, MinionStateComponent& ms, TransformComponent& tf)
        {
            if (ms.current == MinionStateComponent::Dead)
                return;

            begin(e, tf, false);
        });
}

void CScene_InGame::ApplyNetworkActorInterpolation(f32_t dt)
{
    if (!m_bNetworkAuthoritativeGameplay || !m_bNetworkActorInterpolationEnabled)
        return;

    for (auto it = m_NetworkActorInterpStates.begin();
        it != m_NetworkActorInterpStates.end();)
    {
        const EntityID entity = it->first;
        auto& state = it->second;

        if (!m_World.IsAlive(entity) || !m_World.HasComponent<TransformComponent>(entity))
        {
            it = m_NetworkActorInterpStates.erase(it);
            continue;
        }

        if (!state.bActive)
        {
            ++it;
            continue;
        }

        auto& tf = m_World.GetComponent<TransformComponent>(entity);
        state.fElapsedSec += dt;
        const f32_t denom = (state.fDurationSec > 0.001f) ? state.fDurationSec : 0.001f;
        const f32_t t = SmoothStep01(state.fElapsedSec / denom);

        tf.SetPosition(LerpVec3(state.vStartPos, state.vTargetPos, t));
        tf.SetRotation(LerpRotationNear(state.vStartRot, state.vTargetRot, t));

        if (t >= 1.f)
        {
            tf.SetPosition(state.vTargetPos);
            tf.SetRotation(state.vTargetRot);
            state.bActive = false;
        }

        ++it;
    }
}

bool_t CScene_InGame::IsNetworkChampionMoving(EntityID entity) const
{
    const auto it = m_NetworkChampionMoveGraceSec.find(entity);
    return it != m_NetworkChampionMoveGraceSec.end() && it->second > 0.f;
}

void CScene_InGame::UpdateNetworkChampionLocomotion(f32_t dt)
{
    static constexpr f32_t kMoveThresholdSq = 0.0001f; // 1 cm
    static constexpr f32_t kMoveHoldSec = 0.12f;       // bridge 30Hz snapshots

    m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent& champ, RenderComponent& rc, TransformComponent& tf)
        {
            if (rc.bSceneManaged || !rc.pRenderer)
                return;

            const Vec3 pos = tf.GetPosition();
            auto [prevIt, inserted] = m_NetworkChampionPrevPos.try_emplace(e, pos);
            f32_t& moveGrace = m_NetworkChampionMoveGraceSec[e];

            bool_t bPositionMoving = false;
            const bool_t bServerMoving =
                m_World.HasComponent<ReplicatedStateComponent>(e) &&
                (m_World.GetComponent<ReplicatedStateComponent>(e).stateFlags & kSnapshotStateMovingFlag) != 0u;
            const PoseStateComponent* pPose =
                m_World.HasComponent<PoseStateComponent>(e)
                ? &m_World.GetComponent<PoseStateComponent>(e)
                : nullptr;
            const ActionStateComponent* pAction =
                m_World.HasComponent<ActionStateComponent>(e)
                ? &m_World.GetComponent<ActionStateComponent>(e)
                : nullptr;

            const f32_t dx = pos.x - prevIt->second.x;
            const f32_t dz = pos.z - prevIt->second.z;
            const f32_t movedSq = dx * dx + dz * dz;
            if (!inserted && movedSq > kMoveThresholdSq)
            {
                moveGrace = kMoveHoldSec;
            }
            else if (bServerMoving)
            {
                moveGrace = kMoveHoldSec;
            }
            else if (moveGrace > 0.f)
            {
                moveGrace -= dt;
                if (moveGrace < 0.f)
                    moveGrace = 0.f;
            }

            bPositionMoving = (moveGrace > 0.f);
            const bool_t bPoseRequestsIdle =
                pPose && pPose->poseId == static_cast<u16_t>(ePoseStateId::Idle);
            const bool_t bPoseRequestsDeath =
                pPose && pPose->poseId == static_cast<u16_t>(ePoseStateId::Dead);
            if (bPoseRequestsDeath)
                moveGrace = 0.f;

            bool_t bMoving = !bPoseRequestsDeath && (bServerMoving || bPositionMoving);
            if (bPoseRequestsIdle && !bServerMoving && !bPositionMoving)
                bMoving = false;

            prevIt->second = pos;

            if (e == m_PlayerEntity)
                m_bMoving = bMoving;

            NetworkActionAnimationState& actionState = m_NetworkActionAnimStates[e];
            if (bPoseRequestsDeath)
            {
                const u32_t deathSeq = pAction &&
                    pAction->actionId == static_cast<u16_t>(eActionStateId::DeathStart)
                    ? pAction->sequence
                    : static_cast<u32_t>(pPose ? pPose->startTick : 0u);
                if (actionState.baseSeq != deathSeq)
                {
                    actionState = {};
                    actionState.baseSeq = deathSeq;

                    const ChampionDef* cd = FindClientChampionDef(champ.id);
                    if (cd)
                    {
                        const std::string deathAnim = ResolveNetworkDeathAnimName(*cd);
                        if (!deathAnim.empty())
                        {
                            ResetNetworkAnimatorSpeed(rc);
                            rc.pRenderer->PlayAnimationByName(deathAnim, false);
                        }
                    }
                }

                m_NetworkChampionMoving[e] = false;
                return;
            }

            if (pAction && pAction->sequence != 0)
            {
                if (IsNetworkAction(pAction->actionId))
                {
                    if (actionState.actionSeq != pAction->sequence)
                    {
                        const SkillDef* pSkillDef = FindNetworkSkillDef(champ.id, pAction->actionId);
                        actionState = {};
                        actionState.actionSeq = pAction->sequence;
                        actionState.actionId = pAction->actionId;
                        actionState.actionRemainingSec =
                            ResolveNetworkActionDurationSec(
                                champ.id,
                                pSkillDef,
                                pAction->actionId,
                                pAction->stage);
                        actionState.transitionDurationSec =
                            pSkillDef ? pSkillDef->endTransitionDuration : 0.f;
                        if (pSkillDef)
                        {
                            const ChampionDef* pChampionDef = FindClientChampionDef(champ.id);
                            if (pChampionDef)
                            {
                                actionState.transitionIdleAnim =
                                    ResolveNetworkAnimName(*pChampionDef, pSkillDef->endTransitionIdleAnim);
                                actionState.transitionRunAnim =
                                    ResolveNetworkAnimName(*pChampionDef, pSkillDef->endTransitionRunAnim);
                            }
                            else
                            {
                                actionState.transitionIdleAnim =
                                    pSkillDef->endTransitionIdleAnim
                                    ? pSkillDef->endTransitionIdleAnim
                                    : "";
                                actionState.transitionRunAnim =
                                    pSkillDef->endTransitionRunAnim
                                    ? pSkillDef->endTransitionRunAnim
                                    : "";
                            }
                        }
                        actionState.bLoopAction =
                            ShouldLoopNetworkAction(champ.id, pAction->actionId, pAction->stage);
                        actionState.bActionActive = true;
                        actionState.bBaseAnimationPending = !actionState.bLoopAction;
                        actionState.bPassiveDashTriggered = false;
                        PlayLoopNetworkActionIfNeeded(champ.id, pSkillDef, *pAction, rc);
                    }
                }
                else if (pPose && actionState.baseSeq != static_cast<u32_t>(pPose->startTick))
                {
                    actionState.baseSeq = static_cast<u32_t>(pPose->startTick);
                    actionState.bBaseAnimationPending = true;
                    if (actionState.bActionActive)
                    {
                        actionState.bActionActive = false;
                        actionState.bTransitionActive = false;
                        actionState.bLoopAction = false;
                        actionState.transitionRemainingSec = 0.f;
                    }
                }
            }

            if (actionState.passiveDashInputGraceSec > 0.f)
            {
                actionState.passiveDashInputGraceSec -= dt;
                if (actionState.passiveDashInputGraceSec < 0.f)
                    actionState.passiveDashInputGraceSec = 0.f;
            }

            const bool_t bLocalActionProtected =
                e == m_PlayerEntity &&
                IsLocalActionProtected();
            if (bLocalActionProtected)
            {
                m_NetworkChampionMoving[e] = bMoving;
                return;
            }

            if (actionState.bActionActive)
            {
                m_NetworkChampionMoving[e] = bMoving;
                actionState.bDesiredMoving = bMoving;
                if (actionState.bLoopAction && pAction)
                {
                    const SkillDef* pLoopSkillDef =
                        FindNetworkSkillDef(champ.id, pAction->actionId);
                    PlayLoopNetworkActionIfNeeded(champ.id, pLoopSkillDef, *pAction, rc);
                }

                actionState.actionRemainingSec -= dt;
                if (actionState.actionRemainingSec > 0.f)
                    return;

                actionState.bLoopAction = false;
                if (e == m_PlayerEntity && champ.id == eChampion::KALISTA)
                {
                    actionState.bActionActive = false;
                    if (!actionState.bPassiveDashTriggered)
                    {
                        const bool_t bDashStarted =
                            TriggerNetworkPassiveDashFromAction(
                            actionState.actionId,
                            actionState.actionSeq,
                            actionState.bDesiredMoving);
                        if (bDashStarted)
                        {
                            actionState.bPassiveDashTriggered = true;
                            actionState.passiveDashInputGraceSec = 0.f;
                        }
                        else
                        {
                            actionState.passiveDashInputGraceSec =
                                ChampionGameDataDB::ResolvePassiveDashInputGraceSec(eChampion::KALISTA);
                            actionState.bBaseAnimationPending = true;
                        }
                    }
                    if (IsLocalActionProtected())
                    {
                        m_NetworkChampionMoving[e] = bMoving;
                        return;
                    }
                }
                else
                {
                    actionState.bActionActive = false;
                }
                const std::string& transitionAnim =
                    bMoving ? actionState.transitionRunAnim : actionState.transitionIdleAnim;
                if (!transitionAnim.empty() &&
                    actionState.transitionDurationSec > 0.01f)
                {
                    ResetNetworkAnimatorSpeed(rc);
                    rc.pRenderer->PlayAnimationByName(transitionAnim, false);
                    actionState.bTransitionActive = true;
                    actionState.bTransitionMoving = bMoving;
                    actionState.transitionRemainingSec = actionState.transitionDurationSec;
                    LogNetworkEndTransition(
                        e,
                        transitionAnim.c_str(),
                        bMoving,
                        actionState.transitionDurationSec);
                    return;
                }

                actionState.bBaseAnimationPending = true;
            }

            if (e == m_PlayerEntity && m_bKalistaPassiveDashAnimActive)
            {
                m_NetworkChampionMoving[e] = bMoving;
                return;
            }

            if (actionState.bTransitionActive)
            {
                m_NetworkChampionMoving[e] = bMoving;
                actionState.bDesiredMoving = bMoving;

                // 전환 중 이동 상태가 바뀌면 전환 애니메이션을 끊고 즉시 기본 애니메이션으로 넘어간다.
                if (bMoving != actionState.bTransitionMoving)
                {
                    actionState.bTransitionActive = false;
                    actionState.transitionRemainingSec = 0.f;
                    actionState.bBaseAnimationPending = true;
                }
                else
                {
                    actionState.transitionRemainingSec -= dt;
                    if (actionState.transitionRemainingSec > 0.f)
                        return;

                    actionState.bTransitionActive = false;
                    actionState.bBaseAnimationPending = true;
                }
            }

            if (actionState.bBaseAnimationPending)
            {
                const ChampionDef* cd = FindClientChampionDef(champ.id);
                if (!cd)
                    return;

                const std::string baseAnim = ResolveNetworkBaseAnimName(*cd, bMoving);
                if (!baseAnim.empty())
                {
                    ResetNetworkAnimatorSpeed(rc);
                    rc.pRenderer->PlayAnimationByName(baseAnim, true);
                }
                actionState.bBaseAnimationPending = false;
                actionState.bBaseAnimationInitialized = true;
                m_NetworkChampionMoving[e] = bMoving;
                return;
            }

            bool_t& bWasMoving = m_NetworkChampionMoving[e];
            if (!actionState.bBaseAnimationInitialized)
            {
                const ChampionDef* cd = FindClientChampionDef(champ.id);
                if (!cd || !cd->animPrefix || !cd->idleAnimKey || !cd->runAnimKey)
                    return;

                const std::string animName =
                    std::string(cd->animPrefix) + (bMoving ? cd->runAnimKey : cd->idleAnimKey);
                ResetNetworkAnimatorSpeed(rc);
                rc.pRenderer->PlayAnimationByName(animName, true);
                actionState.bBaseAnimationInitialized = true;
                bWasMoving = bMoving;
                return;
            }

            if (bWasMoving == bMoving)
                return;

            const ChampionDef* cd = FindClientChampionDef(champ.id);
            if (!cd || !cd->animPrefix || !cd->idleAnimKey || !cd->runAnimKey)
                return;

            const std::string animName =
                std::string(cd->animPrefix) + (bMoving ? cd->runAnimKey : cd->idleAnimKey);
            ResetNetworkAnimatorSpeed(rc);
            rc.pRenderer->PlayAnimationByName(animName, true);
            bWasMoving = bMoving;
        });
}

Vec3 CScene_InGame::GetPlayerForward() const
{
    const f32_t yaw =
        GetPlayerYaw() -
        ChampionGameDataDB::ResolveVisualYawOffset(GetPlayerChampionId());
    return { sinf(yaw), 0.f, cosf(yaw) };
}

void CScene_InGame::OnUpdate(f32_t dt)
{
    WINTERS_PROFILE_SCOPE("Scene_InGame::OnUpdate");

    CGameInstance::Get()->UI_Set_StatusPanelOpen(CInput::Get().IsKeyDown(VK_TAB));

    if (m_bNetworkAuthoritativeGameplay && m_bNetworkActorInterpolationEnabled)
        CaptureNetworkActorInterpolationStarts();

    const bool_t bNetworkActive = m_bReplayPlaybackMode
        ? false
        : PumpNetwork();

    if (m_bReplayPlaybackMode)
        UpdateReplayPlayback(dt);

    const u64_t appliedSnapshotTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    if (m_bNetworkAuthoritativeGameplay &&
        bNetworkActive &&
        appliedSnapshotTick != 0 &&
        appliedSnapshotTick != m_uNetworkActorInterpSnapshotTick)
    {
        BeginNetworkActorInterpolationForSnapshot(appliedSnapshotTick);
        m_uNetworkActorInterpSnapshotTick = appliedSnapshotTick;
    }

    {
        WINTERS_PROFILE_SCOPE("SyncECS");
        SyncPlayerEntityTransformFromECS();
    }
    {
        WINTERS_PROFILE_SCOPE("Scheduler");

        if (m_pScheduler)
            m_pScheduler->Execute(m_World, dt);
    }

    if (m_pVisionSystem && m_pFogOfWarRenderer && m_pVisionSystem->IsFowTextureDirty())
    {
        m_pFogOfWarRenderer->UpdateTexture(
            m_pVisionSystem->GetFowTextureData(),
            m_pVisionSystem->GetFowTextureDim());
        m_pVisionSystem->ClearFowTextureDirty();
    }


    if (m_bNetworkAuthoritativeGameplay &&
        bNetworkActive &&
        m_bNetworkActorInterpolationEnabled)
    {
        ApplyNetworkActorInterpolation(dt);
    }

    SyncPlayerEntityTransformFromECS();

    if (bNetworkActive || m_bReplayPlaybackMode)
        UpdateNetworkChampionLocomotion(dt);

    ProjectGameplayActorsToMapSurface();

    m_MapTransform.SetRotation(m_vMapRotation);

    if (!m_bNetworkAuthoritativeGameplay
        && m_SylasEntity != NULL_ENTITY
        && m_World.HasComponent<TransformComponent>(m_SylasEntity))
    {
        m_World.GetComponent<TransformComponent>(m_SylasEntity).SetPosition(m_vSylasTestPos);
    }

    ProjectGameplayActorsToMapSurface();

    bool bSkipGroundMove = false;
    if (!m_bReplayPlaybackMode)
    {
        UpdateTargeting();
        UpdateCombatInput(bSkipGroundMove);
    }

    if (ShouldRunInGameSkillSmoke())
    {
        static bool_t s_bSmokeSkillAttempted = false;
        static bool_t s_bSmokeSkillCastObserved = false;
        static bool_t s_bSmokeSkillArmedLogged = false;
        static f32_t s_fSmokeSkillTimer = 0.f;
        static f32_t s_fSmokeSkillWaitLogTimer = 0.f;

        s_fSmokeSkillTimer += dt;
        s_fSmokeSkillWaitLogTimer += dt;

        if (!s_bSmokeSkillArmedLogged)
        {
            Winters::DevSmoke::Log(
                "[SmokeSkill] armed entity=%u renderer=%u\n",
                static_cast<u32_t>(m_PlayerEntity),
                m_pPlayerRenderer ? 1u : 0u);
            s_bSmokeSkillArmedLogged = true;
        }

        if (!s_bSmokeSkillAttempted
            && s_fSmokeSkillTimer >= 1.0f
            && m_PlayerEntity != NULL_ENTITY
            && m_pPlayerRenderer != nullptr)
        {
            const eChampion champ = GetPlayerChampionId();
            const bool_t bHasChampion = m_World.HasComponent<ChampionComponent>(m_PlayerEntity);
            const bool_t bHasSkillState = m_World.HasComponent<SkillStateComponent>(m_PlayerEntity);
            const bool_t bDispatched = DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::Q));
            Winters::DevSmoke::Log(
                "[SmokeSkill] Q dispatch champ=%u entity=%u hasChampion=%u hasSkillState=%u ok=%u\n",
                static_cast<u32_t>(champ),
                static_cast<u32_t>(m_PlayerEntity),
                bHasChampion ? 1u : 0u,
                bHasSkillState ? 1u : 0u,
                bDispatched ? 1u : 0u);
            s_bSmokeSkillAttempted = true;
        }
        else if (!s_bSmokeSkillAttempted
            && s_fSmokeSkillTimer >= 1.0f
            && s_fSmokeSkillWaitLogTimer >= 2.0f)
        {
            Winters::DevSmoke::Log(
                "[SmokeSkill] waiting entity=%u renderer=%u elapsed=%.2f\n",
                static_cast<u32_t>(m_PlayerEntity),
                m_pPlayerRenderer ? 1u : 0u,
                s_fSmokeSkillTimer);
            s_fSmokeSkillWaitLogTimer = 0.f;
        }

        if (s_bSmokeSkillAttempted
            && !s_bSmokeSkillCastObserved
            && m_ActiveSkill.bCastFrameFired
            && m_ActiveSkill.bActive)
        {
            Winters::DevSmoke::Log(
                "[SmokeSkill] castFrame observed champ=%u slot=%u hook=0x%08X\n",
                static_cast<u32_t>(GetPlayerChampionId()),
                static_cast<u32_t>(m_ActiveSkill.slot),
                m_ActiveSkill.legacyHookBridge.castFrameHookId);
            s_bSmokeSkillCastObserved = true;
        }
    }

    const bool bActionLockedBefore = (m_fLastActionTimer > 0.f);
    if (m_fLastActionTimer > 0.f) m_fLastActionTimer -= dt;

    if (m_fEndTransitionTimer > 0.f)
    {
        m_fEndTransitionTimer -= dt;
        if (m_fEndTransitionTimer <= 0.f)
        {
            if (m_pPlayerRenderer && !m_bNetworkAuthoritativeGameplay)
            {
                if (CanResumeBaseAnimation())
                {
                    m_pPlayerRenderer->PlayAnimationByName(m_bMoving ?
                        m_pPlayerRunAnim : m_pPlayerIdleAnim);
                }

            }
            m_pPendingEndAnim = nullptr;
            m_fEndTransitionTimer = 0.f;
        }
    }

    UpdateDash(dt);

    if (m_bNetworkAuthoritativeGameplay && m_ActiveSkill.bActive)
    {
        ClearActiveSkillRuntime();
    }
    else if (m_ActiveSkill.bActive && m_pPlayerRenderer)
    {
        const Engine::CAnimator* pAnim = m_pPlayerRenderer->GetAnimator();
        if (pAnim)
        {
            const f32_t curF = pAnim->GetCurrentFrame();
            const SkillDef& d = m_ActiveSkill.legacyHookBridge;
            const CastSkillCommand& activeCommand = m_ActiveSkill.command;

            const bool bCastHit =
                !m_ActiveSkill.bCastFrameFired
                && d.castFrame > 0.f
                && pAnim->HasFramePassed(d.castFrame, m_ActiveSkill.prevFrame);
            const bool bRecoveryHit =
                !m_ActiveSkill.bRecoveryFrameFired
                && d.recoveryFrame > 0.f
                && pAnim->HasFramePassed(d.recoveryFrame, m_ActiveSkill.prevFrame);

            if (m_bLogFrameEvents)
            {
                char buf[128];
                if (bCastHit)
                {
                    sprintf_s(buf, "[FrameEvent] CAST slot=%u anim=%s frame=%.1f\n",
                        d.slot, d.animKey ? d.animKey : "?", curF);
                    Winters::DevSmoke::Log("%s", buf);
                }
                if (bRecoveryHit)
                {
                    sprintf_s(buf, "[FrameEvent] RECOVERY slot=%u frame=%.1f\n", d.slot, curF);
                    Winters::DevSmoke::Log("%s", buf);
                }
            }

            if (bCastHit)
            {
                m_ActiveSkill.bCastFrameFired = true;

                // Local/offline path only. Network-authoritative gameplay is handled by server commands.
                const eChampion champ = GetPlayerChampionId();
                GameCommand gameCommand{};
                gameCommand.kind = (d.slot == static_cast<uint8_t>(eSkillSlot::BasicAttack))
                    ? eCommandKind::BasicAttack
                    : eCommandKind::CastSkill;
                gameCommand.issuerEntity = m_PlayerEntity;
                gameCommand.slot = d.slot;
                gameCommand.targetEntity = activeCommand.targetEntityId;
                gameCommand.groundPos = activeCommand.groundPos;
                gameCommand.direction = activeCommand.direction;
                TickContext tickCtx{};
                tickCtx.fDt = dt;
                tickCtx.localPlayer = m_PlayerEntity;

                GameplayHookContext gameCtx{};
                gameCtx.pWorld = &m_World;
                gameCtx.casterEntity = m_PlayerEntity;
                gameCtx.casterTeam = m_PlayerTeam;
                gameCtx.casterChampion = champ;
                gameCtx.skillRank = 1;
                if (m_World.HasComponent<SkillRankComponent>(m_PlayerEntity) &&
                    d.slot < SkillRankComponent::kSlotCount)
                {
                    const u8_t rank = m_World.GetComponent<SkillRankComponent>(m_PlayerEntity).ranks[d.slot];
                    gameCtx.skillRank = (rank == 0) ? 1 : rank;
                }
                gameCtx.pDef = &m_ActiveSkill.legacyHookBridge;
                gameCtx.pCommand = &gameCommand;
                gameCtx.pTickCtx = &tickCtx;
                bool gameplayHandled = false;
                if (d.castFrameHookId != 0)
                {
                    gameplayHandled = CGameplayHookRegistry::Instance().Dispatch(
                        d.castFrameHookId, gameCtx
                    );
                }
                //Client Visual FX/Sound
                VisualHookContext visualCtx{};
                visualCtx.pWorld = &m_World;
                visualCtx.casterEntity = m_PlayerEntity;
                visualCtx.pDef = &m_ActiveSkill.legacyHookBridge;
                visualCtx.pCommand = &activeCommand;
                visualCtx.skillStage = m_ActiveSkill.stage;
                visualCtx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                bool visualHandled = false;
                if (d.castFrameHookId != 0)
                    visualHandled = CVisualHookRegistry::Instance().Dispatch(
                        d.castFrameHookId, visualCtx);

                // Legacy local skill hook path for offline/practice visuals.
                bool castHandled = false;
                if (d.castFrameHookId != 0)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &m_World;
                    ctx.casterEntity = m_PlayerEntity;
                    ctx.casterTeam = m_PlayerTeam;
                    ctx.pDef = &m_ActiveSkill.legacyHookBridge;
                    ctx.pCommand = &activeCommand;
                    ctx.skillStage = m_ActiveSkill.stage;
                    ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                    ctx.applyTargetDamage = [this](EntityID target, f32_t damage)
                        {
                            ApplyLocalChampionDamage(
                                target,
                                damage,
                                "SkillHookDamage");
                        };
                    ctx.setLocalLoopAnimations = [this](const char* idle, const char* run, bool_t playNow)
                        {
                            m_pPlayerIdleAnim = idle;
                            m_pPlayerRunAnim = run;
                            if (playNow && m_pPlayerRenderer)
                                m_pPlayerRenderer->PlayAnimationByName(idle, true);
                        };
                    castHandled = CSkillHookRegistry::Instance().Dispatch(
                        d.castFrameHookId, ctx);
                }
                Winters::DevSmoke::Log(
                    "[SmokeSkill] castFrame champ=%u slot=%u hook=0x%08X gameplay=%u visual=%u legacy=%u\n",
                    static_cast<u32_t>(champ),
                    static_cast<u32_t>(d.slot),
                    d.castFrameHookId,
                    gameplayHandled ? 1u : 0u,
                    visualHandled ? 1u : 0u,
                    castHandled ? 1u : 0u);


            }
            if (bRecoveryHit)
            {
                m_ActiveSkill.bRecoveryFrameFired = true;

                //DispatchHook Recovery
                bool recoveryHandled = false;
                if (d.recoveryHookId != 0)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &m_World;
                    ctx.casterEntity = m_PlayerEntity;
                    ctx.casterTeam = m_PlayerTeam;
                    ctx.pDef = &m_ActiveSkill.legacyHookBridge;
                    ctx.pCommand = &activeCommand;
                    ctx.skillStage = m_ActiveSkill.stage;
                    ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                    ctx.pCasterRenderer = m_pPlayerRenderer;
                    ctx.fGlobalAnimSpeed = m_fGlobalAnimSpeed;
                    ctx.startLocalDash = [this](const Vec3& dir)
                        {
                            StartLocalPassiveDash(dir);
                        };
                    ctx.setLocalDashDuration = [this](f32_t duration)
                        {
                            SetLocalPassiveDashDuration(duration);
                        };
                    ctx.getLocalDashDuration = [this]() -> f32_t
                        {
                            return GetLocalPassiveDashDuration();
                        };
                    ctx.setLocalActionAnimActive = [this](bool_t active)
                        {
                            SetLocalActionAnimActive(active);
                        };
                    recoveryHandled = CSkillHookRegistry::Instance().Dispatch(
                        d.recoveryHookId, ctx
                    );
                }
                (void)recoveryHandled;


            }

            if (d.recoveryFrame > 0.f && curF >= d.recoveryFrame)
                ClearActiveSkillRuntime();
            else
                m_ActiveSkill.prevFrame = curF;
        }
    }

    ZedFx::TickShadowCloneModels(m_World, dt);
    UpdateLocalChampionRuntime(dt);
    UpdateFlashCooldown(dt);

    UpdateChampionStateTimers(dt);

    // [B-6.6] player cooldown / stage window
    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<SkillStateComponent>(m_PlayerEntity))
    {
        auto& ss = m_World.GetComponent<SkillStateComponent>(m_PlayerEntity);
        for (int i = 0; i < 5; ++i)
        {
            if (ss.slots[i].cooldownRemaining > 0.f)
            {
                ss.slots[i].cooldownRemaining -= dt;
                if (ss.slots[i].cooldownRemaining <= 0.f)
                {
                    ss.slots[i].cooldownRemaining = 0.f;
                    ss.slots[i].cooldownDuration = 0.f;
                }
            }
            else
            {
                ss.slots[i].cooldownDuration = 0.f;
            }

            if (ss.slots[i].currentStage == 1 && ss.slots[i].stageWindow > 0.f)
            {
                ss.slots[i].stageWindow -= dt;
                if (ss.slots[i].stageWindow <= 0.f)
                    ss.slots[i].currentStage = 0;
            }
        }
    }

    if (m_pCamera)
        m_pCamera->Update(dt, CInput::Get());

    if (!m_bReplayPlaybackMode)
    {
        UpdatePlayerControl(dt, bNetworkActive, bSkipGroundMove, bActionLockedBefore);
    }


    //ECS owned ModelRenderer
    {
        WINTERS_PROFILE_SCOPE("Champion::AnimUpdate");
        m_World.ForEach<ChampionComponent, RenderComponent>(
            [dt](EntityID, ChampionComponent&, RenderComponent& rc)
            {
                if (rc.bSceneManaged) return;
                if (!rc.pRenderer || !rc.bAnimated) return;
                if (!rc.pRenderer->HasSkeleton()) return;
                rc.pRenderer->Update(dt);
            }
        );
    }

    for (MapAmbientProp& prop : m_AmbientProps)
    {
        if (prop.pRenderer)
            prop.pRenderer->Update(dt);
    }

    CJungle_Manager::Get()->Update(dt);

    if (m_pFxSystem)          m_pFxSystem->Update(m_World, dt);
    if (m_pFxBeamSystem)      m_pFxBeamSystem->Update(m_World, dt);
    if (m_pFxMeshSystem)      m_pFxMeshSystem->Update(m_World, dt);

    UpdateLocalPostAnimation();

    {
        if (m_bNetworkAuthoritativeGameplay)
        {
            const Mat4 minionVisualVP = m_pCamera ? m_pCamera->GetViewProjection() : Mat4();
            CMinion_Manager::Get()->TickVisuals(dt, m_pCamera ? &minionVisualVP : nullptr);
        }
        else
        {
            CMinion_Manager::Get()->Tick(dt);
        }
        ProjectGameplayActorsToMapSurface();
    }

    const bool_t bAttackHold = CInput::Get().IsKeyDown('A');
    SetShowAttackRange(bAttackHold);

    const EntityID hoveredEntity = GetHoveredEntity();
    CGameInstance::Get()->UI_Set_AttackMode(bAttackHold);
    CGameInstance::Get()->UI_Set_EnemyHoverCursor(
        hoveredEntity != NULL_ENTITY &&
        IsEnemyOfPlayer(hoveredEntity));
}

void CScene_InGame::OnImGui()
{
    auto& input = CInput::Get();

    if (input.IsKeyPressed('M'))
    {
        using namespace Engine;
        auto pEditor = unique_ptr<IScene>(new CScene_Editor());
        CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::Editor, std::move(pEditor));
        return;
    }

    if (input.IsKeyPressed(VK_F9))
        m_bShowAIDebug = !m_bShowAIDebug;
    if (input.IsKeyPressed(VK_F8))
        m_bShowUITuner = !m_bShowUITuner;
    if (input.IsKeyPressed(VK_F7))
        m_bShowWfxEffectTool = !m_bShowWfxEffectTool;
    if (input.IsKeyPressed(VK_F6))
        m_bShowReplayControl = !m_bShowReplayControl;
    if (input.IsKeyPressed(VK_F10))
        m_bShowLegacyInGameDebug = !m_bShowLegacyInGameDebug;
    if (input.IsKeyPressed(VK_F1))
    {
        m_bShowRenderDebug = !m_bShowRenderDebug;
        if (m_bShowRenderDebug)
        {
            m_bDbgShowColliders = true;
            m_bDbgShowChampions = true;
        }
    }

    if (m_bShowAIDebug)
    {
        WINTERS_PROFILE_SCOPE("UI::AIDebug");
        UI::CAIDebugPanel::Render(m_World, this);
    }

    if (m_bShowRenderDebug)
    {
        WINTERS_PROFILE_SCOPE("UI::RenderDebug");
        UI::CRenderDebugPanel::Render(this);
    }

    if (m_bShowUITuner)
    {
        WINTERS_PROFILE_SCOPE("UI::Tuner");
        CGameInstance::Get()->UI_OnImGui_Tuner();
    }

    if (m_bShowWfxEffectTool)
    {
        WINTERS_PROFILE_SCOPE("UI::WfxEffectTool");
        UI::CWfxEffectToolPanel::Render(this);
    }

    if (m_bReplayPlaybackMode || m_bShowReplayControl || m_bReplayStopRequested)
    {
        WINTERS_PROFILE_SCOPE("UI::Replay");
        DrawReplayControlPanel();
    }

    if (!m_bShowLegacyInGameDebug)
        return;

    UI::CCombatDebugPanel::Render(m_World, this);
    UI::CMapTunerPanel::Render(this);
    UI::CChampionTuner::Render(this);
    UI::CEffectTuner::Render(this);
    UI::CRenderDebugPanel::Render(this);
    UI::CSkillTimingPanel::Render(this);
    CNetworkEventTrace::Instance().DrawImGui();

    ImGui::SetNextWindowSize(ImVec2(220.f, 120.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(180.f, 90.f), ImVec2(360.f, 220.f));
    if (ImGui::Begin("Camera"))
    {
        if (m_pCamera)
        {
            Vec3 eye = m_pCamera->GetEye();
            ImGui::Text("Eye: (%.1f, %.1f, %.1f)", eye.x, eye.y, eye.z);
        }
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        if (m_pCamera)
        {
            bool bFollow = m_pCamera->IsFollowMode();
            if (ImGui::Checkbox("Follow Mode (F2)", &bFollow))
                m_pCamera->SetFollowMode(bFollow);
        }
        ImGui::Checkbox("Log Frame Events", reinterpret_cast<bool*>(&m_bLogFrameEvents));
    }
    ImGui::End();

    if (m_pCamera)
        m_pCamera->OnImGui();
    CMinion_Manager::Get()->OnImGui_Tuner();
}

void CScene_InGame::OnLateUpdate(f32_t /*dt*/)
{
}

void CScene_InGame::OnRender()
{
    Mat4 vp = m_pCamera->GetViewProjection();
    const Vec3 cameraWorld = m_pCamera ? m_pCamera->GetEye() : Vec3{};
    const u64_t lastSnapshotTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    u32_t yawTraceNetId = 0;
    u32_t yawTraceCommandSeq = 0;
    f32_t yawTraceProtectedYaw = 0.f;
    const bool_t bHasYawTraceProtection = m_pSnapshotApplier &&
        m_pSnapshotApplier->GetLocalMoveYawProtectionDebug(
            yawTraceNetId,
            yawTraceCommandSeq,
            yawTraceProtectedYaw);
    (void)yawTraceNetId;
    (void)yawTraceProtectedYaw;
    CGameInstance* pGameInstance = CGameInstance::Get();
    IRHIDevice* pDevice = pGameInstance->Get_RHIDevice();
    void* pAmbientOcclusionSRV =
        m_pWhiteTexture ? m_pWhiteTexture->GetNativeSRV() : nullptr;
    const bool_t bUseDX12RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX12;
    const bool_t bUseDX11RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX11;
    const bool_t bSSAOEnabled = m_pSSAOPass && m_pSSAOPass->GetEnabled();

    const u8_t localTeam = UI::QueryLocalTeam(m_World);
    const bool_t bRevealAllForPlayback = ShouldRevealAllForPlayback();

    if (bUseDX11RHI && m_pNormalPass && bSSAOEnabled)
    {
        WINTERS_PROFILE_SCOPE("Render::NormalPass");
        m_pNormalPass->Begin(pDevice);

        m_Map.UpdateCamera(vp, cameraWorld);
        m_Map.UpdateTransform(m_MapTransform.GetWorldMatrix());
        m_Map.RenderNormalPassFrustumCulled(
            m_pNormalPass->GetStaticShader(),
            m_pNormalPass->GetStaticPipeline(),
            m_pNormalPass->GetSkinnedShader(),
            m_pNormalPass->GetSkinnedPipeline(),
            vp);

        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(m_World, e, localTeam, bRevealAllForPlayback))
                    return;

                FlushTransformForRender(tf);
                rc.pRenderer->UpdateCamera(vp, cameraWorld);
                rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());
                if (m_World.HasComponent<MeshGroupVisibilityComponent>(e)
                    && m_World.GetComponent<MeshGroupVisibilityComponent>(e).bEnabled)
                {
                    const auto& visibility = m_World.GetComponent<MeshGroupVisibilityComponent>(e);
                    rc.pRenderer->RenderNormalPassWithVisibility(
                        m_pNormalPass->GetStaticShader(),
                        m_pNormalPass->GetStaticPipeline(),
                        m_pNormalPass->GetSkinnedShader(),
                        m_pNormalPass->GetSkinnedPipeline(),
                        visibility.mask);
                }
                else
                {
                    rc.pRenderer->RenderNormalPassFrustumCulled(
                        m_pNormalPass->GetStaticShader(),
                        m_pNormalPass->GetStaticPipeline(),
                        m_pNormalPass->GetSkinnedShader(),
                        m_pNormalPass->GetSkinnedPipeline(),
                        vp);
                }
            });

        m_pNormalPass->End(pDevice);

        if (m_pSSAOPass && m_pSSAOPass->GetEnabled())
        {
            WINTERS_PROFILE_SCOPE("Render::SSAO");
            m_pSSAOPass->Execute(
                pDevice,
                m_pNormalPass->GetDepthSRVNative(),
                m_pNormalPass->GetNormalSRVNative(),
                vp);

            if (m_pSSAOPass->GetOutputSRVNative())
                pAmbientOcclusionSRV = m_pSSAOPass->GetOutputSRVNative();
        }
    }

    {
        WINTERS_PROFILE_SCOPE("Map::Render");
        WINTERS_PROFILE_COUNT("Map::MeshCount", m_Map.GetMeshCount());

        {
            WINTERS_PROFILE_SCOPE("Map::UpdateCamera");
            m_Map.UpdateCamera(vp, cameraWorld);
        }
        {
            WINTERS_PROFILE_SCOPE("Map::UpdateTransform");
            m_Map.UpdateTransform(m_MapTransform.GetWorldMatrix());
        }
        {
            WINTERS_PROFILE_SCOPE("Map::SetAmbientOcclusion");
            m_Map.SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
        }
        {
            WINTERS_PROFILE_SCOPE("Map::DrawFrustumCulled");
            m_Map.RenderFrustumCulled(vp);
        }
    }

    {
        WINTERS_PROFILE_SCOPE("Champion::Render");
        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, RenderComponent& rc,
                TransformComponent& tf)
            {
                if (rc.bSceneManaged) return;
                if (!rc.bVisible || !rc.pRenderer) return;
                if (!UI::IsRenderableForLocal(m_World, e, localTeam, bRevealAllForPlayback)) return;
                FlushTransformForRender(tf);
                rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
                rc.pRenderer->UpdateCamera(vp, cameraWorld);
                const bool_t bLocalYawTraceTarget =
                    IsNetworkAuthoritativeGameplay() &&
                    e == GetPlayerEntity();
                if (bLocalYawTraceTarget && bHasYawTraceProtection)
                {
                    rc.pRenderer->SetYawTraceContext(
                        lastSnapshotTick,
                        static_cast<u32_t>(e),
                        static_cast<u32_t>(champion.id),
                        yawTraceCommandSeq,
                        tf.GetRotation().y,
                        GameplayForwardFromVisualYaw(champion.id, tf.GetRotation().y));
                }
                else if (bLocalYawTraceTarget)
                {
                    rc.pRenderer->ClearYawTraceContext();
                }
                rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());
                if (m_World.HasComponent<MeshGroupVisibilityComponent>(e)
                    && m_World.GetComponent<MeshGroupVisibilityComponent>(e).bEnabled)
                {
                    const auto& visibility = m_World.GetComponent<MeshGroupVisibilityComponent>(e);
                    rc.pRenderer->RenderWithVisibility(visibility.mask);
                }
                else
                {
                    rc.pRenderer->RenderFrustumCulled(vp);
                }
            }
        );
    }

    {
        WINTERS_PROFILE_SCOPE("Structure::Render");
        CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);
    }
    {
        WINTERS_PROFILE_SCOPE("Jungle::Render");
        CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    }
    CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);

    for (MapAmbientProp& prop : m_AmbientProps)
    {
        if (!prop.pRenderer)
            continue;
        prop.pRenderer->UpdateCamera(vp, cameraWorld);
        prop.pRenderer->UpdateTransform(prop.transform.GetWorldMatrix());
        prop.pRenderer->RenderFrustumCulled(vp);
    }

    if (!bUseDX12RHI && m_pContactShadowPlane)
    {
        WINTERS_PROFILE_SCOPE("ContactShadow::Render");
        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(m_World, e, localTeam, bRevealAllForPlayback))
                    return;

                FlushTransformForRender(tf);
                m_pContactShadowPlane->SetWorld(
                    BuildContactShadowWorld(tf, 1.22f, 0.055f));
                m_pContactShadowPlane->Render(pDevice, vp);
            });
    }

    if (!bRevealAllForPlayback && m_pFogOfWarRenderer)
    {
        WINTERS_PROFILE_SCOPE("FogOfWar::Render");
        const Engine::CVisionSystem::FowProjection Projection =
            m_pVisionSystem
                ? m_pVisionSystem->GetFowProjection()
                : Engine::CVisionSystem::FowProjection{};
        m_pFogOfWarRenderer->RenderWorldOverlay(
            pDevice,
            vp,
            Projection.vWorldAtUv00,
            Projection.vWorldAtUv10,
            Projection.vWorldAtUv01,
            0.05f);
    }

    {
        WINTERS_PROFILE_SCOPE("DebugDraw::Render");
        UI::CDebugDrawSystem::Render(m_World, this, vp);
    }

    {
        WINTERS_PROFILE_SCOPE("AttackRange::Render");
        RenderAttackRangePreview(*this, vp, pDevice, bUseDX12RHI);
    }

    if (m_pFxMeshSystem && m_pCamera)
        m_pFxMeshSystem->Render(m_World, m_pCamera.get());
    if (m_pFxBeamSystem && m_pCamera)
        m_pFxBeamSystem->Render(m_World, m_pCamera.get());
    if (m_pFxSystem && m_pCamera)
        m_pFxSystem->Render(m_World, m_pCamera.get());

    {
        WINTERS_PROFILE_SCOPE("UIOverlay::Render");
        CGameInstance::Get()->UI_Render_Overlay(vp);
    }

    {
        WINTERS_PROFILE_SCOPE("Minimap::Render");
        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        const f32_t fScreenWidth =
            DisplaySize.x > 0.f ? DisplaySize.x : kFallbackScreenWidth;
        const f32_t fScreenHeight =
            DisplaySize.y > 0.f ? DisplaySize.y : kFallbackScreenHeight;

        UI::MinimapFrameState MinimapState{};
        UI::BuildMinimapFrameState(
            m_World,
            m_pFogOfWarRenderer.get(),
            GetPlayerTeam(),
            fScreenWidth,
            fScreenHeight,
            bRevealAllForPlayback,
            MinimapState);
        UI::CMinimapPanel::RenderRuntime(MinimapState);
    }
}

void CScene_InGame::UpdateReplayPlayback(f32_t dt)
{
    if (!m_pReplayPlayer || !m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
        return;

    if (m_pReplayPlayer->Update(
        dt,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier))
    {
        ProjectGameplayActorsToMapSurface();
    }
}

bool_t CScene_InGame::SendStopReplayRequest()
{
    if (m_bReplayPlaybackMode || m_bReplayStopRequested)
        return m_bReplayStopRequested;

    CGameSessionClient& session = CGameSessionClient::Instance();
    if (!session.IsConnected())
    {
        m_strReplayStatus = "Replay stop requires server session";
        return false;
    }

    m_bReplayStopRequested = session.SendLobbyCommand(
        Shared::Schema::LobbyCommandKind::StopReplay,
        0,
        eChampion::END,
        0,
        1u);

    m_strReplayStatus = m_bReplayStopRequested
        ? "Replay stop requested"
        : "Replay stop request failed";
    return m_bReplayStopRequested;
}

void CScene_InGame::DrawReplayControlPanel()
{
    ImGui::SetNextWindowSize(ImVec2(280.f, 116.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Replay"))
    {
        ImGui::End();
        return;
    }

    if (m_bReplayPlaybackMode)
    {
        if (m_pReplayPlayer)
        {
            bool_t bPaused = m_pReplayPlayer->IsPaused();
            if (ImGui::Checkbox("Pause", &bPaused))
                m_pReplayPlayer->SetPaused(bPaused);

            f32_t speed = m_pReplayPlayer->GetPlaybackRate();
            if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.f))
                m_pReplayPlayer->SetPlaybackRate(speed);

            ImGui::Text(
                "Tick: %llu / %llu",
                static_cast<unsigned long long>(m_pReplayPlayer->GetCurrentTick()),
                static_cast<unsigned long long>(m_pReplayPlayer->GetLastTick()));
        }

        ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Replay playback" : m_strReplayStatus.c_str());
        ImGui::End();
        return;
    }

    if (ImGui::Button("Stop"))
        SendStopReplayRequest();

    ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Recording on server" : m_strReplayStatus.c_str());
    ImGui::End();
}

void CScene_InGame::OnExit()
{
    CGameInstance::Get()->UI_Set_StatusPanelOpen(false);
    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);

    CGameInstance::Get()->UI_Set_InGameBuyItemCallback(nullptr, nullptr);
    CGameInstance::Get()->UI_Set_LevelSkillCallback(nullptr, nullptr);
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

    m_Map.Shutdown();

    if (m_pFxBeamSystem)   m_pFxBeamSystem.reset();
    if (m_pFxMeshSystem)   m_pFxMeshSystem.reset();
    if (m_pFxMeshRenderer) m_pFxMeshRenderer.reset();

    m_AmbientProps.clear();
    m_ChampionRenderers.clear();
    m_NetworkChampionPrevPos.clear();
    m_NetworkChampionMoveGraceSec.clear();
    m_NetworkChampionMoving.clear();
    m_NetworkActorInterpStates.clear();
    m_uNetworkActorInterpSnapshotTick = 0;
    m_NetworkActionAnimStates.clear();
    m_pSSAOPass.reset();
    m_pNormalPass.reset();
    m_pFogOfWarRenderer.reset();
    m_pVisionSystem = nullptr;
    m_BushIndex.Clear();
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
    CJungle_Manager::Get()->Shutdown();
    CStructure_Manager::Get()->Shutdown();
}

void CScene_InGame::SetEntityHoverOutline(EntityID entity, bool_t bEnabled)
{
    if (entity == NULL_ENTITY ||
        !m_World.IsAlive(entity) ||
        !m_World.HasComponent<RenderComponent>(entity))
    {
        return;
    }

    RenderComponent& render = m_World.GetComponent<RenderComponent>(entity);
    if (!render.pRenderer)
        return;

    if (bEnabled)
        render.pRenderer->SetHoverOutline(Vec4{ 1.f, 0.04f, 0.02f, 1.f }, 1.15f);
    else
        render.pRenderer->ClearHoverOutline();
}

void CScene_InGame::SetHoveredTarget(EntityID entity, eTeam team)
{
    const bool_t bShouldOutline =
        entity != NULL_ENTITY &&
        (team == eTeam::Neutral ||
            (team != eTeam::TEAM_END && team != m_PlayerTeam));

    if (m_OutlinedHoverEntity != NULL_ENTITY &&
        (m_OutlinedHoverEntity != entity || !bShouldOutline))
    {
        SetEntityHoverOutline(m_OutlinedHoverEntity, false);
        m_OutlinedHoverEntity = NULL_ENTITY;
    }

    m_HoveredEntity = entity;
    m_HoveredTeam = team;

    if (bShouldOutline)
    {
        SetEntityHoverOutline(entity, true);
        m_OutlinedHoverEntity = entity;
    }
}

void CScene_InGame::UpdateChampionStateTimers(f32_t dt)
{
    m_World.ForEach<YasuoStateComponent>(
        [dt](EntityID, YasuoStateComponent& ys)
        {
            if (ys.qStackTimer > 0.f)
            {
                ys.qStackTimer -= dt;
                if (ys.qStackTimer <= 0.f) ys.qStackCount = 0;
            }
            if (ys.eActiveTimer > 0.f)
            {
                ys.eActiveTimer -= dt;
                if (ys.eActiveTimer <= 0.f) ys.bEActive = false;
            }
        });

    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<RivenStateComponent>(m_PlayerEntity))
    {
        auto& rs = m_World.GetComponent<RivenStateComponent>(m_PlayerEntity);
        if (rs.qStackTimer > 0.f)
        {
            rs.qStackTimer -= dt;
            if (rs.qStackTimer <= 0.f)
            {
                rs.qStackTimer = 0.f;
                rs.qStackCount = 0;
            }
        }

        if (rs.bUlted)
        {
            rs.fUltTimer -= dt;
            if (rs.fUltTimer <= 0.f)
            {
                rs.bUlted = false;
                rs.fUltTimer = 0.f;
                m_pPlayerIdleAnim = "riven_idle1";
                m_pPlayerRunAnim = "riven_run";
                if (m_pPlayerRenderer)
                {
                    m_pPlayerRenderer->PlayAnimationByName(
                        m_bMoving ? m_pPlayerRunAnim : m_pPlayerIdleAnim,
                        true);
                }
            }
        }

        if (rs.fShieldTimer > 0.f)
        {
            rs.fShieldTimer -= dt;
            if (rs.fShieldTimer <= 0.f)
            {
                rs.fShieldTimer = 0.f;
                rs.fShieldRemaining = 0.f;
            }
        }
    }

    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<JaxStateComponent>(m_PlayerEntity))
    {
        auto& js = m_World.GetComponent<JaxStateComponent>(m_PlayerEntity);
        if (js.bEmpowerActive && js.fEmpowerTimer > 0.f)
        {
            js.fEmpowerTimer -= dt;
            if (js.fEmpowerTimer <= 0.f)
            {
                js.bEmpowerActive = false;
                js.fEmpowerTimer = 0.f;
            }
        }

        if (js.bCounterActive && js.fCounterTimer > 0.f)
        {
            js.fCounterTimer -= dt;
            if (js.fCounterTimer <= 0.f)
            {
                js.bCounterActive = false;
                js.fCounterTimer = 0.f;
            }
        }

        if (js.bUltActive && js.fUltTimer > 0.f)
        {
            js.fUltTimer -= dt;
            if (js.fUltTimer <= 0.f)
            {
                js.bUltActive = false;
                js.fUltTimer = 0.f;
                js.ultAttackCounter = 0;
            }
        }
    }

    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<AnnieStateComponent>(m_PlayerEntity))
    {
        auto& as = m_World.GetComponent<AnnieStateComponent>(m_PlayerEntity);
        if (as.bEShieldActive && as.fEShieldTimer > 0.f)
        {
            as.fEShieldTimer -= dt;
            if (as.fEShieldTimer <= 0.f)
            {
                as.bEShieldActive = false;
                as.fEShieldTimer = 0.f;
            }
        }

        if (as.bTibbersActive && as.fTibbersTimer > 0.f)
        {
            as.fTibbersTimer -= dt;
            if (as.fTibbersTimer <= 0.f)
            {
                as.bTibbersActive = false;
                as.fTibbersTimer = 0.f;
                as.vTibbersPos = {};
            }
        }
    }

    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<AsheStateComponent>(m_PlayerEntity))
    {
        auto& as = m_World.GetComponent<AsheStateComponent>(m_PlayerEntity);
        if (as.bQActive && as.fQTimer > 0.f)
        {
            as.fQTimer -= dt;
            if (as.fQTimer <= 0.f)
            {
                as.bQActive = false;
                as.fQTimer = 0.f;
            }
        }
    }
}

void CScene_InGame::UpdateLocalChampionRuntime(f32_t dt)
{
    if (m_pIreliaBladeSystem)
        m_pIreliaBladeSystem->Execute(m_World, dt);
    if (m_pWindWallSystem)
        m_pWindWallSystem->Execute(m_World, dt);

    if (m_bKalistaPassiveDashActive)
        UpdateLocalPassiveDash(dt);

    if (m_bYasuoDashActive)
        UpdateLocalTargetDash(dt);
    if (m_bYasuoRActive)
        UpdateLocalUltimateSequence(dt);

    if (m_pPendingHitSystem)
        m_pPendingHitSystem->Execute(m_World, dt);
    if (m_pYasuoProjectileSystem)
        m_pYasuoProjectileSystem->Execute(m_World, dt);
    if (m_pKalistaProjectileSystem)
        m_pKalistaProjectileSystem->Execute(m_World, dt);
    if (m_pKalistaRendSystem)
        m_pKalistaRendSystem->Execute(m_World, dt);

    Irelia::UpdateLocalBladeState(
        m_World,
        m_pFxMeshRenderer.get(),
        m_PlayerEntity,
        m_PlayerTeam,
        dt,
        ResolveMouseMapSurfacePos(),
        !m_bNetworkAuthoritativeGameplay);
}

bool_t CScene_InGame::CanResumeBaseAnimation() const
{
    return !m_bKalistaPassiveDashActive && !m_bKalistaPassiveDashAnimActive;
}

bool_t CScene_InGame::IsLocalActionProtected() const
{
    return m_fLastActionTimer > 0.f ||
        m_fEndTransitionTimer > 0.f ||
        m_bDashActive ||
        m_bKalistaPassiveDashActive;
}

void CScene_InGame::UpdateLocalPostAnimation()
{
    if (m_bKalistaPassiveDashAnimActive && !m_bKalistaPassiveDashActive)
    {
        const Engine::CAnimator* pAnim = m_pPlayerRenderer
            ? m_pPlayerRenderer->GetAnimator()
            : nullptr;
        if (!pAnim || !pAnim->IsPlaying())
        {
            if (m_pPlayerRenderer && !m_bNetworkAuthoritativeGameplay)
            {
                m_pPlayerRenderer->PlayAnimationByName(
                    m_bMoving ? m_pPlayerRunAnim : m_pPlayerIdleAnim);
            }
            m_bKalistaPassiveDashAnimActive = false;
        }
    }
}

void CScene_InGame::ResetLocalSkillRuntimeState()
{
    Kalista::ClearPassiveDashRequest();
    m_bKalistaPassiveDashAnimActive = false;
    m_bKalistaPassiveDashMoveCommandPending = false;
    m_bKalistaPassiveDashTriggerAfterMove = false;
    m_uKalistaPassiveDashTriggerAnimId = 0;
    m_uKalistaPassiveDashTriggerActionSeq = 0;
    m_vKalistaPassiveDashFaceDir = {};
    m_bKalistaPassiveDashHasFaceDir = false;
}

bool_t CScene_InGame::TryQueueLocalPassiveDashFromCursor()
{
    const eChampion champ = GetPlayerChampionId();
    if (champ != eChampion::KALISTA)
        return false;

    u8_t passiveSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    bool_t bNetworkGraceWindow = false;
    u16_t networkActionId = 0;
    u32_t networkActionSeq = 0;
    bool_t bPassiveDashWindow =
        m_ActiveSkill.bActive &&
        (m_ActiveSkill.slot == 0 || m_ActiveSkill.slot == 1) &&
        !m_ActiveSkill.bRecoveryFrameFired;

    if (bPassiveDashWindow)
    {
        passiveSlot = m_ActiveSkill.slot;
    }
    else if (m_bNetworkAuthoritativeGameplay && m_PlayerEntity != NULL_ENTITY)
    {
        const auto it = m_NetworkActionAnimStates.find(m_PlayerEntity);
        if (it != m_NetworkActionAnimStates.end())
        {
            const auto actionId = static_cast<eActionStateId>(it->second.actionId);
            const bool_t bPassiveAnim =
                actionId == eActionStateId::BasicAttack || actionId == eActionStateId::SkillQ;
            const bool_t bGraceOpen =
                !it->second.bActionActive && it->second.passiveDashInputGraceSec > 0.f;
            if (bPassiveAnim &&
                !it->second.bPassiveDashTriggered &&
                (it->second.bActionActive || bGraceOpen))
            {
                passiveSlot = (actionId == eActionStateId::SkillQ)
                    ? static_cast<u8_t>(eSkillSlot::Q)
                    : static_cast<u8_t>(eSkillSlot::BasicAttack);
                bPassiveDashWindow = true;
                bNetworkGraceWindow = bGraceOpen;
                networkActionId = it->second.actionId;
                networkActionSeq = it->second.actionSeq;
            }
        }
    }

    if (!bPassiveDashWindow || !m_pPlayerTransform || !m_pCamera)
        return false;

    const Vec3 origin = m_pPlayerTransform->GetPosition();
    Vec3 dashTarget = ResolveMouseMapSurfacePos();

    if (passiveSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
        std::fabsf(dashTarget.x) + std::fabsf(dashTarget.z) <= 0.001f)
    {
        const Vec3 forward = GetPlayerForward();
        dashTarget = {
            origin.x + forward.x,
            origin.y,
            origin.z + forward.z
        };
    }

    const f32_t dx = dashTarget.x - origin.x;
    const f32_t dz = dashTarget.z - origin.z;
    const f32_t len = std::sqrtf(dx * dx + dz * dz);
    if (len > 0.1f)
    {
        const Vec3 dashDir{ dx / len, 0.f, dz / len };
        Vec3 faceDir = m_bKalistaPassiveDashHasFaceDir
            ? m_vKalistaPassiveDashFaceDir
            : GetPlayerForward();

        if (!m_bKalistaPassiveDashHasFaceDir && m_ActiveSkill.bActive)
        {
            const auto& activeCmd = m_ActiveSkill.command;
            if (passiveSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
                activeCmd.targetEntityId != NULL_ENTITY &&
                m_World.HasComponent<TransformComponent>(activeCmd.targetEntityId))
            {
                const Vec3 targetPos =
                    m_World.GetComponent<TransformComponent>(activeCmd.targetEntityId).GetPosition();
                faceDir = WintersMath::DirectionXZ(origin, targetPos, faceDir);
            }
            else if (activeCmd.direction.x != 0.f || activeCmd.direction.z != 0.f)
            {
                faceDir = activeCmd.direction;
            }
        }

        SetKalistaPassiveDashFaceDir(
            WintersMath::NormalizeXZ(faceDir, GetPlayerForward(), 0.0001f));
        Kalista::QueuePassiveDash(dashDir);
        if (m_bNetworkAuthoritativeGameplay)
        {
            m_bKalistaPassiveDashMoveCommandPending = true;
            m_bKalistaPassiveDashTriggerAfterMove = bNetworkGraceWindow;
            m_uKalistaPassiveDashTriggerAnimId = networkActionId;
            m_uKalistaPassiveDashTriggerActionSeq = networkActionSeq;
        }
    }

    return true;
}

bool_t CScene_InGame::TriggerNetworkPassiveDashFromAction(u16_t actionId, u32_t actionSeq, bool_t bServerDashLikely)
{
    if (GetPlayerChampionId() != eChampion::KALISTA)
        return false;
    if (m_PlayerEntity == NULL_ENTITY)
        return false;

    const auto action = static_cast<eActionStateId>(actionId);
    const u8_t slot = (action == eActionStateId::SkillQ)
        ? static_cast<u8_t>(eSkillSlot::Q)
        : static_cast<u8_t>(eSkillSlot::BasicAttack);

    if (action != eActionStateId::BasicAttack && action != eActionStateId::SkillQ)
        return false;

    if (!Kalista::HasPassiveDashRequest())
        return false;

    if (actionSeq != 0u &&
        m_uKalistaLastPassiveDashActionSeq == actionSeq)
    {
        Kalista::ClearPassiveDashRequest();
        return false;
    }

    if (m_bKalistaPassiveDashActive ||
        m_bKalistaPassiveDashAnimActive)
    {
        Kalista::ClearPassiveDashRequest();
        return false;
    }

    const SkillDef* pDef = CSkillRegistry::Instance().Find(eChampion::KALISTA, slot);
    if (!pDef)
        pDef = FindSkillDef(eChampion::KALISTA, slot);
    if (!pDef)
        return false;

    SkillHookContext ctx{};
    ctx.pWorld = &m_World;
    ctx.casterEntity = m_PlayerEntity;
    ctx.casterTeam = m_PlayerTeam;
    ctx.pDef = pDef;
    ctx.pCasterRenderer = m_pPlayerRenderer;
    ctx.fGlobalAnimSpeed = m_fGlobalAnimSpeed;
    ctx.actionSeq = actionSeq;
    ctx.startLocalDash = [this](const Vec3& dir)
        {
            StartLocalPassiveDash(dir);
        };
    ctx.setLocalDashDuration = [](f32_t duration)
        {
            SetLocalPassiveDashDuration(duration);
        };
    ctx.getLocalDashDuration = []() -> f32_t
        {
            return GetLocalPassiveDashDuration();
        };
    ctx.setLocalActionAnimActive = [this](bool_t active)
        {
            SetLocalActionAnimActive(active);
        };
    ctx.bPlayPassiveDashAnimation = true;

    const bool_t bWasDashActive =
        m_bKalistaPassiveDashActive ||
        m_bKalistaPassiveDashAnimActive;
    Kalista::OnRecoveryFrame_PassiveDash(ctx);
    const bool_t bDashStarted =
        !bWasDashActive &&
        (m_bKalistaPassiveDashActive ||
            m_bKalistaPassiveDashAnimActive);
    if (bDashStarted)
        m_uKalistaLastPassiveDashActionSeq = actionSeq;
    return bDashStarted;
}

bool_t CScene_InGame::ValidateLocalSkillStart(const SkillDef& def)
{
    if (def.champ == eChampion::YASUO
        && def.slot == static_cast<uint8_t>(eSkillSlot::R))
    {
        if (m_bNetworkAuthoritativeGameplay)
            return true;

        if (!m_pPlayerTransform)
            return false;

        const EntityID airborne = FindAirborneEnemyNear(
            m_pPlayerTransform->GetPosition(),
            Yasuo::GetTuning().rSearchRadius);
        if (airborne == NULL_ENTITY)
        {
            return false;
        }
    }

    return true;
}

void CScene_InGame::StartLocalTargetDash(EntityID target)
{
    if (!m_pPlayerTransform) return;
    if (!m_World.HasComponent<TransformComponent>(target)) return;

    const Vec3 targetPos = m_World.GetComponent<TransformComponent>(target).m_LocalPosition;
    m_vYasuoDashStart = m_pPlayerTransform->GetPosition();
    m_vYasuoDashEnd = { targetPos.x, m_vYasuoDashStart.y, targetPos.z };
    m_fYasuoDashElapsed = 0.f;
    m_bYasuoDashActive = true;
    m_YasuoDashTargetEntity = target;
}

void CScene_InGame::StartLocalUltimateDash(EntityID airborne)
{
    if (!m_pPlayerTransform) return;
    if (!m_World.HasComponent<TransformComponent>(airborne)) return;

    const Vec3 targetPos = m_World.GetComponent<TransformComponent>(airborne).m_LocalPosition;
    m_vYasuoDashStart = m_pPlayerTransform->GetPosition();
    m_vYasuoDashEnd = { targetPos.x, targetPos.y + 0.5f, targetPos.z };
    m_fYasuoDashElapsed = 0.f;
    m_bYasuoDashActive = true;
    m_YasuoDashTargetEntity = airborne;
    m_bYasuoRActive = true;
    m_fYasuoRElapsed = 0.f;
    m_fYasuoRPrevHitTime = 0.f;
    m_iYasuoRHitsFired = 0;
    m_YasuoRTarget = airborne;
}

void CScene_InGame::StartLocalPassiveDash(const Vec3& vForward)
{
    if (!m_pPlayerTransform)
        return;

    const Vec3 vOrigin = m_pPlayerTransform->GetPosition();

    m_vKalistaPassiveDashStart = vOrigin;
    const f32_t dashDist =
        ChampionGameDataDB::ResolvePassiveDashDistance(eChampion::KALISTA);
    m_vKalistaPassiveDashEnd = {
        vOrigin.x + vForward.x * dashDist,
        vOrigin.y,
        vOrigin.z + vForward.z * dashDist
    };
    m_fKalistaPassiveDashElapsed = 0.f;
    m_bKalistaPassiveDashActive = true;
    if (m_bKalistaPassiveDashHasFaceDir)
    {
        const f32_t yaw = ResolveChampionVisualYawNear(
            GetPlayerChampionId(),
            m_vKalistaPassiveDashFaceDir,
            GetPlayerYaw());
        SetPlayerYaw(yaw);
    }

    m_vPlayerDest = m_vKalistaPassiveDashEnd;

    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
    {
        auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
        agent.vTarget = m_vKalistaPassiveDashEnd;
        agent.bHasGoal = false;
        agent.bPathDirty = false;
    }
}



void CScene_InGame::SetLocalActionAnimActive(bool_t active)
{
    m_bKalistaPassiveDashAnimActive = active;
}

EntityID CScene_InGame::FindAirborneEnemyNear(const Vec3& origin, f32_t radius)
{
    EntityID closest = NULL_ENTITY;
    f32_t bestDist2 = radius * radius;

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
        {
            if (champion.team == m_PlayerTeam) return;
            if (!m_World.HasComponent<StunComponent>(entity)) return;

            const f32_t dx = transform.m_LocalPosition.x - origin.x;
            const f32_t dz = transform.m_LocalPosition.z - origin.z;
            const f32_t dist2 = dx * dx + dz * dz;
            if (dist2 < bestDist2)
            {
                bestDist2 = dist2;
                closest = entity;
            }
        });

    return closest;
}

void CScene_InGame::ApplyLocalChampionDamage(EntityID target, f32_t fDamage, const char* pDebugLabel)
{
    if (target == NULL_ENTITY || target == m_PlayerEntity) return;
    if (!m_World.HasComponent<ChampionComponent>(target)) return;

    auto& champion = m_World.GetComponent<ChampionComponent>(target);
    if (champion.team == m_PlayerTeam) return;

    champion.hp = (champion.hp > fDamage) ? (champion.hp - fDamage) : 0.f;

    if (m_World.HasComponent<HealthComponent>(target))
    {
        auto& hp = m_World.GetComponent<HealthComponent>(target);
        hp.fCurrent = champion.hp;
        hp.fMaximum = champion.maxHp;
        hp.bIsDead = (hp.fCurrent <= 0.f);
    }

    NotifyTowerAggroOnChampionHit(m_World, m_PlayerEntity, target);

}

void CScene_InGame::UpdateLocalTargetDash(f32_t dt)
{
    if (!m_pPlayerTransform)
    {
        m_bYasuoDashActive = false;
        m_YasuoDashTargetEntity = NULL_ENTITY;
        return;
    }

    m_fYasuoDashElapsed += dt;
    const f32_t dashDuration = Yasuo::GetTuning().eDashDuration;
    f32_t t = (dashDuration > 0.01f)
        ? (m_fYasuoDashElapsed / dashDuration) : 1.f;
    if (t >= 1.f)
    {
        t = 1.f;
        if (!m_bYasuoRActive
            && m_YasuoDashTargetEntity != NULL_ENTITY
            && m_World.HasComponent<ChampionComponent>(m_YasuoDashTargetEntity))
        {
            ApplyLocalChampionDamage(
                m_YasuoDashTargetEntity,
                Yasuo::GetTuning().eDamage,
                "Yasuo Hit");
        }
        m_bYasuoDashActive = false;
        m_YasuoDashTargetEntity = NULL_ENTITY;
    }

    const Vec3 pos{
        m_vYasuoDashStart.x + (m_vYasuoDashEnd.x - m_vYasuoDashStart.x) * t,
        m_vYasuoDashStart.y + (m_vYasuoDashEnd.y - m_vYasuoDashStart.y) * t,
        m_vYasuoDashStart.z + (m_vYasuoDashEnd.z - m_vYasuoDashStart.z) * t
    };
    m_pPlayerTransform->SetPosition(pos);

    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<TransformComponent>(m_PlayerEntity))
    {
        m_World.GetComponent<TransformComponent>(m_PlayerEntity).m_LocalPosition = pos;
    }
}

void CScene_InGame::UpdateLocalUltimateSequence(f32_t dt)
{
    if (m_bYasuoDashActive)
        return;

    m_fYasuoRElapsed += dt;
    constexpr i32_t kMaxHits = 5;
    while (m_iYasuoRHitsFired < kMaxHits
        && m_YasuoRTarget != NULL_ENTITY
        && m_World.HasComponent<ChampionComponent>(m_YasuoRTarget)
        && m_fYasuoRElapsed >= (static_cast<f32_t>(m_iYasuoRHitsFired + 1) * Yasuo::GetTuning().rHitInterval))
    {
        ApplyLocalChampionDamage(
            m_YasuoRTarget,
            Yasuo::GetTuning().rPerHitDamage,
            "Yasuo Hit");
        m_iYasuoRHitsFired += 1;
        m_fYasuoRPrevHitTime = m_fYasuoRElapsed;
    }

    if (m_fYasuoRElapsed >= Yasuo::GetTuning().rSequenceDuration)
    {
        m_bYasuoRActive = false;
        m_YasuoRTarget = NULL_ENTITY;
        m_iYasuoRHitsFired = 0;
        m_fYasuoRPrevHitTime = 0.f;
    }
}

void CScene_InGame::UpdateLocalPassiveDash(f32_t dt)
{
    if (!m_pPlayerTransform)
    {
        m_bKalistaPassiveDashActive = false;
        m_vKalistaPassiveDashFaceDir = {};
        m_bKalistaPassiveDashHasFaceDir = false;
        return;
    }

    m_fKalistaPassiveDashElapsed += dt;
    const f32_t dashDuration = GetLocalPassiveDashDuration();
    f32_t t = (dashDuration > 0.01f)
        ? (m_fKalistaPassiveDashElapsed / dashDuration) : 1.f;
    if (t >= 1.f)
    {
        t = 1.f;
        m_bKalistaPassiveDashActive = false;
    }

    const Vec3 pos{
        m_vKalistaPassiveDashStart.x + (m_vKalistaPassiveDashEnd.x - m_vKalistaPassiveDashStart.x) * t,
        m_vKalistaPassiveDashStart.y,
        m_vKalistaPassiveDashStart.z + (m_vKalistaPassiveDashEnd.z - m_vKalistaPassiveDashStart.z) * t
    };

    m_pPlayerTransform->SetPosition(pos);

    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<TransformComponent>(m_PlayerEntity))
    {
        m_World.GetComponent<TransformComponent>(m_PlayerEntity).m_LocalPosition = pos;
    }

    if (m_bKalistaPassiveDashHasFaceDir)
    {
        const f32_t yaw = ResolveChampionVisualYawNear(
            GetPlayerChampionId(),
            m_vKalistaPassiveDashFaceDir,
            GetPlayerYaw());
        SetPlayerYaw(yaw);
    }

    if (!m_bKalistaPassiveDashActive)
    {
        m_vKalistaPassiveDashFaceDir = {};
        m_bKalistaPassiveDashHasFaceDir = false;
    }
}

bool_t CScene_InGame::PredictLocalMoveYaw(const Vec3& facingTarget, f32_t& outYaw)
{
    CTransform* playerTransform = GetPlayerTransformPtr();
    if (!playerTransform)
        return false;

    const Vec3 origin = playerTransform->GetPosition();
    const Vec3 direction{
        facingTarget.x - origin.x,
        0.f,
        facingTarget.z - origin.z
    };
    if ((direction.x * direction.x + direction.z * direction.z) <= 0.0001f)
        return false;

    const f32_t yaw =
        ResolveChampionVisualYawNear(
            GetPlayerChampionId(),
            direction,
            playerTransform->GetRotation().y);
    outYaw = yaw;

    SetPlayerYaw(yaw);
    return true;
}

void CScene_InGame::UpdatePlayerControl(f32_t dt, bool_t bNetworkActive, bool_t bSkipGroundMove, bool_t bActionLockedBefore)
{
    const bool_t bActionLocked = (m_fLastActionTimer > 0.f);

    if (m_pPlayerRenderer &&
        (!bNetworkActive || m_bKalistaPassiveDashAnimActive))
    {
        auto* pAnim = m_pPlayerRenderer->GetAnimator();
        if (pAnim)
        {
            f32_t s;
            if (m_bKalistaPassiveDashAnimActive)
            {
                s = m_fGlobalAnimSpeed * Kalista::GetTuning().passiveDashAnimSpeed;
            }
            else if (bActionLocked)
            {
                const SkillVisualStageData visualStage =
                    m_ActiveSkill.bActive
                    ? GetVisualStage(m_ActiveSkill.visual, m_ActiveSkill.stage)
                    : SkillVisualStageData{};
                const f32_t skillSpeed =
                    m_ActiveSkill.bActive ? visualStage.playbackSpeed : 1.f;
                s = m_fAttackSpeedMul * m_fGlobalAnimSpeed * skillSpeed;
            }
            else
            {
                s = m_fGlobalAnimSpeed;
            }
            pAnim->SetPlaySpeed(s);
        }
    }

    if (m_pPlayerTransform && m_pPlayerRenderer)
    {
        auto& input = CInput::Get();
        const bool bImGuiMouse = ImGui::GetIO().WantCaptureMouse;
        const bool_t bMoveIntent = bNetworkActive
            ? input.IsRButtonPressed()
            : input.IsRButtonDown();
        const bool_t bPassiveDashAnimBlocksMove =
            !bNetworkActive && m_bKalistaPassiveDashAnimActive;

        if (!bImGuiMouse &&
            !bSkipGroundMove &&
            (bNetworkActive || !bActionLocked) &&
            !m_bKalistaPassiveDashActive &&
            !bPassiveDashAnimBlocksMove &&
            bMoveIntent)
        {
            Vec3 ground = ResolveMouseMapSurfacePos();
            Vec3 resolvedGround = ground;
            Vec3 predictedFacingTarget = ground;

            const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
            bool_t bAcceptedMoveTarget = false;
            if (bValidGround)
            {
                bAcceptedMoveTarget = TryResolveWalkableMoveTarget(
                    ground,
                    resolvedGround,
                    &predictedFacingTarget);
            }



            if (bAcceptedMoveTarget)
            {
                Vec3 moveIntent = ground;
                predictedFacingTarget = moveIntent;

                if (input.IsRButtonPressed())
                    SpawnMovementIndicator(*this, resolvedGround);

                if (bNetworkActive && m_pCommandSerializer && m_pNetworkView)
                {
                    const bool_t bKalistaPassiveDashMove =
                        m_bKalistaPassiveDashMoveCommandPending;
                    const Vec3 moveFacingDirection = m_pPlayerTransform
                        ? WintersMath::DirectionXZ(
                            m_pPlayerTransform->GetPosition(),
                            ground,
                            Vec3{})
                        : Vec3{};
                    static u32_t s_moveSendPrepTraceCount = 0;
                    if (s_moveSendPrepTraceCount < 512u)
                    {
                        const Vec3 playerPos = m_pPlayerTransform
                            ? m_pPlayerTransform->GetPosition()
                            : Vec3{};
                        const Vec3 rawDir =
                            WintersMath::DirectionXZ(playerPos, ground, Vec3{});
                        const f32_t rawYaw = std::atan2f(rawDir.x, rawDir.z);
                        const f32_t sendYaw = std::atan2f(
                            moveFacingDirection.x,
                            moveFacingDirection.z);
                        char msg[1024]{};
                        sprintf_s(
                            msg,
                            "[YawTrace][ClientMoveSendPrep] mouse=(%d,%d) valid=%u accepted=%u player=(%.3f,%.3f,%.3f) ground=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) moveIntent=(%.3f,%.3f,%.3f) predictedFacing=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) sendDir=(%.3f,%.3f) rawYaw=%.4f sendYaw=%.4f rawVsSendDot=%.4f\n",
                            static_cast<int>(input.GetMouseX()),
                            static_cast<int>(input.GetMouseY()),
                            bValidGround ? 1u : 0u,
                            bAcceptedMoveTarget ? 1u : 0u,
                            playerPos.x,
                            playerPos.y,
                            playerPos.z,
                            ground.x,
                            ground.y,
                            ground.z,
                            resolvedGround.x,
                            resolvedGround.y,
                            resolvedGround.z,
                            moveIntent.x,
                            moveIntent.y,
                            moveIntent.z,
                            predictedFacingTarget.x,
                            predictedFacingTarget.y,
                            predictedFacingTarget.z,
                            rawDir.x,
                            rawDir.z,
                            moveFacingDirection.x,
                            moveFacingDirection.z,
                            rawYaw,
                            sendYaw,
                            rawDir.x * moveFacingDirection.x + rawDir.z * moveFacingDirection.z);
                        Winters::DevSmoke::Log("%s", msg);
                        ++s_moveSendPrepTraceCount;
                    }
                    const u32_t moveSeq =
                        m_pCommandSerializer->SendMove(
                            *m_pNetworkView,
                            moveIntent,
                            moveFacingDirection);
                    if (moveSeq != 0u)
                    {
                        RecordNetworkMovePrediction(
                            moveSeq,
                            resolvedGround,
                            moveFacingDirection);
                    }
                    if (bKalistaPassiveDashMove)
                    {
                        const bool_t bTriggerAfterMove =
                            m_bKalistaPassiveDashTriggerAfterMove;
                        const u16_t triggerAnimId =
                            m_uKalistaPassiveDashTriggerAnimId;
                        const u32_t triggerActionSeq =
                            m_uKalistaPassiveDashTriggerActionSeq;

                        m_bKalistaPassiveDashMoveCommandPending = false;
                        m_bKalistaPassiveDashTriggerAfterMove = false;
                        m_uKalistaPassiveDashTriggerAnimId = 0;
                        m_uKalistaPassiveDashTriggerActionSeq = 0;

                        if (moveSeq != 0u && bTriggerAfterMove)
                        {
                            const bool_t bDashStarted =
                                TriggerNetworkPassiveDashFromAction(
                                    triggerAnimId,
                                    triggerActionSeq,
                                    true);
                            if (bDashStarted &&
                                m_PlayerEntity != NULL_ENTITY)
                            {
                                auto it = m_NetworkActionAnimStates.find(m_PlayerEntity);
                                if (it != m_NetworkActionAnimStates.end())
                                {
                                    it->second.bPassiveDashTriggered = true;
                                    it->second.passiveDashInputGraceSec = 0.f;
                                }
                            }
                        }
                    }
                    else
                    {
                        f32_t predictedYaw = 0.f;
                        if (PredictLocalMoveYaw(predictedFacingTarget, predictedYaw) &&
                            m_pSnapshotApplier)
                        {
                            m_pSnapshotApplier->ProtectLocalMoveYaw(
                                m_pNetworkView->GetMyNetEntityId(),
                                moveSeq,
                                predictedYaw);
                        }
                    }
                    if (m_PlayerEntity != NULL_ENTITY)
                    {
                        f32_t& moveGrace =
                            m_NetworkChampionMoveGraceSec[m_PlayerEntity];
                        moveGrace = (std::max)(moveGrace, 0.16f);
                        m_NetworkChampionMoving[m_PlayerEntity] = true;

                        if (!bKalistaPassiveDashMove &&
                            !m_bMoving &&
                            m_pPlayerRenderer &&
                            m_pPlayerRunAnim)
                        {
                            m_pPlayerRenderer->PlayAnimationByName(
                                m_pPlayerRunAnim,
                                true);
                        }
                        m_bMoving = true;
                    }

                    m_vPlayerDest = resolvedGround;
                    if (m_PlayerEntity != NULL_ENTITY &&
                        m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
                    {
                        auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
                        agent.vTarget = resolvedGround;
                        agent.bHasGoal = true;
                        agent.bPathDirty = true;
                        agent.pathCellsX.clear();
                        agent.pathCellsY.clear();
                        agent.iPathIndex = 0;
                    }
                }
                else
                {
                    m_vPlayerDest = resolvedGround;

                    if (m_PlayerEntity != NULL_ENTITY &&
                        m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
                    {
                        auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
                        agent.vTarget = resolvedGround;
                        agent.bHasGoal = true;
                        agent.bPathDirty = true;
                        agent.pathCellsX.clear();
                        agent.pathCellsY.clear();
                        agent.iPathIndex = 0;
                    }
                }
            }
            else if (input.IsRButtonPressed())
            {
                m_bKalistaPassiveDashMoveCommandPending = false;
                m_bKalistaPassiveDashTriggerAfterMove = false;
                m_uKalistaPassiveDashTriggerAnimId = 0;
                m_uKalistaPassiveDashTriggerActionSeq = 0;
            }
        }

        Vec3 cur = m_pPlayerTransform->GetPosition();
        if (!bNetworkActive)
        {
            Vec3 resolvedCur{};
            if (TryResolveNearestWalkablePosition(cur, resolvedCur, 8))
            {
                if (WintersMath::DistanceSqXZ(resolvedCur, cur) > 0.0001f)
                {
                    cur = resolvedCur;
                    m_pPlayerTransform->SetPosition(cur);
                    if (m_PlayerEntity != NULL_ENTITY &&
                        m_World.HasComponent<TransformComponent>(m_PlayerEntity))
                    {
                        m_World.GetComponent<TransformComponent>(m_PlayerEntity).SetPosition(cur);
                    }
                    m_vPlayerDest.y = cur.y;
                }
            }
        }
        Vec3 localMoveTarget = m_vPlayerDest;
        const f32_t playerArriveRadius =
            ResolvePlayerArriveRadius(m_World, m_PlayerEntity);
        const f32_t playerMoveSpeed =
            ResolvePlayerMoveSpeed(m_World, m_PlayerEntity);
        bool_t bLocalPathControlled = false;
        bool_t bLocalPathReady = false;
        if (!bNetworkActive &&
            m_pNavGrid &&
            m_PlayerEntity != NULL_ENTITY &&
            m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
        {
            bLocalPathControlled = true;
            auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
            if (agent.bHasGoal && !agent.bPathDirty)
            {
                const size_t pathCount =
                    (std::min)(agent.pathCellsX.size(), agent.pathCellsY.size());
                while (agent.iPathIndex < pathCount)
                {
                    Vec3 waypoint = m_pNavGrid->CellToWorld(
                        agent.pathCellsX[agent.iPathIndex],
                        agent.pathCellsY[agent.iPathIndex]);
                    if (!TryProjectToMapSurface(waypoint, 0.05f))
                        waypoint.y = cur.y;

                    if (WintersMath::DistanceSqXZ(cur, waypoint) >
                        playerArriveRadius * playerArriveRadius)
                    {
                        localMoveTarget = waypoint;
                        bLocalPathReady = true;
                        break;
                    }

                    ++agent.iPathIndex;
                }

                if (agent.iPathIndex >= pathCount)
                {
                    agent.bHasGoal = false;
                    agent.pathCellsX.clear();
                    agent.pathCellsY.clear();
                    agent.iPathIndex = 0;
                    localMoveTarget = cur;
                }
            }
            else
            {
                localMoveTarget = cur;
            }
        }

        Vec3 delta = { localMoveTarget.x - cur.x, 0.f, localMoveTarget.z - cur.z };
        f32_t dist = sqrtf(delta.x * delta.x + delta.z * delta.z);

        bool wasMoving = m_bMoving;

        if (bNetworkActive)
        {
            const bool_t bNetworkMoving = IsNetworkChampionMoving(m_PlayerEntity);
            SyncPlayerEntityTransformFromECS();
            cur = m_pPlayerTransform->GetPosition();
            m_bMoving = bNetworkMoving;
        }
        else if (!bActionLocked &&
            !m_bKalistaPassiveDashActive &&
            dist > playerArriveRadius)
        {
            if (bLocalPathControlled && !bLocalPathReady)
            {
                m_bMoving = false;
                if (m_PlayerEntity != NULL_ENTITY &&
                    m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
                {
                    auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
                    if (!agent.bHasGoal)
                        m_vPlayerDest = cur;
                }
            }
            else
            {
                f32_t inv = 1.f / dist;
                Vec3 dir = { delta.x * inv, 0.f, delta.z * inv };
                f32_t step = playerMoveSpeed * dt;
                if (step > dist) step = dist;

                const f32_t navRadius = ResolveAgentRadius(m_World, m_PlayerEntity);
                const Vec3 moveDir = ResolveAvoidedMoveDirection(
                    m_World,
                    m_PlayerEntity,
                    cur,
                    dir,
                    step,
                    [&](const Vec3& candidate)
                    {
                        return IsWalkableMoveSegment(cur, candidate, navRadius);
                    });

                if (fabsf(moveDir.x) + fabsf(moveDir.z) > 0.0001f)
                {
                    Vec3 next = cur;
                    next.x += moveDir.x * step;
                    next.z += moveDir.z * step;
                    if (!IsWalkableMoveSegment(cur, next, navRadius))
                    {
                        m_bMoving = false;
                        m_vPlayerDest = cur;
                        if (m_PlayerEntity != NULL_ENTITY &&
                            m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
                        {
                            auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
                            agent.bHasGoal = false;
                            agent.bPathDirty = false;
                        }
                    }
                    else
                    {
                        cur = next;
                        (void)TryProjectToMapSurface(cur, 0.05f);

                        SetPlayerPosition(cur);

                        f32_t yaw =
                            ResolveChampionVisualYawNear(
                                GetPlayerChampionId(),
                                moveDir,
                                GetPlayerYaw());
                        SetPlayerYaw(yaw);

                        m_bMoving = true;
                    }
                }
                else
                {
                    m_bMoving = false;
                }
            }
        }
        else
        {
            m_bMoving = false;
        }

        const bool bInTransition = (m_fEndTransitionTimer > 0.f);
        if (!m_bKalistaPassiveDashActive
            && !m_bKalistaPassiveDashAnimActive
            && !bNetworkActive
            && !bActionLocked
            && bInTransition
            && m_bMoving != m_bEndTransitionMoving)
        {
            // 전환 중 이동 상태가 바뀌면 end-transition을 끊고 기본 애니메이션으로 즉시 전환.
            m_fEndTransitionTimer = 0.f;
            m_pPendingEndAnim = nullptr;
            m_pPlayerRenderer->PlayAnimationByName(
                m_bMoving ? m_pPlayerRunAnim : m_pPlayerIdleAnim
            );
        }
        else if (!m_bKalistaPassiveDashActive
            && !m_bKalistaPassiveDashAnimActive
            && !bNetworkActive
            && !bActionLocked
            && !bInTransition
            && m_bMoving != wasMoving)
        {
            m_pPlayerRenderer->PlayAnimationByName(
                m_bMoving ? m_pPlayerRunAnim : m_pPlayerIdleAnim
            );
        }
        else if (!m_bKalistaPassiveDashActive
            && !m_bKalistaPassiveDashAnimActive
            && !bNetworkActive
            && bActionLockedBefore
            && !bActionLocked
            && m_pPlayerRenderer)
        {
            const char* pTransition = nullptr;
            f32_t fDur = 0.f;
            if (m_bHasLastSkillVisual)
            {
                pTransition = m_bMoving
                    ? m_LastSkillVisual.endTransitionRunAnim
                    : m_LastSkillVisual.endTransitionIdleAnim;
                fDur = m_LastSkillVisual.endTransitionDuration;
            }

            if (pTransition && fDur > 0.01f)
            {
                m_pPlayerRenderer->PlayAnimationByName(pTransition, false);
                m_pPendingEndAnim = pTransition;
                m_fEndTransitionTimer = fDur;
                m_bEndTransitionMoving = m_bMoving;
            }
            else
            {
                m_pPlayerRenderer->PlayAnimationByName(
                    m_bMoving ? m_pPlayerRunAnim : m_pPlayerIdleAnim
                );
            }
        }
    }

    if (!bNetworkActive)
        SyncPlayerEntityTransformToECS();
}

void CScene_InGame::ProtectNetworkAttackYaw(
    CClientNetwork* pNetworkView,
    u32_t commandSeq,
    const Vec3& facingTarget)
{
    if (commandSeq == 0 ||
        !pNetworkView ||
        !m_pSnapshotApplier ||
        !m_pPlayerTransform)
    {
        return;
    }

    const Vec3 origin = m_pPlayerTransform->GetPosition();
    const Vec3 facingDirection =
        WintersMath::DirectionXZ(origin, facingTarget, Vec3{});
    if (facingDirection.x == 0.f && facingDirection.z == 0.f)
        return;

    const f32_t predictedYaw = ResolveChampionVisualYawNear(
        GetPlayerChampionId(),
        facingDirection,
        m_pPlayerTransform->GetRotation().y);

    SetPlayerYaw(predictedYaw);
    m_pSnapshotApplier->ProtectLocalMoveYaw(
        pNetworkView->GetMyNetEntityId(),
        commandSeq,
        predictedYaw);
    if (GetPlayerChampionId() == eChampion::KALISTA)
    {
        SetKalistaPassiveDashFaceDir(facingDirection);
    }
}

void CScene_InGame::DriveNetworkAttackIntent(bool& outSkipGroundMove)
{
    if (s_NetworkAttackTarget == NULL_ENTITY)
        return;

    outSkipGroundMove = true;

    if (!m_bNetworkAuthoritativeGameplay ||
        !GameplayQuery::IsValidAttackTarget(
            m_World,
            m_PlayerEntity,
            s_NetworkAttackTarget,
            m_PlayerTeam))
    {
        ClearNetworkAttackIntent();
        outSkipGroundMove = false;
        return;
    }

    if (!m_pCommandSerializer || !m_pNetworkView || !m_pEntityIdMap)
    {
        Winters::DevSmoke::Log("[BA] network basic-attack intent skipped: network objects missing\n");
        return;
    }

    if (s_uNetworkAttackCommandFrame < kNetworkAttackCommandIntervalFrames)
    {
        ++s_uNetworkAttackCommandFrame;
        return;
    }
    s_uNetworkAttackCommandFrame = 0u;

    const NetEntityId targetNet = m_pEntityIdMap->ToNet(s_NetworkAttackTarget);
    if (targetNet == NULL_NET_ENTITY)
    {
        char dbg[192]{};
        sprintf_s(dbg,
            "[BA] network basic-attack intent cleared: target has no netId entity=%u\n",
            static_cast<u32_t>(s_NetworkAttackTarget));
        Winters::DevSmoke::Log("%s", dbg);
        ClearNetworkAttackIntent();
        outSkipGroundMove = false;
        return;
    }

    Vec3 cursorGround{};
    Vec3 direction{};
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<TransformComponent>(m_PlayerEntity) &&
        m_World.HasComponent<TransformComponent>(s_NetworkAttackTarget))
    {
        const Vec3 origin = m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition();
        cursorGround = m_World.GetComponent<TransformComponent>(s_NetworkAttackTarget).GetPosition();
        const f32_t dx = cursorGround.x - origin.x;
        const f32_t dz = cursorGround.z - origin.z;
        const f32_t lenSq = dx * dx + dz * dz;
        if (lenSq > 0.0001f)
        {
            const f32_t invLen = 1.f / std::sqrtf(lenSq);
            direction = Vec3{ dx * invLen, 0.f, dz * invLen };
        }
    }
    else if (m_pCamera)
    {
        cursorGround = ResolveMouseMapSurfacePos();

        if (m_PlayerEntity != NULL_ENTITY &&
            m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        {
            const Vec3 origin = m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition();
            const f32_t dx = cursorGround.x - origin.x;
            const f32_t dz = cursorGround.z - origin.z;
            const f32_t lenSq = dx * dx + dz * dz;
            if (lenSq > 0.0001f)
            {
                const f32_t invLen = 1.f / std::sqrtf(lenSq);
                direction = Vec3{ dx * invLen, 0.f, dz * invLen };
            }
        }
    }

    const u32_t attackSeq =
        m_pCommandSerializer->SendBasicAttack(
            *m_pNetworkView,
            targetNet,
            cursorGround,
            direction);
    ProtectNetworkAttackYaw(m_pNetworkView, attackSeq, cursorGround);
    ClearNetworkAttackIntent();
    outSkipGroundMove = true;
}

void CScene_InGame::UpdateTargeting()
{
    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);

    const CDynamicCamera* pCamera = GetCameraPtr();
    if (!pCamera)
        return;
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    const auto ray = CInput::Get().GetMouseWorldRay(
        *pCamera, static_cast<i32_t>(g_iWinSizeX),
        static_cast<i32_t>(g_iWinSizeY));

    EntityID hoveredEntity = NULL_ENTITY;
    eTeam hoveredTeam = eTeam::TEAM_END;

    GameplayQuery::TryFindHoverTarget(
        GetWorld(),
        GetPlayerEntity(),
        GetPlayerTeam(),
        ray.Origin,
        ray.Dir,
        GetChampionHitRadius(),
        GetChampionHitHeight(),
        hoveredEntity,
        hoveredTeam);
    SetHoveredTarget(hoveredEntity, hoveredTeam);
}

void CScene_InGame::UpdateCombatInput(bool& outSkipGroundMove)
{
    outSkipGroundMove = false;

    if (!HasPlayerRenderer())
        return;

    if (IsPlayerStunned())
        return;

    auto& in = CInput::Get();
    const bool bImGuiMouse = ImGui::GetIO().WantCaptureMouse;
    const bool bImGuiKbd = ImGui::GetIO().WantCaptureKeyboard;
    const bool_t bAttackMoveClick =
        !bImGuiMouse && in.IsKeyDown('A') && in.IsLButtonPressed();
    const bool_t bBasicAttackClick =
        !bImGuiMouse && (in.IsRButtonPressed() || bAttackMoveClick);

    if (!bImGuiKbd)
    {
        if (in.IsKeyPressed('P'))
            CGameInstance::Get()->UI_Toggle_InGameShop();

        if (in.IsKeyPressed('B') && IsNetworkAuthoritativeGameplay())
        {
            CCommandSerializer* pCommandSerializer = GetCommandSerializer();
            CClientNetwork* pNetworkView = GetNetworkView();
            if (pCommandSerializer && pNetworkView && pNetworkView->IsConnected())
            {
                ClearNetworkAttackIntent();
                pCommandSerializer->SendRecall(*pNetworkView);
            }
        }

        if (in.IsKeyPressed('Q'))
        {
            ClearNetworkAttackIntent();
            DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::Q));
        }
        if (in.IsKeyPressed('F'))
        {
            ClearNetworkAttackIntent();
            TriggerFlash();
        }

        const u8_t wSlot = static_cast<u8_t>(eSkillSlot::W);
        if (in.IsKeyPressed('W'))
        {
            if (!HasPendingSkillStage(*this, wSlot))
            {
                ClearNetworkAttackIntent();
                const bool_t bDispatched = DispatchSkillInput(wSlot);
                s_bWReleasePending = bDispatched && HasPendingSkillStage(*this, wSlot);
            }
        }
        else if (in.IsKeyReleased('W') &&
            (s_bWReleasePending || HasPendingSkillStage(*this, wSlot)))
        {
            ClearNetworkAttackIntent();
            DispatchSkillInput(wSlot, 2u);
            s_bWReleasePending = false;
        }

        if (in.IsKeyPressed('E'))
        {
            ClearNetworkAttackIntent();
            DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::E));
        }

        if (in.IsKeyPressed('R'))
        {
            ClearNetworkAttackIntent();
            DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::R));
        }
    }

    if (IsNetworkAuthoritativeGameplay())
    {
        if (bBasicAttackClick)
        {
            TryQueueLocalPassiveDashFromCursor();

            const Vec3 vCursorGround = ResolveMouseMapSurfacePos();
            const EntityID target = GameplayQuery::FindAttackTargetNearCursor(
                GetWorld(),
                GetPlayerEntity(),
                GetHoveredEntity(),
                GetPlayerTeam(),
                vCursorGround,
                bAttackMoveClick,
                GetPlayerChampionId(),
                GetBasicAttackRange());

            if (target == NULL_ENTITY)
            {
                if (s_uNetworkAttackMissLogCount < 32u)
                {
                    Winters::DevSmoke::Log(
                        "[BA] network attack intent miss hover=%u hoverTeam=%d playerTeam=%d\n",
                        static_cast<u32_t>(GetHoveredEntity()),
                        static_cast<i32_t>(GetHoveredTeam()),
                        static_cast<i32_t>(GetPlayerTeam()));
                    ++s_uNetworkAttackMissLogCount;
                }
                ClearNetworkAttackIntent();
            }
            else
            {
                s_NetworkAttackTarget = target;
                s_uNetworkAttackCommandFrame = kNetworkAttackCommandIntervalFrames;
                char dbg[128]{};
                sprintf_s(dbg,
                    "[BA] network attack intent target=%u hover=%u\n",
                    static_cast<u32_t>(s_NetworkAttackTarget),
                    static_cast<u32_t>(GetHoveredEntity()));
                Winters::DevSmoke::Log("%s", dbg);
            }
        }

        DriveNetworkAttackIntent(outSkipGroundMove);
        if (outSkipGroundMove)
            return;
    }

    if (!IsNetworkAuthoritativeGameplay() && bBasicAttackClick)
    {
        if (TryQueueLocalPassiveDashFromCursor())
        {
            outSkipGroundMove = true;
        }
        else
        {
            if (GetLastActionTimer() > 0.f
                && GetLastActionLabel()
                && std::strncmp(GetLastActionLabel(), "attack", 6) == 0)
            {
                PreemptAction("Move");
            }

            const bool fired = DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::BasicAttack));
            if (fired)
                outSkipGroundMove = true;
            else
            {
                PreemptAction("Move");
            }
        }
    }
}

void CScene_InGame::FirePlayerAction(const char* actionKey)
{
    using namespace Engine;
    eChampion champ = GetPlayerChampionId();

    const ChampionDef* cd = FindClientChampionDef(champ);
    if (!cd) return;

    string key = actionKey;
    if (strcmp(actionKey, "attack") == 0)
        key = cd->basicAttackKey;

    string animKey = string(cd->animPrefix) + key;
    m_pPlayerRenderer->PlayAnimationByName(animKey);

    m_pLastActionLabel = actionKey;
    m_fLastActionTimer = 1.2f;
}

bool CScene_InGame::IsEnemyOfPlayer(EntityID entity)
{
    if (entity == NULL_ENTITY)
        return false;

    const eTeam team = GameplayStateQuery::ResolveEntityTeam(m_World, entity);

    return team != eTeam::TEAM_END && team != m_PlayerTeam;
}

void CScene_InGame::PreemptAction(const char* reasonLabel)
{
    m_fLastActionTimer = 0.f;
    ClearActiveSkillRuntime();
    ResetLocalSkillRuntimeState();
    m_pLastActionLabel = reasonLabel ? reasonLabel : "(preempt)";

    m_fEndTransitionTimer = 0.f;
    m_pPendingEndAnim = nullptr;

    m_bDashActive = false;
    m_fDashElapsed = 0.f;
    m_DashTargetEntity = NULL_ENTITY;

}

void CScene_InGame::ClearActiveSkillRuntime()
{
    m_ActiveSkill = ActiveSkillRuntime{};
}

void CScene_InGame::BeginActiveSkillRuntime(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    const SkillDef& legacyDef,
    u8_t skillStage)
{
    const u8_t stage = skillStage == 0u ? 1u : skillStage;

    m_ActiveSkill = ActiveSkillRuntime{};
    m_ActiveSkill.bActive = true;
    m_ActiveSkill.champion = gameData.slot.champion;
    m_ActiveSkill.slot = gameData.slot.slot;
    m_ActiveSkill.stage = stage;
    m_ActiveSkill.command = cmd;
    m_ActiveSkill.game = gameData;
    m_ActiveSkill.visual = visualData;
    m_ActiveSkill.legacyHookBridge =
        BuildLegacyHookBridge(gameData, visualData, legacyDef, stage);
    m_ActiveSkill.prevFrame = 0.f;
    m_ActiveSkill.bCastFrameFired = false;
    m_ActiveSkill.bRecoveryFrameFired = false;

    m_LastSkillVisual = visualData;
    m_bHasLastSkillVisual = true;
}

void CScene_InGame::UpdateDash(f32_t dt)
{
    if (!m_bDashActive || !m_pPlayerTransform)
        return;

    m_fDashElapsed += dt;
    const f32_t t = (m_fDashDuration > 0.01f)
        ? (m_fDashElapsed / m_fDashDuration) : 1.f;

    if (t >= 1.f)
    {
        SetPlayerPosition(m_vDashEnd);

        m_bDashActive = false;
        m_fDashElapsed = 0.f;
        m_DashTargetEntity = NULL_ENTITY;

        using namespace Engine;
        if (m_PlayerEntity != NULL_ENTITY
            && m_World.HasComponent<SkillStateComponent>(m_PlayerEntity))
        {
            auto& ss = m_World.GetComponent<SkillStateComponent>(m_PlayerEntity);
            auto& basicAttackSlot = ss.slots[static_cast<uint8_t>(eSkillSlot::BasicAttack)];
            basicAttackSlot.cooldownRemaining = 0.f;
            basicAttackSlot.cooldownDuration = 0.f;
        }
        return;
    }
    const Vec3 p
    {
        m_vDashStart.x + (m_vDashEnd.x - m_vDashStart.x) * t,
        m_vDashStart.y,
        m_vDashStart.z + (m_vDashEnd.z - m_vDashStart.z) * t
    };
    SetPlayerPosition(p);

    if (m_DashTargetEntity != NULL_ENTITY &&
        m_World.HasComponent<TransformComponent>(m_DashTargetEntity))
    {
        const Vec3 tp = m_World.GetComponent<TransformComponent>
            (m_DashTargetEntity).m_LocalPosition;
        const f32_t dx = tp.x - p.x;
        const f32_t dz = tp.z - p.z;
        if (dx * dx + dz * dz > 1e-4f)
        {
            const f32_t yaw = ResolveChampionVisualYawNear(
                GetPlayerChampionId(),
                Vec3{ dx, 0.f, dz },
                GetPlayerYaw());
            SetPlayerYaw(yaw);
        }
    }
}

void CScene_InGame::SendNetworkSkillCommand(u8_t slot, const CastSkillCommand& cmd, u8_t skillStage)
{
    if (!m_pNetworkView || !m_pCommandSerializer)
        return;
    if (!m_pNetworkView->IsConnected())
        return;

    NetEntityId targetNet = NULL_NET_ENTITY;
    if (cmd.targetEntityId != NULL_ENTITY && m_pEntityIdMap)
        targetNet = m_pEntityIdMap->ToNet(cmd.targetEntityId);

    if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
    {
        const u32_t attackSeq = m_pCommandSerializer->SendBasicAttack(
            *m_pNetworkView,
            targetNet,
            cmd.groundPos,
            cmd.direction);
        ProtectNetworkBasicAttackYaw(*this, attackSeq);
    }
    else
    {
        m_pCommandSerializer->SendCastSkill(
            *m_pNetworkView,
            slot,
            targetNet,
            cmd.groundPos,
            cmd.direction,
            skillStage);
    }
}

bool CScene_InGame::DispatchSkillInput(uint8_t slot, u8_t requestedStage)
{
    if (!m_pPlayerRenderer || m_PlayerEntity == NULL_ENTITY)
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u reason=no-player renderer=%u entity=%u\n",
            static_cast<u32_t>(slot),
            m_pPlayerRenderer ? 1u : 0u,
            static_cast<u32_t>(m_PlayerEntity));
        return false;
    }

    if (slot == static_cast<uint8_t>(eSkillSlot::BasicAttack)
        && m_World.HasComponent<DisarmComponent>(m_PlayerEntity))
        return false;

    using namespace Engine;
    eChampion champ = GetPlayerChampionId();
    u8_t lookupSlot = slot;
    if (m_World.HasComponent<SpellbookOverrideComponent>(m_PlayerEntity))
    {
        const auto& spellbook =
            m_World.GetComponent<SpellbookOverrideComponent>(m_PlayerEntity);
        if (spellbook.bActive && spellbook.localSlot == slot)
        {
            champ = spellbook.sourceChampion;
            lookupSlot = spellbook.sourceSlot;
        }
    }
    else if (m_World.HasComponent<FormOverrideComponent>(m_PlayerEntity))
    {
        const auto& form = m_World.GetComponent<FormOverrideComponent>(m_PlayerEntity);
        if (form.bActive &&
            form.skillChampion != eChampion::END &&
            form.skillChampion != eChampion::NONE &&
            slot < 8u &&
            (form.skillSlotMask & static_cast<u8_t>(1u << slot)) != 0u)
        {
            champ = form.skillChampion;
        }
    }
    const SkillDef* def = CSkillRegistry::Instance().Find(champ, lookupSlot);
    if (!def)
        def = FindSkillDef(champ, lookupSlot);
    if (!def)
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u reason=no-def\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ));
        return false;
    }

    SkillGameAtomBundle gameData{};
    SkillVisualData visualData{};
    if (!CSkillRegistry::Instance().ResolveGameAtoms(champ, lookupSlot, gameData))
    {
        gameData = SkillDefAdapters::BuildSkillGameAtomBundle(*def);
    }
    if (!CSkillRegistry::Instance().ResolveSkillVisualData(champ, lookupSlot, visualData))
    {
        visualData = SkillDefAdapters::BuildSkillVisualData(*def);
    }

    if (!m_World.HasComponent<SkillStateComponent>(m_PlayerEntity))
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u reason=no-skill-state entity=%u\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ),
            static_cast<u32_t>(m_PlayerEntity));
        return false;
    }

    if (!IsLocalSkillLearned(*this, slot))
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u reason=unlearned\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ));
        return false;
    }

    if (!ValidateLocalSkillStart(*def))
        return false;

    auto& slotState = m_World.GetComponent<SkillStateComponent>(m_PlayerEntity).slots[slot];

    const bool_t bRequestedStage2 = requestedStage >= 2u;
    const bool_t bLocalStage2Ready =
        slotState.currentStage == 1 && slotState.stageWindow > 0.f;

    if (gameData.stage.stageCount == 2 && (bLocalStage2Ready || bRequestedStage2))
    {
        CastSkillCommand cmd{};
        cmd.slot = slot;
        if (!BuildCastCommand(gameData.target, 2, cmd))
            return false;

        if (m_bNetworkAuthoritativeGameplay)
            RotatePlayerToward(gameData.facing, 2, cmd);

        SendNetworkSkillCommand(slot, cmd, 2);
        if (bRequestedStage2 && !bLocalStage2Ready)
        {
            Winters::DevSmoke::Log(
                "[SkillDispatch] forced stage2 slot=%u champ=%u localWindow=%.2f\n",
                static_cast<u32_t>(slot),
                static_cast<u32_t>(champ),
                slotState.stageWindow);
        }

        if (m_bNetworkAuthoritativeGameplay)
        {
            slotState.currentStage = 0;
            slotState.stageWindow = 0.f;
            return true;
        }

        ApplyLocalPrediction(cmd, gameData, visualData, *def, 2);

        slotState.currentStage = 0;
        slotState.stageWindow = 0.f;
        slotState.cooldownRemaining = gameData.cooldown.cooldownSec;
        slotState.cooldownDuration = gameData.cooldown.cooldownSec;
        return true;
    }

    if (slot != static_cast<uint8_t>(eSkillSlot::BasicAttack)
        && slotState.cooldownRemaining > 0.f)
    {
        return false;
    }

    CastSkillCommand cmd{};
    cmd.slot = slot;
    if (!BuildCastCommand(gameData.target, 1, cmd))
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u mode=%u reason=build-command\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ),
            static_cast<u32_t>(GetTargetShape(gameData.target, 1)));
        return false;
    }

    if (m_bNetworkAuthoritativeGameplay)
    {
        RotatePlayerToward(gameData.facing, 1, cmd);
        SendNetworkSkillCommand(slot, cmd, 1);
        if (gameData.stage.stageCount == 2)
        {
            slotState.currentStage = 1;
            slotState.stageWindow = gameData.stage.stageWindowSec;
        }
        return true;
    }

    if (gameData.stage.stageCount == 2)
    {
        SendNetworkSkillCommand(slot, cmd, 1);
        ApplyLocalPrediction(cmd, gameData, visualData, *def, 1);

        slotState.currentStage = 1;
        slotState.stageWindow = gameData.stage.stageWindowSec;
        return true;
    }

    SendNetworkSkillCommand(slot, cmd, 1);
    ApplyLocalPrediction(cmd, gameData, visualData, *def);
    Winters::DevSmoke::Log(
        "[SkillDispatch] accepted slot=%u champ=%u hook=0x%08X anim=%s\n",
        static_cast<u32_t>(slot),
        static_cast<u32_t>(champ),
        m_ActiveSkill.legacyHookBridge.castFrameHookId,
        m_ActiveSkill.legacyHookBridge.animKey
        ? m_ActiveSkill.legacyHookBridge.animKey
        : "(null)");
    if (slot != static_cast<uint8_t>(eSkillSlot::BasicAttack))
    {
        slotState.cooldownRemaining = gameData.cooldown.cooldownSec;
        slotState.cooldownDuration = gameData.cooldown.cooldownSec;
    }

    return true;
}

bool CScene_InGame::BuildCastCommand(
    const SkillTargetSpec& targetSpec,
    u8_t skillStage,
    CastSkillCommand& outCmd)
{
    const eTargetMode mode = ToLegacyTargetMode(GetTargetShape(targetSpec, skillStage));

    outCmd.resolvedTargetMode = static_cast<uint8_t>(mode);

    switch (mode)
    {
    case eTargetMode::Self:
    {
        outCmd.targetEntityId = m_PlayerEntity;
        return true;
    }
    case eTargetMode::UnitTarget:
    {
        if (!IsEnemyOfPlayer(m_HoveredEntity))
            return false;
        outCmd.targetEntityId = m_HoveredEntity;
        return true;
    }
    case eTargetMode::GroundTarget:
    {
        if (!m_pCamera) return false;
        Vec3 ground = ResolveMouseMapSurfacePos();
        outCmd.groundPos = ground;
        return true;
    }
    case eTargetMode::Direction:
    {
        if (!m_pCamera) return false;
        Vec3 cursor = ResolveMouseMapSurfacePos();
        const Vec3 origin = m_pPlayerTransform ? m_pPlayerTransform->GetPosition() : Vec3{};
        f32_t dx = cursor.x - origin.x;
        f32_t dz = cursor.z - origin.z;
        f32_t len2 = dx * dx + dz * dz;

        if (len2 < 1e-3f)
        {
            Vec3 fwd = m_pCamera->GetForward();
            dx = fwd.x;
            dz = fwd.z;
            len2 = dx * dx + dz * dz;
            if (len2 < 1e-4f) return false;
        }

        const f32_t len = sqrtf(len2);
        outCmd.direction = { dx / len, 0.f, dz / len };
        return true;
    }
    default:
        return false;
    }
}

void CScene_InGame::ApplyLocalPrediction(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    const SkillDef& legacyDef,
    u8_t skillStage)
{
    const SkillDef bridge = BuildLegacyHookBridge(gameData, visualData, legacyDef, skillStage);

    if (m_bNetworkAuthoritativeGameplay)
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] local prediction blocked in network authority slot=%u\n",
            static_cast<u32_t>(bridge.slot));
        return;
    }

    RotatePlayerToward(gameData.facing, skillStage, cmd);

    if (bridge.animKey && m_pPlayerRenderer)
    {
        const eChampion champ = GetPlayerChampionId();
        const ChampionDef* cd = FindClientChampionDef(champ);
        if (cd)
        {
            std::string key = bridge.animKey;
            if (key == "attack") key = cd->basicAttackKey;

            if (bridge.keySwapHookId != 0)
            {
                VisualHookContext visualCtx{};
                visualCtx.pWorld = &m_World;
                visualCtx.casterEntity = m_PlayerEntity;
                visualCtx.pDef = &bridge;
                visualCtx.pCommand = &cmd;
                visualCtx.skillStage = skillStage;
                visualCtx.pKeyOut = &key;
                visualCtx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                const bool visualKeyHandled =
                    CVisualHookRegistry::Instance().Dispatch(bridge.keySwapHookId, visualCtx);

                if (!visualKeyHandled)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &m_World;
                    ctx.casterEntity = m_PlayerEntity;
                    ctx.casterTeam = m_PlayerTeam;
                    ctx.pDef = &bridge;
                    ctx.pCommand = &cmd;
                    ctx.skillStage = skillStage;
                    ctx.pKeyOut = &key;
                    ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                    CSkillHookRegistry::Instance().Dispatch(bridge.keySwapHookId, ctx);
                }
            }

            const std::string full = std::string(cd->animPrefix) + key;
            m_pPlayerRenderer->PlayAnimationByName(
                full,
                ShouldLoopLocalSkillAnimation(bridge, skillStage));

            m_pLastActionLabel = bridge.animKey;
            m_fLastActionTimer = bridge.lockDurationSec > 0.f ? bridge.lockDurationSec : 1.2f;
        }
    }

    bool acceptedHandled = false;
    if (bridge.onCastAcceptedHookId != 0)
    {
        GameCommand gameCommand{};
        gameCommand.kind = (bridge.slot == static_cast<uint8_t>(eSkillSlot::BasicAttack))
            ? eCommandKind::BasicAttack
            : eCommandKind::CastSkill;
        gameCommand.issuerEntity = m_PlayerEntity;
        gameCommand.slot = bridge.slot;
        gameCommand.targetEntity = cmd.targetEntityId;
        gameCommand.groundPos = cmd.groundPos;
        gameCommand.direction = cmd.direction;

        TickContext tickCtx{};
        tickCtx.fDt = 0.f;
        tickCtx.localPlayer = m_PlayerEntity;

        GameplayHookContext gameCtx{};
        gameCtx.pWorld = &m_World;
        gameCtx.casterEntity = m_PlayerEntity;
        gameCtx.casterTeam = m_PlayerTeam;
        gameCtx.casterChampion = bridge.champ;
        gameCtx.skillRank = 1;
        gameCtx.pDef = &bridge;
        gameCtx.pCommand = &gameCommand;
        gameCtx.pTickCtx = &tickCtx;
        const bool gameplayAcceptedHandled =
            CGameplayHookRegistry::Instance().Dispatch(bridge.onCastAcceptedHookId, gameCtx);

        VisualHookContext visualCtx{};
        visualCtx.pWorld = &m_World;
        visualCtx.casterEntity = m_PlayerEntity;
        visualCtx.pDef = &bridge;
        visualCtx.pCommand = &cmd;
        visualCtx.skillStage = skillStage;
        visualCtx.pFxMeshRenderer = m_pFxMeshRenderer.get();
        const bool hasLegacyAcceptedHook =
            CSkillHookRegistry::Instance().Has(bridge.onCastAcceptedHookId);
        const bool suppressVisualAcceptedForLegacy =
            hasLegacyAcceptedHook && bridge.champ == eChampion::IRELIA;
        bool visualAcceptedHandled = false;
        if (!suppressVisualAcceptedForLegacy)
        {
            visualAcceptedHandled =
                CVisualHookRegistry::Instance().Dispatch(bridge.onCastAcceptedHookId, visualCtx);
        }

        SkillHookContext ctx{};
        ctx.pWorld = &m_World;
        ctx.casterEntity = m_PlayerEntity;
        ctx.casterTeam = m_PlayerTeam;
        ctx.pDef = &bridge;
        ctx.pCommand = &cmd;
        ctx.skillStage = skillStage;
        ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
        ctx.startPointDash = [this](const Vec3& start, const Vec3& end, f32_t duration, EntityID target)
            {
                m_bDashActive = true;
                m_fDashElapsed = 0.f;
                m_fDashDuration = duration;
                m_vDashStart = start;
                m_vDashEnd = end;
                m_DashTargetEntity = target;
            };
        ctx.startTargetDash = [this](EntityID target)
            {
                StartLocalTargetDash(target);
            };
        ctx.startUltimateDash = [this](EntityID target)
            {
                StartLocalUltimateDash(target);
            };
        ctx.findAirborneTarget = [this](const Vec3& origin, f32_t radius) -> EntityID
            {
                return FindAirborneEnemyNear(origin, radius);
            };
        ctx.applyTargetDamage = [this](EntityID target, f32_t damage)
            {
                ApplyLocalChampionDamage(
                    target,
                    damage,
                    "SkillHookDamage");
            };
        ctx.setLocalLoopAnimations = [this](const char* idle, const char* run, bool_t playNow)
            {
                m_pPlayerIdleAnim = idle;
                m_pPlayerRunAnim = run;
                if (playNow && m_pPlayerRenderer)
                    m_pPlayerRenderer->PlayAnimationByName(idle, true);
            };
        ctx.getLocalDashDuration = [this]() -> f32_t
            {
                return m_fDashDuration;
            };
        const bool legacyAcceptedHandled =
            CSkillHookRegistry::Instance().Dispatch(bridge.onCastAcceptedHookId, ctx);

        acceptedHandled = gameplayAcceptedHandled || visualAcceptedHandled || legacyAcceptedHandled;
        Winters::DevSmoke::Log(
            "[SkillDispatch] acceptedHook slot=%u champ=%u hook=0x%08X stage=%u gameplay=%u visual=%u legacy=%u\n",
            static_cast<u32_t>(bridge.slot),
            static_cast<u32_t>(bridge.champ),
            bridge.onCastAcceptedHookId,
            static_cast<u32_t>(skillStage),
            gameplayAcceptedHandled ? 1u : 0u,
            visualAcceptedHandled ? 1u : 0u,
            legacyAcceptedHandled ? 1u : 0u);
    }
    (void)acceptedHandled;

    ResetLocalSkillRuntimeState();
    BeginActiveSkillRuntime(cmd, gameData, visualData, legacyDef, skillStage);

    char buf[192];
    const char* modeName = "?";
    switch (static_cast<eTargetMode>(cmd.resolvedTargetMode))
    {
    case eTargetMode::Self:         modeName = "Self";         break;
    case eTargetMode::UnitTarget:   modeName = "UnitTarget";   break;
    case eTargetMode::GroundTarget: modeName = "GroundTarget"; break;
    case eTargetMode::Direction:    modeName = "Direction";    break;
    default: break;
    }
    sprintf_s(buf, "[Cast] slot=%u mode=%s anim=%s target=%u ground=(%.1f,%.1f,%.1f) dir=(%.2f,%.2f)\n",
        cmd.slot, modeName, bridge.animKey ? bridge.animKey : "(null)",
        cmd.targetEntityId,
        cmd.groundPos.x, cmd.groundPos.y, cmd.groundPos.z,
        cmd.direction.x, cmd.direction.z);
    Winters::DevSmoke::Log("%s", buf);
}

void CScene_InGame::OnAuthoritativeSnapshot(
    u64_t serverTick,
    u64_t serverTimeMs,
    u32_t lastAckedCommandSeq,
    u32_t localNetId)
{
    (void)serverTimeMs;
    (void)localNetId;

    PruneAckedNetworkMovePredictions(lastAckedCommandSeq);

    WINTERS_PROFILE_COUNT("Net::ServerTick", serverTick);
    WINTERS_PROFILE_COUNT("Net::LastAckedSeq", lastAckedCommandSeq);
    WINTERS_PROFILE_COUNT("Prediction::PendingMoves",
        static_cast<u64_t>(m_NetworkMovePredictions.size()));
}

void CScene_InGame::RecordNetworkMovePrediction(
    u32_t commandSeq,
    const Vec3& vPredictedTarget,
    const Vec3& vFacingDirection)
{
    if (commandSeq == 0u)
        return;

    NetworkMovePrediction prediction{};
    prediction.commandSeq = commandSeq;
    prediction.vPredictedTarget = vPredictedTarget;
    prediction.vFacingDirection = vFacingDirection;

    m_NetworkMovePredictions.push_back(prediction);
    while (m_NetworkMovePredictions.size() > 64u)
        m_NetworkMovePredictions.pop_front();

    WINTERS_PROFILE_COUNT("Prediction::RecordedMove", 1u);
}

void CScene_InGame::PruneAckedNetworkMovePredictions(u32_t lastAckedCommandSeq)
{
    if (lastAckedCommandSeq == 0u)
        return;

    m_uLastAckedMovePredictionSeq = lastAckedCommandSeq;

    uint64_t prunedCount = 0;
    while (!m_NetworkMovePredictions.empty() &&
        m_NetworkMovePredictions.front().commandSeq <= lastAckedCommandSeq)
    {
        m_NetworkMovePredictions.pop_front();
        ++prunedCount;
    }

    if (prunedCount > 0)
        WINTERS_PROFILE_COUNT("Prediction::AckPruned", prunedCount);
}

void CScene_InGame::RotatePlayerToward(
    const SkillFacingSpec& facingSpec,
    u8_t skillStage,
    const CastSkillCommand& cmd)
{
    const eRotateMode mode = ToLegacyRotateMode(GetFacingMode(facingSpec, skillStage));
    if (mode == eRotateMode::None || !m_pPlayerTransform) return;

    const Vec3 origin = m_pPlayerTransform->GetPosition();
    Vec3 target = origin;

    if (mode == eRotateMode::TowardsTarget
        && cmd.targetEntityId != NULL_ENTITY
        && m_World.HasComponent<TransformComponent>(cmd.targetEntityId))
    {
        target = m_World.GetComponent<TransformComponent>(cmd.targetEntityId).m_LocalPosition;
    }
    else if (mode == eRotateMode::TowardsCursor)
    {
        const bool bHasDir = (fabsf(cmd.direction.x) + fabsf(cmd.direction.z)) > 1e-4f;
        target = bHasDir
            ? Vec3{ origin.x + cmd.direction.x, origin.y, origin.z + cmd.direction.z }
        : cmd.groundPos;
    }

    const f32_t dx = target.x - origin.x;
    const f32_t dz = target.z - origin.z;
    if (dx * dx + dz * dz < 1e-4f) return;

    const f32_t yaw = ResolveChampionVisualYawNear(
        GetPlayerChampionId(),
        Vec3{ dx, 0.f, dz },
        GetPlayerYaw());
    SetPlayerYaw(yaw);

    if (GetPlayerChampionId() == eChampion::KALISTA &&
        (cmd.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
            cmd.slot == static_cast<u8_t>(eSkillSlot::Q)))
    {
        m_vKalistaPassiveDashFaceDir =
            WintersMath::NormalizeXZ(Vec3{ dx, 0.f, dz }, Vec3{}, 0.0001f);
        m_bKalistaPassiveDashHasFaceDir =
            m_vKalistaPassiveDashFaceDir.x != 0.f ||
            m_vKalistaPassiveDashFaceDir.z != 0.f;
    }
}

void CScene_InGame::InitializeMapSurfaceSampler(bool_t bMapLoaded, const wchar_t* pSurfaceMeshPath)
{
    m_pMapSurfaceSampler.reset();
    if (!bMapLoaded)
        return;

    unique_ptr<Engine::CMapSurfaceSampler> sampler(new Engine::CMapSurfaceSampler());
    wchar_t surfacePath[MAX_PATH]{};
    const wchar_t* pPath = (pSurfaceMeshPath && pSurfaceMeshPath[0] != L'\0')
        ? pSurfaceMeshPath
        : L"Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh";
    if (!WintersResolveContentPath(
        pPath,
        surfacePath,
        MAX_PATH) ||
        !sampler->LoadFromWMesh(surfacePath, m_MapTransform.GetWorldMatrix()))
    {
        return;
    }

    m_pMapSurfaceSampler = std::move(sampler);
}

unique_ptr<CNavGrid> CScene_InGame::CreateMapNavGrid() const
{
    const f32_t gridWorldX = CNavGrid::kCellCountX * CNavGrid::kCellSize;
    const f32_t gridWorldZ = CNavGrid::kCellCountY * CNavGrid::kCellSize;
    f32_t centerX = 0.f;
    f32_t centerZ = 0.f;

    if (m_pMapSurfaceSampler && m_pMapSurfaceSampler->IsReady())
    {
        centerX = (m_pMapSurfaceSampler->GetMinX() + m_pMapSurfaceSampler->GetMaxX()) * 0.5f;
        centerZ = (m_pMapSurfaceSampler->GetMinZ() + m_pMapSurfaceSampler->GetMaxZ()) * 0.5f;
    }

    return CNavGrid::Create(
        centerX - gridWorldX * 0.5f,
        centerZ - gridWorldZ * 0.5f);
}

void CScene_InGame::BakeMapWalkableNavGrid()
{
    if (!m_pMapSurfaceSampler || !m_pNavGrid)
        return;

    std::vector<Vec3> seeds{};
    seeds.reserve(64);

    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<TransformComponent>(m_PlayerEntity))
    {
        seeds.push_back(m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition());
    }

    m_World.ForEach<ChampionComponent, TransformComponent>(
        function<void(EntityID, ChampionComponent&, TransformComponent&)>(
            [&](EntityID, ChampionComponent&, TransformComponent& tf)
            {
                seeds.push_back(tf.GetPosition());
            }));

    for (u32_t team = 0; team < 2u; ++team)
    {
        for (u32_t lane = 0; lane < 3u; ++lane)
        {
            const Vec3* pWaypoints = nullptr;
            u32_t count = 0;
            CMinion_Manager::GetWayPoints(
                static_cast<eMinionTeam>(team),
                static_cast<eMinionWay>(lane),
                &pWaypoints,
                &count);

            if (!pWaypoints || count == 0)
                continue;

            for (u32_t i = 0; i < count; ++i)
                seeds.push_back(pWaypoints[i]);
        }
    }

    if (seeds.empty())
        seeds.push_back(Vec3{ 0.f, 0.f, 0.f });

    Engine::MapWalkableBakeDesc desc{};
    desc.playableBaseY = m_fNavPlayableBaseY;
    desc.playableHeightBand = m_fNavPlayableHeightBand;
    desc.minNormalY = m_fNavMinNormalY;
    desc.maxStepHeight = m_fNavMaxStepHeight;

    const bool_t bBaked = Engine::CMapWalkableBaker::BakeIntoNavGrid(
        *m_pMapSurfaceSampler,
        *m_pNavGrid,
        seeds,
        desc);

}

void CScene_InGame::RebuildMapWalkableNavGridForDebug()
{
    wchar_t navGridPath[MAX_PATH] = {};
    if (CMapDataIO::GetNavGridPathW(1, navGridPath, MAX_PATH))
        m_pNavGrid = Engine::CNavGrid::LoadFromFile(navGridPath);

    if (!m_pNavGrid)
    {
        m_pNavGrid = CreateMapNavGrid();
        m_pNavGrid->SetAllWalkable(true);
    }

    Mark_StructuresOnNavGrid();
}

void CScene_InGame::RebuildClientPathNavGrid()
{
    if (!m_pNavGrid)
    {
        m_pPathNavGrid.reset();
        return;
    }

    m_pPathNavGrid = m_pNavGrid->BuildInflated(0.5f);
    if (!m_pPathNavGrid)
    {
        return;
    }

    CPathfinder::PrewarmReachabilityCache(m_pPathNavGrid.get());

}

bool_t CScene_InGame::TryProjectToMapSurface(Vec3& ioPos, f32_t fYOffset) const
{
    if (!m_pMapSurfaceSampler)
        return false;

    f32_t height = 0.f;
    if (!m_pMapSurfaceSampler->SampleHeight(ioPos.x, ioPos.z, height))
        return false;

    ioPos.y = height + fYOffset;
    return true;
}

bool_t CScene_InGame::TryResolveNearestWalkablePosition(
    const Vec3& rawPos,
    Vec3& outPos,
    int32_t maxRadius) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
    {
        outPos = rawPos;
        return true;
    }

    const CNavGrid::Cell cell = pGrid->WorldToCell(rawPos);
    if (pGrid->IsWalkable(cell.x, cell.y))
    {
        outPos = rawPos;
        (void)TryProjectToMapSurface(outPos, 0.05f);
        return true;
    }

    CNavGrid::Cell nearest{};
    if (!pGrid->TryFindNearestWalkableCell(cell, maxRadius, nearest))
        return false;

    outPos = pGrid->CellToWorld(nearest.x, nearest.y);
    if (!TryProjectToMapSurface(outPos, 0.05f))
        outPos.y = rawPos.y;


    return true;
}

bool_t CScene_InGame::TryResolveWalkableMoveTarget(
    const Vec3& rawTarget,
    Vec3& outTarget,
    Vec3* pOutFirstWaypoint) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid || !m_pPlayerTransform)
        return false;

    const Vec3 playerPos = m_pPlayerTransform->GetPosition();
    CNavGrid::Cell start = pGrid->WorldToCell(playerPos);
    const CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);

    auto ProjectMoveTarget = [&](Vec3& ioTarget)
        {
            if (!TryProjectToMapSurface(ioTarget, 0.05f))
            {
                ioTarget.y = playerPos.y;
                return;
            }

            const f32_t surfaceDeltaY = ioTarget.y - playerPos.y;
            if (std::fabs(surfaceDeltaY) <= kMoveTargetMaxSurfaceDeltaY)
                return;

            ioTarget.y = playerPos.y;
        };

    if (!pGrid->IsWalkable(start.x, start.y))
    {
        CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 8, nearestStart))
            return false;
        start = nearestStart;
    }

    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    const bool_t bRawGoalWalkable = pGrid->IsWalkable(rawGoal.x, rawGoal.y);
    const bool_t bRawSegmentWalkable = pGrid->SegmentWalkable(playerPos, rawTarget, 0.f);

    if (bRawGoalWalkable && bRawSegmentWalkable)
    {
        outTarget = rawTarget;
        ProjectMoveTarget(outTarget);
        if (pOutFirstWaypoint)
            *pOutFirstWaypoint = outTarget;
        return true;
    }

    CNavGrid::Cell resolved{};
    std::vector<CNavGrid::Cell> path{};
    if (!CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    ProjectMoveTarget(outTarget);

    if (pOutFirstWaypoint)
    {
        *pOutFirstWaypoint = outTarget;
        const std::vector<CNavGrid::Cell> smoothedPath =
            SmoothClientMovePathCells(*pGrid, path);
        if (smoothedPath.size() > 1)
        {
            Vec3 waypoint = pGrid->CellToWorld(
                smoothedPath[1].x,
                smoothedPath[1].y);
            ProjectMoveTarget(waypoint);

            Vec3 intentFacingTarget = rawTarget;
            ProjectMoveTarget(intentFacingTarget);
            const bool_t bFirstWaypointOpposed = IsFacingCandidateOpposedToIntent(
                playerPos,
                intentFacingTarget,
                waypoint);
            *pOutFirstWaypoint = bFirstWaypointOpposed ? intentFacingTarget : waypoint;
        }
    }


    return true;
}

bool_t CScene_InGame::IsWalkableMoveSegment(const Vec3& from, const Vec3& to, f32_t radiusWorld) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
        return true;

    return pGrid->SegmentWalkable(from, to, m_pPathNavGrid ? 0.f : radiusWorld);
}

Vec3 CScene_InGame::ResolveMouseMapSurfacePos() const
{
    if (!m_pCamera)
        return Vec3{};

    const auto& input = CInput::Get();
    const i32_t screenW = static_cast<i32_t>(g_iWinSizeX);
    const i32_t screenH = static_cast<i32_t>(g_iWinSizeY);
    const CInput::MouseRay ray =
        input.GetMouseWorldRay(*m_pCamera, screenW, screenH);
    Vec3 ground = input.GetMouseGroundPos(*m_pCamera, screenW, screenH);
    bool_t bProjected = false;

    if (m_pMapSurfaceSampler && m_pMapSurfaceSampler->IsReady())
    {
        if (std::fabs(ray.Dir.y) > 0.0001f)
        {
            f32_t t = -ray.Origin.y / ray.Dir.y;
            bool_t bSurfaceHit = std::isfinite(t) && t >= 0.f;
            f32_t height = 0.f;

            for (u32_t i = 0; bSurfaceHit && i < 6u; ++i)
            {
                const Vec3 p{
                    ray.Origin.x + ray.Dir.x * t,
                    ray.Origin.y + ray.Dir.y * t,
                    ray.Origin.z + ray.Dir.z * t
                };
                if (!m_pMapSurfaceSampler->SampleHeight(p.x, p.z, height))
                {
                    bSurfaceHit = false;
                    break;
                }

                const f32_t nextT = (height - ray.Origin.y) / ray.Dir.y;
                if (!std::isfinite(nextT) || nextT < 0.f)
                {
                    bSurfaceHit = false;
                    break;
                }

                if (std::fabs(nextT - t) <= 0.001f)
                {
                    t = nextT;
                    break;
                }
                t = nextT;
            }

            if (bSurfaceHit)
            {
                const Vec3 p{
                    ray.Origin.x + ray.Dir.x * t,
                    ray.Origin.y + ray.Dir.y * t,
                    ray.Origin.z + ray.Dir.z * t
                };
                if (m_pMapSurfaceSampler->SampleHeight(p.x, p.z, height))
                {
                    ground = { p.x, height, p.z };
                    bProjected = true;
                }
            }
        }
    }

    if (!bProjected)
        bProjected = TryProjectToMapSurface(ground, 0.f);

    return ground;
}

void CScene_InGame::ProjectGameplayActorsToMapSurface()
{
    if (!m_pMapSurfaceSampler)
        return;

    m_World.ForEach<ChampionComponent, TransformComponent>(
        function<void(EntityID, ChampionComponent&, TransformComponent&)>(
            [&](EntityID entity, ChampionComponent&, TransformComponent& tf)
            {
                Vec3 pos = tf.GetPosition();
                if (!TryProjectToMapSurface(pos, 0.05f))
                    return;

                tf.SetPosition(pos);
                if (entity == m_PlayerEntity)
                {
                    SetPlayerPosition(pos);
                    m_vPlayerDest.y = pos.y;
                }
            }));

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        function<void(EntityID, MinionStateComponent&, TransformComponent&)>(
            [&](EntityID, MinionStateComponent& state, TransformComponent& tf)
            {
                if (state.current == MinionStateComponent::Dead)
                    return;

                Vec3 pos = tf.GetPosition();
                if (TryProjectToMapSurface(pos, 0.02f))
                    tf.SetPosition(pos);
            }));
}

void CScene_InGame::TriggerFlash()
{
    if (!m_pPlayerTransform || !m_pCamera) return;

    const Vec3 cursor = ResolveMouseMapSurfacePos();
    const Vec3 origin = m_pPlayerTransform->GetPosition();
    const f32_t dx = cursor.x - origin.x;
    const f32_t dz = cursor.z - origin.z;
    const f32_t lenSq = dx * dx + dz * dz;
    if (lenSq < 0.001f) return;

    const f32_t len = std::sqrt(lenSq);
    const f32_t nx = dx / len;
    const f32_t nz = dz / len;
    const Vec3 direction{ nx, 0.f, nz };

    if (m_bNetworkAuthoritativeGameplay &&
        m_pCommandSerializer &&
        m_pNetworkView &&
        m_pNetworkView->IsConnected())
    {
        m_pCommandSerializer->SendFlash(*m_pNetworkView, cursor, direction);
        return;
    }

    if (m_fFlashCooldownLeft > 0.f) return;

    const f32_t useLen = (len > m_fFlashRange) ? m_fFlashRange : len;
    Vec3 dest{ origin.x + nx * useLen, origin.y, origin.z + nz * useLen };
    (void)TryProjectToMapSurface(dest, 0.05f);

    SetPlayerPosition(dest);

    m_fFlashCooldownLeft = m_fFlashCooldown;
}

void CScene_InGame::UpdateFlashCooldown(f32_t dt)
{
    if (m_fFlashCooldownLeft <= 0.f) return;

    m_fFlashCooldownLeft -= dt;
    if (m_fFlashCooldownLeft < 0.f)
        m_fFlashCooldownLeft = 0.f;
}

void CScene_InGame::Mark_StructuresOnNavGrid()
{
    if (!m_pNavGrid)
        return;
    const uint32_t iCount = CStructure_Manager::Get()->Get_Count();
    for (uint32_t i = 0; i < iCount; ++i)
    {
        TransformComponent* pTf = CStructure_Manager::Get()->Get_Transform(i);
        if (!pTf)
            continue;
        const Vec3 vPos = pTf->GetLocalPosition();
        f32_t radius = 2.f;
        EntityID entity = CStructure_Manager::Get()->Get_EntityAt(i);
        if (m_World.HasComponent<StructureComponent>(entity))
        {
            auto& sc = m_World.GetComponent<StructureComponent>(entity);
            if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Nexus))
                radius = 4.f;
            else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Inhibitor))
                radius = 3.f;
        }
        const CNavGrid::Cell center = m_pNavGrid->WorldToCell(vPos);
        const int32_t rCells = static_cast<int32_t>(radius / CNavGrid::kCellSize);
        for (int32_t dy = -rCells; dy <= rCells; ++dy)
             {
            for (int32_t dx = -rCells; dx <= rCells; ++dx)
            {
                if (dx * dx + dy * dy <= rCells * rCells)
                    m_pNavGrid->SetWalkable(center.x + dx, center.y + dy, false);
            }
        }
    }
    RebuildClientPathNavGrid();
}
