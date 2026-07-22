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
#include "GamePlay/LoLMatchContextRuntime.h"
#include "ECS/Components/CoreComponents.h"   // ColliderComponent
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/NavigationThrottleSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/ConcealmentVolumeIndex.h"
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
#include "UI/AttackSpeedLab.h"
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
#include "GameObject/Champion/Viego/Viego_FxPresets.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Irelia/Irelia_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "GameObject/Champion/Yone/Yone_Components.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "GameObject/ChampionSpawnService.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/ChampionSoundCatalog.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/SkillDefVisualDataAdapter.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
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
#include "Shared/GameSim/Components/GameplayComponents.h"   // Stun/Slow/Disarm
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
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
    constexpr f32_t kRecallVisualScale = 1.f / 3.f;

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

    // 챔피언 액션/사망 사운드 — ChampionSoundCatalog 에 매핑된 슬롯만 재생
    bool_t IsNetworkChampionSoundAudible(
        CWorld& world,
        EntityID source,
        EntityID listener,
        eTeam listenerTeam)
    {
        if (source == listener && source != NULL_ENTITY)
            return true;
        if (source == NULL_ENTITY || listener == NULL_ENTITY ||
            !world.IsAlive(source) || !world.IsAlive(listener) ||
            !world.HasComponent<TransformComponent>(source) ||
            !world.HasComponent<TransformComponent>(listener) ||
            !world.HasComponent<VisibilityComponent>(source))
        {
            return false;
        }

        const u8_t team = static_cast<u8_t>(listenerTeam);
        if (team >= static_cast<u8_t>(eTeam::TEAM_END))
            return false;

        const u8_t listenerVisibilityBit = static_cast<u8_t>(1u << team);
        const VisibilityComponent& visibility =
            world.GetComponent<VisibilityComponent>(source);
        if ((visibility.teamVisibilityMask & listenerVisibilityBit) == 0u)
            return false;

        const Vec3 sourcePosition =
            world.GetComponent<TransformComponent>(source).GetPosition();
        const Vec3 listenerPosition =
            world.GetComponent<TransformComponent>(listener).GetPosition();
        const f32_t maxDistance =
            CChampionSoundCatalog::Instance().GetMaxAudibleDistance();
        const f32_t dx = sourcePosition.x - listenerPosition.x;
        const f32_t dz = sourcePosition.z - listenerPosition.z;
        return dx * dx + dz * dz <= maxDistance * maxDistance;
    }

    void PlayNetworkChampionSound(eChampion champion, eChampionSoundSlot slot)
    {
        CChampionSoundCatalog& catalog = CChampionSoundCatalog::Instance();
        const std::wstring* pKey = catalog.Find(champion, slot);
        if (!pKey)
        {
            // 카탈로그 미스는 무음으로 삼켜지면 배선 결함과 구분 불가 — bounded 진단 로그.
            static int s_missLogBudget = 16;
            if (s_missLogBudget > 0)
            {
                --s_missLogBudget;
                char buffer[128]{};
                sprintf_s(buffer,
                    "[Sound] champion sound catalog miss champion=%u slot=%u\n",
                    static_cast<unsigned>(champion), static_cast<unsigned>(slot));
                OutputDebugStringA(buffer);
            }
            return;
        }
        CGameInstance::Get()->PlayEffect(*pKey, catalog.GetVolume());
    }

    void PlayNetworkChampionVoice(
        eChampion champion,
        eChampionVoiceSlot slot,
        u32_t selector)
    {
        u32_t mixedSelector = selector;
        mixedSelector ^= static_cast<u32_t>(champion) * 0x9E3779B9u;
        mixedSelector ^= static_cast<u32_t>(slot) * 0x85EBCA6Bu;
        mixedSelector ^= mixedSelector >> 16u;
        mixedSelector *= 0x7FEB352Du;
        mixedSelector ^= mixedSelector >> 15u;

        CChampionSoundCatalog& catalog = CChampionSoundCatalog::Instance();
        const std::wstring* pKey = catalog.SelectVoice(champion, slot, mixedSelector);
        if (!pKey)
            return;

        CGameInstance::Get()->PlaySoundOn(
            *pKey,
            eSoundChannel::PlayerVoice,
            catalog.GetVoiceVolume());
    }

    f32_t SelectMoveVoiceDelaySec(u32_t selector)
    {
        CChampionSoundCatalog& catalog = CChampionSoundCatalog::Instance();
        const f32_t minDelaySec = catalog.GetMoveVoiceDelayMinSec();
        const f32_t maxDelaySec = catalog.GetMoveVoiceDelayMaxSec();
        if (maxDelaySec <= minDelaySec)
            return minDelaySec;

        u32_t mixedSelector = selector ^ 0xA511E9B3u;
        mixedSelector ^= mixedSelector >> 16u;
        mixedSelector *= 0x7FEB352Du;
        mixedSelector ^= mixedSelector >> 15u;
        const f32_t normalized =
            static_cast<f32_t>(mixedSelector & 0xFFFFu) / 65535.f;
        return minDelaySec + (maxDelaySec - minDelaySec) * normalized;
    }

    void PlayNetworkChampionActionSound(eChampion champion, u16_t actionId, u32_t actionSequence)
    {
        switch (static_cast<eActionStateId>(actionId))
        {
        case eActionStateId::BasicAttack:
            PlayNetworkChampionSound(champion, eChampionSoundSlot::BasicAttack);
            PlayNetworkChampionVoice(champion, eChampionVoiceSlot::BasicAttack, actionSequence);
            break;
        case eActionStateId::SkillQ:
            PlayNetworkChampionSound(champion, eChampionSoundSlot::SkillQ);
            PlayNetworkChampionVoice(champion, eChampionVoiceSlot::SkillQ, actionSequence);
            break;
        case eActionStateId::SkillW:
            PlayNetworkChampionSound(champion, eChampionSoundSlot::SkillW);
            PlayNetworkChampionVoice(champion, eChampionVoiceSlot::SkillW, actionSequence);
            break;
        case eActionStateId::SkillE:
            PlayNetworkChampionSound(champion, eChampionSoundSlot::SkillE);
            PlayNetworkChampionVoice(champion, eChampionVoiceSlot::SkillE, actionSequence);
            break;
        case eActionStateId::SkillR:
            PlayNetworkChampionSound(champion, eChampionSoundSlot::SkillR);
            PlayNetworkChampionVoice(champion, eChampionVoiceSlot::SkillR, actionSequence);
            break;
        default:
            // Recall/DeathStart/ViegoConsumeSoul — 사망은 사망 게이트에서 재생
            break;
        }
    }

    bool_t IsValidPresentationChampion(eChampion champion)
    {
        return champion != eChampion::NONE && champion != eChampion::END;
    }

    eChampion ResolveBasePresentationChampion(
        CWorld& world,
        EntityID entity,
        eChampion baseChampion)
    {
        if (world.HasComponent<FormOverrideComponent>(entity))
        {
            const auto& form = world.GetComponent<FormOverrideComponent>(entity);
            if (form.bActive && IsValidPresentationChampion(form.visualChampion))
                return form.visualChampion;
        }
        return baseChampion;
    }

    eChampion ResolveActionPresentationChampion(
        CWorld& world,
        EntityID entity,
        eChampion baseChampion,
        const ActionStateComponent& action)
    {
        if (baseChampion == eChampion::SYLAS &&
            IsValidPresentationChampion(action.sourceChampion) &&
            action.sourceChampion != eChampion::SYLAS &&
            static_cast<eActionStateId>(action.actionId) == eActionStateId::SkillR)
        {
            return eChampion::SYLAS;
        }

        if (IsValidPresentationChampion(action.sourceChampion))
            return action.sourceChampion;

        if (static_cast<eActionStateId>(action.actionId) ==
            eActionStateId::ViegoConsumeSoul)
        {
            return eChampion::VIEGO;
        }

        if (world.HasComponent<FormOverrideComponent>(entity))
        {
            const auto& form = world.GetComponent<FormOverrideComponent>(entity);
            const u8_t slot = NetworkActionToSkillSlot(action.actionId);
            if (form.bActive &&
                IsValidPresentationChampion(form.skillChampion) &&
                slot < 8u &&
                (form.skillSlotMask & static_cast<u8_t>(1u << slot)) != 0u)
            {
                return form.skillChampion;
            }
        }

        return baseChampion;
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
        const u8_t stageIndex = stage > 0u ? static_cast<u8_t>(stage - 1u) : 0u;
        f32_t playSpeed = 1.f;
        if (const ClientData::ChampionVisualDefinition* pVisual =
            ClientData::FindChampionVisualDefinition(champion))
        {
            if (slot < ClientData::kVisualSkillSlotCount &&
                stageIndex < pVisual->skills[slot].stageCount &&
                stageIndex < ClientData::kVisualSkillStageCount)
            {
                playSpeed = pVisual->skills[slot].stages[stageIndex].animationPlaybackSpeed;
            }
        }
        else if (pDef)
        {
            playSpeed = (stage >= 2u && pDef->stage2VisualPlaySpeed > 0.01f)
                ? pDef->stage2VisualPlaySpeed
                : pDef->visualPlaySpeed;
        }

        return (std::isfinite(playSpeed) && playSpeed > 0.01f) ? playSpeed : 1.f;
    }

    f32_t ResolveNetworkActionRecoveryFrame(
        eChampion champion,
        const SkillDef* pDef,
        u16_t actionId,
        u8_t stage)
    {
        const u8_t slot = NetworkActionToSkillSlot(actionId);
        const u8_t stageIndex = stage > 0u ? static_cast<u8_t>(stage - 1u) : 0u;
        if (const ClientData::ChampionVisualDefinition* pVisual =
            ClientData::FindChampionVisualDefinition(champion))
        {
            if (slot < ClientData::kVisualSkillSlotCount &&
                stageIndex < pVisual->skills[slot].stageCount &&
                stageIndex < ClientData::kVisualSkillStageCount)
            {
                return pVisual->skills[slot].stages[stageIndex].recoveryFrame;
            }
        }

        if (!pDef)
            return 0.f;
        return stage >= 2u
            ? pDef->stage2VisualRecoveryFrame
            : pDef->visualRecoveryFrame;
    }

    f32_t ResolveNetworkActionCommandLockSec(const SkillDef* pDef, u8_t stage)
    {
        if (!pDef)
            return 0.f;
        const f32_t seconds = stage >= 2u
            ? pDef->stage2CommandLockSec
            : pDef->commandLockSec;
        return std::isfinite(seconds) && seconds > 0.f ? seconds : 0.f;
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
            GetDefaultChampionSkillTiming(champion, slot, stage);

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
        const std::string& animName,
        f32_t fAttackSpeedScale,
        f32_t fAnimCorrectionScale)
    {
        if (!std::isfinite(fAttackSpeedScale) || fAttackSpeedScale <= 0.01f)
            fAttackSpeedScale = 1.f;
        if (!std::isfinite(fAnimCorrectionScale) || fAnimCorrectionScale <= 0.01f)
            fAnimCorrectionScale = 1.f;

        const f32_t lockDurationSec =
            ResolveNetworkActionLockDurationSec(champion, pDef, actionId, stage);
        const f32_t scaledLockDurationSec = lockDurationSec / fAttackSpeedScale;
        const bool_t bLoopAction = pDef && (stage >= 2u
            ? pDef->bStage2PresentationLoopWhileActive
            : pDef->bPresentationLoopWhileActive);
        if (bLoopAction ||
            !render.pRenderer ||
            animName.empty())
        {
            return scaledLockDurationSec;
        }

        const f32_t animDurationSec =
            render.pRenderer->GetAnimationDurationSecondsByName(animName);
        if (!std::isfinite(animDurationSec) || animDurationSec <= 0.01f)
            return scaledLockDurationSec;

        const f32_t effectivePlaySpeed =
            ResolveNetworkActionPlaySpeed(champion, pDef, actionId, stage) *
            fAttackSpeedScale *
            fAnimCorrectionScale;
        const f32_t visualDurationSec = animDurationSec / effectivePlaySpeed;
        if (!std::isfinite(visualDurationSec) || visualDurationSec <= 0.01f)
            return scaledLockDurationSec;

        const f32_t recoveryFrame = ResolveNetworkActionRecoveryFrame(
            champion, pDef, actionId, stage);
        const f32_t recoveryDurationSec =
            render.pRenderer->GetAnimationTimeSecondsByFrameByName(
                animName, recoveryFrame) / effectivePlaySpeed;
        if (!std::isfinite(recoveryDurationSec) || recoveryDurationSec <= 0.01f)
            return (std::min)(scaledLockDurationSec, visualDurationSec);

        const f32_t commandLockDurationSec =
            ResolveNetworkActionCommandLockSec(pDef, stage) / fAttackSpeedScale;
        return std::clamp(
            (std::max)(commandLockDurationSec, recoveryDurationSec),
            0.01f,
            scaledLockDurationSec);
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
        const SkillDef* pSkillDef,
        u8_t stage)
    {
        if (!pSkillDef)
            return false;

        return stage >= 2u
            ? pSkillDef->bStage2PresentationLoopWhileActive
            : pSkillDef->bPresentationLoopWhileActive;
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
        if (champion == eChampion::SYLAS &&
            static_cast<eActionStateId>(action.actionId) == eActionStateId::BasicAttack &&
            bStage2)
        {
            return ResolveNetworkAnimName(*pChampionDef, "skinned_mesh_sylas_attack_passive");
        }

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
        if (!ShouldLoopNetworkAction(pSkillDef, action.stage) ||
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
            pCurrentAnim->GetName().find(animName) != std::string::npos)
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

void CScene_InGame::SyncNetworkChampionLevelUpFx()
{
    u8_t localTeam = static_cast<u8_t>(m_PlayerTeam);
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.IsAlive(m_PlayerEntity) &&
        m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        localTeam = static_cast<u8_t>(
            m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team);
    }

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID entity,
            ChampionComponent& champion,
            TransformComponent& transform)
        {
            const auto [it, bInserted] =
                m_NetworkChampionLevels.try_emplace(entity, champion.level);
            if (bInserted)
                return;

            const u8_t previousLevel = it->second;
            it->second = champion.level;
            if (champion.level <= previousLevel)
                return;

            const bool_t bVisibleAlly =
                static_cast<u8_t>(champion.team) == localTeam &&
                UI::IsRenderableForLocal(
                    m_World,
                    entity,
                    localTeam,
                    false);
            if (!bVisibleAlly ||
                UI::IsKalistaCarried(m_World, entity) ||
                m_World.HasComponent<YoneSoulPresentationTag>(entity))
            {
                return;
            }

            if (m_World.HasComponent<HealthComponent>(entity))
            {
                const HealthComponent& health =
                    m_World.GetComponent<HealthComponent>(entity);
                if (health.bIsDead || health.fCurrent <= 0.f)
                    return;
            }

            FxCueContext cue{};
            cue.attachTo = entity;
            cue.vWorldPos = transform.GetPosition();
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = 1.f;

            const eChampion championId = champion.id;
            const StatComponent fallbackStat =
                BuildDefaultChampionStat(championId);
            const f32_t recallRadius =
                GameplayQuery::ResolveAttackRangePreviewRadius(
                    m_World,
                    entity,
                    championId,
                    fallbackStat.attackRange,
                    m_bNetworkAuthoritativeGameplay);
            const f32_t diameter =
                recallRadius * 2.f * kRecallVisualScale;
            cue.bOverrideSize = true;
            cue.fWidthOverride = diameter;
            cue.fHeightOverride = diameter;
            const EntityID fxEntity =
                CFxCuePlayer::Play(m_World, "Recall.Channel", cue);

#if defined(_DEBUG)
            static u32_t s_levelUpFxTraceCount = 0u;
            if (s_levelUpFxTraceCount < 32u)
            {
                char message[224]{};
                sprintf_s(
                    message,
                    "[LevelUpFx] owner=%u previous=%u current=%u fx=%u follow=1 lifetime=1.00 diameter=%.2f\n",
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(previousLevel),
                    static_cast<u32_t>(champion.level),
                    static_cast<u32_t>(fxEntity),
                    diameter);
                OutputDebugStringA(message);
                ++s_levelUpFxTraceCount;
            }
#else
            (void)fxEntity;
#endif
        });
}

void CScene_InGame::StartNetworkRecallFx(EntityID entity)
{
    StopNetworkRecallFx(entity);
    if (entity == NULL_ENTITY ||
        !m_World.IsAlive(entity) ||
        !m_World.HasComponent<TransformComponent>(entity))
    {
        return;
    }

    FxCueContext cue{};
    cue.attachTo = entity;
    cue.vWorldPos = m_World.GetComponent<TransformComponent>(entity).GetPosition();
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = kRecallDurationSec;

    eChampion champion = eChampion::NONE;
    if (m_World.HasComponent<ChampionComponent>(entity))
        champion = m_World.GetComponent<ChampionComponent>(entity).id;
    const StatComponent fallbackStat = BuildDefaultChampionStat(champion);
    const f32_t recallRadius = GameplayQuery::ResolveAttackRangePreviewRadius(
        m_World,
        entity,
        champion,
        fallbackStat.attackRange,
        m_bNetworkAuthoritativeGameplay);
    const f32_t recallDiameter = recallRadius * 2.f * kRecallVisualScale;
    cue.bOverrideSize = true;
    cue.fWidthOverride = recallDiameter;
    cue.fHeightOverride = recallDiameter;

    const EntityID fxEntity =
        CFxCuePlayer::Play(m_World, "Recall.Channel", cue);
    if (fxEntity != NULL_ENTITY && m_World.IsAlive(fxEntity))
        m_NetworkRecallFxHandles[entity] = m_World.GetEntityHandle(fxEntity);

#if defined(_DEBUG)
    bool_t bTextureReady = false;
    if (fxEntity != NULL_ENTITY &&
        m_World.IsAlive(fxEntity) &&
        m_World.HasComponent<FxBillboardComponent>(fxEntity) &&
        m_pFxSystem)
    {
        const auto& recallFx =
            m_World.GetComponent<FxBillboardComponent>(fxEntity);
        bTextureReady = m_pFxSystem->PreloadTextureResource(recallFx.texturePath);
    }

    static u32_t s_recallFxSpawnTraceCount = 0u;
    if (s_recallFxSpawnTraceCount < 32u)
    {
        char message[256]{};
        sprintf_s(
            message,
            "[RecallFx] spawn owner=%u fx=%u spawned=%u texture=%u diameter=%.2f pos=(%.2f,%.2f,%.2f)\n",
            static_cast<u32_t>(entity),
            static_cast<u32_t>(fxEntity),
            fxEntity != NULL_ENTITY ? 1u : 0u,
            bTextureReady ? 1u : 0u,
            recallDiameter,
            cue.vWorldPos.x,
            cue.vWorldPos.y,
            cue.vWorldPos.z);
        OutputDebugStringA(message);
        ++s_recallFxSpawnTraceCount;
    }
#endif
}

void CScene_InGame::StopNetworkRecallFx(EntityID entity)
{
    const auto it = m_NetworkRecallFxHandles.find(entity);
    if (it == m_NetworkRecallFxHandles.end())
        return;

#if defined(_DEBUG)
    static u32_t s_recallFxStopTraceCount = 0u;
    if (s_recallFxStopTraceCount < 32u)
    {
        char message[160]{};
        sprintf_s(
            message,
            "[RecallFx] stop owner=%u alive=%u\n",
            static_cast<u32_t>(entity),
            m_World.IsAlive(it->second) ? 1u : 0u);
        OutputDebugStringA(message);
        ++s_recallFxStopTraceCount;
    }
#endif

    if (m_World.IsAlive(it->second))
        m_World.DestroyEntity(it->second);
    m_NetworkRecallFxHandles.erase(it);
}

void CScene_InGame::ClearNetworkRecallFx()
{
    for (const auto& [entity, handle] : m_NetworkRecallFxHandles)
    {
        (void)entity;
        if (m_World.IsAlive(handle))
            m_World.DestroyEntity(handle);
    }
    m_NetworkRecallFxHandles.clear();
}

void CScene_InGame::InitializeNetworkSession()
{
    const MatchContext& gameContext = Client::CLoLMatchContextRuntime::Instance().Context();
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
            const CGameSessionClient::ServerEndpoint endpoint =
                CGameSessionClient::ResolveServerEndpoint();
            if (!endpoint.bTransportValid)
            {
                Winters::DevSmoke::Log(
                    "[Scene_InGame] invalid --net-transport; expected tcp or udp; network disabled.\n");
            }
            else
            {
                m_pNetwork = CClientNetwork::Create(endpoint.transport);
                m_pNetworkView = m_pNetwork.get();
            }
        }
    }

    m_pSnapshotApplier = CSnapshotApplier::Create();
    m_pEventApplier = CEventApplier::Create();
    m_pCommandSerializer = CCommandSerializer::Create();

    if (m_pSnapshotApplier)
        m_pSnapshotApplier->SetEventApplier(m_pEventApplier.get());

    if (m_pSnapshotApplier)
    {
        m_pSnapshotApplier->SetOnNewEntityCallback(
            [this](u32_t netId, u8_t championId, u8_t team) -> EntityID
            {
                (void)netId;
                const eChampion champion =
                    static_cast<eChampion>(championId);
                const EntityID entity = SpawnChampionEntity(
                    champion,
                    static_cast<eTeam>(team));
                if (entity != NULL_ENTITY)
                    AssignPureECSChampionAlias(champion, entity);
                return entity;
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
                const bool_t bAttached = CChampionSpawnService::AttachVisual(
                    spawnContext,
                    entity,
                    static_cast<eChampion>(championId));
                if (!bAttached)
                    return;

                StopNetworkRecallFx(entity);
                m_NetworkActionAnimStates.erase(entity);
                if (entity == m_PlayerEntity)
                    BindPlayerToECSChampion(entity);

                if (championId == static_cast<u8_t>(eChampion::VIEGO) &&
                    m_pEventApplier)
                {
                    m_pEventApplier->RetryCurrentActionVisual(
                        m_World,
                        entity,
                        static_cast<u16_t>(eActionStateId::SkillR));
                }
            });
        m_pSnapshotApplier->SetOnRemoveEntityCallback(
            [this](EntityID entity)
            {
                Viego::Fx::StopSoulIdle(m_World, entity);
                if (entity == m_PlayerEntity)
                {
                    m_PlayerEntity = NULL_ENTITY;
                    m_pPlayerRenderer = nullptr;
                    m_pPlayerTransform = nullptr;
                    m_bPlayerVoiceMoveInitialized = false;
                    m_fPlayerVoiceMoveDelayRemainingSec = 0.f;
                }
                ClearPureECSChampionAlias(entity);
                UI::CAttackSpeedLab::OnEntityRemoved(m_World, entity);
                m_ChampionRenderers.erase(entity);
                m_NetworkChampionPrevPos.erase(entity);
                m_NetworkChampionMoveGraceSec.erase(entity);
                m_NetworkChampionMoving.erase(entity);
                m_NetworkActorInterpStates.erase(entity);
                StopNetworkRecallFx(entity);
                m_NetworkActionAnimStates.erase(entity);
                m_NetworkChampionLevels.erase(entity);
            });
        m_pSnapshotApplier->SetOnTimelineRebase(
            [this](
                const SnapshotTimelineState& previous,
                const SnapshotTimelineState& next,
                u64_t serverTick)
            {
                RebaseNetworkTimeline(previous, next, serverTick);
            });
        m_pSnapshotApplier->SetOnAuthoritativeSnapshot(
            [this](
                u64_t serverTick,
                u64_t iServerTimeMs,
                u32_t lastAckedCommandSeq,
                u32_t localNetId)
            {
                CGameInstance::Get()->UI_SetMatchContextServerTimeMs(iServerTimeMs);
                OnAuthoritativeSnapshot(
                    serverTick,
                    iServerTimeMs,
                    lastAckedCommandSeq,
                    localNetId);
            });
        m_pSnapshotApplier->SetOnCommandResult(
            [this](
                u64_t serverTick,
                u32_t commandSequence,
                u8_t state,
                u16_t reason,
                u8_t authoritativeSkillSlot,
                u8_t authoritativeSkillStage,
                u64_t stageWindowEndTick)
            {
                OnAuthoritativeCommandResult(
                    serverTick,
                    commandSequence,
                    state,
                    reason,
                    authoritativeSkillSlot,
                    authoritativeSkillStage,
                    stageWindowEndTick);
            });
    }

    if (bDisableLiveNetwork)
        return;

    if (!m_pSnapshotApplier || !m_pEventApplier || !m_pEntityIdMap)
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

                const MatchContext& context = Client::CLoLMatchContextRuntime::Instance().Context();
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
                    ApplyAuthoritativePlayerNetId(bindNetId);
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
                ApplyAuthoritativePlayerNetId(
                    snapshotApplier.GetLocalNetId());
            }
            else if (type == ePacketType::Event)
            {
                eventApplier.OnEvent(m_World, entityMap, payload, len);
            }
        };

    if (m_bUsingSharedNetwork)
    {
        CGameSessionClient::Instance().SetGameFrameCallback(std::move(frameHandler));
        Winters::DevSmoke::Log("[Scene] callbacks registered (snapshot/event/cmd/network)\n");
    }
    else if (m_pNetworkView)
    {
        m_pNetworkView->SetFrameCallback(std::move(frameHandler));
        Winters::DevSmoke::Log("[Scene] callbacks registered (snapshot/event/cmd/network)\n");
    }

    if (bUseNetworkRoster)
    {
        if (m_bUsingSharedNetwork)
        {
            const char* pTransportName =
                m_pNetworkView->GetTransport() == eClientNetworkTransport::Udp
                    ? "UDP"
                    : "TCP";
            Winters::DevSmoke::Log(
                "[Scene_InGame] Reusing BanPick %s session.\n",
                pTransportName);
        }
        else
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Network roster active without shared session; local roster only.\n");
        }
    }
    else
    {
        if (!m_pNetworkView)
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Network transport unavailable; running local-only mode.\n");
            return;
        }

        const CGameSessionClient::ServerEndpoint endpoint =
            CGameSessionClient::ResolveServerEndpoint();
        if (!endpoint.bTransportValid)
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] invalid --net-transport; expected tcp or udp; network disabled.\n");
            return;
        }
        const char* pTransportName =
            endpoint.transport == eClientNetworkTransport::Udp ? "udp" : "tcp";
        if (m_pNetworkView->Connect(endpoint.host.c_str(), endpoint.port))
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Connected to Winters server host=%s port=%u transport=%s source=%s.\n",
                endpoint.host.c_str(),
                static_cast<u32_t>(endpoint.port),
                pTransportName,
                endpoint.bFromCommandLine ? "command-line" : "default");
        }
        else
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Server not reachable host=%s port=%u transport=%s; running local-only mode.\n",
                endpoint.host.c_str(),
                static_cast<u32_t>(endpoint.port),
                pTransportName);
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
            // 사운드 게이트용 — 재생성(리와인드/재접속/비주얼 스왑) 직후 첫 틱은
            // stale 액션/사망 상태가 새 시퀀스처럼 보이므로 무음 래치한다
            const bool_t bFirstAnimStateObservation =
                m_NetworkActionAnimStates.find(e) == m_NetworkActionAnimStates.end();
            NetworkActionAnimationState& actionState = m_NetworkActionAnimStates[e];
            const u64_t serverTick = m_pSnapshotApplier
                ? m_pSnapshotApplier->GetLastAppliedServerTick()
                : 0u;
            const bool_t bRecallActionActive =
                m_World.HasComponent<ReplicatedStateComponent>(e) &&
                (m_World.GetComponent<ReplicatedStateComponent>(e).stateFlags &
                    kSnapshotStateRecallFlag) != 0u;
            const auto recallFxIt = m_NetworkRecallFxHandles.find(e);
            const bool_t bRecallFxAlive =
                recallFxIt != m_NetworkRecallFxHandles.end() &&
                m_World.IsAlive(recallFxIt->second);
            if (bRecallActionActive)
            {
                if (!bRecallFxAlive)
                    StartNetworkRecallFx(e);
            }
            else
            {
                StopNetworkRecallFx(e);
            }
            const bool_t bMoveBlockedByNetworkAction =
                pAction &&
                pAction->sequence != 0u &&
                pAction->movePolicy != eSkillActionMovePolicy::Allow &&
                serverTick < pAction->lockEndTick;
            const bool_t bMoveBlockedByGameplayState =
                m_World.HasComponent<GameplayStateComponent>(e) &&
                (m_World.GetComponent<GameplayStateComponent>(e).stateFlags &
                    kGameplayStateCannotMoveFlag) != 0u;

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
            if (bMoveBlockedByNetworkAction || bMoveBlockedByGameplayState)
                moveGrace = 0.f;

            bPositionMoving =
                !bMoveBlockedByNetworkAction &&
                !bMoveBlockedByGameplayState &&
                (moveGrace > 0.f);
            const bool_t bPoseRequestsIdle =
                pPose && pPose->poseId == static_cast<u16_t>(ePoseStateId::Idle);
            const bool_t bPoseRequestsDeath =
                pPose && pPose->poseId == static_cast<u16_t>(ePoseStateId::Dead);
            const bool_t bOathswornRitual =
                m_World.HasComponent<ReplicatedStateComponent>(e) &&
                (m_World.GetComponent<ReplicatedStateComponent>(e).stateFlags &
                    kSnapshotStateKalistaOathswornRitualFlag) != 0u;
            const bool_t bDeathPresentation =
                bPoseRequestsDeath || bOathswornRitual;
            if (bDeathPresentation)
                moveGrace = 0.f;

            bool_t bMoving =
                !bDeathPresentation &&
                !bMoveBlockedByNetworkAction &&
                !bMoveBlockedByGameplayState &&
                (bServerMoving || bPositionMoving);
            if (bPoseRequestsIdle && !bServerMoving && !bPositionMoving)
                bMoving = false;

            prevIt->second = pos;

            if (e == m_PlayerEntity)
            {
                m_bMoving = bMoving;
                if (!m_bPlayerVoiceMoveInitialized)
                {
                    m_bPlayerVoiceMoveInitialized = true;
                    m_fPlayerVoiceMoveDelayRemainingSec =
                        SelectMoveVoiceDelaySec(m_uPlayerVoiceSelectionCounter);
                }

                if (bMoving && !bFirstAnimStateObservation)
                {
                    m_fPlayerVoiceMoveDelayRemainingSec -= dt;
                    if (m_fPlayerVoiceMoveDelayRemainingSec <= 0.f)
                    {
                        const eChampion moveChampion =
                            ResolveBasePresentationChampion(m_World, e, champ.id);
                        const u32_t selection = ++m_uPlayerVoiceSelectionCounter;
                        PlayNetworkChampionVoice(
                            moveChampion,
                            eChampionVoiceSlot::Move,
                            selection);
                        m_fPlayerVoiceMoveDelayRemainingSec =
                            SelectMoveVoiceDelaySec(selection);
                    }
                }
            }

            if (bDeathPresentation)
            {
                const u32_t deathSeq = pAction &&
                    pAction->actionId == static_cast<u16_t>(eActionStateId::DeathStart)
                    ? pAction->sequence
                    : static_cast<u32_t>(pPose ? pPose->startTick : 0u);
                if (actionState.baseSeq != deathSeq)
                {
                    StopNetworkRecallFx(e);
                    actionState = {};
                    actionState.baseSeq = deathSeq;

                    const eChampion deathChampion =
                        ResolveBasePresentationChampion(m_World, e, champ.id);
                    const ChampionDef* cd = FindClientChampionDef(deathChampion);
                    if (cd)
                    {
                        const std::string deathAnim = ResolveNetworkDeathAnimName(*cd);
                        if (!deathAnim.empty())
                        {
                            ResetNetworkAnimatorSpeed(rc);
                            rc.pRenderer->PlayAnimationByName(deathAnim, false);
                        }
                    }
                    // Kalista 서약 의식은 사망 애니만 재사용하므로 실제 Dead 포즈에서만 재생
                    if (!bFirstAnimStateObservation &&
                        bPoseRequestsDeath &&
                        IsNetworkChampionSoundAudible(
                            m_World,
                            e,
                            m_PlayerEntity,
                            m_PlayerTeam))
                    {
                        PlayNetworkChampionSound(deathChampion, eChampionSoundSlot::Death);
                        PlayNetworkChampionVoice(
                            deathChampion,
                            eChampionVoiceSlot::Death,
                            deathSeq);
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
                        const eChampion actionChampion =
                            ResolveActionPresentationChampion(
                                m_World,
                                e,
                                champ.id,
                                *pAction);
                        const SkillDef* pSkillDef =
                            FindNetworkSkillDef(actionChampion, pAction->actionId);
                        const std::string actionAnimName =
                            ResolveNetworkActionAnimName(
                                actionChampion,
                                pSkillDef,
                                *pAction);
                        const u32_t prevSoundActionSeq = actionState.actionSeq;
                        actionState = {};
                        actionState.actionSeq = pAction->sequence;
                        actionState.actionId = pAction->actionId;
                        // 시퀀스 전진 시에만 재생 — UDP 레인 스큐로 컴포넌트가
                        // 과거 시퀀스로 회귀했다 복구될 때의 이중 재생 방지
                        if (!bFirstAnimStateObservation &&
                            pAction->sequence > prevSoundActionSeq &&
                            IsNetworkChampionSoundAudible(
                                m_World,
                                e,
                                m_PlayerEntity,
                                m_PlayerTeam))
                        {
                            PlayNetworkChampionActionSound(
                                actionChampion,
                                pAction->actionId,
                                pAction->sequence);
                        }
                        UI::AttackSpeedPlaybackScales attackSpeedScales{};
                        const bool_t bBasicAttackPresentation =
                            UI::IsAttackSpeedPlaybackAction(
                                pAction->actionId,
                                pAction->sourceSlot);
                        if (bBasicAttackPresentation)
                        {
                            attackSpeedScales =
                                UI::ResolveAttackSpeedPlaybackScales(m_World, e);
                        }
                        actionState.actionRemainingSec =
                            ResolveNetworkActionDurationSec(
                                actionChampion,
                                pSkillDef,
                                pAction->actionId,
                                pAction->stage,
                                rc,
                                actionAnimName,
                                attackSpeedScales.fAttackSpeedScale,
                                attackSpeedScales.fAnimCorrectionScale);
                        actionState.transitionDurationSec =
                            pSkillDef ? pSkillDef->endTransitionDuration : 0.f;
                        if (pSkillDef)
                        {
                            const ChampionDef* pChampionDef =
                                FindClientChampionDef(actionChampion);
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
                            ShouldLoopNetworkAction(
                                pSkillDef,
                                pAction->stage);
                        actionState.bActionActive = true;
                        actionState.bBaseAnimationPending = !actionState.bLoopAction;
                        actionState.bPassiveDashTriggered = false;
                        PlayLoopNetworkActionIfNeeded(
                            actionChampion,
                            pSkillDef,
                            *pAction,
                            rc);
                    }
                }
                else if (pPose && actionState.baseSeq != static_cast<u32_t>(pPose->startTick))
                {
                    StopNetworkRecallFx(e);
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
                    const eChampion actionChampion =
                        ResolveActionPresentationChampion(
                            m_World,
                            e,
                            champ.id,
                            *pAction);
                    const SkillDef* pLoopSkillDef =
                        FindNetworkSkillDef(actionChampion, pAction->actionId);
                    PlayLoopNetworkActionIfNeeded(
                        actionChampion,
                        pLoopSkillDef,
                        *pAction,
                        rc);
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
                                GetDefaultChampionPassiveDashInputGraceSec(eChampion::KALISTA);
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
                const eChampion visualChampion =
                    ResolveBasePresentationChampion(m_World, e, champ.id);
                const ChampionDef* cd = FindClientChampionDef(visualChampion);
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
                const eChampion visualChampion =
                    ResolveBasePresentationChampion(m_World, e, champ.id);
                const ChampionDef* cd = FindClientChampionDef(visualChampion);
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

            const eChampion visualChampion =
                ResolveBasePresentationChampion(m_World, e, champ.id);
            const ChampionDef* cd = FindClientChampionDef(visualChampion);
            if (!cd || !cd->animPrefix || !cd->idleAnimKey || !cd->runAnimKey)
                return;

            const std::string animName =
                std::string(cd->animPrefix) + (bMoving ? cd->runAnimKey : cd->idleAnimKey);
            ResetNetworkAnimatorSpeed(rc);
            rc.pRenderer->PlayAnimationByName(animName, true);
            bWasMoving = bMoving;
        });
}

bool_t CScene_InGame::ApplyAuthoritativePlayerNetId(NetEntityId netId)
{
    if (netId == NULL_NET_ENTITY || !m_pEntityIdMap)
        return false;

    const EntityID nextPlayer = m_pEntityIdMap->FromNet(netId);
    if (nextPlayer == NULL_ENTITY ||
        !m_World.IsAlive(nextPlayer) ||
        !m_World.HasComponent<ChampionComponent>(nextPlayer))
    {
        return false;
    }

    if (m_pNetworkView)
        m_pNetworkView->SetMyNetEntityId(netId);

    if (nextPlayer == m_PlayerEntity)
    {
        if (!m_World.HasComponent<LocalPlayerTag>(nextPlayer))
            m_World.AddComponent<LocalPlayerTag>(nextPlayer);
        if (!m_pPlayerRenderer || !m_pPlayerTransform)
            BindPlayerToECSChampion(nextPlayer);
        return true;
    }

    const EntityID previousPlayer = m_PlayerEntity;
    ResetLocalControlHandoffState();
    m_NetworkMovePredictions.clear();
    m_uLastAckedMovePredictionSeq = 0u;
    m_bPlayerVoiceMoveInitialized = false;
    m_fPlayerVoiceMoveDelayRemainingSec = 0.f;

    const auto InvalidateNetworkAnimationState = [this](EntityID entity)
    {
        if (entity == NULL_ENTITY)
            return;
        m_NetworkActionAnimStates.erase(entity);
        m_NetworkChampionMoving.erase(entity);
    };
    InvalidateNetworkAnimationState(previousPlayer);
    InvalidateNetworkAnimationState(nextPlayer);

    if (previousPlayer != NULL_ENTITY &&
        m_World.IsAlive(previousPlayer) &&
        m_World.HasComponent<LocalPlayerTag>(previousPlayer))
    {
        m_World.RemoveComponent<LocalPlayerTag>(previousPlayer);
    }
    if (!m_World.HasComponent<LocalPlayerTag>(nextPlayer))
        m_World.AddComponent<LocalPlayerTag>(nextPlayer);

    m_PlayerEntity = nextPlayer;
    BindPlayerToECSChampion(nextPlayer);
    SyncActorHUDStateToEngineUI();
    return true;
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

    WINTERS_PROFILE_GAUGE("Net::SnapshotAppliedTick", serverTick);
    WINTERS_PROFILE_COUNT("Net::SnapshotsApplied", 1u);
    WINTERS_PROFILE_GAUGE("Net::LastAckedSeq", lastAckedCommandSeq);
    WINTERS_PROFILE_GAUGE("Prediction::PendingMoves",
        static_cast<u64_t>(m_NetworkMovePredictions.size()));
}

void CScene_InGame::OnAuthoritativeCommandResult(
    u64_t serverTick,
    u32_t commandSequence,
    u8_t state,
    u16_t reason,
    u8_t authoritativeSkillSlot,
    u8_t authoritativeSkillStage,
    u64_t stageWindowEndTick)
{
    const u32_t previousSequence = authoritativeSkillSlot < 5u
        ? m_uLastSkillCommandResultSeq[authoritativeSkillSlot]
        : 0u;
    const bool_t bNewerSequence = previousSequence == 0u ||
        static_cast<i32_t>(commandSequence - previousSequence) > 0;
    if (state == static_cast<u8_t>(eCommandExecutionState::Unknown) ||
        authoritativeSkillSlot >= 5u ||
        commandSequence == 0u ||
        !bNewerSequence ||
        m_PlayerEntity == NULL_ENTITY ||
        !m_World.HasComponent<SkillStateComponent>(m_PlayerEntity))
    {
        return;
    }

    m_uLastSkillCommandResultSeq[authoritativeSkillSlot] = commandSequence;
    auto& slot = m_World.GetComponent<SkillStateComponent>(m_PlayerEntity)
        .slots[authoritativeSkillSlot];
    slot.currentStage = authoritativeSkillStage == 1u ? 1u : 0u;
    slot.stageWindow = stageWindowEndTick > serverTick
        ? static_cast<f32_t>(stageWindowEndTick - serverTick) *
            DeterministicTime::kFixedDt
        : 0.f;

#if defined(_DEBUG)
    char message[224]{};
    sprintf_s(
        message,
        "[CommandResult][Client] seq=%u state=%u reason=%u slot=%u authoritativeStage=%u windowEnd=%llu\n",
        commandSequence,
        static_cast<u32_t>(state),
        static_cast<u32_t>(reason),
        static_cast<u32_t>(authoritativeSkillSlot),
        static_cast<u32_t>(authoritativeSkillStage),
        static_cast<unsigned long long>(stageWindowEndTick));
    OutputDebugStringA(message);
#else
    (void)reason;
#endif
}

void CScene_InGame::RebaseNetworkTimeline(
    const SnapshotTimelineState& previous,
    const SnapshotTimelineState& next,
    u64_t serverTick)
{
    if (m_pEventApplier && m_pEntityIdMap)
        m_pEventApplier->RebaseTimeline(m_World, *m_pEntityIdMap);

    // The incoming snapshot is a full authoritative base for another branch.
    // Never interpolate or predict from transforms/commands captured before it.
    m_NetworkActorInterpStates.clear();
    m_NetworkChampionLevels.clear();
    m_uNetworkChampionLevelSnapshotTick = 0u;
    m_uNetworkActorInterpSnapshotTick = serverTick;
    m_NetworkMovePredictions.clear();
    m_uLastAckedMovePredictionSeq = 0u;
    std::fill_n(m_uLastSkillCommandResultSeq, 5u, 0u);
    ClearNetworkRecallFx();
    m_NetworkActionAnimStates.clear();
    m_NetworkChampionPrevPos.clear();
    m_NetworkChampionMoveGraceSec.clear();
    m_NetworkChampionMoving.clear();
    m_bPlayerVoiceMoveInitialized = false;
    m_fPlayerVoiceMoveDelayRemainingSec = 0.f;

    if (m_OutlinedHoverEntity != NULL_ENTITY)
        SetEntityHoverOutline(m_OutlinedHoverEntity, false);
    m_HoveredEntity = NULL_ENTITY;
    m_OutlinedHoverEntity = NULL_ENTITY;
    m_HoveredTeam = eTeam::TEAM_END;

    PreemptAction("timeline-rebase");
    m_bKalistaPassiveDashActive = false;
    m_fKalistaPassiveDashElapsed = 0.f;
    m_uKalistaLastPassiveDashActionSeq = 0u;
    m_bYasuoDashActive = false;
    m_fYasuoDashElapsed = 0.f;
    m_YasuoDashTargetEntity = NULL_ENTITY;
    m_bYasuoRActive = false;
    m_fYasuoRElapsed = 0.f;
    m_YasuoRTarget = NULL_ENTITY;
    m_iYasuoRHitsFired = 0;
    m_fYasuoRPrevHitTime = 0.f;

    char message[256]{};
    sprintf_s(
        message,
        "[TimelineRebase] tick=%llu epoch=%llu->%llu branch=%llu->%llu toolRevision=%llu paused=%u speed=%.3f\n",
        static_cast<unsigned long long>(serverTick),
        static_cast<unsigned long long>(previous.timelineEpoch),
        static_cast<unsigned long long>(next.timelineEpoch),
        static_cast<unsigned long long>(previous.branchId),
        static_cast<unsigned long long>(next.branchId),
        static_cast<unsigned long long>(next.toolRevision),
        next.simPaused ? 1u : 0u,
        next.simSpeedMul);
    OutputDebugStringA(message);
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
