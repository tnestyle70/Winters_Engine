#include "Game/SnapshotBuilder.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
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
    bool_t IsMoveBlockingSnapshotAction(eActionStateId actionId)
    {
        switch (actionId)
        {
        case eActionStateId::SkillQ:
        case eActionStateId::SkillW:
        case eActionStateId::SkillE:
        case eActionStateId::SkillR:
            return true;
        default:
            return false;
        }
    }

    u8_t ResolveMoveBlockingActionSlot(eActionStateId actionId)
    {
        switch (actionId)
        {
        case eActionStateId::SkillQ:
            return static_cast<u8_t>(eSkillSlot::Q);
        case eActionStateId::SkillW:
            return static_cast<u8_t>(eSkillSlot::W);
        case eActionStateId::SkillE:
            return static_cast<u8_t>(eSkillSlot::E);
        case eActionStateId::SkillR:
            return static_cast<u8_t>(eSkillSlot::R);
        default:
            return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
    }

    eChampion ResolveSnapshotChampion(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).id;
        if (world.HasComponent<StatComponent>(entity))
            return world.GetComponent<StatComponent>(entity).championId;
        return eChampion::NONE;
    }

    bool_t IsMoveLockedBySnapshotAction(CWorld& world, EntityID entity, u64_t serverTick)
    {
        if (!world.HasComponent<ReplicatedActionComponent>(entity))
            return false;

        const auto& action = world.GetComponent<ReplicatedActionComponent>(entity);
        const auto actionId = static_cast<eActionStateId>(action.actionId);
        if (!IsMoveBlockingSnapshotAction(actionId))
            return false;
        if (serverTick < action.startTick)
            return false;

        const eChampion champion = ResolveSnapshotChampion(world, entity);
        const u8_t slot = ResolveMoveBlockingActionSlot(actionId);
        const u8_t stage = action.stage == 0u ? 1u : action.stage;
        const u64_t lockTicks =
            GetDefaultChampionSkillActionLockTicks(champion, slot, stage);
        return (serverTick - action.startTick) < lockTicks;
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
    NetEntityId yourNetId)
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
        u32_t ownerNet = 0;
        u16_t subtype = 0;
        u16_t projectileKind = 0;
        u32_t projectileOwnerNet = 0;
        u32_t projectileTargetNet = 0;
        f32_t projectileSpeed = 0.f;
        f32_t projectileRadius = 0.f;
        f32_t projectileMaxDist = 0.f;
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
            if (maxMana <= 0.f)
                maxMana = champion.maxMana;
        }
        if (world.HasComponent<YasuoStateComponent>(entity))
        {
            const auto& yasuoState = world.GetComponent<YasuoStateComponent>(entity);
            mana = yasuoState.fPassiveFlow;
            maxMana = yasuoState.fPassiveFlowMax;
            shield = yasuoState.fPassiveShieldRemaining;
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

        if (world.HasComponent<WardComponent>(entity))
        {
            const auto& ward = world.GetComponent<WardComponent>(entity);
            team = static_cast<u8_t>(ward.ownerTeam);
            subtype = ward.bControlWard ? 1u : 0u;
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

        if (world.HasComponent<TurretProjectileComponent>(entity))
        {
            const auto& projectile = world.GetComponent<TurretProjectileComponent>(entity);
            entityKind = Shared::Schema::EntityKind::Projectile;
            projectileOwnerNet = entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = entityMap.ToNet(projectile.targetEntity);
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            ownerNet = projectileOwnerNet;
        }

        if (world.HasComponent<SkillProjectileComponent>(entity))
        {
            const auto& projectile = world.GetComponent<SkillProjectileComponent>(entity);
            entityKind = Shared::Schema::EntityKind::Projectile;
            projectileKind = static_cast<u16_t>(projectile.kind);
            projectileOwnerNet = entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = NULL_NET_ENTITY;
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            projectileMaxDist = projectile.maxDistance;
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
        }

        if (!IsMoveLockedBySnapshotAction(world, entity, serverTick) &&
            world.HasComponent<MoveTargetComponent>(entity) &&
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget)
        {
            stateFlags |= kSnapshotStateMovingFlag;
        }

        if (world.HasComponent<GameplayStateComponent>(entity) &&
            (world.GetComponent<GameplayStateComponent>(entity).stateFlags &
                kGameplayStateInvisibleFlag) != 0u)
        {
            stateFlags |= kSnapshotStateInvisibleFlag;
        }

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
            aiDebugTraceRows.reserve(count);
            const u8_t start = static_cast<u8_t>(
                (ai.debugDecisionTraceHead + kChampionAIDebugTraceCapacity - count) %
                kChampionAIDebugTraceCapacity);
            for (u8_t i = 0u; i < count; ++i)
            {
                const u8_t index = static_cast<u8_t>(
                    (start + i) % kChampionAIDebugTraceCapacity);
                const ChampionAIDecisionTraceEntry& row = ai.debugDecisionTrace[index];
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
                    row.turretDanger));
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
            spellbookRemaining));
    }

    const auto entitiesOffset = fbb.CreateVector(snapshots);
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
        matchScore.Red.iBarons);
    // Finish writes the Snapshot root; Release returns the byte buffer for the server packet path.
    fbb.Finish(snapshot);
    return fbb.Release();
}
