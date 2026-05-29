#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    struct ChampionAIContext
    {
        EntityID enemyChampion = NULL_ENTITY;
        EntityID enemyMinion = NULL_ENTITY;
        EntityID enemyStructure = NULL_ENTITY;
        EntityID alliedWave = NULL_ENTITY;

        f32_t selfHpRatio = 1.f;
        f32_t enemyHpRatio = 1.f;
        f32_t enemyDistance = 999.f;
        f32_t attackRange = 1.5f;
        f32_t waveDistance = 999.f;
        f32_t turretDanger = 0.f;

        bool_t bAlliedWaveNearby = false;
        bool_t bStructureWaveTanking = false;
        bool_t bInsideEnemyTurretDanger = false;
    };

    eTeam EnemyTeam(eTeam team)
    {
        return (team == eTeam::Red) ? eTeam::Blue : eTeam::Red;
    }

    bool_t IsAliveTarget(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            return !health.bIsDead && health.fCurrent > 0.f;
        }

        if (world.HasComponent<MinionComponent>(entity))
            return world.GetComponent<MinionComponent>(entity).hp > 0.f;

        if (world.HasComponent<StructureComponent>(entity))
            return world.GetComponent<StructureComponent>(entity).hp > 0.f;

        return true;
    }

    bool_t TryGetPosition(CWorld& world, EntityID entity, Vec3& outPos)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<TransformComponent>(entity))
        {
            return false;
        }

        outPos = world.GetComponent<TransformComponent>(entity).GetPosition();
        return true;
    }

    f32_t HealthRatio(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return 1.f;

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            if (health.fMaximum > 0.001f)
                return WintersMath::Clamp01(health.fCurrent / health.fMaximum);
        }

        if (world.HasComponent<ChampionComponent>(entity))
        {
            const auto& champion = world.GetComponent<ChampionComponent>(entity);
            if (champion.maxHp > 0.001f)
                return WintersMath::Clamp01(champion.hp / champion.maxHp);
        }

        if (world.HasComponent<MinionComponent>(entity))
        {
            const auto& minion = world.GetComponent<MinionComponent>(entity);
            if (minion.maxHp > 0.001f)
                return WintersMath::Clamp01(minion.hp / minion.maxHp);
        }

        if (world.HasComponent<StructureComponent>(entity))
        {
            const auto& structure = world.GetComponent<StructureComponent>(entity);
            if (structure.maxHp > 0.001f)
                return WintersMath::Clamp01(structure.hp / structure.maxHp);
        }

        return 1.f;
    }

    f32_t ResolveAttackRange(CWorld& world, EntityID self, eChampion champion)
    {
        if (world.HasComponent<StatComponent>(self))
        {
            const f32_t range = world.GetComponent<StatComponent>(self).attackRange;
            if (range > 0.f)
                return range;
        }

        return BuildDefaultChampionStat(champion).attackRange;
    }

    bool_t IsSkillReady(CWorld& world, EntityID self, u8_t slot)
    {
        if (slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
            return false;

        if (slot != static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            if (!world.HasComponent<SkillRankComponent>(self) ||
                world.GetComponent<SkillRankComponent>(self).ranks[slot] == 0u)
                return false;
        }

        if (!world.HasComponent<SkillStateComponent>(self))
            return false;

        return world.GetComponent<SkillStateComponent>(self).slots[slot].cooldownRemaining <= 0.f;
    }

    bool_t CanUseComboStep(
        CWorld& world,
        EntityID self,
        const ChampionAIComboStep& step,
        const ChampionAIContext& ctx)
    {
        if (step.slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
            return false;
        if (step.selfHpMinRatio > 0.f && ctx.selfHpRatio + 0.001f < step.selfHpMinRatio)
            return false;
        if (step.enemyHpMaxRatio < 0.999f && ctx.enemyHpRatio > step.enemyHpMaxRatio)
            return false;
        if (ctx.enemyDistance + 0.001f < step.minRange)
            return false;
        if (step.maxRange > 0.f && ctx.enemyDistance > step.maxRange)
            return false;
        return IsSkillReady(world, self, step.slot);
    }

    enum class eChampionAIBehaviorStatus : u8_t
    {
        Failure,
        Success,
        Running,
    };

    void SetChampionAIIntent(
        ChampionAIComponent& ai,
        eChampionAIIntent intent,
        bool_t bHoldIntent = false)
    {
        ai.intent = intent;
        if (bHoldIntent)
            ai.intentHoldTimer = ai.intentHoldDuration;
    }

    void ClearChampionAICombo(ChampionAIComponent& ai)
    {
        ai.comboTarget = NULL_ENTITY;
        ai.comboStep = 0u;
    }

    void CompleteChampionAICombo(ChampionAIComponent& ai)
    {
        ClearChampionAICombo(ai);
        SetChampionAIIntent(ai, eChampionAIIntent::FarmMinion, true);
        ai.lastDecisionRoll = 1.f;
    }

    u8_t SlotFromChampionAIActionAnimation(eNetAnimId animId)
    {
        switch (animId)
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

    u8_t StageFromChampionAIActionAnimationFlags(u16_t flags)
    {
        const u8_t stage = static_cast<u8_t>((flags >> 12) & 0x0fu);
        return stage == 0u ? 1u : stage;
    }

    bool_t IsChampionAIActionLocked(
        CWorld& world,
        EntityID self,
        eChampion champion,
        const TickContext& tc)
    {
        if (world.HasComponent<CombatActionComponent>(self))
        {
            const auto& action = world.GetComponent<CombatActionComponent>(self);
            if (action.eKind != eCombatActionKind::None &&
                tc.tickIndex < action.uEndTick)
            {
                return true;
            }
        }

        if (!world.HasComponent<NetAnimationComponent>(self))
            return false;

        const auto& anim = world.GetComponent<NetAnimationComponent>(self);
        const auto animId = static_cast<eNetAnimId>(anim.animId);
        switch (animId)
        {
        case eNetAnimId::BasicAttack:
        case eNetAnimId::SkillQ:
        case eNetAnimId::SkillW:
        case eNetAnimId::SkillE:
        case eNetAnimId::SkillR:
            break;
        default:
            return false;
        }

        if (tc.tickIndex < anim.animStartTick)
            return false;

        const u8_t slot = SlotFromChampionAIActionAnimation(animId);
        const u8_t stage = StageFromChampionAIActionAnimationFlags(anim.flags);
        const u64_t lockTicks = GetDefaultChampionSkillActionLockTicks(champion, slot, stage);
        return (tc.tickIndex - anim.animStartTick) < lockTicks;
    }

    GameCommand MakeAICommand(
        ChampionAIComponent& ai,
        const TickContext& tc,
        EntityID self,
        eCommandKind kind)
    {
        GameCommand cmd{};
        cmd.kind = kind;
        cmd.issuerEntity = self;
        cmd.issuedAtTick = tc.tickIndex;
        cmd.sequenceNum = ai.nextCommandSequence++;
        return cmd;
    }

    const char* CommandName(eCommandKind kind)
    {
        switch (kind)
        {
        case eCommandKind::Move:
            return "Move";
        case eCommandKind::CastSkill:
            return "CastSkill";
        case eCommandKind::BasicAttack:
            return "BasicAttack";
        case eCommandKind::Recall:
            return "Recall";
        default:
            return "Other";
        }
    }

    void LogChampionAICommand(
        const char* reason,
        const TickContext& tc,
        EntityID self,
        const ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        const Vec3& commandPos,
        EntityID target,
        eCommandKind kind,
        u8_t slot)
    {
        static u32_t s_logCount = 0;
        if (s_logCount >= 512u)
            return;

        char msg[448]{};
        sprintf_s(msg,
            "[ChampionAI] tick=%llu entity=%u champ=%u team=%u lane=%u state=%u intent=%u action=%u roll=%.3f cmd=%s slot=%u reason=%s target=%u pos=(%.2f,%.2f,%.2f) cmdPos=(%.2f,%.2f,%.2f)\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(self),
            static_cast<u32_t>(champion),
            static_cast<u32_t>(ai.team),
            static_cast<u32_t>(ai.lane),
            static_cast<u32_t>(ai.state),
            static_cast<u32_t>(ai.intent),
            static_cast<u32_t>(ai.lastAction),
            static_cast<double>(ai.lastDecisionRoll),
            CommandName(kind),
            static_cast<u32_t>(slot),
            reason ? reason : "-",
            static_cast<u32_t>(target),
            selfPos.x,
            selfPos.y,
            selfPos.z,
            commandPos.x,
            commandPos.y,
            commandPos.z);
        WintersOutputAIDebugStringA(msg);
        ++s_logCount;
    }

    bool_t HasEquivalentMoveTarget(CWorld& world, EntityID self, const Vec3& goal)
    {
        if (!world.HasComponent<MoveTargetComponent>(self))
            return false;

        const auto& moveTarget = world.GetComponent<MoveTargetComponent>(self);
        if (!moveTarget.bHasTarget)
            return false;

        return WintersMath::DistanceSqXZ(moveTarget.target, goal) <= 0.25f;
    }

    bool_t EmitMoveCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        const Vec3& goal,
        eChampionAIAction action,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        ai.lastAction = action;
        if (HasEquivalentMoveTarget(world, self, goal))
            return false;

        GameCommand move = MakeAICommand(ai, tc, self, eCommandKind::Move);
        move.groundPos = goal;
        outCommands.push_back(move);
        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, goal,
            NULL_ENTITY, move.kind, move.slot);
        return true;
    }

    bool_t EmitBasicAttackCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        eChampionAIAction action,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        if (!IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::BasicAttack)) ||
            target == NULL_ENTITY ||
            !IsAliveTarget(world, target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            return false;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        const f32_t attackRange = ResolveAttackRange(world, self, champion);
        if (attackRange > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > attackRange * attackRange)
        {
            return false;
        }

        ai.lastAction = action;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::BasicAttack);
        cmd.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        cmd.targetEntity = target;
        outCommands.push_back(cmd);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, targetPos,
            target, cmd.kind, cmd.slot);
        return true;
    }

    bool_t EmitSkillCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        u8_t slot,
        const char* reason,
        std::vector<GameCommand>& outCommands,
        u16_t itemId = 0u,
        u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity))
    {
        if (!IsSkillReady(world, self, slot) ||
            target == NULL_ENTITY ||
            !IsAliveTarget(world, target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            return false;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        const f32_t range = GetDefaultChampionSkillRange(champion, slot);
        if (range > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > range * range)
        {
            return false;
        }

        Vec3 commandPos = targetPos;
        Vec3 direction = WintersMath::DirectionXZ(selfPos, targetPos);
        if (targetMode == static_cast<u8_t>(eChampionAIComboTargetMode::AwayFromTarget))
        {
            direction = WintersMath::DirectionXZ(targetPos, selfPos);
            const f32_t castDistance = range > 0.f ? range : 4.f;
            commandPos = Vec3{
                selfPos.x + direction.x * castDistance,
                selfPos.y,
                selfPos.z + direction.z * castDistance
            };
        }

        ai.lastAction = eChampionAIAction::AttackChampion;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
        cmd.slot = slot;
        cmd.itemId = itemId;
        cmd.targetEntity = target;
        cmd.groundPos = commandPos;
        cmd.direction = direction;
        outCommands.push_back(cmd);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, commandPos,
            target, cmd.kind, cmd.slot);
        return true;
    }

    EntityID FindEnemyChampion(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        const Vec3& pos,
        f32_t range)
    {
        EntityID best = NULL_ENTITY;
        f32_t bestSq = range * range;
        world.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, TransformComponent& transform)
            {
                if (world.HasComponent<PracticeDummyTag>(e))
                    return;
                if (champion.team == myTeam || !IsAliveTarget(world, e))
                    return;
                if (!GameplayStateQuery::CanBeTargetedBy(world, self, e))
                    return;

                const f32_t distSq = WintersMath::DistanceSqXZ(pos, transform.GetPosition());
                if (distSq < bestSq)
                {
                    bestSq = distSq;
                    best = e;
                }
            });
        return best;
    }

    EntityID FindAlliedLaneMinion(
        CWorld& world,
        eTeam myTeam,
        u8_t lane,
        const Vec3& pos,
        f32_t range,
        f32_t& outDistance)
    {
        EntityID best = NULL_ENTITY;
        f32_t bestSq = range * range;
        world.ForEach<MinionComponent, TransformComponent>(
            [&](EntityID e, MinionComponent& minion, TransformComponent& transform)
            {
                if (minion.team != myTeam ||
                    minion.laneType != lane ||
                    !IsAliveTarget(world, e))
                {
                    return;
                }

                const f32_t distSq = WintersMath::DistanceSqXZ(pos, transform.GetPosition());
                if (distSq < bestSq)
                {
                    bestSq = distSq;
                    best = e;
                }
            });

        outDistance = (best != NULL_ENTITY) ? std::sqrt(std::max(0.f, bestSq)) : 999.f;
        return best;
    }

    EntityID FindEnemyMinion(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t lane,
        const Vec3& pos,
        f32_t range)
    {
        EntityID best = NULL_ENTITY;
        f32_t bestScore = -1.f;
        const f32_t rangeSq = range * range;
        world.ForEach<MinionComponent, TransformComponent>(
            [&](EntityID e, MinionComponent& minion, TransformComponent& transform)
            {
                if (minion.team == myTeam ||
                    minion.laneType != lane ||
                    !IsAliveTarget(world, e) ||
                    !GameplayStateQuery::CanBeTargetedBy(world, self, e))
                {
                    return;
                }

                const f32_t distSq = WintersMath::DistanceSqXZ(pos, transform.GetPosition());
                if (distSq > rangeSq)
                    return;

                const f32_t hpRatio = (minion.maxHp > 0.001f)
                    ? WintersMath::Clamp01(minion.hp / minion.maxHp)
                    : 1.f;
                const f32_t distanceFit =
                    1.f - WintersMath::Clamp01(std::sqrt(std::max(0.f, distSq)) / std::max(1.f, range));
                const f32_t score = (1.f - hpRatio) * 60.f + distanceFit * 25.f;
                if (score > bestScore)
                {
                    bestScore = score;
                    best = e;
                }
            });
        return best;
    }

    i32_t ObjectiveRank(const StructureComponent& structure)
    {
        const u32_t turretKind =
            static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
        const u32_t inhibitorKind =
            static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Inhibitor);
        const u32_t nexusKind =
            static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);

        if (structure.kind == turretKind)
        {
            switch (static_cast<Winters::Map::eTurretTier>(structure.tier))
            {
            case Winters::Map::eTurretTier::Outer:
                return 0;
            case Winters::Map::eTurretTier::Inner:
                return 1;
            case Winters::Map::eTurretTier::Inhibitor:
                return 2;
            case Winters::Map::eTurretTier::Nexus:
                return 4;
            default:
                return 1000;
            }
        }

        if (structure.kind == inhibitorKind)
            return 3;
        if (structure.kind == nexusKind)
            return 5;
        return 1000;
    }

    bool_t IsStructureOnLanePath(const StructureComponent& structure, u8_t lane, i32_t rank)
    {
        if (rank <= 3)
            return structure.lane == lane;
        return rank == 4 || rank == 5;
    }

    bool_t HasBlockingObjective(CWorld& world, eTeam enemyTeam, u8_t lane, i32_t candidateRank)
    {
        bool_t bBlocked = false;
        world.ForEach<StructureComponent>(
            [&](EntityID e, StructureComponent& structure)
            {
                if (bBlocked ||
                    structure.team != enemyTeam ||
                    !IsAliveTarget(world, e))
                {
                    return;
                }

                const i32_t rank = ObjectiveRank(structure);
                if (rank >= candidateRank)
                    return;

                if (candidateRank <= 3)
                {
                    if (structure.lane == lane)
                        bBlocked = true;
                    return;
                }

                if (candidateRank == 4)
                {
                    if (rank <= 3 && structure.lane == lane)
                        bBlocked = true;
                    return;
                }

                if (candidateRank == 5 &&
                    (rank == 4 || (rank <= 3 && structure.lane == lane)))
                {
                    bBlocked = true;
                }
            });
        return bBlocked;
    }

    bool_t HasAlliedMinionInStructureRange(
        CWorld& world,
        eTeam myTeam,
        u8_t lane,
        EntityID structureEntity,
        const Vec3& structurePos)
    {
        f32_t range = 7.75f;
        if (world.HasComponent<TurretAIComponent>(structureEntity))
            range = world.GetComponent<TurretAIComponent>(structureEntity).attackRange;

        const f32_t rangeSq = (range + 0.5f) * (range + 0.5f);
        bool_t bFound = false;
        world.ForEach<MinionComponent, TransformComponent>(
            [&](EntityID e, MinionComponent& minion, TransformComponent& transform)
            {
                if (bFound ||
                    minion.team != myTeam ||
                    minion.laneType != lane ||
                    !IsAliveTarget(world, e))
                {
                    return;
                }

                if (WintersMath::DistanceSqXZ(structurePos, transform.GetPosition()) <= rangeSq)
                    bFound = true;
            });
        return bFound;
    }

    bool_t CanSiegeStructure(
        CWorld& world,
        eTeam myTeam,
        u8_t lane,
        EntityID structureEntity,
        const StructureComponent& structure,
        const Vec3& structurePos)
    {
        const u32_t turretKind =
            static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
        if (structure.kind == turretKind)
            return HasAlliedMinionInStructureRange(world, myTeam, lane, structureEntity, structurePos);

        return HasAlliedMinionInStructureRange(world, myTeam, lane, structureEntity, structurePos);
    }

    f32_t ComputeTurretDanger(
        CWorld& world,
        eTeam myTeam,
        u8_t lane,
        const Vec3& pos,
        bool_t& outInsideDanger)
    {
        f32_t danger = 0.f;
        outInsideDanger = false;
        const u32_t turretKind =
            static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);

        world.ForEach<StructureComponent, TransformComponent>(
            [&](EntityID e, StructureComponent& structure, TransformComponent& transform)
            {
                if (structure.kind != turretKind ||
                    structure.team == myTeam ||
                    structure.team == eTeam::Neutral ||
                    structure.lane != lane ||
                    !IsAliveTarget(world, e))
                {
                    return;
                }

                f32_t range = 7.75f;
                if (world.HasComponent<TurretAIComponent>(e))
                    range = world.GetComponent<TurretAIComponent>(e).attackRange;

                const Vec3 turretPos = transform.GetPosition();
                const f32_t distSq = WintersMath::DistanceSqXZ(pos, turretPos);
                const f32_t rangeSq = range * range;
                const f32_t warningRange = range + 1.5f;
                if (distSq > warningRange * warningRange)
                    return;

                const bool_t bWaveTanking =
                    HasAlliedMinionInStructureRange(world, myTeam, lane, e, turretPos);
                if (distSq <= rangeSq)
                {
                    outInsideDanger = true;
                    danger = std::max(danger, bWaveTanking ? 0.45f : 1.25f);
                }
                else
                {
                    danger = std::max(danger, bWaveTanking ? 0.15f : 0.75f);
                }
            });

        return danger;
    }

    EntityID FindEnemyStructure(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t lane,
        const Vec3& pos,
        f32_t range,
        bool_t& outWaveTanking)
    {
        EntityID best = NULL_ENTITY;
        i32_t bestRank = 1000;
        f32_t bestSq = range * range;
        outWaveTanking = false;
        const eTeam enemyTeam = EnemyTeam(myTeam);

        world.ForEach<StructureComponent, TransformComponent>(
            [&](EntityID e, StructureComponent& structure, TransformComponent& transform)
            {
                if (structure.team != enemyTeam ||
                    !IsAliveTarget(world, e) ||
                    !GameplayStateQuery::CanBeTargetedBy(world, self, e))
                {
                    return;
                }

                const i32_t rank = ObjectiveRank(structure);
                if (rank >= 1000 ||
                    !IsStructureOnLanePath(structure, lane, rank) ||
                    HasBlockingObjective(world, enemyTeam, lane, rank))
                {
                    return;
                }

                const Vec3 targetPos = transform.GetPosition();
                const bool_t bCanSiege =
                    CanSiegeStructure(world, myTeam, lane, e, structure, targetPos);
                if (!bCanSiege)
                    return;

                const f32_t distSq = WintersMath::DistanceSqXZ(pos, targetPos);
                if (distSq > range * range)
                    return;

                if (rank < bestRank || (rank == bestRank && distSq < bestSq))
                {
                    bestRank = rank;
                    bestSq = distSq;
                    best = e;
                    outWaveTanking = true;
                }
            });
        return best;
    }

    ChampionAIContext BuildChampionAIContext(
        CWorld& world,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos)
    {
        ChampionAIContext ctx{};
        ctx.selfHpRatio = HealthRatio(world, self);
        ctx.attackRange = ResolveAttackRange(world, self, champion.id);

        EntityID targetChampion = ai.lockedChampion;
        Vec3 targetChampionPos{};
        if (targetChampion == NULL_ENTITY ||
            !IsAliveTarget(world, targetChampion) ||
            !GameplayStateQuery::CanBeTargetedBy(world, self, targetChampion) ||
            !TryGetPosition(world, targetChampion, targetChampionPos) ||
            WintersMath::DistanceSqXZ(selfPos, targetChampionPos) > ai.leashRange * ai.leashRange)
        {
            targetChampion = FindEnemyChampion(
                world,
                self,
                champion.team,
                selfPos,
                ai.championScanRange);
            ai.lockedChampion = targetChampion;
            if (targetChampion != NULL_ENTITY)
                TryGetPosition(world, targetChampion, targetChampionPos);
        }

        ctx.enemyChampion = targetChampion;
        if (targetChampion != NULL_ENTITY)
        {
            const f32_t distSq = WintersMath::DistanceSqXZ(selfPos, targetChampionPos);
            ctx.enemyDistance = std::sqrt(std::max(0.f, distSq));
            ctx.enemyHpRatio = HealthRatio(world, targetChampion);
        }

        ctx.enemyMinion = FindEnemyMinion(
            world,
            self,
            champion.team,
            ai.lane,
            selfPos,
            ai.minionScanRange);

        f32_t waveDistance = 999.f;
        ctx.alliedWave = FindAlliedLaneMinion(
            world,
            champion.team,
            ai.lane,
            selfPos,
            std::max(80.f, ai.structureScanRange + ai.minionScanRange),
            waveDistance);
        ctx.waveDistance = waveDistance;
        ctx.bAlliedWaveNearby =
            ctx.alliedWave != NULL_ENTITY &&
            waveDistance <= ai.waveJoinRange;

        ctx.enemyStructure = FindEnemyStructure(
            world,
            self,
            champion.team,
            ai.lane,
            selfPos,
            ai.structureScanRange,
            ctx.bStructureWaveTanking);

        ctx.turretDanger = ComputeTurretDanger(
            world,
            champion.team,
            ai.lane,
            selfPos,
            ctx.bInsideEnemyTurretDanger);
        return ctx;
    }

    bool_t TryEmitAttackChampionSkill(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIProfile& profile,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const u8_t count = std::min(profile.skillRuleCount, static_cast<u8_t>(4));
        for (u8_t i = 0; i < count; ++i)
        {
            const ChampionAISkillRule& rule = profile.skillRules[i];
            if (rule.score <= 0.f ||
                rule.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
                ctx.enemyDistance + 0.001f < rule.minRange)
            {
                continue;
            }

            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rule.slot, "lane-attack-champion-skill", outCommands))
            {
                return true;
            }
        }

        return false;
    }

    bool_t EmitChampionAIComboStep(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIComboStep& step,
        std::vector<GameCommand>& outCommands)
    {
        if (step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "combo-attack-champion-ba", outCommands);
        }

        return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            step.slot, "combo-attack-champion-skill", outCommands,
            step.itemId, step.targetMode);
    }

    bool_t TryEmitAttackChampionCombo(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const ChampionAIComboPlan& combo = GetChampionAIComboPlan(champion.id);
        if (combo.stepCount == 0u)
            return false;

        const bool_t bWasActive = ai.comboTarget == target;
        if (!bWasActive)
        {
            ai.comboTarget = target;
            ai.comboStep = 0u;
        }

        const u8_t stepCount = std::min(combo.stepCount, static_cast<u8_t>(8u));
        const u8_t index = static_cast<u8_t>(ai.comboStep % stepCount);
        const ChampionAIComboStep& step = combo.steps[index];
        if (!CanUseComboStep(world, self, step, ctx))
        {
            if (bWasActive &&
                step.maxRange > 0.f &&
                ctx.enemyDistance > step.maxRange)
            {
                Vec3 targetPos{};
                if (TryGetPosition(world, target, targetPos))
                {
                    const bool_t bMoveEmitted = EmitMoveCommand(
                        world, tc, self, ai, champion.id, selfPos, targetPos,
                        eChampionAIAction::AttackChampion, "combo-attack-champion-chase",
                        outCommands
                    );
                    return bMoveEmitted || bWasActive;
                }
            }
            if (!bWasActive)
                ClearChampionAICombo(ai);
            return bWasActive;
        }

        if (EmitChampionAIComboStep(world, tc, self, ai, champion, selfPos,
            target, step, outCommands))
        {
            const u8_t nextStep = static_cast<u8_t>((index + 1u) % stepCount);
            ai.comboStep = nextStep;
            if (nextStep == 0u)
                CompleteChampionAICombo(ai);
            return true;
        }

        if (!bWasActive)
            ClearChampionAICombo(ai);
        return bWasActive;
    }

    u32_t MakeChampionAIRoll(const TickContext& tc, EntityID self, const ChampionAIComponent& ai)
    {
        u32_t x = static_cast<u32_t>(tc.tickIndex) ^
            static_cast<u32_t>(tc.tickIndex >> 32) ^
            (static_cast<u32_t>(self) * 747796405u) ^
            (ai.nextCommandSequence * 2891336453u);
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return x & 0xFFFFu;
    }

    bool_t CanAttackChampion(const ChampionAIComponent& ai, const ChampionAIContext& ctx)
    {
        if (ctx.enemyChampion == NULL_ENTITY)
            return false;
        if (ctx.selfHpRatio <= ai.retreatHpRatio + 0.10f)
            return false;
        if (ctx.bInsideEnemyTurretDanger)
            return false;
        if (ctx.turretDanger > 0.85f && !ctx.bStructureWaveTanking)
            return false;
        return true;
    }

    u32_t ActionBit(eChampionAIAction action)
    {
        switch (action)
        {
        case eChampionAIAction::MoveToSafeAnchor:
            return kChampionAIActionBitMoveToSafeAnchor;
        case eChampionAIAction::FollowWave:
            return kChampionAIActionBitFollowWave;
        case eChampionAIAction::AttackMinion:
            return kChampionAIActionBitAttackMinion;
        case eChampionAIAction::AttackChampion:
            return kChampionAIActionBitAttackChampion;
        case eChampionAIAction::AttackStructure:
            return kChampionAIActionBitAttackStructure;
        case eChampionAIAction::Retreat:
            return kChampionAIActionBitRetreat;
        default:
            return 0u;
        }
    }

    bool_t IsActionAvailable(u32_t mask, eChampionAIAction action)
    {
        const u32_t bit = ActionBit(action);
        return bit != 0u && (mask & bit) != 0u;
    }

    u32_t BuildChampionAIAvailableActionMask(const ChampionAIComponent& ai, const ChampionAIContext& ctx)
    {
        u32_t mask =
            kChampionAIActionBitMoveToSafeAnchor |
            kChampionAIActionBitFollowWave |
            kChampionAIActionBitRetreat;

        if (ctx.enemyMinion != NULL_ENTITY)
            mask |= kChampionAIActionBitAttackMinion;
        if (CanAttackChampion(ai, ctx))
            mask |= kChampionAIActionBitAttackChampion;
        if (ctx.enemyStructure != NULL_ENTITY && ctx.bStructureWaveTanking)
            mask |= kChampionAIActionBitAttackStructure;

        return mask;
    }

    u32_t BuildChampionAIAvailableSkillMask(
        CWorld& world,
        EntityID self,
        eChampion champion,
        const ChampionAIProfile& profile,
        const ChampionAIContext& ctx)
    {
        u32_t mask = 0u;
        if (ctx.enemyChampion == NULL_ENTITY)
            return mask;

        const u8_t count = std::min(profile.skillRuleCount, static_cast<u8_t>(4));
        for (u8_t i = 0; i < count; ++i)
        {
            const ChampionAISkillRule& rule = profile.skillRules[i];
            if (rule.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
                rule.slot >= static_cast<u8_t>(eSkillSlot::SLOT_END) ||
                ctx.enemyDistance + 0.001f < rule.minRange ||
                !IsSkillReady(world, self, rule.slot))
            {
                continue;
            }

            const f32_t range = GetDefaultChampionSkillRange(champion, rule.slot);
            if (range > 0.f && ctx.enemyDistance > range)
                continue;

            mask |= 1u << (rule.slot - 1u);
        }

        return mask;
    }

    void SampleLaneCombatIntent(
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        ai.intentHoldTimer = std::max(0.f, ai.intentHoldTimer - tc.fDt);
        if (ai.intentHoldTimer > 0.f)
        {
            if (ai.intent != eChampionAIIntent::AttackChampion ||
                CanAttackChampion(ai, ctx))
            {
                return;
            }
        }

        ai.intentHoldTimer = ai.intentHoldDuration;

        if (!CanAttackChampion(ai, ctx))
        {
            ai.intent = eChampionAIIntent::FarmMinion;
            ai.lastDecisionRoll = 1.f;
            return;
        }

        const u32_t roll = MakeChampionAIRoll(tc, self, ai);
        ai.lastDecisionRoll = static_cast<f32_t>(roll) / 65535.f;
        ai.intent = (ai.lastDecisionRoll < WintersMath::Clamp01(ai.attackChampionChance))
            ? eChampionAIIntent::AttackChampion
            : eChampionAIIntent::FarmMinion;
    }

    bool_t ShouldAttackChampion(
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        SampleLaneCombatIntent(tc, self, ai, ctx);
        return ai.intent == eChampionAIIntent::AttackChampion &&
            CanAttackChampion(ai, ctx);
    }

    bool_t TryEmitAttackChampion(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const EntityID target = ctx.enemyChampion;
        if (target == NULL_ENTITY)
            return false;

        if (TryEmitAttackChampionCombo(world, tc, self, ai, champion, selfPos,
            target, ctx, outCommands))
        {
            return true;
        }

        const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
        if (TryEmitAttackChampionSkill(world, tc, self, ai, champion, selfPos,
            target, profile, ctx, outCommands))
        {
            return true;
        }

        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            target, eChampionAIAction::AttackChampion, "lane-attack-champion-ba", outCommands))
        {
            return true;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            targetPos, eChampionAIAction::AttackChampion,
            "lane-attack-champion-move", outCommands);
    }

    bool_t EmitMoveToTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        eChampionAIAction action,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        return EmitMoveCommand(world, tc, self, ai, champion, selfPos,
            targetPos, action, reason, outCommands);
    }

    bool_t HasActiveRecall(CWorld& world, EntityID self)
    {
        return world.HasComponent<RecallComponent>(self) &&
            world.GetComponent<RecallComponent>(self).bActive;
    }

    bool_t HasReachedGoal(const Vec3& selfPos, const Vec3& goal, f32_t radius)
    {
        return WintersMath::DistanceSqXZ(selfPos, goal) <= radius * radius;
    }

    void ClearChampionAITargets(ChampionAIComponent& ai)
    {
        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        ai.alliedWave = NULL_ENTITY;
        ClearChampionAICombo(ai);
        ai.bStructureWaveTanking = false;
        ai.bInsideEnemyTurretDanger = false;
    }

    bool_t EmitRecall(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::Recalling;
        ai.intent = eChampionAIIntent::Recall;
        ai.lastAction = eChampionAIAction::Recall;
        ClearChampionAITargets(ai);

        if (HasActiveRecall(world, self))
            return false;

        GameCommand recall = MakeAICommand(ai, tc, self, eCommandKind::Recall);
        outCommands.push_back(recall);
        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, selfPos,
            NULL_ENTITY, recall.kind, recall.slot);
        return true;
    }

    bool_t EmitRetreat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::Retreat;
        ai.intent = eChampionAIIntent::Retreat;
        ClearChampionAITargets(ai);
        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.retreatGoal, eChampionAIAction::Retreat, "lane-retreat", outCommands);
    }

    void ExecuteRecalling(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::Recalling;
        ai.intent = eChampionAIIntent::Recall;
        ai.lastAction = eChampionAIAction::Recall;
        ClearChampionAITargets(ai);

        if (HasActiveRecall(world, self))
            return;

        ai.state = eChampionAIState::MoveToOuterTurret;
        ai.intent = eChampionAIIntent::FarmMinion;
        ai.bWaveJoined = false;
        EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.safeAnchor, eChampionAIAction::MoveToSafeAnchor,
            "recall-return-to-lane", outCommands);
    }

    eChampionAIBehaviorStatus TickActiveChampionCombo(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ai.comboTarget != ctx.enemyChampion ||
            ai.comboTarget == NULL_ENTITY ||
            !CanAttackChampion(ai, ctx))
        {
            return eChampionAIBehaviorStatus::Failure;
        }

        const size_t commandCountBefore = outCommands.size();
        if (!TryEmitAttackChampionCombo(world, tc, self, ai, champion, selfPos,
            ctx.enemyChampion, ctx, outCommands))
        {
            return eChampionAIBehaviorStatus::Failure;
        }

        return outCommands.size() > commandCountBefore
            ? eChampionAIBehaviorStatus::Success
            : eChampionAIBehaviorStatus::Running;
    }

    bool_t TryExecuteStructureAttack(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.enemyChampion != NULL_ENTITY ||
            ctx.enemyStructure == NULL_ENTITY ||
            !ctx.bStructureWaveTanking)
        {
            return false;
        }

        SetChampionAIIntent(ai, eChampionAIIntent::SiegeStructure);
        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyStructure, eChampionAIAction::AttackStructure,
            "lane-attack-structure-ba", outCommands))
        {
            return true;
        }

        return EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyStructure, eChampionAIAction::AttackStructure,
            "lane-attack-structure-move", outCommands);
    }

    bool_t TryStartChampionAttack(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        return ShouldAttackChampion(tc, self, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands);
    }

    bool_t TryExecuteMinionFarm(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.enemyMinion == NULL_ENTITY)
            return false;

        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyMinion, eChampionAIAction::AttackMinion,
            "lane-attack-minion-ba", outCommands))
        {
            return true;
        }

        return EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyMinion, eChampionAIAction::AttackMinion,
            "lane-attack-minion-move", outCommands);
    }

    bool_t TryExecuteFollowWave(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.alliedWave != NULL_ENTITY &&
            EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                ctx.alliedWave, eChampionAIAction::FollowWave,
                "lane-follow-wave", outCommands))
        {
            return true;
        }

        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.laneGoal, eChampionAIAction::FollowWave, "lane-goal", outCommands);
    }

    void ExecuteLaneCombat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::LaneCombat;
        ai.lockedChampion = ctx.enemyChampion;
        ai.targetMinion = ctx.enemyMinion;
        ai.targetStructure = ctx.enemyStructure;
        ai.alliedWave = ctx.alliedWave;
        ai.bStructureWaveTanking = ctx.bStructureWaveTanking;
        ai.bInsideEnemyTurretDanger = ctx.bInsideEnemyTurretDanger;

        if (ctx.selfHpRatio <= ai.retreatHpRatio ||
            (ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY) ||
            (ctx.turretDanger > 0.85f && !ctx.bStructureWaveTanking))
        {
            EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
            return;
        }

        const eChampionAIBehaviorStatus comboStatus =
            TickActiveChampionCombo(world, tc, self, ai, champion, selfPos, ctx, outCommands);
        if (comboStatus != eChampionAIBehaviorStatus::Failure)
            return;

        if (TryExecuteStructureAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (TryStartChampionAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        SetChampionAIIntent(ai, eChampionAIIntent::FarmMinion);
        ClearChampionAICombo(ai);

        if (TryExecuteMinionFarm(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        TryExecuteFollowWave(world, tc, self, ai, champion, selfPos, ctx, outCommands);
    }

    void ExecuteMoveToOuterTurret(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.bAlliedWaveNearby)
        {
            ai.bWaveJoined = true;
            ExecuteLaneCombat(world, tc, self, ai, champion, selfPos, ctx, outCommands);
            return;
        }

        if (WintersMath::DistanceSqXZ(selfPos, ai.safeAnchor) <= 1.5f * 1.5f)
        {
            ai.state = eChampionAIState::WaitForWave;
            return;
        }

        ai.state = eChampionAIState::MoveToOuterTurret;
        EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.safeAnchor, eChampionAIAction::MoveToSafeAnchor,
            "move-to-outer-turret", outCommands);
    }

    void ExecuteWaitForWave(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.bAlliedWaveNearby)
        {
            ai.bWaveJoined = true;
            ExecuteLaneCombat(world, tc, self, ai, champion, selfPos, ctx, outCommands);
            return;
        }

        ai.state = eChampionAIState::WaitForWave;
        EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.safeAnchor, eChampionAIAction::MoveToSafeAnchor,
            "wait-for-wave", outCommands);
    }

    bool_t TryConsumeChampionAIDebugOverride(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const eChampionAIAction action = ai.debugForcedAction;
        const u8_t skillSlot = ai.debugForcedSkillSlot;
        const bool_t bForceAction = ai.bDebugForceAction;

        if (!bForceAction && !IsActionAvailable(ai.debugAvailableActionMask, action))
            return false;

        switch (action)
        {
        case eChampionAIAction::MoveToSafeAnchor:
            ai.state = eChampionAIState::MoveToOuterTurret;
            return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
                ai.safeAnchor, eChampionAIAction::MoveToSafeAnchor,
                "debug-move-safe-anchor", outCommands);
        case eChampionAIAction::FollowWave:
            ai.state = eChampionAIState::LaneCombat;
            if (ctx.alliedWave != NULL_ENTITY &&
                EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                    ctx.alliedWave, eChampionAIAction::FollowWave,
                    "debug-follow-wave", outCommands))
            {
                return true;
            }

            return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
                ai.laneGoal, eChampionAIAction::FollowWave,
                "debug-follow-lane-goal", outCommands);
        case eChampionAIAction::AttackMinion:
            ai.state = eChampionAIState::LaneCombat;
            ai.intent = eChampionAIIntent::FarmMinion;
            if (ctx.enemyMinion == NULL_ENTITY)
                return false;
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyMinion, eChampionAIAction::AttackMinion,
                "debug-attack-minion-ba", outCommands))
            {
                return true;
            }

            return EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyMinion, eChampionAIAction::AttackMinion,
                "debug-attack-minion-move", outCommands);
        case eChampionAIAction::AttackChampion:
            ai.state = eChampionAIState::LaneCombat;
            if (ctx.enemyChampion == NULL_ENTITY)
                return false;
            SetChampionAIIntent(ai, eChampionAIIntent::AttackChampion, true);
            ai.lockedChampion = ctx.enemyChampion;
            if (skillSlot > 0u)
            {
                return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                    ctx.enemyChampion, skillSlot, "debug-attack-champion-skill",
                    outCommands);
            }

            return TryEmitAttackChampion(world, tc, self, ai, champion, selfPos,
                ctx, outCommands);
        case eChampionAIAction::AttackStructure:
            ai.state = eChampionAIState::LaneCombat;
            ai.intent = eChampionAIIntent::SiegeStructure;
            if (ctx.enemyStructure == NULL_ENTITY)
                return false;
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyStructure, eChampionAIAction::AttackStructure,
                "debug-attack-structure-ba", outCommands))
            {
                return true;
            }

            return EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyStructure, eChampionAIAction::AttackStructure,
                "debug-attack-structure-move", outCommands);
        case eChampionAIAction::Retreat:
            return EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
        default:
            return false;
        }
    }
}

void CChampionAISystem::Execute(CWorld& world, const TickContext& tc, std::vector<GameCommand>& outCommands)
{
    world.ForEach<ChampionAIComponent, ChampionComponent, TransformComponent>(
        [&](EntityID self, ChampionAIComponent& ai, ChampionComponent& champion, TransformComponent& selfTf)
        {
            if (!IsAliveTarget(world, self))
            {
                ai.state = eChampionAIState::Dead;
                ai.lastAction = eChampionAIAction::Retreat;
                return;
            }

            ai.decisionTimer -= tc.fDt;

            const Vec3 selfPos = selfTf.GetPosition();
            const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
            ai.champion = champion.id;
            ai.team = champion.team;
            ai.championScanRange = profile.championScanRange;
            ai.minionScanRange = profile.minionScanRange;
            ai.structureScanRange = profile.structureScanRange;
            ai.leashRange = profile.leashRange;
            ai.retreatHpRatio = profile.retreatHpRatio;
            ai.reengageHpRatio = profile.reengageHpRatio;

            const ChampionAIContext ctx =
                BuildChampionAIContext(world, self, ai, champion, selfPos);

            ai.debugAvailableActionMask = BuildChampionAIAvailableActionMask(ai, ctx);
            ai.debugAvailableSkillMask =
                BuildChampionAIAvailableSkillMask(world, self, champion.id, profile, ctx);

            if (HasActiveRecall(world, self))
            {
                ExecuteRecalling(world, tc, self, ai, champion, selfPos, outCommands);
                return;
            }

            const bool_t bHasDebugOverride = ai.debugForcedDecisionCount > 0;
            if (ai.decisionTimer > 0.f && !bHasDebugOverride)
                return;

            if (IsChampionAIActionLocked(world, self, champion.id, tc))
                return;

            ai.decisionTimer = ai.decisionInterval;

            if (ai.debugForcedDecisionCount > 0)
            {
                --ai.debugForcedDecisionCount;
                if (TryConsumeChampionAIDebugOverride(
                    world, tc, self, ai, champion, selfPos, ctx, outCommands))
                {
                    return;
                }
            }

            if (ai.state == eChampionAIState::Retreat)
            {
                if (HasReachedGoal(selfPos, ai.retreatGoal, 1.5f))
                {
                    EmitRecall(world, tc, self, ai, champion.id, selfPos,
                        "retreat-arrived-recall", outCommands);
                    return;
                }

                if (ctx.selfHpRatio < ai.reengageHpRatio || ctx.bInsideEnemyTurretDanger)
                {
                    EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
                    return;
                }
            }

            switch (ai.state)
            {
            case eChampionAIState::MoveToOuterTurret:
                ExecuteMoveToOuterTurret(world, tc, self, ai, champion, selfPos, ctx, outCommands);
                break;
            case eChampionAIState::WaitForWave:
                ExecuteWaitForWave(world, tc, self, ai, champion, selfPos, ctx, outCommands);
                break;
            case eChampionAIState::Recalling:
                ExecuteRecalling(world, tc, self, ai, champion, selfPos, outCommands);
                break;
            case eChampionAIState::LaneCombat:
            case eChampionAIState::Retreat:
            default:
                ExecuteLaneCombat(world, tc, self, ai, champion, selfPos, ctx, outCommands);
                break;
            }
        });
}
