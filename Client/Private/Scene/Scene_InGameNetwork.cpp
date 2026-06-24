// Scene_InGameNetwork.cpp — CScene_InGame의 네트워크 스냅샷-적용/보간/예측 책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp에서 verbatim 이동. 동작/시그니처/호출순서 불변.
// local-only prediction(ApplyLocalPrediction/StartLocal*Dash)과 절대 같은 파일에 두지 않는다.
// 설계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md
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
#include "Scene/Scene_InGameInternal.h"
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

namespace
{
    constexpr f32_t kNetworkActorInterpDurationSec = 0.055f;
    constexpr f32_t kNetworkActorInterpTeleportSq = 9.f;
    constexpr f32_t kNetworkActorInterpMinMoveSq = 0.0001f;
    constexpr f32_t kNetworkActorInterpMinYaw = 0.0005f;

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

    bool_t IsMoveBlockingNetworkAction(u16_t actionId)
    {
        switch (static_cast<eActionStateId>(actionId))
        {
        case eActionStateId::SkillQ:
        case eActionStateId::SkillW:
        case eActionStateId::SkillE:
        case eActionStateId::SkillR:
        case eActionStateId::ViegoConsumeSoul:
            return true;
        default:
            return false;
        }
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

    f32_t ResolveNetworkActionPlaySpeed(
        eChampion champion,
        const SkillDef* pDef,
        u16_t actionId,
        u8_t stage)
    {
        const u8_t slot = NetworkActionToSkillSlot(actionId);
        f32_t playSpeed = 1.f;
        if (ChampionGameDataDB::FindSkill(champion, slot))
        {
            playSpeed =
                ChampionGameDataDB::ResolveSkillTiming(champion, slot, stage).animPlaySpeed;
        }
        else if (pDef)
        {
            playSpeed = (stage >= 2u && pDef->stage2PlaySpeed > 0.01f)
                ? pDef->stage2PlaySpeed
                : pDef->animPlaySpeed;
        }

        return (std::isfinite(playSpeed) && playSpeed > 0.01f) ? playSpeed : 1.f;
    }

    f32_t ResolveNetworkActionLockDurationSec(
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

    f32_t ResolveNetworkActionDurationSec(
        eChampion champion,
        const SkillDef* pDef,
        u16_t actionId,
        u8_t stage,
        const RenderComponent& render,
        const std::string& animName)
    {
        const f32_t lockDurationSec =
            ResolveNetworkActionLockDurationSec(champion, pDef, actionId, stage);
        const bool_t bLoopAction =
            champion == eChampion::JAX &&
            static_cast<eActionStateId>(actionId) == eActionStateId::SkillE &&
            stage <= 1u;
        if (bLoopAction ||
            !render.pRenderer ||
            animName.empty())
        {
            return lockDurationSec;
        }

        const f32_t animDurationSec =
            render.pRenderer->GetAnimationDurationSecondsByName(animName);
        if (!std::isfinite(animDurationSec) || animDurationSec <= 0.01f)
            return lockDurationSec;

        const f32_t playSpeed =
            ResolveNetworkActionPlaySpeed(champion, pDef, actionId, stage);
        const f32_t visualDurationSec = animDurationSec / playSpeed;
        if (!std::isfinite(visualDurationSec) || visualDurationSec <= 0.01f)
            return lockDurationSec;

        return (std::min)(lockDurationSec, visualDurationSec);
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
            ResolveNetworkActionPlaySpeed(
                champion,
                pSkillDef,
                action.actionId,
                action.stage));
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
    else
    {
        const CGameSessionClient::ServerEndpoint endpoint =
            CGameSessionClient::ResolveServerEndpoint();
        if (m_pNetworkView->Connect(endpoint.host.c_str(), endpoint.port))
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Connected to Winters server host=%s port=%u source=%s.\n",
                endpoint.host.c_str(),
                static_cast<u32_t>(endpoint.port),
                endpoint.bFromCommandLine ? "command-line" : "default");
        }
        else
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Server not reachable host=%s port=%u; running local-only mode.\n",
                endpoint.host.c_str(),
                static_cast<u32_t>(endpoint.port));
        }
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
            NetworkActionAnimationState& actionState = m_NetworkActionAnimStates[e];
            const bool_t bMoveBlockedByNetworkAction =
                pAction &&
                pAction->sequence != 0u &&
                IsMoveBlockingNetworkAction(pAction->actionId) &&
                (actionState.bActionActive ||
                    actionState.actionSeq != pAction->sequence);

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
            if (bMoveBlockedByNetworkAction)
                moveGrace = 0.f;

            bPositionMoving = !bMoveBlockedByNetworkAction && (moveGrace > 0.f);
            const bool_t bPoseRequestsIdle =
                pPose && pPose->poseId == static_cast<u16_t>(ePoseStateId::Idle);
            const bool_t bPoseRequestsDeath =
                pPose && pPose->poseId == static_cast<u16_t>(ePoseStateId::Dead);
            if (bPoseRequestsDeath)
                moveGrace = 0.f;

            bool_t bMoving =
                !bPoseRequestsDeath &&
                !bMoveBlockedByNetworkAction &&
                (bServerMoving || bPositionMoving);
            if (bPoseRequestsIdle && !bServerMoving && !bPositionMoving)
                bMoving = false;

            prevIt->second = pos;

            if (e == m_PlayerEntity)
                m_bMoving = bMoving;

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
                        const std::string actionAnimName =
                            ResolveNetworkActionAnimName(champ.id, pSkillDef, *pAction);
                        actionState = {};
                        actionState.actionSeq = pAction->sequence;
                        actionState.actionId = pAction->actionId;
                        actionState.actionRemainingSec =
                            ResolveNetworkActionDurationSec(
                                champ.id,
                                pSkillDef,
                                pAction->actionId,
                                pAction->stage,
                                rc,
                                actionAnimName);
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
