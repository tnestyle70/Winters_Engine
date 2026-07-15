#include "Game/GameRoom.h"

#include "Game/ReplayRecorder.h"
#include "Security/LagCompensation.h"
#include "Shared/GameSim/Core/Checkpoint/WorldKeyframe.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"
#include "ECS/Systems/SpatialHashSystem.h"

#include <iostream>
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"

#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/AreaAuraComponent.h"
#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionAssistCredit.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PendingHitComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/ProjectileBarrierComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/ShadowCloneComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Components/ZedSimComponent.h"
#include "Shared/GameSim/Definitions/ExperienceDef.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"
#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"

#include "ECS/Components/CoreComponents.h"

#if defined(_DEBUG)
#include <Windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    u64_t MakeReplayCommandKey(u32_t sessionId, u32_t sequenceNum)
    {
        return (static_cast<u64_t>(sessionId) << 32u) |
            static_cast<u64_t>(sequenceNum);
    }

    Winters::Replay::eReplayCommandDomain ClassifyReplayCommandDomain(
        const GameCommandWire& wire)
    {
        if (wire.kind == eCommandKind::AIDebugControl)
            return Winters::Replay::eReplayCommandDomain::AuthoringMutation;

        if (wire.kind == eCommandKind::PracticeControl)
        {
            switch (wire.practiceOperation)
            {
            case ePracticeOperation::SetSimulationPaused:
            case ePracticeOperation::StepSimulationTicks:
            case ePracticeOperation::SetSimulationTimeScale:
            case ePracticeOperation::RewindSimulationSeconds:
                return Winters::Replay::eReplayCommandDomain::ControlPlane;
            default:
                return Winters::Replay::eReplayCommandDomain::AuthoringMutation;
            }
        }

        return Winters::Replay::eReplayCommandDomain::PlayerInput;
    }

#if defined(_DEBUG)
    constexpr u32_t kPracticeGoldCap = 1000000u;
    constexpr size_t kPracticeSpawnQuota = 100u;
    constexpr f32_t kPracticeOverrideValueMax = 1000000.f;
    constexpr f32_t kAttackSpeedLabMin = 0.8f;
    constexpr f32_t kAttackSpeedLabMax = 2.5f;
    constexpr u8_t kAttackSpeedLabStartLevel = 6u;
    constexpr u32_t kAttackSpeedLabStartGold = 10000u;

    bool_t IsFinitePracticePosition(const Vec3& position)
    {
        return std::isfinite(position.x) &&
            std::isfinite(position.y) &&
            std::isfinite(position.z);
    }

    bool_t TryResolvePracticeByte(
        f32_t value,
        u8_t minimum,
        u8_t maximum,
        u8_t& outValue)
    {
        if (!std::isfinite(value) ||
            value < static_cast<f32_t>(minimum) ||
            value > static_cast<f32_t>(maximum) ||
            std::floor(value) != value)
        {
            return false;
        }

        outValue = static_cast<u8_t>(value);
        return true;
    }

    bool_t IsValidPracticeEffectParam(u32_t rawParam)
    {
        return rawParam > static_cast<u32_t>(eSkillEffectParamId::None) &&
            rawParam <= static_cast<u32_t>(eSkillEffectParamId::BonusAttackSpeed);
    }

    bool_t IsValidPracticeEffectValue(
        eSkillEffectParamId param,
        f32_t value)
    {
        if (!std::isfinite(value))
            return false;

        switch (param)
        {
        case eSkillEffectParamId::HalfAngleCos:
            return value >= -1.f && value <= 1.f;
        case eSkillEffectParamId::MoveSpeedMul:
            return value >= 0.f && value <= 4.f;
        case eSkillEffectParamId::MissingHealthDamageRatio:
            return value >= 0.f && value <= 10.f;
        default:
            return value >= 0.f && value <= kPracticeOverrideValueMax;
        }
    }

    const ChampionAITuningParam* ResolveChampionAITuningParamForValidation(
        const ChampionAIComponent& ai,
        eChampionAITuningId tuningId)
    {
        switch (tuningId)
        {
        case eChampionAITuningId::ChampionScanRange:
            return &ai.tuning.championScanRange;
        case eChampionAITuningId::MinionScanRange:
            return &ai.tuning.minionScanRange;
        case eChampionAITuningId::StructureScanRange:
            return &ai.tuning.structureScanRange;
        case eChampionAITuningId::LeashRange:
            return &ai.tuning.leashRange;
        case eChampionAITuningId::RetreatHpRatio:
            return &ai.tuning.retreatHpRatio;
        case eChampionAITuningId::ReengageHpRatio:
            return &ai.tuning.reengageHpRatio;
        case eChampionAITuningId::ChampionScoreMargin:
            return &ai.tuning.championScoreMargin;
        case eChampionAITuningId::TurretDangerThreshold:
            return &ai.tuning.turretDangerThreshold;
        case eChampionAITuningId::PostComboBASelfHpMinRatio:
            return &ai.tuning.postComboBASelfHpMinRatio;
        case eChampionAITuningId::PostComboBAEnemyHpMargin:
            return &ai.tuning.postComboBAEnemyHpMargin;
        case eChampionAITuningId::PostComboBAWindow:
            return &ai.tuning.postComboBAWindow;
        case eChampionAITuningId::LowHpExecuteThreshold:
            return &ai.tuning.lowHpExecuteThreshold;
        case eChampionAITuningId::DiveScanRange:
            return &ai.tuning.diveScanRange;
        case eChampionAITuningId::DiveExtraBAWindow:
            return &ai.tuning.diveExtraBAWindow;
        default:
            return nullptr;
        }
    }

    void ResetPracticeCooldowns(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<SkillStateComponent>(entity))
        {
            auto& skillState = world.GetComponent<SkillStateComponent>(entity);
            for (auto& slot : skillState.slots)
            {
                slot.cooldownRemaining = 0.f;
                slot.cooldownDuration = 0.f;
            }
        }

        if (world.HasComponent<ChampionComponent>(entity))
        {
            auto& champion = world.GetComponent<ChampionComponent>(entity);
            for (f32_t& cooldown : champion.cooldowns)
                cooldown = 0.f;
        }

        if (world.HasComponent<SummonerSpellStateComponent>(entity))
        {
            world.GetComponent<SummonerSpellStateComponent>(entity) =
                SummonerSpellStateComponent{};
        }

        if (world.HasComponent<ViegoSimComponent>(entity))
        {
            auto& viego = world.GetComponent<ViegoSimComponent>(entity);
            if (viego.bHasOriginalSkillState)
            {
                for (auto& slot : viego.originalSkillState.slots)
                {
                    slot.cooldownRemaining = 0.f;
                    slot.cooldownDuration = 0.f;
                }
            }
        }
    }

    void ClearChampionControlIntent(CWorld& world, EntityID entity)
    {
        if (!world.IsAlive(entity))
            return;
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
        if (world.HasComponent<AttackChaseComponent>(entity))
            world.RemoveComponent<AttackChaseComponent>(entity);
    }

    bool_t HasStrictAttackSpeedLabRoster(
        CWorld& world,
        const EntityIdMap& entityMap,
        const CLobbyAuthority* pLobbyAuthority,
        u32_t requestSessionId)
    {
        if (!pLobbyAuthority ||
            pLobbyAuthority->GetPhase() != eRoomPhase::InGame ||
            requestSessionId == 0u)
        {
            return false;
        }

        const LobbySlotState* pSlots = pLobbyAuthority->GetSlots();
        const u32_t slotCount = pLobbyAuthority->GetSlotCount();
        if (!pSlots || slotCount != kGameRosterSlotCount)
            return false;

        u32_t humanCount = 0u;
        u32_t botCount = 0u;
        u32_t blueCount = 0u;
        u32_t redCount = 0u;
        std::vector<NetEntityId> rosterNetIds;
        rosterNetIds.reserve(slotCount);
        for (u32_t i = 0u; i < slotCount; ++i)
        {
            const LobbySlotState& slot = pSlots[i];
            if (slot.slotId != i ||
                slot.bHuman == slot.bBot ||
                slot.bDummy ||
                slot.team != (i < 5u ? 0u : 1u) ||
                slot.champion == eChampion::NONE ||
                slot.champion == eChampion::END ||
                slot.netId == NULL_NET_ENTITY)
            {
                return false;
            }
            if ((slot.bHuman && slot.sessionId != requestSessionId) ||
                (slot.bBot && slot.sessionId != 0u))
            {
                return false;
            }

            const EntityID entity = entityMap.FromNet(slot.netId);
            if (entity == NULL_ENTITY ||
                !world.IsAlive(entity) ||
                !world.HasComponent<ChampionComponent>(entity) ||
                !world.HasComponent<NetEntityIdComponent>(entity) ||
                world.GetComponent<NetEntityIdComponent>(entity).netId !=
                    slot.netId ||
                (slot.bHuman &&
                    world.HasComponent<ChampionAIComponent>(entity)) ||
                (slot.bBot &&
                    !world.HasComponent<ChampionAIComponent>(entity)))
            {
                return false;
            }

            const ChampionComponent& champion =
                world.GetComponent<ChampionComponent>(entity);
            if (champion.id != slot.champion ||
                champion.team != static_cast<eTeam>(slot.team))
            {
                return false;
            }

            rosterNetIds.push_back(slot.netId);
            humanCount += slot.bHuman ? 1u : 0u;
            botCount += slot.bBot ? 1u : 0u;
            blueCount += slot.team == 0u ? 1u : 0u;
            redCount += slot.team == 1u ? 1u : 0u;
        }

        std::sort(rosterNetIds.begin(), rosterNetIds.end());
        if (std::adjacent_find(rosterNetIds.begin(), rosterNetIds.end()) !=
            rosterNetIds.end())
        {
            return false;
        }

        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
        return champions.size() == kGameRosterSlotCount &&
            humanCount == 1u &&
            botCount == 9u &&
            blueCount == 5u &&
            redCount == 5u;
    }

    void TransferPracticeControlState(
        CWorld& world,
        EntityID source,
        EntityID target)
    {
        if (!world.IsAlive(source) || !world.IsAlive(target) || source == target)
            return;

        if (world.HasComponent<PracticePlayerComponent>(source))
        {
            const PracticePlayerComponent state =
                world.GetComponent<PracticePlayerComponent>(source);
            if (world.HasComponent<PracticePlayerComponent>(target))
                world.GetComponent<PracticePlayerComponent>(target) = state;
            else
                world.AddComponent<PracticePlayerComponent>(target, state);
            world.RemoveComponent<PracticePlayerComponent>(source);
        }

        const auto spawned =
            DeterministicEntityIterator<PracticeSpawnedTag>::CollectSorted(world);
        for (EntityID entity : spawned)
        {
            if (!world.IsAlive(entity))
                continue;
            auto& tag = world.GetComponent<PracticeSpawnedTag>(entity);
            if (tag.ownerEntity == source)
                tag.ownerEntity = target;
        }
    }

    template<typename Component, typename Predicate>
    void AppendReplacementCleanupEntities(
        CWorld& world,
        std::vector<EntityID>& entities,
        Predicate&& predicate)
    {
        const auto candidates =
            DeterministicEntityIterator<Component>::CollectSorted(world);
        for (EntityID entity : candidates)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<Component>(entity) ||
                world.HasComponent<ChampionComponent>(entity))
            {
                continue;
            }
            if (predicate(world.GetComponent<Component>(entity)))
                entities.push_back(entity);
        }
    }

    std::vector<EntityID> CollectChampionReplacementCleanupEntities(
        CWorld& world,
        EntityID source,
        NetEntityId sourceNetId)
    {
        std::vector<EntityID> entities;
        const EntityHandle sourceHandle = world.GetEntityHandle(source);
        AppendReplacementCleanupEntities<AnnieTibbersComponent>(
            world, entities,
            [&](const AnnieTibbersComponent& value)
            {
                return value.owner == source;
            });
        AppendReplacementCleanupEntities<KalistaSentinelComponent>(
            world, entities,
            [&](const KalistaSentinelComponent& value)
            {
                return value.owner == source;
            });
        AppendReplacementCleanupEntities<ShadowCloneComponent>(
            world, entities,
            [&](const ShadowCloneComponent& value)
            {
                return value.owner == source;
            });
        AppendReplacementCleanupEntities<ProjectileBarrierComponent>(
            world, entities,
            [&](const ProjectileBarrierComponent& value)
            {
                return value.sourceEntity == source;
            });
        AppendReplacementCleanupEntities<AreaAuraComponent>(
            world, entities,
            [&](const AreaAuraComponent& value)
            {
                return value.owner == source;
            });
        AppendReplacementCleanupEntities<LeeSinWardOwnerComponent>(
            world, entities,
            [&](const LeeSinWardOwnerComponent& value)
            {
                return value.owner == source;
            });
        AppendReplacementCleanupEntities<SkillProjectileComponent>(
            world, entities,
            [&](const SkillProjectileComponent& value)
            {
                return value.sourceEntity == source ||
                    value.targetEntity == source;
            });
        AppendReplacementCleanupEntities<StructureProjectileComponent>(
            world, entities,
            [&](const StructureProjectileComponent& value)
            {
                return value.sourceEntity == source ||
                    value.targetEntity == source;
            });
        AppendReplacementCleanupEntities<PendingHitComponent>(
            world, entities,
            [&](const PendingHitComponent& value)
            {
                return value.ownerEntity == source;
            });
        AppendReplacementCleanupEntities<DamageRequestComponent>(
            world, entities,
            [&](const DamageRequestComponent& value)
            {
                return value.source == source || value.target == source;
            });
        AppendReplacementCleanupEntities<ViegoSoulComponent>(
            world, entities,
            [&](const ViegoSoulComponent& value)
            {
                return value.deadChampion == source ||
                    value.eligibleViego == source;
            });
        AppendReplacementCleanupEntities<ReplicatedEventComponent>(
            world, entities,
            [&](const ReplicatedEventComponent& value)
            {
                return value.sourceEntity == source ||
                    value.targetEntity == source ||
                    value.projectileEntity == source ||
                    value.sourceNetOverride == sourceNetId ||
                    value.targetNetOverride == sourceNetId ||
                    value.projectileNetOverride == sourceNetId;
            });
        AppendReplacementCleanupEntities<TowerAggroNotifyComponent>(
            world, entities,
            [&](const TowerAggroNotifyComponent& value)
            {
                return value.attackerEntity == source ||
                    value.victimEntity == source;
            });
        AppendReplacementCleanupEntities<EzrealEssenceFluxMarkComponent>(
            world, entities,
            [&](const EzrealEssenceFluxMarkComponent& value)
            {
                return value.hSource == sourceHandle ||
                    value.hTarget == sourceHandle;
            });

        const std::vector<EntityID> projectileEntities = entities;
        for (EntityID entity : projectileEntities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<SkillProjectileComponent>(entity))
            {
                continue;
            }
            const EntityID ledger =
                world.GetComponent<SkillProjectileComponent>(entity)
                    .sharedHitLedgerEntity;
            if (ledger != NULL_ENTITY &&
                world.IsAlive(ledger) &&
                !world.HasComponent<ChampionComponent>(ledger))
            {
                entities.push_back(ledger);
            }
        }

        std::sort(entities.begin(), entities.end());
        entities.erase(
            std::unique(entities.begin(), entities.end()),
            entities.end());
        entities.erase(
            std::remove(entities.begin(), entities.end(), source),
            entities.end());
        return entities;
    }

    void ScrubDestroyedChampionReferences(
        CWorld& world,
        EntityID source)
    {
        const EntityHandle sourceHandle = world.GetEntityHandle(source);

        const auto minionStates =
            DeterministicEntityIterator<MinionStateComponent>::CollectSorted(world);
        for (EntityID entity : minionStates)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<MinionStateComponent>(entity))
            {
                continue;
            }

            MinionStateComponent& state =
                world.GetComponent<MinionStateComponent>(entity);
            if (state.attackTargetId != source)
                continue;

            state.attackTargetId = NULL_ENTITY;
            state.attackTimer = 0.f;
            state.bHitFired = false;
            state.bAttackAnimRequested = false;
            state.targetScanCooldown = 0.f;
            if (state.current == MinionStateComponent::Chase ||
                state.current == MinionStateComponent::Attack)
            {
                state.current = world.HasComponent<AnnieTibbersComponent>(entity)
                    ? MinionStateComponent::Idle
                    : MinionStateComponent::LaneMove;
                state.visualState = MinionStateComponent::Idle;
            }
        }

        const auto turrets =
            DeterministicEntityIterator<TurretComponent>::CollectSorted(world);
        for (EntityID entity : turrets)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<TurretComponent>(entity))
            {
                continue;
            }
            TurretComponent& turret = world.GetComponent<TurretComponent>(entity);
            if (turret.targetId == source)
                turret.targetId = NULL_ENTITY;
        }

        const auto turretAIs =
            DeterministicEntityIterator<TurretAIComponent>::CollectSorted(world);
        for (EntityID entity : turretAIs)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<TurretAIComponent>(entity))
            {
                continue;
            }
            TurretAIComponent& ai = world.GetComponent<TurretAIComponent>(entity);
            const bool_t bClearedAggro = ai.aggroTargetId == source;
            if (ai.attackTargetId == source)
                ai.attackTargetId = NULL_ENTITY;
            if (bClearedAggro)
            {
                ai.aggroTargetId = NULL_ENTITY;
                ai.aggroLockTimer = 0.f;
            }
        }

        const auto jungleAIs =
            DeterministicEntityIterator<JungleAIComponent>::CollectSorted(world);
        for (EntityID entity : jungleAIs)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<JungleAIComponent>(entity))
            {
                continue;
            }
            JungleAIComponent& ai = world.GetComponent<JungleAIComponent>(entity);
            if (ai.target != source)
                continue;

            ai.target = NULL_ENTITY;
            ai.bAggro = false;
            if (world.HasComponent<AttackChaseComponent>(entity))
                world.RemoveComponent<AttackChaseComponent>(entity);
            if (world.HasComponent<MoveTargetComponent>(entity))
                world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
            if (world.HasComponent<CombatActionComponent>(entity) &&
                world.GetComponent<CombatActionComponent>(entity).entityTarget == source)
            {
                world.RemoveComponent<CombatActionComponent>(entity);
            }
        }

        const auto championAIs =
            DeterministicEntityIterator<ChampionAIComponent>::CollectSorted(world);
        for (EntityID entity : championAIs)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<ChampionAIComponent>(entity))
            {
                continue;
            }

            ChampionAIComponent& ai =
                world.GetComponent<ChampionAIComponent>(entity);
            const auto ClearReference = [&](EntityID& reference)
            {
                if (reference != source)
                    return false;
                reference = NULL_ENTITY;
                return true;
            };

            bool_t bDecisionTargetCleared = false;
            bDecisionTargetCleared =
                ClearReference(ai.lockedChampion) || bDecisionTargetCleared;
            bDecisionTargetCleared =
                ClearReference(ai.comboTarget) || bDecisionTargetCleared;
            bDecisionTargetCleared =
                ClearReference(ai.lowHpEnemyChampion) || bDecisionTargetCleared;
            bDecisionTargetCleared =
                ClearReference(ai.diveTarget) || bDecisionTargetCleared;

            bool_t bAnyTargetCleared = bDecisionTargetCleared;
            bAnyTargetCleared =
                ClearReference(ai.targetMinion) || bAnyTargetCleared;
            bAnyTargetCleared =
                ClearReference(ai.targetStructure) || bAnyTargetCleared;
            bAnyTargetCleared =
                ClearReference(ai.alliedWave) || bAnyTargetCleared;
            bAnyTargetCleared =
                ClearReference(ai.debugLastCommandTarget) || bAnyTargetCleared;

            if (ClearReference(ai.lastSeenEnemyChampion))
            {
                ai.lastSeenEnemyChampionTick = 0u;
                ai.lastSeenEnemyChampionPos = {};
                bDecisionTargetCleared = true;
                bAnyTargetCleared = true;
            }
            for (ChampionAIDecisionTraceEntry& trace : ai.debugDecisionTrace)
            {
                if (trace.target == source)
                    trace.target = NULL_ENTITY;
            }

            if (bDecisionTargetCleared)
            {
                ai.comboStep = 0u;
                ai.divePhase = eChampionAIDivePhase::None;
                ai.diveExtraBACount = 0u;
                ai.fPostComboBATimer = 0.f;
                ai.fDiveExtraBATimer = 0.f;
                ai.bCanAttackChampion = false;
                ai.bPostComboBAAllowed = false;
            }
            if (bAnyTargetCleared)
            {
                ai.decisionTimer = 0.f;
                ai.intentHoldTimer = 0.f;
            }
        }

        const auto tibbersEntities =
            DeterministicEntityIterator<AnnieTibbersComponent>::CollectSorted(world);
        for (EntityID entity : tibbersEntities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<AnnieTibbersComponent>(entity))
            {
                continue;
            }
            AnnieTibbersComponent& tibbers =
                world.GetComponent<AnnieTibbersComponent>(entity);
            if (tibbers.commandTarget == source)
            {
                tibbers.commandTarget = NULL_ENTITY;
                tibbers.bHasCommandPosition = false;
            }
        }

        const auto attackChases =
            DeterministicEntityIterator<AttackChaseComponent>::CollectSorted(world);
        for (EntityID entity : attackChases)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<AttackChaseComponent>(entity) &&
                world.GetComponent<AttackChaseComponent>(entity).target == source)
            {
                world.RemoveComponent<AttackChaseComponent>(entity);
            }
        }

        const auto combatActions =
            DeterministicEntityIterator<CombatActionComponent>::CollectSorted(world);
        for (EntityID entity : combatActions)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<CombatActionComponent>(entity) &&
                world.GetComponent<CombatActionComponent>(entity).entityTarget == source)
            {
                world.RemoveComponent<CombatActionComponent>(entity);
            }
        }

        const auto commandQueues =
            DeterministicEntityIterator<CommandQueueComponent>::CollectSorted(world);
        for (EntityID entity : commandQueues)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<CommandQueueComponent>(entity))
            {
                continue;
            }
            CommandQueueComponent& commands =
                world.GetComponent<CommandQueueComponent>(entity);
            if (commands.current.targetEntity == source)
            {
                commands.current = {};
                commands.bActive = false;
            }
            commands.queue.erase(
                std::remove_if(
                    commands.queue.begin(),
                    commands.queue.end(),
                    [&](const Command& command)
                    {
                        return command.targetEntity == source;
                    }),
                commands.queue.end());
        }

        const auto fioras =
            DeterministicEntityIterator<FioraSimComponent>::CollectSorted(world);
        for (EntityID entity : fioras)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<FioraSimComponent>(entity))
            {
                continue;
            }
            FioraSimComponent& fiora = world.GetComponent<FioraSimComponent>(entity);
            if (fiora.grandChallengeTarget == source)
            {
                fiora.grandChallengeTarget = NULL_ENTITY;
                fiora.bGrandChallengeActive = false;
                fiora.grandChallengeTimerSec = 0.f;
            }
        }

        const auto irelias =
            DeterministicEntityIterator<IreliaSimComponent>::CollectSorted(world);
        for (EntityID entity : irelias)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<IreliaSimComponent>(entity))
            {
                continue;
            }
            IreliaSimComponent& irelia = world.GetComponent<IreliaSimComponent>(entity);
            if (irelia.dashTarget == source)
            {
                irelia.dashTarget = NULL_ENTITY;
                irelia.bDashActive = false;
                irelia.dashElapsedSec = 0.f;
                irelia.dashDurationSec = 0.f;
            }

            const auto RemoveTrackedTarget = [&](EntityID* pTargets, u8_t& count)
            {
                u8_t writeIndex = 0u;
                for (u8_t readIndex = 0u; readIndex < count; ++readIndex)
                {
                    if (pTargets[readIndex] == source)
                        continue;
                    pTargets[writeIndex++] = pTargets[readIndex];
                }
                for (u8_t i = writeIndex; i < count; ++i)
                    pTargets[i] = NULL_ENTITY;
                count = writeIndex;
            };
            RemoveTrackedTarget(irelia.rHitTargets, irelia.rHitTargetCount);
            RemoveTrackedTarget(irelia.rWallTargets, irelia.rWallTargetCount);
        }

        const auto viegos =
            DeterministicEntityIterator<ViegoSimComponent>::CollectSorted(world);
        for (EntityID entity : viegos)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<ViegoSimComponent>(entity))
            {
                continue;
            }
            const ViegoSimComponent& viego =
                world.GetComponent<ViegoSimComponent>(entity);
            if (viego.pendingPossessedTarget == source ||
                viego.possessedTarget == source)
            {
                ViegoGameSim::ClearPossession(world, entity);
            }
        }

        const auto fluxMarks =
            DeterministicEntityIterator<EzrealEssenceFluxMarkComponent>::CollectSorted(world);
        for (EntityID entity : fluxMarks)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<EzrealEssenceFluxMarkComponent>(entity))
            {
                continue;
            }
            const EzrealEssenceFluxMarkComponent& mark =
                world.GetComponent<EzrealEssenceFluxMarkComponent>(entity);
            if (mark.hSource == sourceHandle || mark.hTarget == sourceHandle)
                world.RemoveComponent<EzrealEssenceFluxMarkComponent>(entity);
        }

        const auto kindredFloors =
            DeterministicEntityIterator<KindredHealthFloorComponent>::CollectSorted(world);
        for (EntityID entity : kindredFloors)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<KindredHealthFloorComponent>(entity) &&
                world.GetComponent<KindredHealthFloorComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<KindredHealthFloorComponent>(entity);
            }
        }

        const auto leeSinMarks =
            DeterministicEntityIterator<LeeSinQMarkComponent>::CollectSorted(world);
        for (EntityID entity : leeSinMarks)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<LeeSinQMarkComponent>(entity) &&
                world.GetComponent<LeeSinQMarkComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<LeeSinQMarkComponent>(entity);
            }
        }

        const auto zedMarks =
            DeterministicEntityIterator<ZedDeathMarkComponent>::CollectSorted(world);
        for (EntityID entity : zedMarks)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<ZedDeathMarkComponent>(entity) &&
                world.GetComponent<ZedDeathMarkComponent>(entity).entitySource == source)
            {
                world.RemoveComponent<ZedDeathMarkComponent>(entity);
            }
        }

        const auto kindreds =
            DeterministicEntityIterator<KindredSimComponent>::CollectSorted(world);
        for (EntityID entity : kindreds)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<KindredSimComponent>(entity))
            {
                continue;
            }
            KindredSimComponent& kindred =
                world.GetComponent<KindredSimComponent>(entity);
            if (kindred.markedTarget == source)
            {
                kindred.markedTarget = NULL_ENTITY;
                kindred.mountingDreadHitCount = 0u;
                kindred.fEMarkRemainingSec = 0.f;
            }
        }

        const auto assistCredits =
            DeterministicEntityIterator<ChampionAssistCreditComponent>::CollectSorted(world);
        for (EntityID entity : assistCredits)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<ChampionAssistCreditComponent>(entity))
            {
                continue;
            }
            auto& credits = world.GetComponent<ChampionAssistCreditComponent>(entity);
            for (auto& credit : credits.Credits)
            {
                if (credit.SourceEntity == source)
                    credit = ChampionAssistCreditComponent::Credit{};
            }
        }
    }

    void ClearChampionReplacementRelations(
        CWorld& world,
        EntityID source)
    {
        ScrubDestroyedChampionReferences(world, source);

        const auto statusEntities =
            DeterministicEntityIterator<StatusEffectComponent>::CollectSorted(world);
        for (EntityID entity : statusEntities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<StatusEffectComponent>(entity))
            {
                continue;
            }

            std::vector<eStatusEffectId> sourceEffects;
            const StatusEffectComponent& effects =
                world.GetComponent<StatusEffectComponent>(entity);
            for (u8_t i = 0u; i < effects.count; ++i)
            {
                if (effects.active[i].sourceEntity == source &&
                    effects.active[i].effectId != eStatusEffectId::None)
                {
                    sourceEffects.push_back(effects.active[i].effectId);
                }
            }
            std::sort(
                sourceEffects.begin(),
                sourceEffects.end(),
                [](eStatusEffectId lhs, eStatusEffectId rhs)
                {
                    return static_cast<u16_t>(lhs) < static_cast<u16_t>(rhs);
                });
            sourceEffects.erase(
                std::unique(sourceEffects.begin(), sourceEffects.end()),
                sourceEffects.end());
            for (eStatusEffectId effectId : sourceEffects)
            {
                GameplayStatus::RemoveStatusEffect(
                    world,
                    entity,
                    effectId,
                    source);
            }
        }

        const auto buffEntities =
            DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
        for (EntityID entity : buffEntities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<BuffComponent>(entity))
            {
                continue;
            }

            BuffComponent& buffs = world.GetComponent<BuffComponent>(entity);
            u8_t writeIndex = 0u;
            for (u8_t readIndex = 0u; readIndex < buffs.count; ++readIndex)
            {
                if (buffs.buffs[readIndex].source == source)
                    continue;
                if (writeIndex != readIndex)
                    buffs.buffs[writeIndex] = buffs.buffs[readIndex];
                ++writeIndex;
            }
            if (writeIndex == buffs.count)
                continue;
            for (u8_t i = writeIndex; i < buffs.count; ++i)
                buffs.buffs[i] = BuffInstance{};
            buffs.count = writeIndex;
            if (world.HasComponent<StatComponent>(entity))
                world.GetComponent<StatComponent>(entity).bDirty = true;
        }

        std::vector<EntityID> rebuildGameplayState;
        const auto stunEntities =
            DeterministicEntityIterator<StunComponent>::CollectSorted(world);
        for (EntityID entity : stunEntities)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<StunComponent>(entity) &&
                world.GetComponent<StunComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<StunComponent>(entity);
                rebuildGameplayState.push_back(entity);
            }
        }
        const auto slowEntities =
            DeterministicEntityIterator<SlowComponent>::CollectSorted(world);
        for (EntityID entity : slowEntities)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<SlowComponent>(entity) &&
                world.GetComponent<SlowComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<SlowComponent>(entity);
                rebuildGameplayState.push_back(entity);
            }
        }
        const auto disarmEntities =
            DeterministicEntityIterator<DisarmComponent>::CollectSorted(world);
        for (EntityID entity : disarmEntities)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<DisarmComponent>(entity) &&
                world.GetComponent<DisarmComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<DisarmComponent>(entity);
                rebuildGameplayState.push_back(entity);
            }
        }
        const auto forcedMotionEntities =
            DeterministicEntityIterator<ForcedMotionComponent>::CollectSorted(world);
        for (EntityID entity : forcedMotionEntities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<ForcedMotionComponent>(entity) ||
                world.GetComponent<ForcedMotionComponent>(entity).sourceEntity != source)
            {
                continue;
            }

            const ForcedMotionComponent motion =
                world.GetComponent<ForcedMotionComponent>(entity);
            if (world.HasComponent<TransformComponent>(entity))
                world.GetComponent<TransformComponent>(entity).SetPosition(motion.end);
            world.RemoveComponent<ForcedMotionComponent>(entity);
            rebuildGameplayState.push_back(entity);
        }
        std::sort(rebuildGameplayState.begin(), rebuildGameplayState.end());
        rebuildGameplayState.erase(
            std::unique(
                rebuildGameplayState.begin(),
                rebuildGameplayState.end()),
            rebuildGameplayState.end());
        for (EntityID entity : rebuildGameplayState)
            GameplayStatus::RebuildGameplayState(world, entity);

        const auto relationEntities =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
        for (EntityID entity : relationEntities)
        {
            if (!world.IsAlive(entity) || entity == source)
                continue;

            bool_t bRebuildGameplayState = false;
            if (world.HasComponent<StunComponent>(entity) &&
                world.GetComponent<StunComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<StunComponent>(entity);
                bRebuildGameplayState = true;
            }
            if (world.HasComponent<SlowComponent>(entity) &&
                world.GetComponent<SlowComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<SlowComponent>(entity);
                bRebuildGameplayState = true;
            }
            if (world.HasComponent<DisarmComponent>(entity) &&
                world.GetComponent<DisarmComponent>(entity).sourceEntity == source)
            {
                world.RemoveComponent<DisarmComponent>(entity);
                bRebuildGameplayState = true;
            }
            if (world.HasComponent<ForcedMotionComponent>(entity) &&
                world.GetComponent<ForcedMotionComponent>(entity).sourceEntity == source)
            {
                const ForcedMotionComponent motion =
                    world.GetComponent<ForcedMotionComponent>(entity);
                if (world.HasComponent<TransformComponent>(entity))
                {
                    world.GetComponent<TransformComponent>(entity).SetPosition(
                        motion.end);
                }
                world.RemoveComponent<ForcedMotionComponent>(entity);
                bRebuildGameplayState = true;
            }
            if (world.HasComponent<KindredHealthFloorComponent>(entity) &&
                world.GetComponent<KindredHealthFloorComponent>(entity)
                    .sourceEntity == source)
            {
                world.RemoveComponent<KindredHealthFloorComponent>(entity);
            }
            if (world.HasComponent<LeeSinQMarkComponent>(entity) &&
                world.GetComponent<LeeSinQMarkComponent>(entity)
                    .sourceEntity == source)
            {
                world.RemoveComponent<LeeSinQMarkComponent>(entity);
            }
            if (world.HasComponent<ZedDeathMarkComponent>(entity) &&
                world.GetComponent<ZedDeathMarkComponent>(entity)
                    .entitySource == source)
            {
                world.RemoveComponent<ZedDeathMarkComponent>(entity);
            }
            if (world.HasComponent<KalistaOathswornByComponent>(entity) &&
                world.GetComponent<KalistaOathswornByComponent>(entity)
                    .entityKalista == source)
            {
                world.RemoveComponent<KalistaOathswornByComponent>(entity);
            }
            if (world.HasComponent<KalistaFateCallCarriedComponent>(entity) &&
                world.GetComponent<KalistaFateCallCarriedComponent>(entity)
                    .entityOwner == source)
            {
                world.RemoveComponent<KalistaFateCallCarriedComponent>(entity);
            }
            if (world.HasComponent<KalistaOathswornComponent>(entity) &&
                world.GetComponent<KalistaOathswornComponent>(entity)
                    .entityAlly == source)
            {
                world.RemoveComponent<KalistaOathswornComponent>(entity);
            }
            if (world.HasComponent<KalistaFateCallComponent>(entity) &&
                world.GetComponent<KalistaFateCallComponent>(entity)
                    .entityCarried == source)
            {
                world.RemoveComponent<KalistaFateCallComponent>(entity);
            }
            if (world.HasComponent<KindredSimComponent>(entity) &&
                world.GetComponent<KindredSimComponent>(entity).markedTarget == source)
            {
                auto& kindred = world.GetComponent<KindredSimComponent>(entity);
                kindred.markedTarget = NULL_ENTITY;
                kindred.mountingDreadHitCount = 0u;
                kindred.fEMarkRemainingSec = 0.f;
            }
            if (world.HasComponent<AttackChaseComponent>(entity) &&
                world.GetComponent<AttackChaseComponent>(entity).target == source)
            {
                world.RemoveComponent<AttackChaseComponent>(entity);
            }
            if (world.HasComponent<CombatActionComponent>(entity) &&
                world.GetComponent<CombatActionComponent>(entity)
                    .entityTarget == source)
            {
                world.RemoveComponent<CombatActionComponent>(entity);
            }
            if (world.HasComponent<ChampionAssistCreditComponent>(entity))
            {
                auto& credits =
                    world.GetComponent<ChampionAssistCreditComponent>(entity);
                for (auto& credit : credits.Credits)
                {
                    if (credit.SourceEntity == source)
                        credit = ChampionAssistCreditComponent::Credit{};
                }
            }
            if (bRebuildGameplayState)
                GameplayStatus::RebuildGameplayState(world, entity);
        }
    }

    void TracePracticeCommand(
        const GameCommand& cmd,
        bool_t bAccepted,
        const char* reason)
    {
        static std::atomic<u32_t> s_traceCount{ 0u };
        if (s_traceCount.fetch_add(1u, std::memory_order_relaxed) >= 256u)
            return;

        char message[256]{};
        sprintf_s(
            message,
            "[Practice] result=%s sid=%u seq=%u op=%u reason=%s\n",
            bAccepted ? "accept" : "reject",
            cmd.sourceSessionId,
            cmd.sequenceNum,
            static_cast<u32_t>(cmd.practiceOperation),
            reason ? reason : "none");
        OutputDebugStringA(message);
    }

    void TraceAIDebugCommand(
        const GameCommand& cmd,
        bool_t bForwarded,
        const char* reason)
    {
        static std::atomic<u32_t> s_traceCount{ 0u };
        if (s_traceCount.fetch_add(1u, std::memory_order_relaxed) >= 256u)
            return;

        char message[256]{};
        sprintf_s(
            message,
            "[AIDebug] result=%s sid=%u seq=%u target=%u reason=%s\n",
            bForwarded ? "forward" : "reject",
            cmd.sourceSessionId,
            cmd.sequenceNum,
            static_cast<u32_t>(cmd.targetEntity),
            reason ? reason : "none");
        OutputDebugStringA(message);
    }
#endif
}

void CGameRoom::RecordReplayCommand(
    u64_t tick,
    const PendingCommand& pending,
    Winters::Replay::eReplayCommandDomain domain,
    Winters::Replay::eReplayJournalOutcome outcome)
{
    if (Winters::Replay::ShouldAdvanceToolRevision(domain, outcome) &&
        m_toolRevision != (std::numeric_limits<u64_t>::max)())
    {
        ++m_toolRevision;
    }

    if (!Winters::Replay::ShouldJournalReplayCommand(outcome) ||
        !m_pReplayRecorder ||
        m_bReplayFinalized)
    {
        return;
    }

    Winters::Replay::ReplayCommandPayload journal{};
    journal.sourceSessionId = pending.sessionId;
    journal.sequenceNum = pending.sequenceNum;
    journal.kind = static_cast<u8_t>(pending.wire.kind);
    journal.slot = pending.wire.slot;
    journal.itemId = pending.wire.itemId;
    journal.targetNetId = static_cast<u32_t>(pending.wire.targetNet);
    journal.groundPos[0] = pending.wire.groundPos.x;
    journal.groundPos[1] = pending.wire.groundPos.y;
    journal.groundPos[2] = pending.wire.groundPos.z;
    journal.direction[0] = pending.wire.direction.x;
    journal.direction[1] = pending.wire.direction.y;
    journal.direction[2] = pending.wire.direction.z;
    journal.practiceOperation =
        static_cast<u16_t>(pending.wire.practiceOperation);
    journal.practiceValue = pending.wire.practiceValue;
    journal.practiceFlags = pending.wire.practiceFlags;
    journal.clientTick = pending.wire.clientTick;
    Winters::Replay::SetReplayCommandDomain(journal, domain);
    m_pReplayRecorder->RecordCommand(tick, journal);
}

void CGameRoom::RecordPendingReplayCommand(
    u64_t tick,
    const GameCommand& command,
    Winters::Replay::eReplayJournalOutcome outcome)
{
    const u64_t key = MakeReplayCommandKey(
        command.sourceSessionId,
        command.sequenceNum);
    const auto it = m_pendingReplayCommands.find(key);
    if (it == m_pendingReplayCommands.end())
        return;

    const PendingCommand pending = it->second;
    m_pendingReplayCommands.erase(it);
    RecordReplayCommand(
        tick,
        pending,
        ClassifyReplayCommandDomain(pending.wire),
        outcome);
}

void CGameRoom::Phase_DrainCommands(TickContext& tc)
{
    std::vector<PendingCommand> drained = m_commandIngress.DrainSorted();

    for (const auto& pending : drained)
    {
        const EntityID controlledEntity = m_sessionBinding.ResolveControlledEntity(
            pending.sessionId,
            m_world,
            m_entityMap,
            m_pLobbyAuthority.get());
        if (controlledEntity == NULL_ENTITY)
            continue;

        GameCommand cmd = BuildServerCommand(
            pending.wire, pending.sessionId, controlledEntity, m_entityMap);
        cmd.issuedAtTick = tc.tickIndex;
        cmd.rewindTicks = 0;

        CCommandIngress::TraceCommandTiming(pending, tc.tickIndex);

        m_pendingExecCommands.push_back(cmd);
        m_lastSimCommandSeqBySession[pending.sessionId] = pending.sequenceNum;

        m_pendingReplayCommands[
            MakeReplayCommandKey(pending.sessionId, pending.sequenceNum)] =
            pending;
    }
}

void CGameRoom::TickPausedControlLane()
{
    const GameplayDefinitionPack& definitions =
        ServerData::GetActiveLoLGameplayDefinitionPack();
    TickContext tc{
        m_tickIndex,
        DeterministicTime::kFixedDt,
        DeterministicTime::TickToSec(m_tickIndex),
        &m_rng, &m_entityMap, NULL_ENTITY, this
    };
    tc.pLagCompensation = m_pLagCompensation.get();
    tc.pDefinitions = &definitions;

    // While paused, only control-lane commands mutate state; gameplay
    // inputs are consumed as void but still acked so client prediction prunes.
    std::vector<PendingCommand> drained = m_commandIngress.DrainSorted();
    for (const auto& pending : drained)
    {
        m_lastSimCommandSeqBySession[pending.sessionId] = pending.sequenceNum;

        if (pending.wire.kind != eCommandKind::PracticeControl &&
            pending.wire.kind != eCommandKind::AIDebugControl)
        {
            RecordReplayCommand(
                tc.tickIndex,
                pending,
                Winters::Replay::eReplayCommandDomain::PlayerInput,
                Winters::Replay::eReplayJournalOutcome::PausedGameplayVoid);
            continue;
        }

        const Winters::Replay::eReplayCommandDomain domain =
            ClassifyReplayCommandDomain(pending.wire);

        const EntityID controlledEntity = m_sessionBinding.ResolveControlledEntity(
            pending.sessionId,
            m_world,
            m_entityMap,
            m_pLobbyAuthority.get());
        if (controlledEntity == NULL_ENTITY)
        {
            RecordReplayCommand(
                tc.tickIndex,
                pending,
                domain,
                Winters::Replay::eReplayJournalOutcome::RejectedToolCommand);
            continue;
        }

        GameCommand cmd = BuildServerCommand(
            pending.wire, pending.sessionId, controlledEntity, m_entityMap);
        cmd.issuedAtTick = m_tickIndex;
        cmd.rewindTicks = 0;

        bool_t bAccepted = false;
        if (TryHandlePracticeControl(tc, cmd, bAccepted))
        {
            const bool_t bCommitDeferred =
                m_PendingPracticeControlChange.eKind !=
                    PracticeControlChangeKind::None &&
                m_PendingPracticeControlChange.uSessionId ==
                    cmd.sourceSessionId &&
                m_PendingPracticeControlChange.tCommand.sequenceNum ==
                    cmd.sequenceNum;
            if (bCommitDeferred)
                bAccepted = CommitPendingPracticeControlChange(tc);
            RecordReplayCommand(
                tc.tickIndex,
                pending,
                domain,
                bAccepted
                    ? Winters::Replay::eReplayJournalOutcome::AcceptedToolCommand
                    : Winters::Replay::eReplayJournalOutcome::RejectedToolCommand);
            continue;
        }
        if (TryHandleAIDebugControl(tc, cmd, bAccepted))
        {
            RecordReplayCommand(
                tc.tickIndex,
                pending,
                domain,
                bAccepted
                    ? Winters::Replay::eReplayJournalOutcome::AcceptedToolCommand
                    : Winters::Replay::eReplayJournalOutcome::RejectedToolCommand);
            continue;
        }
        m_pExecutor->ExecuteCommand(m_world, tc, cmd);
    }

    Phase_BroadcastSnapshot(tc);
}

void CGameRoom::CaptureKeyframeIfDue(const TickContext& tc)
{
#if defined(_DEBUG)
    static constexpr u64_t kKeyframeIntervalTicks = 30ull;
    static constexpr size_t kKeyframeCapacity = 90u;

    if (tc.tickIndex == 0ull || (tc.tickIndex % kKeyframeIntervalTicks) != 0ull)
        return;

    RoomKeyframe keyframe{};
    keyframe.tick = tc.tickIndex;
    if (!SimCheckpoint::SaveWorldKeyframe(
        m_world, m_rng, m_entityMap, tc.tickIndex, keyframe.simBytes))
    {
        std::cerr << "[ChronoBreak] keyframe capture failed at tick "
            << tc.tickIndex << "\n";
        return;
    }
    keyframe.waveState = m_serverMinionWaves.CaptureWaveState();
    keyframe.turretActivationAccum =
        m_pTurretAI ? m_pTurretAI->GetActivationAccum() : 0.f;
    keyframe.bPracticeModeEnabled = m_bPracticeModeEnabled;

    m_keyframes.push_back(std::move(keyframe));
    if (m_keyframes.size() > kKeyframeCapacity)
        m_keyframes.erase(m_keyframes.begin());
#else
    (void)tc;
#endif
}

void CGameRoom::PerformPendingRewind()
{
#if defined(_DEBUG)
    CancelPendingPracticeControlChange(m_tickIndex);
    const u64_t targetTick = m_pendingRewindToTick;
    m_pendingRewindToTick = 0;

    const RoomKeyframe* pKeyframe = nullptr;
    for (const RoomKeyframe& kf : m_keyframes)
    {
        if (kf.tick <= targetTick && (!pKeyframe || kf.tick > pKeyframe->tick))
            pKeyframe = &kf;
    }
    if (!pKeyframe)
    {
        std::cerr << "[ChronoBreak] rewind aborted - no keyframe <= tick "
            << targetTick << "\n";
        return;
    }

    u64_t restoredTick = 0;
    if (!SimCheckpoint::RestoreWorldKeyframe(
        m_world, m_rng, m_entityMap, restoredTick, pKeyframe->simBytes))
    {
        std::cerr << "[ChronoBreak] rewind rejected - room sim state unchanged\n";
        return;
    }

    m_tickIndex = restoredTick;
    m_visibleTickIndex.store(restoredTick, std::memory_order_relaxed);
    m_serverMinionWaves.RestoreWaveState(pKeyframe->waveState);
    if (m_pTurretAI)
        m_pTurretAI->SetActivationAccum(pKeyframe->turretActivationAccum);
    m_bPracticeModeEnabled = pKeyframe->bPracticeModeEnabled;

    m_PracticeSpawnedEntities =
        DeterministicEntityIterator<PracticeSpawnedTag>::CollectSorted(m_world);

    m_commandIngress.Clear();
    m_pendingExecCommands.clear();
    m_pendingReplayCommands.clear();
    if (m_pLagCompensation)
    {
        m_pLagCompensation->Reset();
        m_pLagCompensation->RecordHistory(m_world, restoredTick);
    }
    m_lastBroadcastActionSeq.clear();
    m_lastReplaySnapshotTick = ~0ull;
    m_lastReplayToolRevision = ~0ull;

    // 복원 틱보다 미래의 키프레임은 무효 — 폐기(타임라인 포크).
    m_keyframes.erase(
        std::remove_if(m_keyframes.begin(), m_keyframes.end(),
            [&](const RoomKeyframe& kf) { return kf.tick > restoredTick; }),
        m_keyframes.end());

    // spatial index는 복원 상태에서 재유도 (비트 정확 직렬화는 P2 — S015 문서 참조).
    if (m_pSpatialSystem)
        m_pSpatialSystem->Execute(m_world, DeterministicTime::kFixedDt);

    // 디자이너 UX: 되감기 직후는 일시정지로 착지 — Step/Resume으로 명시적 진행.
    m_bSimPaused = true;
    m_simStepBudget = 0;

    // A branch identity changes only after the authoritative checkpoint and
    // all room-owned rewind state have been restored successfully. Failed
    // lookup/restore paths above leave both values untouched.
    ++m_timelineEpoch;
    ++m_timelineBranchId;

    std::cerr << "[ChronoBreak] rewound to tick " << restoredTick
        << " epoch=" << m_timelineEpoch
        << " branch=" << m_timelineBranchId
        << " (paused)\n";
#endif
}

void CGameRoom::OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch)
{
    if (!batch)
        return;

    const u64_t acceptedTick = GetCurrentTickIndex();
    const u64_t recvMs = static_cast<u64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    m_commandIngress.AcceptCommandBatch(
        sessionId,
        batch,
        acceptedTick,
        recvMs);
}

void CGameRoom::EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
    u64_t acceptedTick, u64_t recvTimeMs, u64_t clientTimestampMs)
{
    m_commandIngress.EnqueueCommand(
        sessionId,
        wire,
        acceptedTick,
        recvTimeMs,
        clientTimestampMs);
}

bool_t CGameRoom::TryHandlePracticeControl(
    const TickContext& tc,
    const GameCommand& cmd,
    bool_t& outAccepted)
{
    outAccepted = false;
    if (cmd.kind != eCommandKind::PracticeControl)
        return false;

#if !defined(_DEBUG)
    (void)tc;
    return true;
#else
    const auto Finish = [&](bool_t bAccepted, const char* reason)
    {
        outAccepted = bAccepted;
        TracePracticeCommand(cmd, bAccepted, reason);
        return true;
    };

    if (!m_pLobbyAuthority ||
        cmd.sourceSessionId == 0u ||
        cmd.sourceSessionId != m_pLobbyAuthority->GetHostSessionId())
    {
        return Finish(false, "host-required");
    }

    if (cmd.practiceOperation == ePracticeOperation::SetEnabled)
    {
        if (!std::isfinite(cmd.practiceValue) ||
            (cmd.practiceValue != 0.f && cmd.practiceValue != 1.f))
        {
            return Finish(false, "enabled-must-be-zero-or-one");
        }

        const bool_t bEnable = cmd.practiceValue == 1.f;
        if (!bEnable)
        {
            CancelPendingPracticeControlChange(tc.tickIndex);
            m_bSimPaused = false;
            m_simStepBudget = 0;
            m_simSpeedMul.store(1.f, std::memory_order_relaxed);
            ClearPracticeSpawns();
            if (cmd.issuerEntity != NULL_ENTITY && m_world.IsAlive(cmd.issuerEntity))
            {
                if (m_world.HasComponent<PracticePlayerComponent>(cmd.issuerEntity))
                    m_world.RemoveComponent<PracticePlayerComponent>(cmd.issuerEntity);
            }
            // 오버라이드는 대상 확장으로 임의 챔피언에 붙을 수 있으므로 전체 스윕.
            const auto overrideEntities =
                DeterministicEntityIterator<PracticeSkillEffectOverrideComponent>
                    ::CollectSorted(m_world);
            for (EntityID overrideEntity : overrideEntities)
            {
                if (m_world.IsAlive(overrideEntity))
                {
                    m_world.RemoveComponent<PracticeSkillEffectOverrideComponent>(
                        overrideEntity);
                }
            }

            const auto championStatOverrideEntities =
                DeterministicEntityIterator<PracticeChampionStatOverrideComponent>
                    ::CollectSorted(m_world);
            for (EntityID overrideEntity : championStatOverrideEntities)
            {
                if (!m_world.IsAlive(overrideEntity))
                    continue;
                m_world.RemoveComponent<PracticeChampionStatOverrideComponent>(
                    overrideEntity);
                if (m_world.HasComponent<StatComponent>(overrideEntity))
                    m_world.GetComponent<StatComponent>(overrideEntity).bDirty = true;
            }

            const auto itemStatOverrideEntities =
                DeterministicEntityIterator<PracticeItemStatOverrideComponent>
                    ::CollectSorted(m_world);
            for (EntityID overrideEntity : itemStatOverrideEntities)
            {
                if (!m_world.IsAlive(overrideEntity))
                    continue;
                m_world.RemoveComponent<PracticeItemStatOverrideComponent>(overrideEntity);
                if (m_world.HasComponent<StatComponent>(overrideEntity))
                    m_world.GetComponent<StatComponent>(overrideEntity).bDirty = true;
            }
        }

        m_bPracticeModeEnabled = bEnable;
        return Finish(true, bEnable ? "enabled" : "disabled-and-cleared");
    }

    if (!m_bPracticeModeEnabled)
        return Finish(false, "room-disabled");

    if (cmd.practiceOperation == ePracticeOperation::None)
        return Finish(false, "operation-none");

    if (cmd.issuerEntity == NULL_ENTITY || !m_world.IsAlive(cmd.issuerEntity))
        return Finish(false, "issuer-not-alive");

    switch (cmd.practiceOperation)
    {
    case ePracticeOperation::SetSimulationPaused:
    {
        if (!std::isfinite(cmd.practiceValue) ||
            (cmd.practiceValue != 0.f && cmd.practiceValue != 1.f))
        {
            return Finish(false, "paused-must-be-zero-or-one");
        }
        m_bSimPaused = cmd.practiceValue == 1.f;
        if (!m_bSimPaused)
            m_simStepBudget = 0;
        return Finish(true, m_bSimPaused ? "sim-paused" : "sim-resumed");
    }

    case ePracticeOperation::StepSimulationTicks:
    {
        if (!m_bSimPaused)
            return Finish(false, "step-requires-pause");
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < 1.f || cmd.practiceValue > 300.f)
        {
            return Finish(false, "step-out-of-range");
        }
        m_simStepBudget += static_cast<u32_t>(cmd.practiceValue);
        return Finish(true, "step-scheduled");
    }

    case ePracticeOperation::SetSimulationTimeScale:
    {
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < 0.1f || cmd.practiceValue > 8.f)
        {
            return Finish(false, "timescale-out-of-range");
        }
        m_simSpeedMul.store(cmd.practiceValue, std::memory_order_relaxed);
        return Finish(true, "timescale-set");
    }

    case ePracticeOperation::RewindSimulationSeconds:
    {
        if (m_PendingPracticeControlChange.eKind !=
            PracticeControlChangeKind::None)
        {
            return Finish(false, "rewind-conflicts-with-control-change");
        }
        constexpr f32_t kMinRewindSeconds =
            1.f / static_cast<f32_t>(DeterministicTime::kTicksPerSecond);
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < kMinRewindSeconds || cmd.practiceValue > 60.f)
        {
            return Finish(false, "rewind-seconds-out-of-range");
        }
        const u64_t rewindTicks = std::max<u64_t>(
            1ull,
            static_cast<u64_t>(std::llround(
                static_cast<f64_t>(cmd.practiceValue) *
                static_cast<f64_t>(DeterministicTime::kTicksPerSecond))));
        const u64_t targetTick =
            m_tickIndex > rewindTicks ? m_tickIndex - rewindTicks : 1ull;
        bool_t bHasKeyframe = false;
        for (const RoomKeyframe& kf : m_keyframes)
        {
            if (kf.tick <= targetTick)
            {
                bHasKeyframe = true;
                break;
            }
        }
        if (!bHasKeyframe)
            return Finish(false, "rewind-no-keyframe");
        m_pendingRewindToTick = targetTick;
        return Finish(true, "rewind-scheduled");
    }

    case ePracticeOperation::SetOptions:
    {
        if ((cmd.practiceFlags & ~kPracticeAllOptionFlags) != 0u)
            return Finish(false, "unknown-option-flag");
        if (!m_world.HasComponent<ChampionComponent>(cmd.issuerEntity))
            return Finish(false, "issuer-not-champion");

        auto& practice = m_world.HasComponent<PracticePlayerComponent>(cmd.issuerEntity)
            ? m_world.GetComponent<PracticePlayerComponent>(cmd.issuerEntity)
            : m_world.AddComponent<PracticePlayerComponent>(
                cmd.issuerEntity,
                PracticePlayerComponent{});
        practice.optionFlags = cmd.practiceFlags;
        ++practice.revision;
        return Finish(true, "options-replaced");
    }

    case ePracticeOperation::RestoreHealthMana:
    {
        if (!m_world.HasComponent<HealthComponent>(cmd.issuerEntity) ||
            !m_world.HasComponent<ChampionComponent>(cmd.issuerEntity))
        {
            return Finish(false, "health-or-champion-missing");
        }

        auto& health = m_world.GetComponent<HealthComponent>(cmd.issuerEntity);
        auto& champion = m_world.GetComponent<ChampionComponent>(cmd.issuerEntity);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return Finish(false, "restore-does-not-revive");

        health.fCurrent = health.fMaximum;
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
        champion.mana = champion.maxMana;
        return Finish(true, "health-mana-restored");
    }

    case ePracticeOperation::ResetCooldowns:
        ResetPracticeCooldowns(m_world, cmd.issuerEntity);
        return Finish(true, "cooldowns-reset");

    case ePracticeOperation::AddGold:
    {
        if (!std::isfinite(cmd.practiceValue) || cmd.practiceValue <= 0.f)
            return Finish(false, "gold-must-be-finite-positive");
        if (!m_world.HasComponent<GoldComponent>(cmd.issuerEntity))
            return Finish(false, "gold-component-missing");

        auto& gold = m_world.GetComponent<GoldComponent>(cmd.issuerEntity);
        gold.amount = (std::min)(gold.amount, kPracticeGoldCap);
        const u32_t addition = static_cast<u32_t>((std::min)(
            cmd.practiceValue,
            static_cast<f32_t>(kPracticeGoldCap)));
        const u32_t remaining = kPracticeGoldCap - gold.amount;
        gold.amount += (std::min)(addition, remaining);
        return Finish(true, "gold-added");
    }

    case ePracticeOperation::SetLevel:
    {
        u8_t level = 0u;
        if (!TryResolvePracticeByte(
            cmd.practiceValue,
            1u,
            ChampionExperienceCurveDef::kMaxChampionLevel,
            level))
        {
            return Finish(false, "level-out-of-range");
        }
        if (!m_world.HasComponent<StatComponent>(cmd.issuerEntity) ||
            !m_world.HasComponent<ChampionComponent>(cmd.issuerEntity) ||
            !m_world.HasComponent<SkillRankComponent>(cmd.issuerEntity))
        {
            return Finish(false, "level-components-missing");
        }

        CExperienceSystem::InitializeChampionExperience(
            m_world,
            cmd.issuerEntity,
            level);
        auto& stat = m_world.GetComponent<StatComponent>(cmd.issuerEntity);
        stat.level = level;
        stat.bDirty = true;
        m_world.GetComponent<ChampionComponent>(cmd.issuerEntity).level = level;
        CSkillRankSystem::SyncPointsForLevel(
            m_world.GetComponent<SkillRankComponent>(cmd.issuerEntity),
            level);
        return Finish(true, "level-set");
    }

    case ePracticeOperation::Teleport:
    {
        if (!IsFinitePracticePosition(cmd.groundPos))
            return Finish(false, "teleport-position-non-finite");
        if (!m_world.HasComponent<TransformComponent>(cmd.issuerEntity))
            return Finish(false, "transform-missing");

        Vec3 resolvedPosition{};
        if (!TryResolveServerWalkablePosition(
            cmd.groundPos,
            16,
            resolvedPosition))
        {
            return Finish(false, "teleport-nav-reject");
        }

        if (m_world.HasComponent<MoveTargetComponent>(cmd.issuerEntity))
        {
            m_world.GetComponent<MoveTargetComponent>(cmd.issuerEntity) =
                MoveTargetComponent{};
        }
        if (m_world.HasComponent<AttackChaseComponent>(cmd.issuerEntity))
            m_world.RemoveComponent<AttackChaseComponent>(cmd.issuerEntity);
        if (m_world.HasComponent<CombatActionComponent>(cmd.issuerEntity))
            m_world.RemoveComponent<CombatActionComponent>(cmd.issuerEntity);
        if (m_world.HasComponent<RecallComponent>(cmd.issuerEntity))
            m_world.RemoveComponent<RecallComponent>(cmd.issuerEntity);

        StartActionState(
            m_world,
            cmd.issuerEntity,
            eActionStateId::None,
            tc.tickIndex);
        GameplayStatus::ClearStatusEffects(m_world, cmd.issuerEntity);
        if (m_world.HasComponent<ForcedMotionComponent>(cmd.issuerEntity))
            m_world.RemoveComponent<ForcedMotionComponent>(cmd.issuerEntity);
        if (m_world.HasComponent<VelocityComponent>(cmd.issuerEntity))
            m_world.GetComponent<VelocityComponent>(cmd.issuerEntity) = VelocityComponent{};

        m_world.GetComponent<TransformComponent>(cmd.issuerEntity).SetPosition(
            resolvedPosition);
        PositionDiscontinuityComponent& discontinuity =
            m_world.HasComponent<PositionDiscontinuityComponent>(cmd.issuerEntity)
                ? m_world.GetComponent<PositionDiscontinuityComponent>(cmd.issuerEntity)
                : m_world.AddComponent<PositionDiscontinuityComponent>(
                    cmd.issuerEntity,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = tc.tickIndex;
        SetPoseState(
            m_world,
            cmd.issuerEntity,
            ePoseStateId::Idle,
            tc.tickIndex,
            true);
        return Finish(true, "teleported-nav-resolved");
    }

    case ePracticeOperation::SpawnMinion:
    {
        u8_t team = 0u;
        if (!TryResolvePracticeByte(cmd.practiceValue, 0u, 1u, team))
            return Finish(false, "minion-team-out-of-range");
        if (cmd.slot > 3u)
            return Finish(false, "minion-role-out-of-range");
        if (cmd.practiceFlags > 2u)
            return Finish(false, "minion-lane-out-of-range");
        if (!IsFinitePracticePosition(cmd.groundPos))
            return Finish(false, "minion-position-non-finite");

        m_PracticeSpawnedEntities.erase(
            std::remove_if(
                m_PracticeSpawnedEntities.begin(),
                m_PracticeSpawnedEntities.end(),
                [&](EntityID entity)
                {
                    return !m_world.IsAlive(entity) ||
                        !m_world.HasComponent<PracticeSpawnedTag>(entity);
                }),
            m_PracticeSpawnedEntities.end());
        if (m_PracticeSpawnedEntities.size() >= kPracticeSpawnQuota)
            return Finish(false, "minion-room-quota");

        Vec3 resolvedPosition{};
        if (!TryResolveServerWalkablePosition(
            cmd.groundPos,
            16,
            resolvedPosition))
        {
            return Finish(false, "minion-nav-reject");
        }

        const EntityID minion = SpawnServerMinion(
            static_cast<eTeam>(team),
            cmd.slot,
            static_cast<u8_t>(cmd.practiceFlags),
            resolvedPosition);
        if (minion == NULL_ENTITY || !m_world.IsAlive(minion))
            return Finish(false, "minion-spawn-failed");

        PracticeSpawnedTag tag{};
        tag.ownerEntity = cmd.issuerEntity;
        m_world.AddComponent<PracticeSpawnedTag>(minion, tag);
        m_PracticeSpawnedEntities.push_back(minion);
        return Finish(true, "minion-spawned");
    }

    case ePracticeOperation::ClearPracticeSpawns:
        ClearPracticeSpawns();
        return Finish(true, "practice-spawns-cleared");

    case ePracticeOperation::TakeControlRosterChampion:
    {
        if (m_pendingRewindToTick != 0u)
            return Finish(false, "control-change-conflicts-with-rewind");
        if (!HasStrictAttackSpeedLabRoster(
            m_world,
            m_entityMap,
            m_pLobbyAuthority.get(),
            cmd.sourceSessionId))
        {
            return Finish(false, "strict-5v5-human1-bot9-required");
        }
        if (m_PendingPracticeControlChange.eKind !=
            PracticeControlChangeKind::None)
        {
            return Finish(false, "control-change-already-pending");
        }
        if (cmd.targetEntity == NULL_ENTITY ||
            cmd.targetEntity == cmd.issuerEntity ||
            !m_world.IsAlive(cmd.targetEntity) ||
            !m_world.HasComponent<ChampionComponent>(cmd.targetEntity))
        {
            return Finish(false, "roster-control-target-invalid");
        }

        const NetEntityId sourceNet = m_entityMap.ToNet(cmd.issuerEntity);
        const NetEntityId targetNet = m_entityMap.ToNet(cmd.targetEntity);
        bool_t bRosterBot = false;
        const LobbySlotState* pSlots = GetLobbySlots();
        for (u32_t i = 0u; pSlots && i < GetLobbySlotCount(); ++i)
        {
            if (pSlots[i].netId == targetNet &&
                pSlots[i].bBot &&
                !pSlots[i].bHuman &&
                !pSlots[i].bDummy)
            {
                bRosterBot = true;
                break;
            }
        }
        if (sourceNet == NULL_NET_ENTITY ||
            targetNet == NULL_NET_ENTITY ||
            !bRosterBot)
        {
            return Finish(false, "target-is-not-roster-bot");
        }

        m_PendingPracticeControlChange.eKind =
            PracticeControlChangeKind::TakeRosterChampion;
        m_PendingPracticeControlChange.uSessionId = cmd.sourceSessionId;
        m_PendingPracticeControlChange.uSourceNetId = sourceNet;
        m_PendingPracticeControlChange.uTargetNetId = targetNet;
        m_PendingPracticeControlChange.tCommand = cmd;
        return Finish(true, "roster-control-change-pending");
    }

    case ePracticeOperation::ReplaceControlledChampion:
    {
        if (m_pendingRewindToTick != 0u)
            return Finish(false, "control-change-conflicts-with-rewind");
        if (!HasStrictAttackSpeedLabRoster(
            m_world,
            m_entityMap,
            m_pLobbyAuthority.get(),
            cmd.sourceSessionId))
        {
            return Finish(false, "strict-5v5-human1-bot9-required");
        }
        if (m_PendingPracticeControlChange.eKind !=
            PracticeControlChangeKind::None)
        {
            return Finish(false, "control-change-already-pending");
        }
        if (m_bSimPaused)
            return Finish(false, "replacement-requires-running-simulation");
        if (cmd.practiceFlags >
            static_cast<u32_t>((std::numeric_limits<u8_t>::max)()))
        {
            return Finish(false, "replacement-champion-id-out-of-range");
        }

        const eChampion champion = static_cast<eChampion>(
            static_cast<u8_t>(cmd.practiceFlags));
        const GameplayDefinitionPack& definitions =
            ServerData::GetActiveLoLGameplayDefinitionPack();
        if (champion == eChampion::NONE ||
            champion == eChampion::END ||
            !definitions.FindChampion(champion))
        {
            return Finish(false, "replacement-champion-not-registered");
        }

        const NetEntityId sourceNet = m_entityMap.ToNet(cmd.issuerEntity);
        if (sourceNet == NULL_NET_ENTITY)
            return Finish(false, "replacement-source-net-missing");

        m_PendingPracticeControlChange.eKind =
            PracticeControlChangeKind::ReplaceControlledChampion;
        m_PendingPracticeControlChange.uSessionId = cmd.sourceSessionId;
        m_PendingPracticeControlChange.uSourceNetId = sourceNet;
        m_PendingPracticeControlChange.eChampionId = champion;
        m_PendingPracticeControlChange.tCommand = cmd;
        return Finish(true, "controlled-champion-replacement-pending");
    }

    case ePracticeOperation::SpawnChampion:
    {
        u8_t team = 0u;
        if (!TryResolvePracticeByte(cmd.practiceValue, 0u, 1u, team))
            return Finish(false, "champion-team-out-of-range");
        if (cmd.slot > 1u)
            return Finish(false, "champion-brain-out-of-range");
        if (cmd.practiceFlags < static_cast<u32_t>(eChampion::IRELIA) ||
            cmd.practiceFlags > static_cast<u32_t>(eChampion::LEESIN))
        {
            return Finish(false, "champion-id-out-of-range");
        }
        if (!IsFinitePracticePosition(cmd.groundPos))
            return Finish(false, "champion-position-non-finite");

        const auto champion = static_cast<eChampion>(cmd.practiceFlags);
        const bool_t bDummy = cmd.slot == 0u;
        if (bDummy && champion == eChampion::SYLAS)
            return Finish(false, "champion-dummy-sylas-reserved");

        m_PracticeSpawnedEntities.erase(
            std::remove_if(
                m_PracticeSpawnedEntities.begin(),
                m_PracticeSpawnedEntities.end(),
                [&](EntityID entity)
                {
                    return !m_world.IsAlive(entity) ||
                        !m_world.HasComponent<PracticeSpawnedTag>(entity);
                }),
            m_PracticeSpawnedEntities.end());
        if (m_PracticeSpawnedEntities.size() >= kPracticeSpawnQuota)
            return Finish(false, "champion-room-quota");

        Vec3 resolvedPosition{};
        if (!TryResolveServerWalkablePosition(
            cmd.groundPos,
            16,
            resolvedPosition))
        {
            return Finish(false, "champion-nav-reject");
        }

        LobbySlotState practiceSlot{};
        practiceSlot.team = team;
        practiceSlot.bBot = true;
        practiceSlot.bDummy = bDummy;
        practiceSlot.champion = champion;
        const EntityID championEntity = SpawnChampionForLobbySlot(practiceSlot);
        if (championEntity == NULL_ENTITY || !m_world.IsAlive(championEntity))
            return Finish(false, "champion-spawn-failed");

        if (m_world.HasComponent<TransformComponent>(championEntity))
        {
            m_world.GetComponent<TransformComponent>(championEntity)
                .SetPosition(resolvedPosition);
        }
        if (m_world.HasComponent<RespawnComponent>(championEntity))
        {
            m_world.GetComponent<RespawnComponent>(championEntity).spawnPos =
                resolvedPosition;
        }

        PracticeSpawnedTag tag{};
        tag.ownerEntity = cmd.issuerEntity;
        m_world.AddComponent<PracticeSpawnedTag>(championEntity, tag);
        m_PracticeSpawnedEntities.push_back(championEntity);
        return Finish(true, "champion-spawned");
    }

    case ePracticeOperation::ApplySkillEffectOverride:
    {
        if (cmd.slot > 4u)
            return Finish(false, "override-slot-out-of-range");
        if (!IsValidPracticeEffectParam(cmd.practiceFlags))
            return Finish(false, "override-param-invalid");

        const auto param = static_cast<eSkillEffectParamId>(cmd.practiceFlags);
        if (!IsValidPracticeEffectValue(param, cmd.practiceValue))
            return Finish(false, "override-value-out-of-range");

        // targetNet 지정 시 해당 챔피언(적 봇 포함)에 오버라이드를 부착 — 미지정이면 기존처럼 본인.
        const EntityID overrideTarget =
            cmd.targetEntity != NULL_ENTITY ? cmd.targetEntity : cmd.issuerEntity;
        if (!m_world.IsAlive(overrideTarget) ||
            !m_world.HasComponent<ChampionComponent>(overrideTarget))
        {
            return Finish(false, "override-target-not-champion");
        }

        auto& overrides =
            m_world.HasComponent<PracticeSkillEffectOverrideComponent>(overrideTarget)
            ? m_world.GetComponent<PracticeSkillEffectOverrideComponent>(overrideTarget)
            : m_world.AddComponent<PracticeSkillEffectOverrideComponent>(
                overrideTarget,
                PracticeSkillEffectOverrideComponent{});

        const u8_t paramId = static_cast<u8_t>(param);
        for (u8_t index = 0u; index < overrides.count; ++index)
        {
            auto& entry = overrides.entries[index];
            if (entry.slot == cmd.slot && entry.paramId == paramId)
            {
                entry.value = cmd.practiceValue;
                ++overrides.revision;
                return Finish(true, "override-replaced");
            }
        }

        if (overrides.count >= PracticeSkillEffectOverrideComponent::kMaxEntries)
            return Finish(false, "override-capacity");

        auto& entry = overrides.entries[overrides.count++];
        entry.slot = cmd.slot;
        entry.paramId = paramId;
        entry.value = cmd.practiceValue;
        ++overrides.revision;
        return Finish(true, "override-added");
    }

    case ePracticeOperation::ClearSkillEffectOverrides:
    {
        const EntityID clearTarget =
            cmd.targetEntity != NULL_ENTITY ? cmd.targetEntity : cmd.issuerEntity;
        if (m_world.IsAlive(clearTarget) &&
            m_world.HasComponent<PracticeSkillEffectOverrideComponent>(clearTarget))
        {
            m_world.RemoveComponent<PracticeSkillEffectOverrideComponent>(clearTarget);
        }
        return Finish(true, "overrides-cleared");
    }

    case ePracticeOperation::ApplyChampionStatOverride:
    {
        if (cmd.slot == 0u ||
            cmd.slot >= static_cast<u8_t>(eChampionStatOverrideId::Count))
        {
            return Finish(false, "stat-override-id-invalid");
        }
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < 0.f ||
            cmd.practiceValue > kPracticeOverrideValueMax)
        {
            return Finish(false, "stat-override-value-out-of-range");
        }

        const eChampionStatOverrideId statId =
            static_cast<eChampionStatOverrideId>(cmd.slot);
        if (statId == eChampionStatOverrideId::EffectiveAttackSpeed &&
            (cmd.practiceValue < kAttackSpeedLabMin ||
                cmd.practiceValue > kAttackSpeedLabMax))
        {
            return Finish(false, "effective-attack-speed-out-of-range");
        }

        const EntityID overrideTarget =
            cmd.targetEntity != NULL_ENTITY ? cmd.targetEntity : cmd.issuerEntity;
        if (!m_world.IsAlive(overrideTarget) ||
            !m_world.HasComponent<ChampionComponent>(overrideTarget))
        {
            return Finish(false, "stat-override-target-not-champion");
        }

        auto& overrides =
            m_world.HasComponent<PracticeChampionStatOverrideComponent>(overrideTarget)
            ? m_world.GetComponent<PracticeChampionStatOverrideComponent>(overrideTarget)
            : m_world.AddComponent<PracticeChampionStatOverrideComponent>(
                overrideTarget,
                PracticeChampionStatOverrideComponent{});

        bool_t bStored = false;
        for (u8_t index = 0u; index < overrides.count; ++index)
        {
            auto& entry = overrides.entries[index];
            if (entry.statId == cmd.slot)
            {
                entry.value = cmd.practiceValue;
                ++overrides.revision;
                bStored = true;
                break;
            }
        }
        if (!bStored)
        {
            if (overrides.count >= PracticeChampionStatOverrideComponent::kMaxEntries)
                return Finish(false, "stat-override-capacity");
            auto& entry = overrides.entries[overrides.count++];
            entry.statId = cmd.slot;
            entry.value = cmd.practiceValue;
            ++overrides.revision;
        }

        if (m_world.HasComponent<StatComponent>(overrideTarget))
            m_world.GetComponent<StatComponent>(overrideTarget).bDirty = true;
        return Finish(true, bStored ? "stat-override-replaced" : "stat-override-added");
    }

    case ePracticeOperation::ClearChampionStatOverrides:
    {
        const EntityID clearTarget =
            cmd.targetEntity != NULL_ENTITY ? cmd.targetEntity : cmd.issuerEntity;
        if (m_world.IsAlive(clearTarget) &&
            m_world.HasComponent<PracticeChampionStatOverrideComponent>(clearTarget))
        {
            m_world.RemoveComponent<PracticeChampionStatOverrideComponent>(clearTarget);
            if (m_world.HasComponent<StatComponent>(clearTarget))
                m_world.GetComponent<StatComponent>(clearTarget).bDirty = true;
        }
        return Finish(true, "stat-overrides-cleared");
    }

    case ePracticeOperation::ApplyItemStatOverride:
    {
        // flags = (itemId << 8) | eItemStatOverrideField
        const u16_t itemId = static_cast<u16_t>(cmd.practiceFlags >> 8);
        const u8_t fieldId = static_cast<u8_t>(cmd.practiceFlags & 0xFFu);
        if (fieldId == 0u ||
            fieldId >= static_cast<u8_t>(eItemStatOverrideField::Count))
        {
            return Finish(false, "item-override-field-invalid");
        }
        if (CItemRegistry::Instance().Find(itemId) == nullptr)
            return Finish(false, "item-override-item-unknown");
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < 0.f ||
            cmd.practiceValue > kPracticeOverrideValueMax)
        {
            return Finish(false, "item-override-value-out-of-range");
        }

        const EntityID overrideTarget =
            cmd.targetEntity != NULL_ENTITY ? cmd.targetEntity : cmd.issuerEntity;
        if (!m_world.IsAlive(overrideTarget) ||
            !m_world.HasComponent<ChampionComponent>(overrideTarget))
        {
            return Finish(false, "item-override-target-not-champion");
        }

        auto& overrides =
            m_world.HasComponent<PracticeItemStatOverrideComponent>(overrideTarget)
            ? m_world.GetComponent<PracticeItemStatOverrideComponent>(overrideTarget)
            : m_world.AddComponent<PracticeItemStatOverrideComponent>(
                overrideTarget,
                PracticeItemStatOverrideComponent{});

        bool_t bStored = false;
        for (u8_t index = 0u; index < overrides.count; ++index)
        {
            auto& entry = overrides.entries[index];
            if (entry.itemId == itemId && entry.fieldId == fieldId)
            {
                entry.value = cmd.practiceValue;
                ++overrides.revision;
                bStored = true;
                break;
            }
        }
        if (!bStored)
        {
            if (overrides.count >= PracticeItemStatOverrideComponent::kMaxEntries)
                return Finish(false, "item-override-capacity");
            auto& entry = overrides.entries[overrides.count++];
            entry.itemId = itemId;
            entry.fieldId = fieldId;
            entry.value = cmd.practiceValue;
            ++overrides.revision;
        }

        if (m_world.HasComponent<StatComponent>(overrideTarget))
            m_world.GetComponent<StatComponent>(overrideTarget).bDirty = true;
        return Finish(true, bStored ? "item-override-replaced" : "item-override-added");
    }

    case ePracticeOperation::ClearItemStatOverrides:
    {
        const EntityID clearTarget =
            cmd.targetEntity != NULL_ENTITY ? cmd.targetEntity : cmd.issuerEntity;
        if (m_world.IsAlive(clearTarget) &&
            m_world.HasComponent<PracticeItemStatOverrideComponent>(clearTarget))
        {
            m_world.RemoveComponent<PracticeItemStatOverrideComponent>(clearTarget);
            if (m_world.HasComponent<StatComponent>(clearTarget))
                m_world.GetComponent<StatComponent>(clearTarget).bDirty = true;
        }
        return Finish(true, "item-overrides-cleared");
    }

    case ePracticeOperation::ApplyStructureStatOverride:
    {
        // slot = eStructureStatOverrideId, value = 대체값. 대상 kind 의 모든 구조물에 즉시 적용.
        if (cmd.slot == 0u ||
            cmd.slot >= static_cast<u8_t>(eStructureStatOverrideId::Count))
        {
            return Finish(false, "structure-stat-id-invalid");
        }
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < 1.f ||
            cmd.practiceValue > kPracticeOverrideValueMax)
        {
            return Finish(false, "structure-stat-value-out-of-range");
        }

        const eStructureStatOverrideId statId =
            static_cast<eStructureStatOverrideId>(cmd.slot);
        u32_t appliedCount = 0u;

        if (statId == eStructureStatOverrideId::TurretAttackDamage)
        {
            m_world.ForEach<TurretAIComponent>(
                [&](EntityID, TurretAIComponent& ai)
                {
                    ai.attackDamage = cmd.practiceValue;
                    ++appliedCount;
                });
            return Finish(appliedCount > 0u,
                appliedCount > 0u ? "turret-damage-applied" : "structure-stat-no-target");
        }

        eStructureKind targetKind = eStructureKind::Turret;
        if (statId == eStructureStatOverrideId::InhibitorMaxHp)
            targetKind = eStructureKind::Inhibitor;
        else if (statId == eStructureStatOverrideId::NexusMaxHp)
            targetKind = eStructureKind::Nexus;

        m_world.ForEach<StructureComponent, HealthComponent>(
            [&](EntityID entity, StructureComponent& structure, HealthComponent& health)
            {
                if (structure.kind != static_cast<u32_t>(targetKind) || health.bIsDead)
                    return;
                health.fMaximum = cmd.practiceValue;
                health.fCurrent = cmd.practiceValue;
                structure.hp = health.fCurrent;
                structure.maxHp = health.fMaximum;
                if (m_world.HasComponent<TurretComponent>(entity))
                {
                    auto& turret = m_world.GetComponent<TurretComponent>(entity);
                    turret.hp = health.fCurrent;
                    turret.maxHp = health.fMaximum;
                }
                ++appliedCount;
            });
        return Finish(appliedCount > 0u,
            appliedCount > 0u ? "structure-hp-applied" : "structure-stat-no-target");
    }

    case ePracticeOperation::ClearStructureStatOverrides:
    {
        const StructureGameDef& structureDef =
            ServerData::GetActiveLoLSpawnObjectDefinitionPack().structure;
        m_world.ForEach<StructureComponent, HealthComponent>(
            [&](EntityID entity, StructureComponent& structure, HealthComponent& health)
            {
                if (health.bIsDead)
                    return;
                const f32_t defMax =
                    ServerData::GetActiveLoLSpawnObjectDefinitionPack().ResolveStructureMaxHp(
                        static_cast<eStructureKind>(structure.kind));
                health.fMaximum = defMax;
                health.fCurrent = defMax;
                structure.hp = defMax;
                structure.maxHp = defMax;
                if (m_world.HasComponent<TurretComponent>(entity))
                {
                    auto& turret = m_world.GetComponent<TurretComponent>(entity);
                    turret.hp = defMax;
                    turret.maxHp = defMax;
                }
                if (m_world.HasComponent<TurretAIComponent>(entity))
                {
                    auto& ai = m_world.GetComponent<TurretAIComponent>(entity);
                    ai.attackDamage =
                        structure.tier == static_cast<u32_t>(Winters::Map::eTurretTier::Nexus)
                        ? structureDef.turretAI.nexusAttackDamage
                        : structureDef.turretAI.attackDamage;
                }
            });
        return Finish(true, "structure-stats-restored");
    }

    case ePracticeOperation::ReloadGameplayDefinitions:
    {
        std::string reloadError;
        if (!ServerData::TryReloadRuntimeGameplayDefinitions(reloadError))
        {
            std::cerr << "[Data] runtime definition reload failed: "
                << reloadError << "\n";
            return Finish(false, "definition-reload-failed");
        }

        // 스탯 계열은 재계산 트리거 필요. 스킬 효과/쿨다운/마나/사거리는 쿼리 시점 해석이라 즉시 반영.
        u32_t refreshedCount = 0u;
        m_world.ForEach<StatComponent>(
            [&](EntityID, StatComponent& stat)
            {
                // 중립몹/미니언(championId=NONE)은 챔피언 스탯 재계산 대상이
                // 아니다 — dirty 로 만들면 SpawnObject 팩 값과 어긋난다.
                if (stat.championId == eChampion::NONE)
                    return;
                stat.bDirty = true;
                ++refreshedCount;
            });

        // 보상/XP 레지스트리도 리로드된 경제 정의로 재적재 (팩 미장착 시 기존 값 유지).
        if (const EconomyGameplayDef* pEconomy =
            ServerData::GetActiveLoLGameplayDefinitionPack().FindEconomy())
        {
            CRewardRegistry::Instance().LoadFromEconomyDef(*pEconomy);
        }

        // 아이템 레지스트리도 리로드된 아이템 정의로 재적재 (팩 미장착 시 기존 값 유지).
        std::size_t itemDefCount = 0u;
        if (const ItemDef* pItemDefs =
            ServerData::GetActiveLoLGameplayDefinitionPack().FindItems(itemDefCount))
        {
            CItemRegistry::Instance().LoadFromItemDefs(pItemDefs, itemDefCount);
        }

        std::cerr << "[Data] runtime definitions reloaded rev="
            << ServerData::GetRuntimeGameplayDefinitionRevision()
            << " statRefresh=" << refreshedCount << "\n";
        return Finish(true, "definitions-reloaded");
    }

    default:
        return Finish(false, "operation-unknown");
    }
#endif
}

bool_t CGameRoom::TryHandleAIDebugControl(
    const TickContext& tc,
    const GameCommand& cmd,
    bool_t& outAccepted)
{
    outAccepted = false;
    if (cmd.kind != eCommandKind::AIDebugControl)
        return false;

#if !defined(_DEBUG)
    (void)tc;
    return true;
#else
    if (!m_pLobbyAuthority ||
        cmd.sourceSessionId == 0u ||
        cmd.sourceSessionId != m_pLobbyAuthority->GetHostSessionId())
    {
        TraceAIDebugCommand(cmd, false, "host-required");
        return true;
    }

    if (!m_bPracticeModeEnabled)
    {
        TraceAIDebugCommand(cmd, false, "practice-required");
        return true;
    }

    if (cmd.targetEntity == NULL_ENTITY ||
        !m_world.IsAlive(cmd.targetEntity) ||
        !m_world.HasComponent<ChampionAIComponent>(cmd.targetEntity))
    {
        TraceAIDebugCommand(cmd, false, "target-ai-required");
        return true;
    }

    if (cmd.itemId == kChampionAIDebugTuneRuntimeItemId)
    {
        const auto tuningId = static_cast<eChampionAITuningId>(cmd.slot);
        const ChampionAITuningParam* pParam =
            ResolveChampionAITuningParamForValidation(
                m_world.GetComponent<ChampionAIComponent>(cmd.targetEntity),
                tuningId);
        if (!pParam ||
            !std::isfinite(cmd.groundPos.x) ||
            cmd.groundPos.x < pParam->fMin ||
            cmd.groundPos.x > pParam->fMax)
        {
            TraceAIDebugCommand(cmd, false, "tuning-id-or-value-invalid");
            return true;
        }
    }
    else if (cmd.itemId != kChampionAIDebugResetTuningItemId &&
        cmd.itemId != kChampionAIDebugClearOverrideItemId)
    {
        if (cmd.itemId > static_cast<u16_t>(eChampionAIAction::Recall) ||
            (cmd.slot > static_cast<u8_t>(eSkillSlot::R) &&
                cmd.slot != kChampionAIDebugForceActionSkillSlot))
        {
            TraceAIDebugCommand(cmd, false, "action-or-slot-invalid");
            return true;
        }
    }

    m_pExecutor->ExecuteCommand(m_world, tc, cmd);
    outAccepted = true;
    TraceAIDebugCommand(cmd, true, "authorized-authoring-mutation");
    return true;
#endif
}

void CGameRoom::TickPracticeControls(const TickContext& tc)
{
    (void)tc;
#if defined(_DEBUG)
    if (!m_bPracticeModeEnabled)
        return;

    const auto entities =
        DeterministicEntityIterator<PracticePlayerComponent>::CollectSorted(m_world);
    for (EntityID entity : entities)
    {
        const u32_t flags =
            m_world.GetComponent<PracticePlayerComponent>(entity).optionFlags;

        if ((flags & kPracticeInfiniteHealthFlag) != 0u &&
            m_world.HasComponent<HealthComponent>(entity))
        {
            auto& health = m_world.GetComponent<HealthComponent>(entity);
            health.fCurrent = health.fMaximum;
            health.bIsDead = false;
            if (m_world.HasComponent<ChampionComponent>(entity))
            {
                auto& champion = m_world.GetComponent<ChampionComponent>(entity);
                champion.hp = health.fCurrent;
                champion.maxHp = health.fMaximum;
            }
        }

        if ((flags & kPracticeInfiniteManaFlag) != 0u &&
            m_world.HasComponent<ChampionComponent>(entity))
        {
            auto& champion = m_world.GetComponent<ChampionComponent>(entity);
            champion.mana = champion.maxMana;
        }

        if ((flags & kPracticeNoCooldownFlag) != 0u)
            ResetPracticeCooldowns(m_world, entity);

        if ((flags & kPracticeInfiniteGoldFlag) != 0u &&
            m_world.HasComponent<GoldComponent>(entity))
        {
            m_world.GetComponent<GoldComponent>(entity).amount = kPracticeGoldCap;
        }
    }
#endif
}

void CGameRoom::CancelPendingPracticeControlChange(u64_t tick)
{
    if (m_PendingPracticeControlChange.eKind ==
        PracticeControlChangeKind::None)
    {
        return;
    }

    const GameCommand command = m_PendingPracticeControlChange.tCommand;
    m_PendingPracticeControlChange = {};
    RecordPendingReplayCommand(
        tick,
        command,
        Winters::Replay::eReplayJournalOutcome::RejectedToolCommand);
}

bool_t CGameRoom::CommitPendingPracticeControlChange(const TickContext& tc)
{
#if !defined(_DEBUG)
    (void)tc;
    m_PendingPracticeControlChange = {};
    return false;
#else
    const PendingPracticeControlChange pending =
        m_PendingPracticeControlChange;
    m_PendingPracticeControlChange = {};
    if (pending.eKind == PracticeControlChangeKind::None ||
        !m_pLobbyAuthority ||
        m_pendingRewindToTick != 0u ||
        !HasStrictAttackSpeedLabRoster(
            m_world,
            m_entityMap,
            m_pLobbyAuthority.get(),
            pending.uSessionId))
    {
        return false;
    }

    EntityID source = NULL_ENTITY;
    if (!m_sessionBinding.TryGetAlive(
        pending.uSessionId,
        m_world,
        source) ||
        m_entityMap.ToNet(source) != pending.uSourceNetId)
    {
        return false;
    }

    u8_t sourceSlotId = kInvalidGameRosterSlot;
    if (!m_pLobbyAuthority->TryGetSessionSlot(
        pending.uSessionId,
        sourceSlotId))
    {
        return false;
    }
    const LobbySlotState* pCurrentSourceSlot =
        m_pLobbyAuthority->TryGetSlot(sourceSlotId);
    if (!pCurrentSourceSlot ||
        !pCurrentSourceSlot->bHuman ||
        pCurrentSourceSlot->bBot ||
        pCurrentSourceSlot->sessionId != pending.uSessionId ||
        pCurrentSourceSlot->netId != pending.uSourceNetId)
    {
        return false;
    }

    if (pending.eKind == PracticeControlChangeKind::TakeRosterChampion)
    {
        const EntityID target = m_entityMap.FromNet(pending.uTargetNetId);
        if (target == NULL_ENTITY ||
            !m_world.IsAlive(target) ||
            !m_world.HasComponent<ChampionComponent>(target))
        {
            return false;
        }

        u8_t targetSlotId = kInvalidGameRosterSlot;
        const LobbySlotState* pSlots = GetLobbySlots();
        for (u32_t i = 0u; pSlots && i < GetLobbySlotCount(); ++i)
        {
            if (pSlots[i].netId == pending.uTargetNetId &&
                pSlots[i].bBot &&
                !pSlots[i].bHuman &&
                !pSlots[i].bDummy)
            {
                targetSlotId = pSlots[i].slotId;
                break;
            }
        }
        if (targetSlotId == kInvalidGameRosterSlot)
            return false;

        const LobbyAuthorityResult result =
            m_pLobbyAuthority->TransferInGameHumanControl(
                pending.uSessionId,
                pending.uTargetNetId);
        if (!result.bSendHello)
            return false;

        LobbySlotState* pSourceSlot =
            m_pLobbyAuthority->TryGetSlot(sourceSlotId);
        LobbySlotState* pTargetSlot =
            m_pLobbyAuthority->TryGetSlot(targetSlotId);
        if (!pSourceSlot || !pTargetSlot)
            return false;

        TransferPracticeControlState(m_world, source, target);
        ClearChampionControlIntent(m_world, source);
        ClearChampionControlIntent(m_world, target);
        ConfigureChampionControlRole(source, *pSourceSlot);
        ConfigureChampionControlRole(target, *pTargetSlot);
        m_sessionBinding.Bind(pending.uSessionId, target);

        m_keyframes.clear();
        m_pendingRewindToTick = 0u;
        ApplyLobbyAuthorityResult(result);
        return true;
    }

    if (pending.eKind !=
        PracticeControlChangeKind::ReplaceControlledChampion)
    {
        return false;
    }

    const GameplayDefinitionPack& definitions =
        ServerData::GetActiveLoLGameplayDefinitionPack();
    if (!definitions.FindChampion(pending.eChampionId))
        return false;

    const Vec3 sourcePosition =
        m_world.HasComponent<TransformComponent>(source)
            ? m_world.GetComponent<TransformComponent>(source).GetPosition()
            : GetSpawnPositionForLobbySlot(*pCurrentSourceSlot);

    LobbySlotState replacementSlot = *pCurrentSourceSlot;
    replacementSlot.netId = NULL_NET_ENTITY;
    replacementSlot.champion = pending.eChampionId;
    const EntityID replacement = SpawnChampionForLobbySlot(replacementSlot);
    if (replacement == NULL_ENTITY || !m_world.IsAlive(replacement))
    {
        m_sessionBinding.Bind(pending.uSessionId, source);
        return false;
    }

    const bool_t bCanonicalSpawn =
        m_world.HasComponent<StatComponent>(replacement) &&
        m_world.GetComponent<StatComponent>(replacement).level ==
            kAttackSpeedLabStartLevel &&
        m_world.HasComponent<ChampionComponent>(replacement) &&
        m_world.GetComponent<ChampionComponent>(replacement).level ==
            kAttackSpeedLabStartLevel &&
        m_world.HasComponent<GoldComponent>(replacement) &&
        m_world.GetComponent<GoldComponent>(replacement).amount ==
            kAttackSpeedLabStartGold;
    if (!bCanonicalSpawn)
    {
        m_entityMap.Unbind(replacementSlot.netId);
        m_world.DestroyEntity(replacement);
        m_sessionBinding.Bind(pending.uSessionId, source);
        return false;
    }

    m_world.GetComponent<TransformComponent>(replacement).SetPosition(
        sourcePosition);
    PositionDiscontinuityComponent& discontinuity =
        m_world.HasComponent<PositionDiscontinuityComponent>(replacement)
            ? m_world.GetComponent<PositionDiscontinuityComponent>(replacement)
            : m_world.AddComponent<PositionDiscontinuityComponent>(
                replacement,
                PositionDiscontinuityComponent{});
    discontinuity.uTick = tc.tickIndex;

    const LobbyAuthorityResult result =
        m_pLobbyAuthority->ReplaceInGameControlledChampion(
            pending.uSessionId,
            pending.eChampionId,
            replacementSlot.netId);
    if (!result.bSendHello)
    {
        m_entityMap.Unbind(replacementSlot.netId);
        m_world.DestroyEntity(replacement);
        m_sessionBinding.Bind(pending.uSessionId, source);
        return false;
    }

    TransferPracticeControlState(m_world, source, replacement);
    const std::vector<EntityID> cleanupEntities =
        CollectChampionReplacementCleanupEntities(
            m_world,
            source,
            pending.uSourceNetId);
    for (EntityID entity : cleanupEntities)
    {
        if (!m_world.IsAlive(entity))
            continue;
        m_lastBroadcastActionSeq.erase(entity);
        const NetEntityId netId = m_entityMap.ToNet(entity);
        if (netId != NULL_NET_ENTITY)
            m_entityMap.Unbind(netId);
        m_world.DestroyEntity(entity);
    }
    ClearChampionReplacementRelations(m_world, source);
    m_PracticeSpawnedEntities.erase(
        std::remove_if(
            m_PracticeSpawnedEntities.begin(),
            m_PracticeSpawnedEntities.end(),
            [&](EntityID entity)
            {
                return !m_world.IsAlive(entity) ||
                    !m_world.HasComponent<PracticeSpawnedTag>(entity);
            }),
        m_PracticeSpawnedEntities.end());
    m_lastBroadcastActionSeq.erase(source);
    m_entityMap.Unbind(pending.uSourceNetId);
    m_world.DestroyEntity(source);
    m_sessionBinding.Bind(pending.uSessionId, replacement);

    if (m_pSpatialSystem)
        m_pSpatialSystem->Execute(m_world, DeterministicTime::kFixedDt);
    if (m_pLagCompensation)
    {
        m_pLagCompensation->Reset();
        m_pLagCompensation->RecordHistory(m_world, tc.tickIndex);
    }

    m_keyframes.clear();
    m_pendingRewindToTick = 0u;
    ApplyLobbyAuthorityResult(result);
    return true;
#endif
}

void CGameRoom::ClearPracticeSpawns()
{
#if defined(_DEBUG)
    const auto entities =
        DeterministicEntityIterator<PracticeSpawnedTag>::CollectSorted(m_world);
    for (EntityID entity : entities)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<PracticeSpawnedTag>(entity))
        {
            continue;
        }

        const NetEntityId netId = m_entityMap.ToNet(entity);
        if (netId != NULL_NET_ENTITY)
            m_entityMap.Unbind(netId);
        m_world.DestroyEntity(entity);
    }
#endif
    m_PracticeSpawnedEntities.clear();
}
