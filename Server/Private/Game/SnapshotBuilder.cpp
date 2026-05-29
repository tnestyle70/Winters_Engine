#include "Game/SnapshotBuilder.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
//Viego Soul
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

#include <algorithm>
#include <cstdio>
#include <vector>

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
        u16_t animId = static_cast<u16_t>(eNetAnimId::Idle);
        u16_t animPhaseFrame = 0;
        u64_t animStartTick = 0;
        u32_t actionSeq = 0;
        u16_t animPlaybackRateQ8 = 256;
        u16_t animFlags = 0;
        u8_t championId = 0;
        u8_t team = 0;
        u8_t level = 1;
        f32_t xpCurrent = 0.f;
        f32_t xpRequired = 0.f;
        u8_t skillPoints = 0;
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
            entityKind = Shared::Schema::EntityKind::Champion;
            stateFlags |= kSnapshotStateViegoSoulFlag;
        }

        if (world.HasComponent<FormOverrideComponent>(entity))
        {
            const auto& form = world.GetComponent<FormOverrideComponent>(entity);
            if (form.bActive &&
                form.visualChampion != eChampion::END &&
                form.visualChampion != eChampion::NONE)
            {
                championId = static_cast<u8_t>(form.visualChampion);
                if (entityKind == Shared::Schema::EntityKind::Champion)
                    subtype = static_cast<u16_t>(form.visualChampion);
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

        if (world.HasComponent<NetAnimationComponent>(entity))
        {
            const auto& anim = world.GetComponent<NetAnimationComponent>(entity);
            animId = anim.animId;
            animPhaseFrame = anim.animPhaseFrame;
            animStartTick = anim.animStartTick;
            actionSeq = anim.actionSeq;
            animPlaybackRateQ8 = anim.playbackRateQ8;
            animFlags = anim.flags;
        }

        if (world.HasComponent<MoveTargetComponent>(entity) &&
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
                ownerNet = entityMap.ToNet(target);
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

        if (world.HasComponent<GoldComponent>(entity))
            gold = world.GetComponent<GoldComponent>(entity).amount;

        if (world.HasComponent<InventoryComponent>(entity))
        {
            const InventoryComponent& inventory = world.GetComponent<InventoryComponent>(entity);
            inventoryItemIds.reserve(inventory.count);
            for (u8_t i = 0; i < inventory.count && i < InventoryComponent::kMaxSlots; ++i)
                inventoryItemIds.push_back(inventory.itemIds[i]);
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

        const auto cooldownOffset = fbb.CreateVector(cooldowns);
        const auto cooldownDurationOffset = fbb.CreateVector(cooldownDurations);
        const auto rankOffset = fbb.CreateVector(ranks);
        const auto inventoryOffset = fbb.CreateVector(inventoryItemIds);

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
                    "[YawTrace][ServerSnapshot] tick=%llu ack=%u yourNet=%u net=%u entity=%u champion=%u pos=(%.3f,%.3f,%.3f) rawYaw=%.4f wireYaw=%.4f anim=%u actionSeq=%u state=0x%08X hasMove=%u moveHasTarget=%u moveTarget=(%.3f,%.3f,%.3f) pathIndex=%u pathCount=%u path0=(%.3f,%.3f,%.3f) hasFacing=%u lockTicks=%u facingSeq=%u facingTarget=(%.3f,%.3f,%.3f) facingDir=(%.3f,%.3f)\n",
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
                    static_cast<u32_t>(animId),
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
            animId,
            animPhaseFrame,
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
            animStartTick,
            actionSeq,
            animPlaybackRateQ8,
            animFlags,
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
            abilityHaste));
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
        0);

    fbb.Finish(snapshot);
    return fbb.Release();
}
