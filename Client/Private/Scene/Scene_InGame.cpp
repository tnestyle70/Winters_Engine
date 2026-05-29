#define _CRT_SECURE_NO_WARNINGS

#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/EventApplier.h"
#include "Network/Client/GameSessionClient.h"
#include "Network/Client/SnapshotApplier.h"

#include <Windows.h>
#include "Scene/InGameBootstrapBridge.h"
#include "Scene/InGameChampionStateBridge.h"
#include "Scene/InGameCombatInputBridge.h"
#include "Scene/GameplayQuery.h"
#include "Scene/InGameDebugBridge.h"
#include "Scene/InGameLifecycleBridge.h"
#include "Scene/InGameNetworkBridge.h"
#include "Scene/InGamePlayerControlBridge.h"
#include "Scene/InGamePlayerTransformBridge.h"
#include "Scene/InGameRenderBridge.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/InGameSkillDispatchBridge.h"
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
#include "ECS/Systems/GameplayCollisionSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/TurretAISystem.h"
#include "ECS/Systems/TurretProjectileSystem.h"
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
#include "UI/MinimapPanel.h"
#include "UI/WfxEffectToolPanel.h"

#include "Resource/Animator.h"
#include "Resource/Animation.h"

#include "GameObject/ChampionDef.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/Champion/Zed/ZedFxPresets.h"
#include "GameObject/ChampionSpawnService.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameContext.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
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
#include "ECS/Systems/StatusEffectSystem.h"
#include "ECS/Components/GameplayComponents.h"   // Stun/Slow/Disarm
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "Renderer/FxStaticMeshRenderer.h"

#include "ECS/Components/MeshGroupVisibilityComponent.h"

#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "Renderer/FogOfWarRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <vector>

CScene_InGame::CScene_InGame() = default;
CScene_InGame::~CScene_InGame() = default;

namespace
{
    constexpr f32_t kMoveTargetMaxSurfaceDeltaY = 3.f;
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

    bool_t IsNetworkActionAnim(u16_t animId)
    {
        const auto id = static_cast<eNetAnimId>(animId);
        return id == eNetAnimId::BasicAttack ||
            id == eNetAnimId::SkillQ ||
            id == eNetAnimId::SkillW ||
            id == eNetAnimId::SkillE ||
            id == eNetAnimId::SkillR ||
            id == eNetAnimId::ViegoConsumeSoul;
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

    u8_t NetworkAnimToSkillSlot(u16_t animId)
    {
        switch (static_cast<eNetAnimId>(animId))
        {
        case eNetAnimId::SkillQ:
            return static_cast<u8_t>(eSkillSlot::Q);
        case eNetAnimId::SkillW:
            return static_cast<u8_t>(eSkillSlot::W);
        case eNetAnimId::SkillE:
            return static_cast<u8_t>(eSkillSlot::E);
        case eNetAnimId::SkillR:
            return static_cast<u8_t>(eSkillSlot::R);
        case eNetAnimId::BasicAttack:
        default:
            return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
    }

    const SkillDef* FindNetworkSkillDef(eChampion champion, u16_t animId)
    {
        const u8_t slot = NetworkAnimToSkillSlot(animId);
        const SkillDef* pDef = CSkillRegistry::Instance().Find(champion, slot);
        if (!pDef)
            pDef = FindSkillDef(champion, slot);
        return pDef;
    }

    u8_t NetworkStageFromFlags(u16_t flags)
    {
        const u8_t stage = static_cast<u8_t>((flags >> 12) & 0x0fu);
        return stage == 0u ? 1u : stage;
    }

    f32_t ResolveNetworkActionDurationSec(eChampion champion, const SkillDef* pDef, u16_t animId, u16_t flags)
    {
        const u8_t slot = NetworkAnimToSkillSlot(animId);
        const u8_t stage = NetworkStageFromFlags(flags);
        const ChampionSkillTimingDefaults timing =
            GetDefaultChampionSkillTiming(champion, slot, stage);
        if (timing.lockDurationSec > 0.01f)
            return timing.lockDurationSec;

        if (stage >= 2u && pDef && pDef->stage2LockSec > 0.01f)
            return pDef->stage2LockSec;

        if (pDef && pDef->lockDurationSec > 0.01f)
            return pDef->lockDurationSec;

        return static_cast<eNetAnimId>(animId) == eNetAnimId::BasicAttack
            ? 0.45f
            : 0.6f;
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
            OutputDebugStringA("[Scene_InGame] server ready skipped: missing local slot\n");
            return;
        }

        const bool_t bSent = session.SendLobbyCommand(
            Shared::Schema::LobbyCommandKind::SetReady,
            context.MySlotId,
            eChampion::END,
            0,
            1u);

        if (bSent)
            OutputDebugStringA("[Scene_InGame] server ready sent after OnEnter\n");
    }

}

bool CScene_InGame::OnEnter()
{
    const bool_t bEntered = CInGameBootstrapBridge::Enter(*this);
    if (bEntered)
        SendServerInGameReady();
    return bEntered;
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

    CInGameNetworkBridge::ReplayLastHelloIfShared(m_bUsingSharedNetwork);

    char dbg[192]{};
    sprintf_s(dbg, "[ECS:RosterOnly] created=%d total=%u player=%u champion=%u\n",
        rosterResult.bCreatedAny ? 1 : 0,
        m_World.GetEntityCount(),
        static_cast<u32_t>(m_PlayerEntity),
        static_cast<u32_t>(GetPlayerChampionId()));
    Winters::DevSmoke::Log("%s", dbg);
}
bool CScene_InGame::HasPlayerTransform() const
{
    return CInGamePlayerTransformBridge::HasPlayerTransform(*this);
}

Vec3 CScene_InGame::GetPlayerPosition() const
{
    return CInGamePlayerTransformBridge::GetPlayerPosition(*this);
}

void CScene_InGame::SetPlayerPosition(const Vec3& v)
{
    CInGamePlayerTransformBridge::SetPlayerPosition(*this, v);
}

f32_t CScene_InGame::GetPlayerYaw() const
{
    return CInGamePlayerTransformBridge::GetPlayerYaw(*this);
}

void CScene_InGame::SetPlayerYaw(f32_t yaw)
{
    CInGamePlayerTransformBridge::SetPlayerYaw(*this, yaw);
}

void CScene_InGame::SyncPlayerEntityTransformFromECS()
{
    CInGamePlayerTransformBridge::SyncFromECS(*this);
}

void CScene_InGame::SyncPlayerEntityTransformToECS()
{
    CInGamePlayerTransformBridge::SyncToECS(*this);
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
            const NetAnimationComponent* pNetAnim =
                m_World.HasComponent<NetAnimationComponent>(e)
                ? &m_World.GetComponent<NetAnimationComponent>(e)
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
            const bool_t bAnimRequestsIdle =
                pNetAnim && pNetAnim->animId == static_cast<u16_t>(eNetAnimId::Idle);
            const bool_t bAnimRequestsDeath =
                pNetAnim && pNetAnim->animId == static_cast<u16_t>(eNetAnimId::Death);
            if (bAnimRequestsDeath)
                moveGrace = 0.f;

            bool_t bMoving = !bAnimRequestsDeath && (bServerMoving || bPositionMoving);
            if (bAnimRequestsIdle && !bServerMoving && !bPositionMoving)
                bMoving = false;

            prevIt->second = pos;

            if (e == m_PlayerEntity)
                m_bMoving = bMoving;

            NetworkActionAnimationState& actionState = m_NetworkActionAnimStates[e];
            if (bAnimRequestsDeath)
            {
                if (pNetAnim && actionState.baseSeq != pNetAnim->actionSeq)
                {
                    actionState = {};
                    actionState.baseSeq = pNetAnim->actionSeq;

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

            if (pNetAnim && pNetAnim->actionSeq != 0)
            {
                if (IsNetworkActionAnim(pNetAnim->animId))
                {
                    if (actionState.actionSeq != pNetAnim->actionSeq)
                    {
                        const SkillDef* pSkillDef = FindNetworkSkillDef(champ.id, pNetAnim->animId);
                        actionState = {};
                        actionState.actionSeq = pNetAnim->actionSeq;
                        actionState.animId = pNetAnim->animId;
                        actionState.actionRemainingSec =
                            ResolveNetworkActionDurationSec(
                                champ.id,
                                pSkillDef,
                                pNetAnim->animId,
                                pNetAnim->flags);
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
                        actionState.bActionActive = true;
                        actionState.bBaseAnimationPending = true;
                        actionState.bPassiveDashTriggered = false;
                    }
                }
                else if (actionState.baseSeq != pNetAnim->actionSeq)
                {
                    actionState.baseSeq = pNetAnim->actionSeq;
                    actionState.bBaseAnimationPending = true;
                    if (actionState.bActionActive)
                    {
                        actionState.bActionActive = false;
                        actionState.bTransitionActive = false;
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
                CInGameChampionStateBridge::IsLocalActionProtected(*this);
            if (bLocalActionProtected)
            {
                m_NetworkChampionMoving[e] = bMoving;
                return;
            }

            if (actionState.bActionActive)
            {
                m_NetworkChampionMoving[e] = bMoving;
                actionState.bDesiredMoving = bMoving;
                actionState.actionRemainingSec -= dt;
                if (actionState.actionRemainingSec > 0.f)
                    return;

                if (e == m_PlayerEntity && champ.id == eChampion::KALISTA)
                {
                    actionState.bActionActive = false;
                    if (!actionState.bPassiveDashTriggered)
                    {
                        const bool_t bDashStarted =
                            CInGameChampionStateBridge::TriggerNetworkPassiveDashFromAction(
                            *this,
                            actionState.animId,
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
                                Kalista::GetTuning().passiveDashInputGraceSec;
                            actionState.bBaseAnimationPending = true;
                        }
                    }
                    if (CInGameChampionStateBridge::IsLocalActionProtected(*this))
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
                actionState.transitionRemainingSec -= dt;
                if (actionState.transitionRemainingSec > 0.f)
                    return;

                actionState.bTransitionActive = false;
                actionState.bBaseAnimationPending = true;
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
    return CInGamePlayerTransformBridge::GetPlayerForward(*this);
}

void CScene_InGame::OnUpdate(f32_t dt)
{
    static int s_frameCount = 0;
    ++s_frameCount;
    if (s_frameCount <= 3)
    {
        char dbg[64];
        sprintf_s(dbg, "[OnUpdate #%d] enter\n", s_frameCount);
        OutputDebugStringA(dbg);
    }

    WINTERS_PROFILE_SCOPE("Scene_InGame::OnUpdate");
    if (m_bNetworkAuthoritativeGameplay && m_bNetworkActorInterpolationEnabled)
        CaptureNetworkActorInterpolationStarts();

    const bool_t bNetworkActive =
        CInGameNetworkBridge::Pump(m_pNetworkView, m_bUsingSharedNetwork);

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
    if (s_frameCount <= 3) OutputDebugStringA("  [A] after SyncECS\n");
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

    if (s_frameCount <= 3) OutputDebugStringA("  [B] after Scheduler\n");

    if (m_bNetworkAuthoritativeGameplay &&
        bNetworkActive &&
        m_bNetworkActorInterpolationEnabled)
    {
        ApplyNetworkActorInterpolation(dt);
    }

    SyncPlayerEntityTransformFromECS();

    if (bNetworkActive)
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

    UpdateTargeting();

    bool bSkipGroundMove = false;
    UpdateCombatInput(bSkipGroundMove);

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
            && m_bCastFrameFired
            && m_pLastDispatchedSkill != nullptr)
        {
            Winters::DevSmoke::Log(
                "[SmokeSkill] castFrame observed champ=%u slot=%u hook=0x%08X\n",
                static_cast<u32_t>(GetPlayerChampionId()),
                static_cast<u32_t>(m_pLastDispatchedSkill->slot),
                m_pLastDispatchedSkill->castFrameHookId);
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
                if (CInGameChampionStateBridge::CanResumeBaseAnimation(*this))
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

    if (m_bNetworkAuthoritativeGameplay && m_pActiveSkillDef)
    {
        m_pActiveSkillDef = nullptr;
        m_fActivePrevFrame = 0.f;
        m_bCastFrameFired = false;
        m_bRecoveryFrameFired = false;
    }
    else if (m_pActiveSkillDef && m_pPlayerRenderer)
    {
        const Engine::CAnimator* pAnim = m_pPlayerRenderer->GetAnimator();
        if (pAnim)
        {
            const f32_t curF = pAnim->GetCurrentFrame();
            const SkillDef& d = *m_pActiveSkillDef;

            const bool bCastHit =
                !m_bCastFrameFired
                && d.castFrame > 0.f
                && pAnim->HasFramePassed(d.castFrame, m_fActivePrevFrame);
            const bool bRecoveryHit =
                !m_bRecoveryFrameFired
                && d.recoveryFrame > 0.f
                && pAnim->HasFramePassed(d.recoveryFrame, m_fActivePrevFrame);

            if (m_bLogFrameEvents)
            {
                char buf[128];
                if (bCastHit)
                {
                    sprintf_s(buf, "[FrameEvent] CAST slot=%u anim=%s frame=%.1f\n",
                        d.slot, d.animKey ? d.animKey : "?", curF);
                    OutputDebugStringA(buf);
                    Winters::DevSmoke::Log("%s", buf);
                }
                if (bRecoveryHit)
                {
                    sprintf_s(buf, "[FrameEvent] RECOVERY slot=%u frame=%.1f\n", d.slot, curF);
                    OutputDebugStringA(buf);
                    Winters::DevSmoke::Log("%s", buf);
                }
            }

            if (bCastHit)
            {
                m_bCastFrameFired = true;

                // Local/offline path only. Network-authoritative gameplay is handled by server commands.
                const eChampion champ = GetPlayerChampionId();
                GameCommand gameCommand{};
                gameCommand.kind = (d.slot == static_cast<uint8_t>(eSkillSlot::BasicAttack))
                    ? eCommandKind::BasicAttack
                    : eCommandKind::CastSkill;
                gameCommand.issuerEntity = m_PlayerEntity;
                gameCommand.slot = d.slot;
                gameCommand.targetEntity = m_ActiveSkillCommandStorage.targetEntityId;
                gameCommand.groundPos = m_ActiveSkillCommandStorage.groundPos;
                gameCommand.direction = m_ActiveSkillCommandStorage.direction;
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
                gameCtx.pDef = m_pActiveSkillDef;
                gameCtx.pCommand = &gameCommand;
                gameCtx.pTickCtx = &tickCtx;
                bool gameplayHandled = false;
                if (m_pActiveSkillDef->castFrameHookId != 0)
                {
                    gameplayHandled = CGameplayHookRegistry::Instance().Dispatch(
                        m_pActiveSkillDef->castFrameHookId, gameCtx
                    );
                }
                //Client Visual FX/Sound
                VisualHookContext visualCtx{};
                visualCtx.pWorld = &m_World;
                visualCtx.casterEntity = m_PlayerEntity;
                visualCtx.pDef = m_pActiveSkillDef;
                visualCtx.pCommand = &m_ActiveSkillCommandStorage;
                visualCtx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                bool visualHandled = false;
                if (m_pActiveSkillDef->castFrameHookId != 0)
                    visualHandled = CVisualHookRegistry::Instance().Dispatch(
                        m_pActiveSkillDef->castFrameHookId, visualCtx);

                // Legacy local skill hook path for offline/practice visuals.
                bool castHandled = false;
                if (m_pActiveSkillDef && m_pActiveSkillDef->castFrameHookId != 0)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &m_World;
                    ctx.casterEntity = m_PlayerEntity;
                    ctx.casterTeam = m_PlayerTeam;
                    ctx.pDef = m_pActiveSkillDef;
                    ctx.pCommand = &m_ActiveSkillCommandStorage;
                    ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                    ctx.applyTargetDamage = [this](EntityID target, f32_t damage)
                        {
                            CInGameChampionStateBridge::ApplyLocalChampionDamage(
                                *this,
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
                        m_pActiveSkillDef->castFrameHookId, ctx);
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
                m_bRecoveryFrameFired = true;

                //DispatchHook Recovery
                bool recoveryHandled = false;
                if (m_pActiveSkillDef && m_pActiveSkillDef->recoveryHookId != 0)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &m_World;
                    ctx.casterEntity = m_PlayerEntity;
                    ctx.casterTeam = m_PlayerTeam;
                    ctx.pDef = m_pActiveSkillDef;
                    ctx.pCommand = &m_ActiveSkillCommandStorage;
                    ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                    ctx.pCasterRenderer = m_pPlayerRenderer;
                    ctx.fGlobalAnimSpeed = m_fGlobalAnimSpeed;
                    ctx.startLocalDash = [this](const Vec3& dir)
                        {
                            CInGameChampionStateBridge::StartLocalPassiveDash(*this, dir);
                        };
                    ctx.setLocalDashDuration = [this](f32_t duration)
                        {
                            CInGameChampionStateBridge::SetLocalPassiveDashDuration(duration);
                        };
                    ctx.getLocalDashDuration = [this]() -> f32_t
                        {
                            return CInGameChampionStateBridge::GetLocalPassiveDashDuration();
                        };
                    ctx.setLocalActionAnimActive = [this](bool_t active)
                        {
                            CInGameChampionStateBridge::SetLocalActionAnimActive(*this, active);
                        };
                    recoveryHandled = CSkillHookRegistry::Instance().Dispatch(
                        m_pActiveSkillDef->recoveryHookId, ctx
                    );
                }
                (void)recoveryHandled;


            }

            if (d.recoveryFrame > 0.f && curF >= d.recoveryFrame)
                m_pActiveSkillDef = nullptr;
            else
                m_fActivePrevFrame = curF;
        }
    }

    if (m_pFxSystem)          m_pFxSystem->Update(m_World, dt);
    if (m_pFxBeamSystem)      m_pFxBeamSystem->Update(m_World, dt);
    if (m_pFxMeshSystem) m_pFxMeshSystem->Update(m_World, dt);
    ZedFx::TickShadowCloneModels(m_World, dt);
    CInGameChampionStateBridge::UpdateLocalRuntime(*this, dt);
    UpdateFlashCooldown(dt);

    CInGameChampionStateBridge::Update(*this, dt);

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

    CInGamePlayerControlBridge::Update(
        *this,
        dt,
        bNetworkActive,
        bSkipGroundMove,
        bActionLockedBefore);

    m_fElapsed += dt;

    m_CubeTransform.SetRotationX(m_fElapsed * 0.8f);

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

    CInGameChampionStateBridge::UpdateLocalPostAnimation(*this);

    {
        static int s_fc = 0;
        ++s_fc;
        if (s_fc <= 3) OutputDebugStringA("  [C] before MinionMgr::Tick\n");
        if (m_bNetworkAuthoritativeGameplay)
            CMinion_Manager::Get()->TickVisuals(dt);
        else
            CMinion_Manager::Get()->Tick(dt);
        if (!m_bNetworkAuthoritativeGameplay && m_pGameplayCollisionSystem)
            m_pGameplayCollisionSystem->Execute(m_World, dt);
        ProjectGameplayActorsToMapSurface();
        if (s_fc <= 3) OutputDebugStringA("  [D] after MinionMgr::Tick\n");
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
        UI::CAIDebugPanel::Render(m_World, this);

    if (m_bShowRenderDebug)
        UI::CRenderDebugPanel::Render(this);

    if (m_bShowUITuner)
        CGameInstance::Get()->UI_OnImGui_Tuner();

    UI::CWfxEffectToolPanel::Render(this);

    if (!m_bShowLegacyInGameDebug)
        return;

    InGameDebugBridgeDesc debugDesc{
        *this,
        m_World,
        m_pFogOfWarRenderer.get(),
        m_pCamera.get(),
        m_PlayerTeam,
        m_bLogFrameEvents
    };
    CInGameDebugBridge::Render(debugDesc);
}

void CScene_InGame::OnLateUpdate(f32_t /*dt*/)
{
}

void CScene_InGame::OnRender()
{
    CInGameRenderBridge::Render(*this);
}

void CScene_InGame::OnSnapshot(const u8_t* bytes, u32_t len)
{
    CInGameNetworkBridge::ApplySnapshot(
        m_World,
        m_pSnapshotApplier.get(),
        m_pEntityIdMap.get(),
        bytes,
        len);
    ProjectGameplayActorsToMapSurface();
    // Sim-5 will replay unacked local inputs after this authority snapshot is applied.
}

void CScene_InGame::OnExit()
{
    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);
    CInGameLifecycleBridge::Shutdown(*this);
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

void CScene_InGame::UpdateTargeting()
{
    CInGameCombatInputBridge::UpdateTargeting(*this);
}

void CScene_InGame::UpdateCombatInput(bool& outSkipGroundMove)
{
    CInGameCombatInputBridge::UpdateCombatInput(*this, outSkipGroundMove);
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
    m_pActiveSkillDef = nullptr;
    m_fActivePrevFrame = 0.f;
    CInGameChampionStateBridge::ResetLocalSkillRuntimeState(*this);
    m_pLastActionLabel = reasonLabel ? reasonLabel : "(preempt)";

    m_fEndTransitionTimer = 0.f;
    m_pPendingEndAnim = nullptr;

    m_bDashActive = false;
    m_fDashElapsed = 0.f;
    m_DashTargetEntity = NULL_ENTITY;

    char dbg[96];
    sprintf_s(dbg, "[Preempt] reason=%s\n", m_pLastActionLabel);
    OutputDebugStringA(dbg);
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
        OutputDebugStringA("[Dash] completed, BA cooldown reset\n");
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

bool CScene_InGame::DispatchSkillInput(uint8_t slot)
{
    return CInGameSkillDispatchBridge::DispatchSkillInput(*this, slot);
}

bool CScene_InGame::BuildCastCommand(const SkillDef& def, CastSkillCommand& outCmd)
{
    return CInGameSkillDispatchBridge::BuildCastCommand(*this, def, outCmd);
}
void CScene_InGame::ApplyLocalPrediction(const CastSkillCommand& cmd, const SkillDef& def, u8_t skillStage)
{
    CInGameSkillDispatchBridge::ApplyLocalPrediction(*this, cmd, def, skillStage);
}

void CScene_InGame::RotatePlayerToward(eRotateMode mode, const CastSkillCommand& cmd)
{
    CInGameSkillDispatchBridge::RotatePlayerToward(*this, mode, cmd);
}

void CScene_InGame::InitializeMapSurfaceSampler(bool_t bMapLoaded)
{
    m_pMapSurfaceSampler.reset();
    if (!bMapLoaded)
        return;

    unique_ptr<Engine::CMapSurfaceSampler> sampler(new Engine::CMapSurfaceSampler());
    wchar_t surfacePath[MAX_PATH]{};
    if (!WintersResolveContentPath(
        L"Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh",
        surfacePath,
        MAX_PATH) ||
        !sampler->LoadFromWMesh(surfacePath, m_MapTransform.GetWorldMatrix()))
    {
        OutputDebugStringA("[MapSurface] load failed; y projection disabled\n");
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

    char msg[192]{};
    sprintf_s(
        msg,
        "[NavGrid] terrain bake ok=%u walkable=%u seeds=%zu baseY=%.2f band=%.2f normalY=%.2f step=%.2f\n",
        bBaked ? 1u : 0u,
        m_pNavGrid->CountWalkableCells(),
        seeds.size(),
        desc.playableBaseY,
        desc.playableHeightBand,
        desc.minNormalY,
        desc.maxStepHeight);
    OutputDebugStringA(msg);
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
        OutputDebugStringA("[NavGrid] path grid inflate failed\n");
        return;
    }

    char msg[192]{};
    sprintf_s(
        msg,
        "[NavGrid] path grid inflated radius=0.50 authored=%u path=%u authoredHash=%08X pathHash=%08X\n",
        m_pNavGrid->CountWalkableCells(),
        m_pPathNavGrid->CountWalkableCells(),
        m_pNavGrid->ComputeContentHash(),
        m_pPathNavGrid->ComputeContentHash());
    OutputDebugStringA(msg);
}

bool_t CScene_InGame::TryProjectToMapSurface(Vec3& ioPos, f32_t fYOffset) const
{
    const Vec3 before = ioPos;
    if (!m_pMapSurfaceSampler)
    {
        static u32_t s_projectNoSamplerTraceCount = 0;
        if (false && s_projectNoSamplerTraceCount < 64u)
        {
            char msg[256]{};
            sprintf_s(
                msg,
                "[YawTrace][ProjectMapSurface] result=no-sampler in=(%.3f,%.3f,%.3f) yOffset=%.3f\n",
                before.x,
                before.y,
                before.z,
                fYOffset);
            OutputDebugStringA(msg);
            ++s_projectNoSamplerTraceCount;
        }
        return false;
    }

    f32_t height = 0.f;
    if (!m_pMapSurfaceSampler->SampleHeight(ioPos.x, ioPos.z, height))
    {
        static u32_t s_projectFailTraceCount = 0;
        if (false && s_projectFailTraceCount < 128u)
        {
            char msg[256]{};
            sprintf_s(
                msg,
                "[YawTrace][ProjectMapSurface] result=sample-fail in=(%.3f,%.3f,%.3f) yOffset=%.3f\n",
                before.x,
                before.y,
                before.z,
                fYOffset);
            OutputDebugStringA(msg);
            ++s_projectFailTraceCount;
        }
        return false;
    }

    ioPos.y = height + fYOffset;
    static u32_t s_projectTraceCount = 0;
    if (false && s_projectTraceCount < 512u)
    {
        char msg[320]{};
        sprintf_s(
            msg,
            "[YawTrace][ProjectMapSurface] result=ok in=(%.3f,%.3f,%.3f) out=(%.3f,%.3f,%.3f) height=%.3f yOffset=%.3f\n",
            before.x,
            before.y,
            before.z,
            ioPos.x,
            ioPos.y,
            ioPos.z,
            height,
            fYOffset);
        if (false)
            OutputDebugStringA(msg);
        ++s_projectTraceCount;
    }
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

    static u32_t s_snapLogCount = 0;
    if (s_snapLogCount < 32u)
    {
        char msg[224]{};
        sprintf_s(
            msg,
            "[MoveTarget] snap-out blocked=(%d,%d) nearest=(%d,%d) pos=(%.2f,%.2f)->(%.2f,%.2f)\n",
            cell.x,
            cell.y,
            nearest.x,
            nearest.y,
            rawPos.x,
            rawPos.z,
            outPos.x,
            outPos.z);
        OutputDebugStringA(msg);
        ++s_snapLogCount;
    }

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
    static u32_t s_resolveEnterTraceCount = 0;
    if (false && s_resolveEnterTraceCount < 512u)
    {
        const Vec3 rawDir = WintersMath::DirectionXZ(playerPos, rawTarget, Vec3{});
        char msg[512]{};
        sprintf_s(
            msg,
            "[YawTrace][ClientResolveEnter] pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) start=(%d,%d) rawGoal=(%d,%d)\n",
            playerPos.x,
            playerPos.y,
            playerPos.z,
            rawTarget.x,
            rawTarget.y,
            rawTarget.z,
            rawDir.x,
            rawDir.z,
            start.x,
            start.y,
            rawGoal.x,
            rawGoal.y);
        OutputDebugStringA(msg);
        ++s_resolveEnterTraceCount;
    }

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

            static u32_t s_badSurfaceLogCount = 0;
            if (s_badSurfaceLogCount < 64u)
            {
                char msg[224]{};
                sprintf_s(
                    msg,
                    "[MoveTarget] reject-surface-y playerY=%.3f sampledY=%.3f delta=%.3f xz=(%.3f,%.3f)\n",
                    playerPos.y,
                    ioTarget.y,
                    surfaceDeltaY,
                    ioTarget.x,
                    ioTarget.z);
                OutputDebugStringA(msg);
                ++s_badSurfaceLogCount;
            }
            ioTarget.y = playerPos.y;
        };

    if (!pGrid->IsWalkable(start.x, start.y))
    {
        CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 8, nearestStart))
            return false;
        static u32_t s_startBlockedLogCount = 0;
        if (s_startBlockedLogCount < 64u)
        {
            char msg[224]{};
            sprintf_s(
                msg,
                "[MoveTarget] start-blocked player=(%.2f,%.2f) cell=(%d,%d) nearest=(%d,%d)\n",
                playerPos.x,
                playerPos.z,
                start.x,
                start.y,
                nearestStart.x,
                nearestStart.y);
            OutputDebugStringA(msg);
            ++s_startBlockedLogCount;
        }
        start = nearestStart;
    }

    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[MoveTarget] reject=out-of-nav-bounds goal=(%d,%d) origin=(%.2f,%.2f)\n",
            rawGoal.x,
            rawGoal.y,
            pGrid->Get_OriginX(),
            pGrid->Get_OriginZ());
        OutputDebugStringA(msg);
        return false;
    }

    const bool_t bRawGoalWalkable = pGrid->IsWalkable(rawGoal.x, rawGoal.y);
    const bool_t bRawSegmentWalkable = pGrid->SegmentWalkable(playerPos, rawTarget, 0.f);
    static u32_t s_resolveCheckTraceCount = 0;
    if (false && s_resolveCheckTraceCount < 512u)
    {
        const Vec3 rawDir = WintersMath::DirectionXZ(playerPos, rawTarget, Vec3{});
        char msg[640]{};
        sprintf_s(
            msg,
            "[YawTrace][ClientResolveCheck] pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) start=(%d,%d) goal=(%d,%d) rawGoalWalkable=%u segmentWalkable=%u\n",
            playerPos.x,
            playerPos.y,
            playerPos.z,
            rawTarget.x,
            rawTarget.y,
            rawTarget.z,
            rawDir.x,
            rawDir.z,
            start.x,
            start.y,
            rawGoal.x,
            rawGoal.y,
            bRawGoalWalkable ? 1u : 0u,
            bRawSegmentWalkable ? 1u : 0u);
        if (false)
            OutputDebugStringA(msg);
        ++s_resolveCheckTraceCount;
    }

    if (bRawGoalWalkable && bRawSegmentWalkable)
    {
        outTarget = rawTarget;
        ProjectMoveTarget(outTarget);
        if (pOutFirstWaypoint)
            *pOutFirstWaypoint = outTarget;
        const Vec3 rawDir = WintersMath::DirectionXZ(playerPos, rawTarget, Vec3{});
        const Vec3 resolvedDir = WintersMath::DirectionXZ(playerPos, outTarget, Vec3{});
        const f32_t rawVsResolvedDot =
            rawDir.x * resolvedDir.x + rawDir.z * resolvedDir.z;
        const bool_t bRawOpposesResolved =
            (rawDir.x != 0.f || rawDir.z != 0.f) &&
            (resolvedDir.x != 0.f || resolvedDir.z != 0.f) &&
            rawVsResolvedDot < -0.10f;
        char msg[768]{};
        sprintf_s(
            msg,
            "[YawTrace][ClientResolve] mode=direct pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) first=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) resolvedDir=(%.3f,%.3f) rawVsResolvedDot=%.4f rawOppResolved=%u start=(%d,%d) goal=(%d,%d)\n",
            playerPos.x,
            playerPos.y,
            playerPos.z,
            rawTarget.x,
            rawTarget.y,
            rawTarget.z,
            outTarget.x,
            outTarget.y,
            outTarget.z,
            outTarget.x,
            outTarget.y,
            outTarget.z,
            rawDir.x,
            rawDir.z,
            resolvedDir.x,
            resolvedDir.z,
            rawVsResolvedDot,
            bRawOpposesResolved ? 1u : 0u,
            start.x,
            start.y,
            rawGoal.x,
            rawGoal.y);
        if (false)
            OutputDebugStringA(msg);
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
        char msg[160]{};
        sprintf_s(
            msg,
            "[MoveTarget] raw blocked goal=(%d,%d) reject=no-reachable-cell\n",
            rawGoal.x,
            rawGoal.y);
        OutputDebugStringA(msg);
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    ProjectMoveTarget(outTarget);

    if (pOutFirstWaypoint)
    {
        *pOutFirstWaypoint = outTarget;
        const std::vector<CNavGrid::Cell> smoothedPath =
            SmoothClientMovePathCells(*pGrid, path);
        bool_t bFirstWaypointOpposed = false;
        if (smoothedPath.size() > 1)
        {
            Vec3 waypoint = pGrid->CellToWorld(
                smoothedPath[1].x,
                smoothedPath[1].y);
            ProjectMoveTarget(waypoint);

            Vec3 intentFacingTarget = rawTarget;
            ProjectMoveTarget(intentFacingTarget);
            bFirstWaypointOpposed = IsFacingCandidateOpposedToIntent(
                playerPos,
                intentFacingTarget,
                waypoint);
            *pOutFirstWaypoint = bFirstWaypointOpposed ? intentFacingTarget : waypoint;
        }

        const Vec3 rawDir = WintersMath::DirectionXZ(playerPos, rawTarget, Vec3{});
        const Vec3 resolvedDir = WintersMath::DirectionXZ(playerPos, outTarget, Vec3{});
        const Vec3 firstDir = WintersMath::DirectionXZ(playerPos, *pOutFirstWaypoint, Vec3{});
        const f32_t rawVsResolvedDot =
            rawDir.x * resolvedDir.x + rawDir.z * resolvedDir.z;
        const f32_t rawVsFirstDot =
            rawDir.x * firstDir.x + rawDir.z * firstDir.z;
        const bool_t bRawOpposesResolved =
            (rawDir.x != 0.f || rawDir.z != 0.f) &&
            (resolvedDir.x != 0.f || resolvedDir.z != 0.f) &&
            rawVsResolvedDot < -0.10f;
        const bool_t bRawOpposesFirst =
            (rawDir.x != 0.f || rawDir.z != 0.f) &&
            (firstDir.x != 0.f || firstDir.z != 0.f) &&
            rawVsFirstDot < -0.10f;

        char msg[1024]{};
        sprintf_s(
            msg,
            "[YawTrace][ClientResolve] mode=path pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) first=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) resolvedDir=(%.3f,%.3f) firstDir=(%.3f,%.3f) rawVsResolvedDot=%.4f rawVsFirstDot=%.4f rawOppResolved=%u rawOppFirst=%u opposed=%u start=(%d,%d) goal=(%d,%d) corrected=(%d,%d) rawPath=%zu\n",
            playerPos.x,
            playerPos.y,
            playerPos.z,
            rawTarget.x,
            rawTarget.y,
            rawTarget.z,
            outTarget.x,
            outTarget.y,
            outTarget.z,
            pOutFirstWaypoint->x,
            pOutFirstWaypoint->y,
            pOutFirstWaypoint->z,
            rawDir.x,
            rawDir.z,
            resolvedDir.x,
            resolvedDir.z,
            firstDir.x,
            firstDir.z,
            rawVsResolvedDot,
            rawVsFirstDot,
            bRawOpposesResolved ? 1u : 0u,
            bRawOpposesFirst ? 1u : 0u,
            bFirstWaypointOpposed ? 1u : 0u,
            start.x,
            start.y,
            rawGoal.x,
            rawGoal.y,
            resolved.x,
            resolved.y,
            path.size());
        if (false)
            OutputDebugStringA(msg);
    }

    if (false && (resolved.x != rawGoal.x || resolved.y != rawGoal.y))
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[MoveTarget] raw goal=(%d,%d) bfs-corrected=(%d,%d) path=%zu\n",
            rawGoal.x,
            rawGoal.y,
            resolved.x,
            resolved.y,
            path.size());
        OutputDebugStringA(msg);
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
    const Vec3 hitY0 = ground;
    const Vec3 beforeProject = ground;
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

    static u32_t s_mouseGroundTraceCount = 0;
    if (s_mouseGroundTraceCount < 512u)
    {
        const Vec3 camEye = m_pCamera->GetEye();
        const Vec3 camAt = m_pCamera->GetAt();
        const Vec3 playerPos = m_pPlayerTransform
            ? m_pPlayerTransform->GetPosition()
            : Vec3{};
        char msg[1024]{};
        sprintf_s(
            msg,
            "[YawTrace][MouseGround] mouse=(%d,%d) camEye=(%.3f,%.3f,%.3f) camAt=(%.3f,%.3f,%.3f) rayOrigin=(%.3f,%.3f,%.3f) rayDir=(%.3f,%.3f,%.3f) hitY0=(%.3f,%.3f,%.3f) beforeProject=(%.3f,%.3f,%.3f) afterProject=(%.3f,%.3f,%.3f) player=(%.3f,%.3f,%.3f) projected=%u\n",
            static_cast<int>(input.GetMouseX()),
            static_cast<int>(input.GetMouseY()),
            camEye.x,
            camEye.y,
            camEye.z,
            camAt.x,
            camAt.y,
            camAt.z,
            ray.Origin.x,
            ray.Origin.y,
            ray.Origin.z,
            ray.Dir.x,
            ray.Dir.y,
            ray.Dir.z,
            hitY0.x,
            hitY0.y,
            hitY0.z,
            beforeProject.x,
            beforeProject.y,
            beforeProject.z,
            ground.x,
            ground.y,
            ground.z,
            playerPos.x,
            playerPos.y,
            playerPos.z,
            bProjected ? 1u : 0u);
        OutputDebugStringA(msg);
        ++s_mouseGroundTraceCount;
    }
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
    if (m_fFlashCooldownLeft > 0.f) return;
    if (!m_pPlayerTransform || !m_pCamera) return;

    const Vec3 cursor = ResolveMouseMapSurfacePos();
    const Vec3 origin = m_pPlayerTransform->GetPosition();
    const f32_t dx = cursor.x - origin.x;
    const f32_t dz = cursor.z - origin.z;
    const f32_t lenSq = dx * dx + dz * dz;
    if (lenSq < 0.001f) return;

    const f32_t len = std::sqrt(lenSq);
    const f32_t useLen = (len > m_fFlashRange) ? m_fFlashRange : len;
    const f32_t nx = dx / len;
    const f32_t nz = dz / len;
    Vec3 dest{ origin.x + nx * useLen, origin.y, origin.z + nz * useLen };
    (void)TryProjectToMapSurface(dest, 0.05f);

    SetPlayerPosition(dest);

    m_fFlashCooldownLeft = m_fFlashCooldown;

    char dbg[160]{};
    sprintf_s(dbg, "[Flash] (%.1f,%.1f) -> (%.1f,%.1f) len=%.2f\n",
        origin.x, origin.z, dest.x, dest.z, useLen);
    ::OutputDebugStringA(dbg);
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
    char msg[192]{};
    sprintf_s(
        msg,
        "[NavGrid] %u structures marked as blocked walkable=%u hash=%08X\n",
        iCount,
        m_pNavGrid->CountWalkableCells(),
        m_pNavGrid->ComputeContentHash());
    OutputDebugStringA(msg);
    RebuildClientPathNavGrid();
}
