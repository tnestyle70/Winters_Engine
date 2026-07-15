#include "Game/SnapshotBuilder.h"

#include "Game/ServerProjectileAuthority.h"

#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ProjectileBarrierComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
#include "Shared/GameSim/Components/ReplicatedPoseComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
//Viego Soul
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/EffectAnchorSubtype.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{
    bool_t IsMoveLockedBySnapshotAction(CWorld& world, EntityID entity, u64_t serverTick)
    {
        if (!world.HasComponent<ReplicatedActionComponent>(entity))
            return false;

        const auto& action = world.GetComponent<ReplicatedActionComponent>(entity);
        if (serverTick < action.startTick)
            return false;
        return action.movePolicy != eSkillActionMovePolicy::Allow &&
            serverTick < action.lockEndTick;
    }
}

std::unique_ptr<CSnapshotBuilder> CSnapshotBuilder::Create()
{
    return std::unique_ptr<CSnapshotBuilder>(new CSnapshotBuilder());
}

flatbuffers::DetachedBuffer CSnapshotBuilder::Build(
    CWorld& world,
    const EntityIdMap& entityMap,
    u64_t serverTick,
    u64_t serverTimeMs,
    u64_t rngState,
    u32_t lastAckedSeq,
    NetEntityId yourNetId,
    u64_t timelineEpoch,
    u64_t branchId,
    u64_t toolRevision,
    bool_t simPaused,
    f32_t simSpeedMul)
{
    flatbuffers::FlatBufferBuilder fbb(2048);

    const auto entities = DeterministicEntityIterator<TransformComponent>::CollectSorted(world);

    struct SnapshotEntity
    {
        NetEntityId netId = NULL_NET_ENTITY;
        EntityID entity = NULL_ENTITY;
    };

    std::vector<SnapshotEntity> sorted;
    sorted.reserve(entities.size());
    for (EntityID entity : entities)
    {
        const NetEntityId netId = entityMap.ToNet(entity);
        if (netId != NULL_NET_ENTITY)
            sorted.push_back({ netId, entity });
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const SnapshotEntity& lhs, const SnapshotEntity& rhs)
        {
            return lhs.netId < rhs.netId;
        });

    std::vector<flatbuffers::Offset<Shared::Schema::EntitySnapshot>> snapshots;
    snapshots.reserve(sorted.size());

    MatchScoreComponent matchScore{};
    bool_t bHasMatchScore = false;
    world.ForEach<MatchScoreComponent>(
        [&](EntityID, MatchScoreComponent& score)
        {
            if (!bHasMatchScore)
            {
                matchScore = score;
                bHasMatchScore = true;
            }
        }
    );

    for (const auto& item : sorted)
    {
        const EntityID entity = item.entity;
        const auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 pos = transform.GetLocalPosition();
        const Vec3 rot = transform.GetRotation();

        f32_t hp = 0.f;
        f32_t maxHp = 0.f;
        f32_t mana = 0.f;
        f32_t maxMana = 0.f;
        f32_t shield = 0.f;
        f32_t moveSpeed = 0.f;
        f32_t ad = 0.f;
        f32_t ap = 0.f;
        f32_t armor = 0.f;
        f32_t mr = 0.f;
        f32_t attackSpeed = 0.f;
        f32_t attackRange = 0.f;
        f32_t critChance = 0.f;
        f32_t abilityHaste = 0.f;
        f32_t yaw = NormalizeChampionVisualYaw(rot.y);
        u16_t poseId = static_cast<u16_t>(eReplicatedPoseId::Idle);
        u64_t poseStartTick = 0;
        u16_t actionId = static_cast<u16_t>(eReplicatedActionId::None);
        u64_t actionStartTick = 0;
        u32_t actionSeq = 0;
        u8_t actionStage = 1;
        u8_t actionSourceChampionId = 0;
        u8_t actionSourceSlot = 0;
        u8_t actionMovePolicy = 0;
        u64_t actionLockEndTick = 0;
        u32_t actionCommandSeq = 0;
        f32_t minionAttackWindupSec = 0.f;
        f32_t minionAttackRecoverySec = 0.f;
        u8_t championId = 0;
        u8_t team = 0;
        u8_t level = 1;
        f32_t xpCurrent = 0.f;
        f32_t xpRequired = 0.f;
        u8_t skillPoints = 0;
        u8_t lethalTempoStacks = 0;
        u32_t buffMask = 0;
        u32_t statHash = 0;
        u32_t stateFlags = 0;
        u32_t gameplayStateFlags = 0;
        f32_t gameplayMoveSpeedMul = 1.f;
        u8_t forcedMotionKind = 0u;
        f32_t forcedMotionRemainingSec = 0.f;
        u32_t ownerNet = 0;
        u16_t subtype = 0;
        u16_t projectileKind = 0;
        u32_t projectileOwnerNet = 0;
        u32_t projectileTargetNet = 0;
        f32_t projectileSpeed = 0.f;
        f32_t projectileRadius = 0.f;
        f32_t projectileMaxDist = 0.f;
        Vec3 projectileDirection{};
        f32_t projectileTraveledDist = 0.f;
        u8_t baseChampionId = 0;
        u8_t visualChampionId = 0;
        u8_t skillChampionId = 0;
        u8_t skillSlotMask = 0;
        u8_t spellbookChampionId = 0;
        u8_t spellbookSlot = 0;
        f32_t spellbookRemaining = 0.f;
        Shared::Schema::EntityKind entityKind = Shared::Schema::EntityKind::None;

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            hp = health.fCurrent;
            maxHp = health.fMaximum;
            if (health.bIsDead || health.fCurrent <= 0.f)
                stateFlags |= kSnapshotStateDeadFlag;
        }

        if (world.HasComponent<StatComponent>(entity))
        {
            const auto& stat = world.GetComponent<StatComponent>(entity);
            mana = stat.manaMax;
            maxMana = stat.manaMax;
            moveSpeed = stat.moveSpeed;
            ad = stat.ad;
            ap = stat.ap;
            armor = stat.armor;
            mr = stat.mr;
            attackSpeed = stat.attackSpeed;
            attackRange = stat.attackRange;
            critChance = stat.critChance;
            abilityHaste = stat.abilityHaste;
            level = stat.level;
            buffMask = stat.buffMaskHash;
            statHash = stat.itemMaskHash ^ (stat.buffMaskHash * 16777619u);
        }

        if (world.HasComponent<ChampionComponent>(entity))
        {
            const auto& champion = world.GetComponent<ChampionComponent>(entity);
            championId = static_cast<u8_t>(champion.id);
            baseChampionId = championId;
            visualChampionId = championId;
            skillChampionId = championId;
            team = static_cast<u8_t>(champion.team);
            subtype = static_cast<u16_t>(champion.id);
            entityKind = Shared::Schema::EntityKind::Champion;
            if (maxHp <= 0.f)
                maxHp = champion.maxHp;
            mana = champion.mana;
            if (maxMana <= 0.f)
                maxMana = champion.maxMana;
            shield = (std::max)(0.f, champion.shield);
        }
        if (world.HasComponent<YasuoStateComponent>(entity))
        {
            const auto& yasuoState = world.GetComponent<YasuoStateComponent>(entity);
            mana = yasuoState.fPassiveFlow;
            maxMana = yasuoState.fPassiveFlowMax;
        }

        //Viego Soul 처리
        if (world.HasComponent<ViegoSoulComponent>(entity))
        {
            const auto& soul = world.GetComponent<ViegoSoulComponent>(entity);
            championId = static_cast<u8_t>(soul.champion);
            subtype = static_cast<u16_t>(soul.champion);
            entityKind = Shared::Schema::EntityKind::EffectAnchor;
            stateFlags |= kSnapshotStateViegoSoulFlag;
        }

        if (world.HasComponent<KalistaSentinelComponent>(entity))
        {
            const auto& sentinel = world.GetComponent<KalistaSentinelComponent>(entity);
            championId = static_cast<u8_t>(eChampion::KALISTA);
            baseChampionId = championId;
            visualChampionId = championId;
            skillChampionId = championId;
            team = static_cast<u8_t>(sentinel.team);
            subtype = EffectAnchorSubtype::KalistaWSentinel;
            ownerNet = entityMap.ToNet(sentinel.owner);
            entityKind = Shared::Schema::EntityKind::EffectAnchor;
        }

        if (world.HasComponent<FormOverrideComponent>(entity))
        {
            const auto& form = world.GetComponent<FormOverrideComponent>(entity);
            if (form.bActive)
            {
                if (form.visualChampion != eChampion::END &&
                    form.visualChampion != eChampion::NONE)
                {
                    visualChampionId = static_cast<u8_t>(form.visualChampion);
                }
                if (form.skillChampion != eChampion::END &&
                    form.skillChampion != eChampion::NONE)
                {
                    skillChampionId = static_cast<u8_t>(form.skillChampion);
                    skillSlotMask = form.skillSlotMask;
                }
            }
        }

        if (world.HasComponent<SpellbookOverrideComponent>(entity))
        {
            const auto& spellbook = world.GetComponent<SpellbookOverrideComponent>(entity);
            if (spellbook.bActive &&
                spellbook.sourceChampion != eChampion::END &&
                spellbook.sourceChampion != eChampion::NONE)
            {
                spellbookChampionId = static_cast<u8_t>(spellbook.sourceChampion);
                spellbookSlot = spellbook.sourceSlot;
                spellbookRemaining = spellbook.fRemainingSec;
            }
        }


        if (world.HasComponent<ExperienceComponent>(entity))
        {
            const auto& xp = world.GetComponent<ExperienceComponent>(entity);
            xpCurrent = xp.current;
            xpRequired = xp.requiredForNextLevel;
            if (xp.level > 0)
                level = xp.level;
        }

        if (world.HasComponent<MinionStateComponent>(entity))
        {
            const auto& minion = world.GetComponent<MinionStateComponent>(entity);
            team = static_cast<u8_t>(minion.team);
            subtype = minion.type;
            moveSpeed = minion.moveSpeed;
            minionAttackWindupSec = minion.attackWindup;
            minionAttackRecoverySec = minion.attackRecovery;
            entityKind = Shared::Schema::EntityKind::Minion;
            if (minion.current == MinionStateComponent::Attack)
                stateFlags |= kSnapshotStateAttackFlag;
            if (minion.current == MinionStateComponent::LaneMove ||
                minion.current == MinionStateComponent::Chase)
            {
                stateFlags |= kSnapshotStateMovingFlag;
            }
        }

        if (world.HasComponent<MinionComponent>(entity))
        {
            const auto& minion = world.GetComponent<MinionComponent>(entity);
            team = static_cast<u8_t>(minion.team);
            subtype = minion.roleType;
            entityKind = Shared::Schema::EntityKind::Minion;
            if (maxHp <= 0.f)
                maxHp = minion.maxHp;
        }

        if (world.HasComponent<StructureComponent>(entity))
        {
            const auto& structure = world.GetComponent<StructureComponent>(entity);
            team = static_cast<u8_t>(structure.team);
            subtype = static_cast<u16_t>(structure.kind);
            entityKind = Shared::Schema::EntityKind::Inhibitor;
            if (maxHp <= 0.f)
                maxHp = structure.maxHp;
        }

        if (world.HasComponent<TurretComponent>(entity))
        {
            const auto& turret = world.GetComponent<TurretComponent>(entity);
            team = static_cast<u8_t>(turret.team);
            subtype = turret.tier;
            entityKind = Shared::Schema::EntityKind::Turret;
            if (maxHp <= 0.f)
                maxHp = turret.maxHp;
            if (turret.targetId != NULL_ENTITY && world.IsAlive(turret.targetId))
                ownerNet = entityMap.ToNet(turret.targetId);
        }

        if (world.HasComponent<NexusTag>(entity))
        {
            entityKind = Shared::Schema::EntityKind::Nexus;
        }

        if (world.HasComponent<VisionSensorComponent>(entity))
        {
            const auto& ward = world.GetComponent<VisionSensorComponent>(entity);
            team = ward.ownerTeam;
            subtype = ward.bControlSensor ? 1u : 0u;
            entityKind = Shared::Schema::EntityKind::Ward;
            if (maxHp <= 0.f)
                maxHp = 1.f;
        }

        if (world.HasComponent<JungleComponent>(entity))
        {
            const auto& jungle = world.GetComponent<JungleComponent>(entity);
            team = static_cast<u8_t>(eTeam::Neutral);
            subtype = static_cast<u16_t>(jungle.subKind);
            entityKind = Shared::Schema::EntityKind::JungleMonster;
            if (maxHp <= 0.f)
                maxHp = jungle.maxHp;
        }

        if (world.HasComponent<StructureProjectileComponent>(entity))
        {
            const auto& projectile = world.GetComponent<StructureProjectileComponent>(entity);
            entityKind = Shared::Schema::EntityKind::Projectile;
            projectileKind = CServerProjectileAuthority::kStructureProjectileKind;
            projectileOwnerNet = projectile.uSourceNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uSourceNetAtSpawn
                : entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = projectile.uTargetNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uTargetNetAtSpawn
                : entityMap.ToNet(projectile.targetEntity);
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            projectileMaxDist = projectile.maxDistance;
            projectileDirection = projectile.direction;
            projectileTraveledDist = projectile.traveledDistance;
            ownerNet = projectileOwnerNet;
        }

        if (world.HasComponent<SkillProjectileComponent>(entity))
        {
            const auto& projectile = world.GetComponent<SkillProjectileComponent>(entity);
            entityKind = Shared::Schema::EntityKind::Projectile;
            projectileKind = static_cast<u16_t>(projectile.kind);
            projectileOwnerNet = projectile.uSourceNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uSourceNetAtSpawn
                : entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = projectile.uTargetNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uTargetNetAtSpawn
                : entityMap.ToNet(projectile.targetEntity);
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            projectileMaxDist = projectile.maxDistance;
            projectileDirection = projectile.direction;
            projectileTraveledDist = projectile.traveledDistance;
            ownerNet = projectileOwnerNet;
            team = static_cast<u8_t>(projectile.sourceTeam);
        }

        if (world.HasComponent<ReplicatedPoseComponent>(entity))
        {
            const auto& pose = world.GetComponent<ReplicatedPoseComponent>(entity);
            poseId = pose.poseId;
            poseStartTick = pose.startTick;
        }

        if (world.HasComponent<ReplicatedActionComponent>(entity))
        {
            const auto& action = world.GetComponent<ReplicatedActionComponent>(entity);
            actionId = action.actionId;
            actionStartTick = action.startTick;
            actionSeq = action.sequence;
            actionStage = action.stage;
            actionSourceChampionId = static_cast<u8_t>(action.sourceChampion);
            actionSourceSlot = action.sourceSlot;
            actionMovePolicy = static_cast<u8_t>(action.movePolicy);
            actionLockEndTick = action.lockEndTick;
            actionCommandSeq = action.commandSequence;
        }

        if (!IsMoveLockedBySnapshotAction(world, entity, serverTick) &&
            world.HasComponent<MoveTargetComponent>(entity) &&
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget)
        {
            stateFlags |= kSnapshotStateMovingFlag;
        }

        if (world.HasComponent<GameplayStateComponent>(entity))
        {
            const auto& gameplay = world.GetComponent<GameplayStateComponent>(entity);
            gameplayStateFlags = gameplay.stateFlags;
            gameplayMoveSpeedMul = gameplay.fMoveSpeedMul;
            if ((gameplay.stateFlags & kGameplayStateInvisibleFlag) != 0u)
                stateFlags |= kSnapshotStateInvisibleFlag;
        }

        if (world.HasComponent<KalistaFateCallCarriedComponent>(entity) &&
            world.GetComponent<KalistaFateCallCarriedComponent>(entity).bHidden)
        {
            stateFlags |= kSnapshotStateKalistaCarriedFlag;
        }

        if (world.HasComponent<KalistaOathswornByComponent>(entity))
        {
            const EntityID kalista =
                world.GetComponent<KalistaOathswornByComponent>(entity).entityKalista;
            if (kalista != NULL_ENTITY &&
                world.IsAlive(kalista) &&
                world.HasComponent<KalistaOathswornComponent>(kalista) &&
                world.GetComponent<KalistaOathswornComponent>(kalista).eStage ==
                    eKalistaOathswornStage::Binding)
            {
                stateFlags |= kSnapshotStateKalistaOathswornRitualFlag;
            }
        }

        if (world.HasComponent<ForcedMotionComponent>(entity))
        {
            const auto& motion = world.GetComponent<ForcedMotionComponent>(entity);
            forcedMotionKind = static_cast<u8_t>(motion.kind);
            forcedMotionRemainingSec =
                (std::max)(0.f, motion.fDurationSec - motion.fElapsedSec);
        }

        if (world.HasComponent<AnnieTibbersComponent>(entity))
            ownerNet = entityMap.ToNet(world.GetComponent<AnnieTibbersComponent>(entity).owner);

        u32_t aiDebugAvailableActionMask = 0u;
        u32_t aiDebugAvailableSkillMask = 0u;
        u32_t aiDebugTargetNet = NULL_NET_ENTITY;
        u32_t aiDebugLowHpEnemyNet = NULL_NET_ENTITY;
        u32_t aiDebugDiveTargetNet = NULL_NET_ENTITY;
        u32_t aiDebugLastCommandTargetNet = NULL_NET_ENTITY;
        u8_t aiDebugLastCommandKind = 0u;
        u8_t aiDebugLastCommandSlot = 0u;
        u8_t aiDebugDivePhase = 0u;
        u8_t aiDebugLastBlockReason = 0u;
        u32_t aiDebugFlags = 0u;
        f32_t aiDebugChampionScore = 0.f;
        f32_t aiDebugFarmScore = 0.f;
        f32_t aiDebugStructureScore = 0.f;
        f32_t aiDebugSelfHpRatio = 1.f;
        f32_t aiDebugEnemyHpRatio = 1.f;
        f32_t aiDebugEnemyDistance = 999.f;
        f32_t aiDebugAttackRange = 1.5f;
        f32_t aiDebugTurretDanger = 0.f;
        f32_t aiDebugLowHpEnemyRatio = 1.f;
        f32_t aiDebugLowHpEnemyDistance = 999.f;
        f32_t aiDebugChampionScanRange = 0.f;
        f32_t aiDebugMinionScanRange = 0.f;
        f32_t aiDebugStructureScanRange = 0.f;
        f32_t aiDebugLeashRange = 0.f;
        f32_t aiDebugRetreatHpRatio = 0.f;
        f32_t aiDebugReengageHpRatio = 0.f;
        f32_t aiDebugChampionScoreMargin = 0.f;
        f32_t aiDebugTurretDangerThreshold = 0.f;
        f32_t aiDebugPostComboBASelfHpMinRatio = 0.f;
        f32_t aiDebugPostComboBAEnemyHpMargin = 0.f;
        f32_t aiDebugPostComboBAWindow = 0.f;
        f32_t aiDebugLowHpExecuteThreshold = 0.f;
        f32_t aiDebugDiveScanRange = 0.f;
        f32_t aiDebugDiveExtraBAWindow = 0.f;
        f32_t aiDebugFlashRange = 0.f;
        f32_t aiDebugPostComboBATimer = 0.f;
        Vec3 aiDebugLastCommandPos{};

        if (world.HasComponent<ChampionAIComponent>(entity))
        {
            const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
            stateFlags |= kChampionAIDebugPresentFlag;
            stateFlags |= (static_cast<u32_t>(ai.state) << kChampionAIStateShift) & kChampionAIStateMask;
            stateFlags |= (static_cast<u32_t>(ai.lastAction) << kChampionAIActionShift) & kChampionAIActionMask;
            stateFlags |= (static_cast<u32_t>(ai.intent) << kChampionAIIntentShift) & kChampionAIIntentMask;
            stateFlags |= (ai.debugAvailableActionMask << kChampionAIAvailableActionShift) & kChampionAIAvailableActionMask;
            stateFlags |= (ai.debugAvailableSkillMask << kChampionAIAvailableSkillShift) & kChampionAIAvailableSkillMask;
            if (ai.debugForcedDecisionCount > 0)
                stateFlags |= kChampionAIDebugOverrideFlag;

            EntityID target = ai.lockedChampion;
            if (target == NULL_ENTITY)
                target = ai.targetMinion;
            if (target == NULL_ENTITY)
                target = ai.targetStructure;
            if (target == NULL_ENTITY)
                target = ai.alliedWave;

            if (target != NULL_ENTITY)
            {
                ownerNet = entityMap.ToNet(target);
                aiDebugTargetNet = ownerNet;
            }

            aiDebugAvailableActionMask = ai.debugAvailableActionMask;
            aiDebugAvailableSkillMask = ai.debugAvailableSkillMask;
            aiDebugLowHpEnemyNet = ai.lowHpEnemyChampion != NULL_ENTITY
                ? entityMap.ToNet(ai.lowHpEnemyChampion)
                : NULL_NET_ENTITY;
            aiDebugDiveTargetNet = ai.diveTarget != NULL_ENTITY
                ? entityMap.ToNet(ai.diveTarget)
                : NULL_NET_ENTITY;
            aiDebugLastCommandTargetNet = ai.debugLastCommandTarget != NULL_ENTITY
                ? entityMap.ToNet(ai.debugLastCommandTarget)
                : NULL_NET_ENTITY;
            aiDebugLastCommandKind = ai.debugLastCommandKind;
            aiDebugLastCommandSlot = ai.debugLastCommandSlot;
            aiDebugDivePhase = static_cast<u8_t>(ai.divePhase);
            aiDebugLastBlockReason = static_cast<u8_t>(ai.debugLastBlockReason);
            if (ai.bCanAttackChampion)
                aiDebugFlags |= kChampionAIDebugCanAttackChampionFlag;
            if (ai.bPostComboBAAllowed)
                aiDebugFlags |= kChampionAIDebugPostComboBAAllowedFlag;
            if (ai.bMidDefenseActive)
                aiDebugFlags |= kChampionAIDebugMidDefenseActiveFlag;
            aiDebugFlags |=
                (static_cast<u32_t>(ai.brainType) << kChampionAIDebugBrainTypeShift) &
                kChampionAIDebugBrainTypeMask;
            aiDebugChampionScore = ai.fChampionDecisionScore;
            aiDebugFarmScore = ai.fFarmDecisionScore;
            aiDebugStructureScore = ai.fStructureDecisionScore;
            aiDebugSelfHpRatio = ai.fDecisionSelfHpRatio;
            aiDebugEnemyHpRatio = ai.fDecisionEnemyHpRatio;
            aiDebugEnemyDistance = ai.fDecisionEnemyDistance;
            aiDebugAttackRange = ai.fDecisionAttackRange;
            aiDebugTurretDanger = ai.fDecisionTurretDanger;
            aiDebugLowHpEnemyRatio = ai.fDecisionLowHpEnemyRatio;
            aiDebugLowHpEnemyDistance = ai.fDecisionLowHpEnemyDistance;
            aiDebugChampionScanRange = ai.fDecisionChampionScanRange;
            aiDebugMinionScanRange = ai.minionScanRange;
            aiDebugStructureScanRange = ai.structureScanRange;
            aiDebugLeashRange = ai.leashRange;
            aiDebugRetreatHpRatio = ai.retreatHpRatio;
            aiDebugReengageHpRatio = ai.reengageHpRatio;
            aiDebugChampionScoreMargin = ai.fChampionScoreMargin;
            aiDebugTurretDangerThreshold = ai.fTurretDangerThreshold;
            aiDebugPostComboBASelfHpMinRatio = ai.fPostComboBASelfHpMinRatio;
            aiDebugPostComboBAEnemyHpMargin = ai.fPostComboBAEnemyHpMargin;
            aiDebugPostComboBAWindow = ai.fPostComboBAWindow;
            aiDebugLowHpExecuteThreshold = ai.fLowHpExecuteThreshold;
            aiDebugDiveScanRange = ai.fDecisionDiveScanRange;
            aiDebugDiveExtraBAWindow = ai.fDiveExtraBAWindow;
            aiDebugFlashRange = ai.fDecisionFlashRange;
            aiDebugPostComboBATimer = ai.fPostComboBATimer;
            aiDebugLastCommandPos = ai.debugLastCommandPos;
        }

        std::vector<f32_t> cooldowns;
        std::vector<f32_t> cooldownDurations;
        if (world.HasComponent<SkillStateComponent>(entity))
        {
            const auto& skillState = world.GetComponent<SkillStateComponent>(entity);
            cooldowns.reserve(5);
            cooldownDurations.reserve(5);
            for (u8_t i = 0; i < 5; ++i)
            {
                cooldowns.push_back(skillState.slots[i].cooldownRemaining);
                cooldownDurations.push_back(skillState.slots[i].cooldownDuration);
            }
        }

        u32_t gold = 0;
        std::vector<u16_t> inventoryItemIds;
        u16_t kills = 0;
        u16_t deaths = 0;
        u16_t assists = 0;
        std::vector<u16_t> summonerSpellIds;
        std::vector<f32_t> summonerSpellCooldowns;
        std::vector<f32_t> summonerSpellCooldownDurations;

        if (world.HasComponent<GoldComponent>(entity))
            gold = world.GetComponent<GoldComponent>(entity).amount;

        if (world.HasComponent<InventoryComponent>(entity))
        {
            const InventoryComponent& inventory = world.GetComponent<InventoryComponent>(entity);
            inventoryItemIds.reserve(inventory.count);
            for (u8_t i = 0; i < inventory.count && i < InventoryComponent::kMaxSlots; ++i)
                inventoryItemIds.push_back(inventory.itemIds[i]);
        }

        if (world.HasComponent<ChampionScoreComponent>(entity))
        {
            const ChampionScoreComponent& score =
                world.GetComponent<ChampionScoreComponent>(entity);
            kills = score.iKills;
            deaths = score.iDeaths;
            assists = score.iAssists;
            summonerSpellIds.reserve(ChampionScoreComponent::kSummonerSpellSlotCount);
            for (u8_t i = 0; i < ChampionScoreComponent::kSummonerSpellSlotCount; ++i)
                summonerSpellIds.push_back(score.iSummonerSpellIds[i]);
        }

        if (world.HasComponent<SummonerSpellStateComponent>(entity))
        {
            const SummonerSpellStateComponent& spells =
                world.GetComponent<SummonerSpellStateComponent>(entity);
            summonerSpellCooldowns.reserve(SummonerSpellStateComponent::kSlotCount);
            summonerSpellCooldownDurations.reserve(SummonerSpellStateComponent::kSlotCount);
            for (u8_t i = 0; i < SummonerSpellStateComponent::kSlotCount; ++i)
            {
                summonerSpellCooldowns.push_back(spells.cooldownRemaining[i]);
                summonerSpellCooldownDurations.push_back(spells.cooldownDuration[i]);
            }
        }

        std::vector<u8_t> ranks;
        if (world.HasComponent<SkillRankComponent>(entity))
        {
            const auto& skillRank = world.GetComponent<SkillRankComponent>(entity);
            skillPoints = skillRank.pointsAvailable;
            ranks.reserve(SkillRankComponent::kSlotCount);
            for (u8_t i = 0; i < SkillRankComponent::kSlotCount; ++i)
                ranks.push_back(skillRank.ranks[i]);
        }

        if (world.HasComponent<RuneRuntimeComponent>(entity))
            lethalTempoStacks =
                world.GetComponent<RuneRuntimeComponent>(entity).iLethalTempoStacks;

        const auto cooldownOffset = fbb.CreateVector(cooldowns);
        const auto cooldownDurationOffset = fbb.CreateVector(cooldownDurations);
        const auto rankOffset = fbb.CreateVector(ranks);
        const auto inventoryOffset = fbb.CreateVector(inventoryItemIds);
        const auto summonerSpellOffset = fbb.CreateVector(summonerSpellIds);
        const auto summonerSpellCooldownOffset = fbb.CreateVector(summonerSpellCooldowns);
        const auto summonerSpellCooldownDurationOffset =
            fbb.CreateVector(summonerSpellCooldownDurations);

        std::vector<flatbuffers::Offset<Shared::Schema::AIDebugTraceRow>> aiDebugTraceRows;
        if (world.HasComponent<ChampionAIComponent>(entity))
        {
            const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
            const u8_t count = std::min<u8_t>(
                ai.debugDecisionTraceCount,
                kChampionAIDebugTraceCapacity);
            // The authoritative AI keeps a bounded 16-row ring, but the
            // 30 Hz snapshot carries only the newest evidence row. F9 builds
            // selected-bot branch history locally while open, avoiding the
            // previous 16x per-bot replication cost on every snapshot.
            if (count > 0u)
            {
                const u8_t index = static_cast<u8_t>(
                    (ai.debugDecisionTraceHead +
                        kChampionAIDebugTraceCapacity - 1u) %
                    kChampionAIDebugTraceCapacity);
                const ChampionAIDecisionTraceEntry& row = ai.debugDecisionTrace[index];
                const ChampionAIDecisionTraceEntry* pShadowRow = nullptr;
                if (world.HasComponent<ChampionAIResearchDebugComponent>(entity))
                {
                    const auto& research =
                        world.GetComponent<ChampionAIResearchDebugComponent>(entity);
                    if (research.bShadowDecisionPresent &&
                        research.shadowDecision.tick == row.tick &&
                        research.shadowDecision.commandSequence == row.commandSequence)
                    {
                        pShadowRow = &research.shadowDecision;
                    }
                }
                const u32_t rowTargetNet = row.target != NULL_ENTITY
                    ? entityMap.ToNet(row.target)
                    : NULL_NET_ENTITY;
                aiDebugTraceRows.push_back(Shared::Schema::CreateAIDebugTraceRow(
                    fbb,
                    row.tick,
                    static_cast<u8_t>(row.state),
                    static_cast<u8_t>(row.intent),
                    static_cast<u8_t>(row.action),
                    static_cast<u8_t>(row.divePhase),
                    static_cast<u8_t>(row.blockReason),
                    row.commandKind,
                    row.commandSlot,
                    rowTargetNet,
                    row.commandPos.x,
                    row.commandPos.y,
                    row.commandPos.z,
                    row.championScore,
                    row.farmScore,
                    row.structureScore,
                    row.selfHpRatio,
                    row.enemyHpRatio,
                    row.enemyDistance,
                    row.turretDanger,
                    row.retreatScore,
                    ai.fSkillCastMinInterval,
                    ai.fSkillCastCooldownTimer,
                    row.legalCandidateMask,
                    row.illegalCandidateMask,
                    row.commandSequence,
                    row.executorReason,
                    row.executorState,
                    row.comboStep,
                    pShadowRow ? pShadowRow->shadowPolicyRevision : 0u,
                    pShadowRow ? pShadowRow->shadowPolicySha256Prefix : 0u,
                    pShadowRow ? pShadowRow->shadowLogits[0] : 0.f,
                    pShadowRow ? pShadowRow->shadowLogits[1] : 0.f,
                    pShadowRow ? pShadowRow->shadowLogits[2] : 0.f,
                    pShadowRow ? pShadowRow->shadowLogits[3] : 0.f,
                    pShadowRow ? pShadowRow->shadowSelectedMargin : 0.f,
                    pShadowRow ? pShadowRow->shadowTopFeatureContribution : 0.f,
                    pShadowRow ? pShadowRow->shadowLegalCandidateMask : 0u,
                    pShadowRow ? pShadowRow->shadowTopFeatureIndex : 0xFFFFu,
                    pShadowRow ? pShadowRow->shadowStatus : 0u,
                    pShadowRow ? pShadowRow->shadowActiveCandidateKind : 0u,
                    pShadowRow ? pShadowRow->shadowSelectedCandidateKind : 0u,
                    pShadowRow ? pShadowRow->bShadowDisagreed : false));
            }
        }
        const auto aiDebugTraceOffset = fbb.CreateVector(aiDebugTraceRows);

        if (item.netId == yourNetId && championId != 0)
        {
            static u32_t s_snapshotYawTraceCount = 0;
            if (s_snapshotYawTraceCount < 2048u)
            {
                const bool_t bHasMove =
                    world.HasComponent<MoveTargetComponent>(entity);
                const MoveTargetComponent* pMove = bHasMove
                    ? &world.GetComponent<MoveTargetComponent>(entity)
                    : nullptr;
                const Vec3 moveTarget = pMove ? pMove->target : Vec3{};
                const Vec3 facingTarget = pMove ? pMove->facingTarget : Vec3{};
                const Vec3 facingDir = pMove ? pMove->facingDirection : Vec3{};
                const Vec3 path0 =
                    (pMove && pMove->pathCount > 0)
                        ? pMove->pathWaypoints[0]
                        : Vec3{};
                char msg[1024]{};
                sprintf_s(
                    msg,
                    "[YawTrace][ServerSnapshot] tick=%llu ack=%u yourNet=%u net=%u entity=%u champion=%u pos=(%.3f,%.3f,%.3f) rawYaw=%.4f wireYaw=%.4f pose=%u action=%u actionSeq=%u state=0x%08X hasMove=%u moveHasTarget=%u moveTarget=(%.3f,%.3f,%.3f) pathIndex=%u pathCount=%u path0=(%.3f,%.3f,%.3f) hasFacing=%u lockTicks=%u facingSeq=%u facingTarget=(%.3f,%.3f,%.3f) facingDir=(%.3f,%.3f)\n",
                    static_cast<unsigned long long>(serverTick),
                    lastAckedSeq,
                    yourNetId,
                    item.netId,
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(championId),
                    pos.x,
                    pos.y,
                    pos.z,
                    rot.y,
                    yaw,
                    static_cast<u32_t>(poseId),
                    static_cast<u32_t>(actionId),
                    actionSeq,
                    stateFlags,
                    bHasMove ? 1u : 0u,
                    (pMove && pMove->bHasTarget) ? 1u : 0u,
                    moveTarget.x,
                    moveTarget.y,
                    moveTarget.z,
                    pMove ? static_cast<u32_t>(pMove->pathIndex) : 0u,
                    pMove ? static_cast<u32_t>(pMove->pathCount) : 0u,
                    path0.x,
                    path0.y,
                    path0.z,
                    (pMove && pMove->bHasFacingTarget) ? 1u : 0u,
                    pMove ? static_cast<u32_t>(pMove->facingLockTicks) : 0u,
                    pMove ? pMove->facingSequenceNum : 0u,
                    facingTarget.x,
                    facingTarget.y,
                    facingTarget.z,
                    facingDir.x,
                    facingDir.z);
                WintersOutputAIDebugStringA(msg);
                ++s_snapshotYawTraceCount;
            }
        }
        // Pack the per-entity gameplay and UI fields into the FlatBuffer entity row.
        snapshots.push_back(Shared::Schema::CreateEntitySnapshot(
            fbb,
            item.netId,
            championId,
            team,
            level,
            xpCurrent,
            xpRequired,
            skillPoints,
            hp,
            mana,
            pos.x,
            pos.y,
            pos.z,
            yaw,
            moveSpeed,
            poseId,
            poseStartTick,
            actionId,
            actionStartTick,
            actionSeq,
            actionStage,
            cooldownOffset,
            cooldownDurationOffset,
            rankOffset,
            buffMask,
            statHash,
            entityKind,
            ownerNet,
            subtype,
            maxHp,
            maxMana,
            shield,
            stateFlags,
            projectileKind,
            projectileOwnerNet,
            projectileTargetNet,
            projectileSpeed,
            projectileRadius,
            projectileMaxDist,
            gold,
            inventoryOffset,
            ad,
            ap,
            armor,
            mr,
            attackSpeed,
            attackRange,
            critChance,
            abilityHaste,
            kills,
            deaths,
            assists,
            summonerSpellOffset,
            summonerSpellCooldownOffset,
            summonerSpellCooldownDurationOffset,
            aiDebugAvailableActionMask,
            aiDebugAvailableSkillMask,
            aiDebugTargetNet,
            aiDebugLowHpEnemyNet,
            aiDebugDiveTargetNet,
            aiDebugLastCommandTargetNet,
            aiDebugLastCommandKind,
            aiDebugLastCommandSlot,
            aiDebugDivePhase,
            aiDebugFlags,
            aiDebugChampionScore,
            aiDebugFarmScore,
            aiDebugStructureScore,
            aiDebugSelfHpRatio,
            aiDebugEnemyHpRatio,
            aiDebugEnemyDistance,
            aiDebugAttackRange,
            aiDebugTurretDanger,
            aiDebugLowHpEnemyRatio,
            aiDebugLowHpEnemyDistance,
            aiDebugChampionScanRange,
            aiDebugDiveScanRange,
            aiDebugFlashRange,
            aiDebugPostComboBATimer,
            aiDebugLastCommandPos.x,
            aiDebugLastCommandPos.y,
            aiDebugLastCommandPos.z,
            aiDebugLastBlockReason,
            aiDebugMinionScanRange,
            aiDebugStructureScanRange,
            aiDebugLeashRange,
            aiDebugRetreatHpRatio,
            aiDebugReengageHpRatio,
            aiDebugChampionScoreMargin,
            aiDebugTurretDangerThreshold,
            aiDebugPostComboBASelfHpMinRatio,
            aiDebugPostComboBAEnemyHpMargin,
            aiDebugPostComboBAWindow,
            aiDebugLowHpExecuteThreshold,
            aiDebugDiveExtraBAWindow,
            aiDebugTraceOffset,
            lethalTempoStacks,
            baseChampionId,
            visualChampionId,
            skillChampionId,
            skillSlotMask,
            spellbookChampionId,
            spellbookSlot,
            spellbookRemaining,
            actionSourceChampionId,
            actionSourceSlot,
            actionMovePolicy,
            actionLockEndTick,
            actionCommandSeq,
            minionAttackWindupSec,
            minionAttackRecoverySec,
            gameplayStateFlags,
            gameplayMoveSpeedMul,
            forcedMotionKind,
            forcedMotionRemainingSec,
            projectileDirection.x,
            projectileDirection.y,
            projectileDirection.z,
            projectileTraveledDist));
    }

    const auto entitiesOffset = fbb.CreateVector(snapshots);

    struct GameplayStateRow
    {
        Shared::Schema::GameplayStateKind kind =
            Shared::Schema::GameplayStateKind::None;
        NetEntityId sourceNet = NULL_NET_ENTITY;
        NetEntityId targetNet = NULL_NET_ENTITY;
        u64_t startTick = 0u;
        u64_t expireTick = 0u;
        u16_t stackCount = 0u;
        u8_t rank = 0u;
        u32_t flags = 0u;
        Vec3 position{};
        Vec3 direction{};
        f32_t magnitude0 = 0.f;
        f32_t magnitude1 = 0.f;
    };

    std::vector<GameplayStateRow> gameplayStateRows;
    const auto buffEntities =
        DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : buffEntities)
    {
        const BuffComponent& buffs = world.GetComponent<BuffComponent>(entity);
        for (u8_t i = 0u;
            i < buffs.count && i < BuffComponent::kMaxBuffs;
            ++i)
        {
            const BuffInstance& buff = buffs.buffs[i];
            if (buff.buffDefId != kEzrealRisingSpellForceBuffDefId ||
                buff.stackCount == 0u ||
                (buff.uExpireTick != 0u && buff.uExpireTick <= serverTick))
            {
                continue;
            }

            const NetEntityId sourceNet = buff.source != NULL_ENTITY
                ? entityMap.ToNet(buff.source)
                : entityMap.ToNet(entity);
            if (sourceNet == NULL_NET_ENTITY)
                continue;

            GameplayStateRow row{};
            row.kind = Shared::Schema::GameplayStateKind::EzrealRisingSpellForce;
            row.sourceNet = sourceNet;
            row.expireTick = buff.uExpireTick;
            row.stackCount = buff.stackCount;
            row.magnitude0 = buff.bonusAttackSpeedPerStack;
            gameplayStateRows.push_back(row);
        }
    }

    const auto fluxMarkEntities =
        DeterministicEntityIterator<EzrealEssenceFluxMarkComponent>::CollectSorted(world);
    for (EntityID entity : fluxMarkEntities)
    {
        const EzrealEssenceFluxMarkComponent& mark =
            world.GetComponent<EzrealEssenceFluxMarkComponent>(entity);
        if (mark.uSourceNet == NULL_NET_ENTITY ||
            mark.uTargetNet == NULL_NET_ENTITY ||
            mark.uExpireTick <= serverTick)
        {
            continue;
        }

        GameplayStateRow row{};
        row.kind = Shared::Schema::GameplayStateKind::EzrealEssenceFlux;
        row.sourceNet = mark.uSourceNet;
        row.targetNet = mark.uTargetNet;
        row.expireTick = mark.uExpireTick;
        row.rank = mark.uRank;
        gameplayStateRows.push_back(row);
    }

    const auto barrierEntities =
        DeterministicEntityIterator<ProjectileBarrierComponent>::CollectSorted(world);
    for (EntityID entity : barrierEntities)
    {
        const ProjectileBarrierComponent& barrier =
            world.GetComponent<ProjectileBarrierComponent>(entity);
        const NetEntityId sourceNet = entityMap.ToNet(barrier.sourceEntity);
        if (sourceNet == NULL_NET_ENTITY || barrier.expireTick <= serverTick)
            continue;

        GameplayStateRow row{};
        row.kind = Shared::Schema::GameplayStateKind::YasuoWindWall;
        row.sourceNet = sourceNet;
        row.startTick = barrier.spawnTick;
        row.expireTick = barrier.expireTick;
        row.position = barrier.center;
        row.direction = barrier.direction;
        row.magnitude0 = barrier.halfLength;
        row.magnitude1 = barrier.halfThickness;
        gameplayStateRows.push_back(row);
    }

    std::sort(
        gameplayStateRows.begin(),
        gameplayStateRows.end(),
        [](const GameplayStateRow& lhs, const GameplayStateRow& rhs)
        {
            if (lhs.kind != rhs.kind)
                return lhs.kind < rhs.kind;
            if (lhs.sourceNet != rhs.sourceNet)
                return lhs.sourceNet < rhs.sourceNet;
            if (lhs.targetNet != rhs.targetNet)
                return lhs.targetNet < rhs.targetNet;
            return lhs.startTick < rhs.startTick;
        });

    std::vector<flatbuffers::Offset<Shared::Schema::GameplayStateSnapshot>>
        gameplayStates;
    gameplayStates.reserve(gameplayStateRows.size());
    for (const GameplayStateRow& row : gameplayStateRows)
    {
        gameplayStates.push_back(Shared::Schema::CreateGameplayStateSnapshot(
            fbb,
            row.kind,
            row.sourceNet,
            row.targetNet,
            row.startTick,
            row.expireTick,
            row.stackCount,
            row.rank,
            row.flags,
            row.position.x,
            row.position.y,
            row.position.z,
            row.direction.x,
            row.direction.y,
            row.direction.z,
            row.magnitude0,
            row.magnitude1));
    }
    const auto gameplayStatesOffset = fbb.CreateVector(gameplayStates);

    const auto snapshot = Shared::Schema::CreateSnapshot(
        fbb,
        serverTick,
        serverTimeMs,
        rngState,
        entitiesOffset,
        lastAckedSeq,
        yourNetId,
        0,
        matchScore.Blue.iTotalKills,
        matchScore.Red.iTotalKills,
        matchScore.Blue.iDestroyedTurrets,
        matchScore.Red.iDestroyedTurrets,
        matchScore.Blue.iDragons,
        matchScore.Red.iDragons,
        matchScore.Blue.iBarons,
        matchScore.Red.iBarons,
        timelineEpoch,
        branchId,
        toolRevision,
        simPaused,
        simSpeedMul,
        gameplayStatesOffset);
    // Finish writes the Snapshot root; Release returns the byte buffer for the server packet path.
    fbb.Finish(snapshot);
    return fbb.Release();
}
