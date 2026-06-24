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
#include "Manager/AmbientProp_Manager.h"
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
#include "Renderer/RHISceneRenderer.h"

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

CScene_InGame::CScene_InGame() = default;

CScene_InGame::CScene_InGame(const wstring_t& replayPath)
    : m_bReplayPlaybackMode(true)
    , m_strReplayPath(replayPath)
{
}

CScene_InGame::~CScene_InGame() = default;

namespace
{
    bool_t ShouldRunInGameSkillSmoke()
    {
        return HasCommandLineToken(L"--banpick-smoke")
            && !HasCommandLineToken(L"--smoke-no-skill");
    }

    bool_t IsValidChampionId(eChampion champion)
    {
        return champion != eChampion::END && champion != eChampion::NONE;
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

Vec3 CScene_InGame::GetPlayerForward() const
{
    const f32_t yaw =
        GetPlayerYaw() -
        ClientData::ResolveChampionModelYawOffset(GetPlayerChampionId());
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

    const bool_t bPlayerDead = IsPlayerDead();
    if (bPlayerDead)
        ApplyPlayerDeathInputLock();

    bool bSkipGroundMove = false;
    if (!m_bReplayPlaybackMode && !bPlayerDead)
    {
        UpdateTargeting();
        UpdateCombatInput(bSkipGroundMove);
    }

    if (!bPlayerDead && ShouldRunInGameSkillSmoke())
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
    if (m_fNetworkMoveInputLockTimer > 0.f) m_fNetworkMoveInputLockTimer -= dt;

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

    if (!bPlayerDead)
        UpdateDash(dt);

    if (bPlayerDead && m_ActiveSkill.bActive)
    {
        ClearActiveSkillRuntime();
    }
    else if (m_bNetworkAuthoritativeGameplay && m_ActiveSkill.bActive)
    {
        ClearActiveSkillRuntime();
    }
    else if (!bPlayerDead && m_ActiveSkill.bActive && m_pPlayerRenderer)
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
    if (!bPlayerDead)
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

    if (!m_bReplayPlaybackMode && !bPlayerDead)
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

    CAmbientProp_Manager::Get()->Tick(dt);

    CJungle_Manager::Get()->Update(dt);

    if (m_pFxSystem)          m_pFxSystem->Update(m_World, dt);
    if (m_pFxBeamSystem)      m_pFxBeamSystem->Update(m_World, dt);
    if (m_pFxMeshSystem)      m_pFxMeshSystem->Update(m_World, dt);

    if (!bPlayerDead)
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

    const bool_t bAttackHold = !bPlayerDead && CInput::Get().IsKeyDown('A');
    SetShowAttackRange(bAttackHold);

    const EntityID hoveredEntity = GetHoveredEntity();
    CGameInstance::Get()->UI_Set_AttackMode(bAttackHold);
    CGameInstance::Get()->UI_Set_EnemyHoverCursor(
        !bPlayerDead &&
        hoveredEntity != NULL_ENTITY &&
        IsEnemyOfPlayer(hoveredEntity));
}

void CScene_InGame::OnLateUpdate(f32_t /*dt*/)
{
}
