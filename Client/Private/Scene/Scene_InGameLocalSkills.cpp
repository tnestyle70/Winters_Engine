// Scene_InGameLocalSkills.cpp — CScene_InGame의 클라 로컬 예측(스킬 디스패치/대시/플레이어 컨트롤/플래시) 책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp에서 verbatim 이동. 동작/시그니처/호출순서 불변.
// compass 허용 local-only prediction. snapshot-apply(Scene_InGameNetwork.cpp)와 절대 같은 파일에 두지 않는다.
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
    constexpr f32_t kPlayerAvoidancePadding = 0.05f;

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

bool_t CScene_InGame::IssuePlayerMoveTarget(
    const Vec3& rawGround,
    bool_t bNetworkActive,
    bool_t bSpawnIndicator)
{
    if (IsPlayerDead())
        return false;

    Vec3 ground = rawGround;
    Vec3 resolvedGround = ground;
    Vec3 predictedFacingTarget = ground;

    const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
    if (!bValidGround)
        return false;

    if (!TryResolveWalkableMoveTarget(ground, resolvedGround, &predictedFacingTarget))
        return false;

    Vec3 moveIntent = ground;
    predictedFacingTarget = moveIntent;

    if (bSpawnIndicator)
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
            const auto& input = CInput::Get();
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
                1u,
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

    return true;
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
    if (IsPlayerDead())
    {
        ApplyPlayerDeathInputLock();
        return;
    }

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
        Vec3 minimapCameraTarget{};
        const bool_t bMinimapCameraJumpPressed =
            !bImGuiMouse &&
            input.IsLButtonPressed() &&
            TryResolveMinimapClickTarget(minimapCameraTarget);
        if (bMinimapCameraJumpPressed && m_pCamera)
            m_pCamera->JumpToWorldXZ(minimapCameraTarget);

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
            const Vec3 ground = ResolveMouseMapSurfacePos();
            const bool_t bPressedMoveIntent =
                input.IsRButtonPressed();
            const bool_t bAcceptedMoveTarget =
                IssuePlayerMoveTarget(ground, bNetworkActive, bPressedMoveIntent);

            if (!bAcceptedMoveTarget && bPressedMoveIntent)
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

void CScene_InGame::UpdateDash(f32_t dt)
{
    if (IsPlayerDead())
    {
        m_bDashActive = false;
        m_fDashElapsed = 0.f;
        m_DashTargetEntity = NULL_ENTITY;
        return;
    }

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
    if (IsPlayerDead())
        return false;

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

void CScene_InGame::TriggerFlash()
{
    if (IsPlayerDead())
        return;

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
