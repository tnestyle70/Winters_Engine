#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/WardDefinitions.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Core/World/World.h"
#include "ECS/Components/VisionComponents.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
    struct ChampionAIContext
    {
        EntityID enemyChampion = NULL_ENTITY;
        EntityID lowHpEnemyChampion = NULL_ENTITY;
        EntityID diveTarget = NULL_ENTITY;
        EntityID enemyMinion = NULL_ENTITY;
        EntityID enemyStructure = NULL_ENTITY;
        EntityID alliedWave = NULL_ENTITY;
        EntityID airborneChampion = NULL_ENTITY;
        EntityID yasuoDashMinion = NULL_ENTITY;

        f32_t selfHpRatio = 1.f;
        f32_t enemyHpRatio = 1.f;
        f32_t selfGold = 0.f;
        f32_t enemyGold = 0.f;
        u8_t  selfLevel = 1;
        u8_t  enemyLevel = 1;
        f32_t enemyDistance = 999.f;
        f32_t lowHpEnemyRatio = 1.f;
        f32_t lowHpEnemyDistance = 999.f;
        f32_t attackRange = 1.5f;
        f32_t waveDistance = 999.f;
        f32_t turretDanger = 0.f;
        f32_t airborneDistance = 999.f;
        f32_t yasuoDashMinionDistance = 999.f;

        u8_t yasuoQStage = 1u;
        bool_t bYasuoEActive = false;

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

    f32_t ResolveAttackRange(
        CWorld& world,
        EntityID self,
        const TickContext& tc,
        eChampion champion)
    {
        if (world.HasComponent<StatComponent>(self))
        {
            const f32_t range = world.GetComponent<StatComponent>(self).attackRange;
            if (range > 0.f)
                return range;
        }

        return GameplayDefinitionQuery::ResolveAttackRange(
            world,
            self,
            tc,
            champion);
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

    bool_t TryFindFlashSlot(CWorld& world, EntityID self, u8_t& outSlot)
    {
        outSlot = 0u;
        if (!world.HasComponent<ChampionScoreComponent>(self))
            return true;

        const auto& score = world.GetComponent<ChampionScoreComponent>(self);
        for (u8_t i = 0; i < ChampionScoreComponent::kSummonerSpellSlotCount; ++i)
        {
            if (score.iSummonerSpellIds[i] == ChampionScoreComponent::kSummonerSpellFlash)
            {
                outSlot = i;
                return true;
            }
        }

        return false;
    }

    bool_t IsFlashReady(CWorld& world, EntityID self)
    {
        u8_t slot = 0u;
        if (!TryFindFlashSlot(world, self, slot))
            return false;
        if (!world.HasComponent<SummonerSpellStateComponent>(self))
            return true;

        return world.GetComponent<SummonerSpellStateComponent>(self)
            .cooldownRemaining[slot] <= 0.f;
    }

    bool_t IsWardBehindTargetComboStep(const ChampionAIComboStep& step)
    {
        return step.itemId == kTrinketWardItemId &&
            step.targetMode == static_cast<u8_t>(eChampionAIComboTargetMode::WardBehindTarget);
    }

    bool_t IsLastOwnWardComboStep(const ChampionAIComboStep& step)
    {
        return step.targetMode == static_cast<u8_t>(eChampionAIComboTargetMode::LastOwnWard);
    }

    bool_t IsSylasHijackComboStep(const ChampionAIComboStep& step)
    {
        return step.targetMode == static_cast<u8_t>(eChampionAIComboTargetMode::SylasHijackTarget);
    }

    bool_t IsSylasStolenUltimateComboStep(const ChampionAIComboStep& step)
    {
        return step.targetMode ==
            static_cast<u8_t>(eChampionAIComboTargetMode::SylasStolenUltimateTarget);
    }

    bool_t HasActiveSylasStolenUltimate(CWorld& world, EntityID self)
    {
        if (!world.HasComponent<SpellbookOverrideComponent>(self))
            return false;

        const auto& spellbook = world.GetComponent<SpellbookOverrideComponent>(self);
        return spellbook.bActive &&
            spellbook.localSlot == static_cast<u8_t>(eSkillSlot::R) &&
            spellbook.fRemainingSec > 0.f;
    }

    Vec3 ResolveWardBehindTargetPosition(const Vec3& selfPos, const Vec3& targetPos)
    {
        Vec3 direction = WintersMath::DirectionXZ(selfPos, targetPos, Vec3{});
        if (direction.x == 0.f && direction.z == 0.f)
            direction = Vec3{ 0.f, 0.f, 1.f };

        Vec3 desired{
            targetPos.x + direction.x * 1.25f,
            selfPos.y,
            targetPos.z + direction.z * 1.25f
        };
        if (WintersMath::DistanceSqXZ(selfPos, desired) > kWardPlacementRange * kWardPlacementRange)
        {
            desired = Vec3{
                selfPos.x + direction.x * kWardPlacementRange,
                selfPos.y,
                selfPos.z + direction.z * kWardPlacementRange
            };
        }

        return desired;
    }

    bool_t TryFindOwnedWardNear(
        CWorld& world,
        EntityID owner,
        const Vec3& selfPos,
        const Vec3& desiredPos,
        f32_t maxRange,
        EntityID& outWard)
    {
        outWard = NULL_ENTITY;
        f32_t bestScore = (std::numeric_limits<f32_t>::max)();
        const f32_t maxRangeSq = maxRange > 0.f ? maxRange * maxRange : 49.f;

        world.ForEach<LeeSinWardOwnerComponent, VisionSensorComponent, TransformComponent>(
            [&](EntityID entity, LeeSinWardOwnerComponent& wardOwner, VisionSensorComponent&, TransformComponent& transform)
            {
                if (wardOwner.owner != owner)
                    return;

                const Vec3 wardPos = transform.GetPosition();
                if (WintersMath::DistanceSqXZ(selfPos, wardPos) > maxRangeSq)
                    return;

                const f32_t score = WintersMath::DistanceSqXZ(desiredPos, wardPos);
                if (score < bestScore)
                {
                    bestScore = score;
                    outWard = entity;
                }
            });

        return outWard != NULL_ENTITY;
    }

    bool_t IsComboStepUnlearned(
        CWorld& world,
        EntityID self,
        const ChampionAIComboStep& step)
    {
        if (step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
            return false;
        if (step.slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
            return true;
        if (!world.HasComponent<SkillRankComponent>(self))
            return true;

        return world.GetComponent<SkillRankComponent>(self).ranks[step.slot] == 0u;
    }

    bool_t IsStage2ComboStepReady(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        eChampion champion,
        const ChampionAIComboStep& step,
        const ChampionAIContext& ctx)
    {
        if (step.itemId != 2u ||
            step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
            step.slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
        {
            return false;
        }
        if (IsComboStepUnlearned(world, self, step))
            return false;
        if (!world.HasComponent<SkillStateComponent>(self))
            return false;

        const auto& skillSlot = world.GetComponent<SkillStateComponent>(self).slots[step.slot];
        if (skillSlot.currentStage != 1u || skillSlot.stageWindow <= 0.f)
            return false;
        if (!GameplayDefinitionQuery::IsSkillTwoStage(world, self, tc, champion, step.slot))
            return false;

        if (champion == eChampion::LEESIN &&
            step.slot == static_cast<u8_t>(eSkillSlot::Q))
        {
            if (ctx.enemyChampion == NULL_ENTITY ||
                !world.HasComponent<LeeSinQMarkComponent>(ctx.enemyChampion))
            {
                return false;
            }

            const auto& mark = world.GetComponent<LeeSinQMarkComponent>(ctx.enemyChampion);
            if (mark.sourceEntity != self || mark.fRemainingSec <= 0.f)
                return false;
        }

        return true;
    }

    bool_t CanUseComboStep(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        eChampion champion,
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

        f32_t maxRange = step.maxRange;
        if (maxRange <= 0.f)
        {
            maxRange = step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack)
                ? ctx.attackRange
                : GameplayDefinitionQuery::ResolveSkillRange(
                    world,
                    self,
                    tc,
                    champion,
                    step.slot);
        }
        if (maxRange > 0.f && ctx.enemyDistance > maxRange)
            return false;

        if (step.itemId == 2u)
            return IsStage2ComboStepReady(world, tc, self, champion, step, ctx);

        if (IsSylasHijackComboStep(step))
        {
            return champion == eChampion::SYLAS &&
                step.slot == static_cast<u8_t>(eSkillSlot::R) &&
                !HasActiveSylasStolenUltimate(world, self) &&
                IsSkillReady(world, self, step.slot) &&
                SylasGameSim::CanHijackUltimate(
                    world,
                    tc,
                    self,
                    ctx.enemyChampion);
        }

        if (IsSylasStolenUltimateComboStep(step))
        {
            return champion == eChampion::SYLAS &&
                step.slot == static_cast<u8_t>(eSkillSlot::R) &&
                HasActiveSylasStolenUltimate(world, self) &&
                IsSkillReady(world, self, step.slot);
        }

        if (IsLastOwnWardComboStep(step))
        {
            Vec3 selfPos{};
            Vec3 enemyPos{};
            EntityID ward = NULL_ENTITY;
            if (!TryGetPosition(world, self, selfPos) ||
                !TryGetPosition(world, ctx.enemyChampion, enemyPos))
            {
                return false;
            }

            const f32_t wardRange = GameplayDefinitionQuery::ResolveSkillRange(
                world,
                self,
                tc,
                champion,
                step.slot);
            return IsSkillReady(world, self, step.slot) &&
                TryFindOwnedWardNear(
                    world,
                    self,
                    selfPos,
                    ResolveWardBehindTargetPosition(selfPos, enemyPos),
                    wardRange > 0.f ? wardRange : 7.f,
                    ward);
        }

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

    void SetChampionAIState(
        ChampionAIComponent& ai,
        eChampionAIState state,
        eChampionAIDecisionBlockReason reason = eChampionAIDecisionBlockReason::None)
    {
        if (ai.state != state)
            ai.debugLastBlockReason = reason;
        ai.state = state;
    }

    void SetChampionAIBlockReason(
        ChampionAIComponent& ai,
        eChampionAIDecisionBlockReason reason)
    {
        ai.debugLastBlockReason = reason;
    }

    void PushChampionAIDecisionTrace(
        ChampionAIComponent& ai,
        const TickContext& tc,
        EntityID target)
    {
        ChampionAIDecisionTraceEntry entry{};
        entry.tick = tc.tickIndex;
        entry.state = ai.state;
        entry.intent = ai.intent;
        entry.action = ai.lastAction;
        entry.divePhase = ai.divePhase;
        entry.blockReason = ai.debugLastBlockReason;
        entry.commandKind = ai.debugLastCommandKind;
        entry.commandSlot = ai.debugLastCommandSlot;
        entry.target = target;
        entry.commandPos = ai.debugLastCommandPos;
        entry.championScore = ai.fChampionDecisionScore;
        entry.farmScore = ai.fFarmDecisionScore;
        entry.structureScore = ai.fStructureDecisionScore;
        entry.selfHpRatio = ai.fDecisionSelfHpRatio;
        entry.enemyHpRatio = ai.fDecisionEnemyHpRatio;
        entry.enemyDistance = ai.fDecisionEnemyDistance;
        entry.turretDanger = ai.fDecisionTurretDanger;

        ai.debugDecisionTrace[ai.debugDecisionTraceHead] = entry;
        ai.debugDecisionTraceHead =
            static_cast<u8_t>((ai.debugDecisionTraceHead + 1u) % kChampionAIDebugTraceCapacity);
        if (ai.debugDecisionTraceCount < kChampionAIDebugTraceCapacity)
            ++ai.debugDecisionTraceCount;
    }

    void ClearChampionAICombo(ChampionAIComponent& ai)
    {
        ai.comboTarget = NULL_ENTITY;
        ai.comboStep = 0u;
    }

    bool_t ShouldContinueBasicAttackAfterCombo(
        const ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        if (ctx.enemyChampion == NULL_ENTITY)
            return false;

        return ctx.selfHpRatio > ai.fPostComboBASelfHpMinRatio &&
            ctx.enemyHpRatio + ai.fPostComboBAEnemyHpMargin < ctx.selfHpRatio;
    }

    void CompleteChampionAICombo(ChampionAIComponent& ai, const ChampionAIContext& ctx)
    {
        ClearChampionAICombo(ai);
        ai.bPostComboBAAllowed = ShouldContinueBasicAttackAfterCombo(ai, ctx);
        ai.fPostComboBATimer = ai.fPostComboBAWindow;
        SetChampionAIIntent(ai,
            ai.bPostComboBAAllowed
                ? eChampionAIIntent::AttackChampion
                : eChampionAIIntent::FarmMinion,
            true);
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

        if (!world.HasComponent<ActionStateComponent>(self))
            return false;

        const auto& action = world.GetComponent<ActionStateComponent>(self);
        const auto actionId = static_cast<eActionStateId>(action.actionId);
        if (!IsReplicatedGameplayAction(actionId))
            return false;

        if (tc.tickIndex < action.startTick)
            return false;

        const u8_t slot = SkillSlotFromActionId(actionId);
        const u8_t stage = action.stage == 0u ? 1u : action.stage;
        const u64_t lockTicks = GameplayDefinitionQuery::ResolveSkillActionLockTicks(
            world,
            self,
            tc,
            champion,
            slot,
            stage);
        return (tc.tickIndex - action.startTick) < lockTicks;
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

    void RecordChampionAICommandDebug(
        ChampionAIComponent& ai,
        EntityID target,
        eCommandKind kind,
        u8_t slot,
        const Vec3& commandPos)
    {
        ai.debugLastCommandKind = static_cast<u8_t>(kind);
        ai.debugLastCommandSlot = slot;
        ai.debugLastCommandTarget = target;
        ai.debugLastCommandPos = commandPos;
        ai.debugLastBlockReason = eChampionAIDecisionBlockReason::None;
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
        case eCommandKind::Flash:
            return "Flash";
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

        char msg[640]{};
        sprintf_s(msg,
            "[ChampionAI] tick=%llu entity=%u champ=%u team=%u lane=%u state=%u intent=%u action=%u scoreC=%.2f scoreF=%.2f scoreS=%.2f selfHp=%.2f enemyHp=%.2f dist=%.2f range=%.2f turret=%.2f canChamp=%u postBA=%u cmd=%s slot=%u reason=%s target=%u pos=(%.2f,%.2f,%.2f) cmdPos=(%.2f,%.2f,%.2f)\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(self),
            static_cast<u32_t>(champion),
            static_cast<u32_t>(ai.team),
            static_cast<u32_t>(ai.lane),
            static_cast<u32_t>(ai.state),
            static_cast<u32_t>(ai.intent),
            static_cast<u32_t>(ai.lastAction),
            static_cast<double>(ai.fChampionDecisionScore),
            static_cast<double>(ai.fFarmDecisionScore),
            static_cast<double>(ai.fStructureDecisionScore),
            static_cast<double>(ai.fDecisionSelfHpRatio),
            static_cast<double>(ai.fDecisionEnemyHpRatio),
            static_cast<double>(ai.fDecisionEnemyDistance),
            static_cast<double>(ai.fDecisionAttackRange),
            static_cast<double>(ai.fDecisionTurretDanger),
            static_cast<u32_t>(ai.bCanAttackChampion ? 1u : 0u),
            static_cast<u32_t>(ai.bPostComboBAAllowed ? 1u : 0u),
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
        RecordChampionAICommandDebug(ai, NULL_ENTITY, move.kind, move.slot, goal);
        PushChampionAIDecisionTrace(ai, tc, NULL_ENTITY);
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
        if (!IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::BasicAttack)))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::SkillCooldown);
            return false;
        }
        if (target == NULL_ENTITY)
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
            return false;
        }
        if (!IsAliveTarget(world, target))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetDead);
            return false;
        }
        if (!GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetUntargetable);
            return false;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
            return false;
        }

        const f32_t attackRange = ResolveAttackRange(world, self, tc, champion);
        if (attackRange > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > attackRange * attackRange)
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetOutOfRange);
            return false;
        }

        ai.lastAction = action;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::BasicAttack);
        cmd.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        cmd.targetEntity = target;
        outCommands.push_back(cmd);
        RecordChampionAICommandDebug(ai, target, cmd.kind, cmd.slot, targetPos);
        PushChampionAIDecisionTrace(ai, tc, target);

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
        u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity),
        eChampionAIAction action = eChampionAIAction::AttackChampion)
    {
        const bool_t bStage2 = itemId == 2u;
        if (!bStage2 && !IsSkillReady(world, self, slot))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::SkillCooldown);
            return false;
        }
        if (bStage2)
        {
            if (!world.HasComponent<SkillStateComponent>(self))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::SkillCooldown);
                return false;
            }

            const auto& skillSlot = world.GetComponent<SkillStateComponent>(self).slots[slot];
            if (skillSlot.currentStage != 1u ||
                skillSlot.stageWindow <= 0.f ||
                !GameplayDefinitionQuery::IsSkillTwoStage(world, self, tc, champion, slot))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::SkillCooldown);
                return false;
            }
        }
        if (target == NULL_ENTITY)
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
            return false;
        }
        if (!IsAliveTarget(world, target))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetDead);
            return false;
        }
        if (!GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetUntargetable);
            return false;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
            return false;
        }

        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            self,
            tc,
            champion,
            slot);
        if (range > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > range * range)
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetOutOfRange);
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

        ai.lastAction = action;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
        cmd.slot = slot;
        cmd.itemId = itemId;
        cmd.targetEntity = target;
        cmd.groundPos = commandPos;
        cmd.direction = direction;
        outCommands.push_back(cmd);
        RecordChampionAICommandDebug(ai, target, cmd.kind, cmd.slot, commandPos);
        PushChampionAIDecisionTrace(ai, tc, target);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, commandPos,
            target, cmd.kind, cmd.slot);
        return true;
    }

    bool_t EmitFlashCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        const Vec3& desiredGoal,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        if (!IsFlashReady(world, self))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::FlashNotReady);
            return false;
        }

        const f32_t flashRange =
            GameplayDefinitionQuery::ResolveSummonerSpellRange(
                tc,
                ChampionScoreComponent::kSummonerSpellFlash);
        if (flashRange <= 0.f)
            return false;

        const Vec3 dir = WintersMath::DirectionXZ(selfPos, desiredGoal);
        if (dir.x == 0.f && dir.z == 0.f)
            return false;

        ai.lastAction = eChampionAIAction::UseFlashEscape;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::Flash);
        cmd.groundPos = desiredGoal;
        cmd.direction = dir;
        outCommands.push_back(cmd);
        RecordChampionAICommandDebug(ai, NULL_ENTITY, cmd.kind, cmd.slot, desiredGoal);
        PushChampionAIDecisionTrace(ai, tc, NULL_ENTITY);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, desiredGoal,
            NULL_ENTITY, cmd.kind, cmd.slot);
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

    EntityID FindLowHpEnemyChampion(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        const Vec3& pos,
        f32_t range,
        f32_t hpThreshold,
        f32_t& outHpRatio,
        f32_t& outDistance)
    {
        EntityID best = NULL_ENTITY;
        f32_t bestScore = -1.f;
        f32_t bestDistSq = 999.f * 999.f;
        outHpRatio = 1.f;
        outDistance = 999.f;

        const f32_t rangeSq = range * range;
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
                if (distSq > rangeSq)
                    return;

                const f32_t hpRatio = HealthRatio(world, e);
                if (hpRatio > hpThreshold)
                    return;

                const f32_t score = (hpThreshold - hpRatio) * 100.f -
                    std::sqrt(std::max(0.f, distSq));
                if (score > bestScore)
                {
                    bestScore = score;
                    bestDistSq = distSq;
                    outHpRatio = hpRatio;
                    best = e;
                }
            });

        outDistance = (best != NULL_ENTITY) ? std::sqrt(std::max(0.f, bestDistSq)) : 999.f;
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

    EntityID FindYasuoDashMinionTowardChampion(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t lane,
        const Vec3& selfPos,
        EntityID targetChampion,
        f32_t eRange,
        f32_t scanRange,
        f32_t& outDistance)
    {
        outDistance = 999.f;

        Vec3 championPos{};
        if (!TryGetPosition(world, targetChampion, championPos))
            return NULL_ENTITY;

        constexpr f32_t kYasuoEDashThroughDistance = 0.75f;
        constexpr f32_t kYasuoEDashMaxDistance = 5.5f;

        const f32_t currentDistSq = WintersMath::DistanceSqXZ(selfPos, championPos);
        const f32_t eRangeSq = eRange * eRange;
        const f32_t scanRangeSq = scanRange * scanRange;
        EntityID best = NULL_ENTITY;
        f32_t bestScore = 0.f;
        f32_t bestDistSq = 999.f * 999.f;

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

                const Vec3 minionPos = transform.GetPosition();
                const f32_t minionDistSq = WintersMath::DistanceSqXZ(selfPos, minionPos);
                if (minionDistSq > eRangeSq || minionDistSq > scanRangeSq)
                    return;

                const Vec3 dir = WintersMath::DirectionXZ(selfPos, minionPos);
                const f32_t minionDist = std::sqrt(std::max(0.f, minionDistSq));
                const f32_t dashDistance =
                    std::min(minionDist + kYasuoEDashThroughDistance, kYasuoEDashMaxDistance);
                const Vec3 dashEnd{
                    selfPos.x + dir.x * dashDistance,
                    selfPos.y,
                    selfPos.z + dir.z * dashDistance
                };

                const f32_t afterDistSq = WintersMath::DistanceSqXZ(dashEnd, championPos);
                if (afterDistSq + 0.01f >= currentDistSq)
                    return;

                const f32_t score = currentDistSq - afterDistSq;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestDistSq = minionDistSq;
                    best = e;
                }
            });

        outDistance = (best != NULL_ENTITY) ? std::sqrt(std::max(0.f, bestDistSq)) : 999.f;
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
        const TickContext& tc,
        const Vec3& selfPos)
    {
        ChampionAIContext ctx{};
        ctx.selfHpRatio = HealthRatio(world, self);
        ctx.attackRange = ResolveAttackRange(world, self, tc, champion.id);

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

        if (world.HasComponent<GoldComponent>(self))
            ctx.selfGold = static_cast<f32_t>(world.GetComponent<GoldComponent>(self).amount);
        if (world.HasComponent<StatComponent>(self))
            ctx.selfLevel = world.GetComponent<StatComponent>(self).level;
        if (targetChampion != NULL_ENTITY)
        {
            if (world.HasComponent<GoldComponent>(targetChampion))
                ctx.enemyGold =
                    static_cast<f32_t>(world.GetComponent<GoldComponent>(targetChampion).amount);
            if (world.HasComponent<StatComponent>(targetChampion))
                ctx.enemyLevel = world.GetComponent<StatComponent>(targetChampion).level;
        }

        ctx.lowHpEnemyChampion = FindLowHpEnemyChampion(
            world,
            self,
            champion.team,
            selfPos,
            ai.fDiveScanRange,
            ai.fLowHpExecuteThreshold,
            ctx.lowHpEnemyRatio,
            ctx.lowHpEnemyDistance);
        ctx.diveTarget = ctx.lowHpEnemyChampion;

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

        if (champion.id == eChampion::YASUO)
        {
            ctx.yasuoQStage = YasuoGameSim::ResolveQVariantStage(world, self);
            if (world.HasComponent<YasuoStateComponent>(self))
            {
                const auto& yasuo = world.GetComponent<YasuoStateComponent>(self);
                ctx.bYasuoEActive = yasuo.bEActive;
            }

            const f32_t rRange = GameplayDefinitionQuery::ResolveSkillRange(
                world,
                self,
                tc,
                eChampion::YASUO,
                static_cast<u8_t>(eSkillSlot::R));
            ctx.airborneChampion = YasuoGameSim::FindAirborneTarget(
                world,
                self,
                champion.team,
                rRange > 0.f ? rRange : 14.f);
            if (ctx.airborneChampion != NULL_ENTITY)
            {
                Vec3 airbornePos{};
                if (TryGetPosition(world, ctx.airborneChampion, airbornePos))
                    ctx.airborneDistance =
                        std::sqrt(std::max(0.f, WintersMath::DistanceSqXZ(selfPos, airbornePos)));
            }

            if (ctx.enemyChampion != NULL_ENTITY)
            {
                const f32_t eRange = GameplayDefinitionQuery::ResolveSkillRange(
                    world,
                    self,
                    tc,
                    eChampion::YASUO,
                    static_cast<u8_t>(eSkillSlot::E));
                ctx.yasuoDashMinion = FindYasuoDashMinionTowardChampion(
                    world,
                    self,
                    champion.team,
                    ai.lane,
                    selfPos,
                    ctx.enemyChampion,
                    eRange,
                    ai.minionScanRange,
                    ctx.yasuoDashMinionDistance);
            }
        }

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
        if (IsWardBehindTargetComboStep(step))
        {
            Vec3 targetPos{};
            if (!TryGetPosition(world, target, targetPos))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
                return false;
            }

            const Vec3 wardPos = ResolveWardBehindTargetPosition(selfPos, targetPos);
            GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::UseItem);
            cmd.slot = step.slot;
            cmd.itemId = step.itemId;
            cmd.targetEntity = NULL_ENTITY;
            cmd.groundPos = wardPos;
            cmd.direction = WintersMath::DirectionXZ(selfPos, wardPos, Vec3{});
            outCommands.push_back(cmd);
            RecordChampionAICommandDebug(ai, target, cmd.kind, cmd.slot, wardPos);
            PushChampionAIDecisionTrace(ai, tc, target);
            LogChampionAICommand("combo-attack-champion-ward", tc, self, ai, champion.id, selfPos,
                wardPos, target, cmd.kind, cmd.slot);
            return true;
        }

        if (step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "combo-attack-champion-ba", outCommands);
        }

        if (IsLastOwnWardComboStep(step))
        {
            Vec3 targetPos{};
            if (!TryGetPosition(world, target, targetPos))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
                return false;
            }

            EntityID ward = NULL_ENTITY;
            const f32_t wardRange = GameplayDefinitionQuery::ResolveSkillRange(
                world,
                self,
                tc,
                champion.id,
                step.slot);
            if (!TryFindOwnedWardNear(
                world,
                self,
                selfPos,
                ResolveWardBehindTargetPosition(selfPos, targetPos),
                wardRange > 0.f ? wardRange : 7.f,
                ward))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
                return false;
            }

            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, ward,
                step.slot, "combo-attack-champion-ward-hop", outCommands,
                step.itemId, static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity));
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

        const u8_t stepCount = std::min(combo.stepCount, static_cast<u8_t>(10u));
        const u8_t index = static_cast<u8_t>(ai.comboStep % stepCount);
        const ChampionAIComboStep& step = combo.steps[index];
        if (IsComboStepUnlearned(world, self, step))
        {
            const u8_t nextStep = static_cast<u8_t>((index + 1u) % stepCount);
            ai.comboStep = nextStep;
            if (nextStep == 0u)
                CompleteChampionAICombo(ai, ctx);
            return true;
        }

        if (!CanUseComboStep(world, tc, self, champion.id, step, ctx))
        {
            if (bWasActive &&
                ctx.enemyDistance > 0.f)
            {
                f32_t maxRange = step.maxRange;
                if (maxRange <= 0.f)
                {
                    maxRange = step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack)
                        ? ctx.attackRange
                        : GameplayDefinitionQuery::ResolveSkillRange(
                            world,
                            self,
                            tc,
                            champion.id,
                            step.slot);
                }
                if (maxRange <= 0.f || ctx.enemyDistance <= maxRange)
                {
                    // 사거리 안인데 조건 불충족(쿨다운/스테이지/강탈/override 등):
                    // 정지 대신 다음 step으로 진행해 콤보가 교착되지 않게 한다.
                    const u8_t nextComboStep =
                        static_cast<u8_t>((index + 1u) % stepCount);
                    ai.comboStep = nextComboStep;
                    if (nextComboStep == 0u)
                        CompleteChampionAICombo(ai, ctx);
                    return bWasActive;
                }

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
                CompleteChampionAICombo(ai, ctx);
            return true;
        }

        if (!bWasActive)
            ClearChampionAICombo(ai);
        return bWasActive;
    }

    bool_t CanAttackChampion(const ChampionAIComponent& ai, const ChampionAIContext& ctx)
    {
        if (ctx.enemyChampion == NULL_ENTITY)
            return false;
        if (ctx.selfHpRatio <= ai.retreatHpRatio)
            return false;
        if (ctx.bInsideEnemyTurretDanger)
            return false;
        if (ctx.turretDanger > ai.fTurretDangerThreshold && !ctx.bStructureWaveTanking)
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
            return kChampionAIActionBitAttackUnit;
        case eChampionAIAction::AttackChampion:
            return kChampionAIActionBitAttackChampion;
        case eChampionAIAction::AttackStructure:
            return kChampionAIActionBitAttackStructure;
        case eChampionAIAction::UseFlashEscape:
            return kChampionAIActionBitUseFlashEscape;
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

    u32_t BuildChampionAIAvailableActionMask(
        CWorld& world,
        EntityID self,
        const ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        u32_t mask =
            kChampionAIActionBitMoveToSafeAnchor |
            kChampionAIActionBitFollowWave |
            kChampionAIActionBitRetreat;

        if (ctx.enemyMinion != NULL_ENTITY)
            mask |= kChampionAIActionBitAttackUnit;
        if (CanAttackChampion(ai, ctx))
            mask |= kChampionAIActionBitAttackChampion;
        if (ctx.enemyStructure != NULL_ENTITY &&
            ctx.alliedWave != NULL_ENTITY &&
            ctx.bStructureWaveTanking)
            mask |= kChampionAIActionBitAttackStructure;
        if (ctx.lowHpEnemyChampion != NULL_ENTITY &&
            IsFlashReady(world, self))
            mask |= kChampionAIActionBitUseFlashEscape;

        return mask;
    }

    u32_t BuildChampionAIAvailableSkillMask(
        CWorld& world,
        EntityID self,
        eChampion champion,
        const TickContext& tc,
        const ChampionAIProfile& profile,
        const ChampionAIContext& ctx)
    {
        u32_t mask = 0u;
        if (champion == eChampion::YASUO &&
            ctx.airborneChampion != NULL_ENTITY &&
            IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::R)))
        {
            mask |= 1u << (static_cast<u8_t>(eSkillSlot::R) - 1u);
        }

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

            const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
                world,
                self,
                tc,
                champion,
                rule.slot);
            if (range > 0.f && ctx.enemyDistance > range)
                continue;

            mask |= 1u << (rule.slot - 1u);
        }

        return mask;
    }

    f32_t ClampChampionAITuningParam(const ChampionAITuningParam& param)
    {
        return std::min(std::max(param.fCurrent, param.fMin), param.fMax);
    }

    f32_t ResolveChampionAITuningParam(
        const ChampionAITuningParam& param,
        f32_t fallback,
        bool_t bOverrideProfile)
    {
        if (!bOverrideProfile && !param.bOverride)
            return fallback;

        return ClampChampionAITuningParam(param);
    }

    void ApplyChampionAIProfileAndTuning(
        ChampionAIComponent& ai,
        const ChampionAIProfile& profile)
    {
        const bool_t bOverrideProfile = ai.tuning.bOverrideProfile;
        ai.championScanRange = ResolveChampionAITuningParam(
            ai.tuning.championScanRange, profile.championScanRange, bOverrideProfile);
        ai.minionScanRange = ResolveChampionAITuningParam(
            ai.tuning.minionScanRange, profile.minionScanRange, bOverrideProfile);
        ai.structureScanRange = ResolveChampionAITuningParam(
            ai.tuning.structureScanRange, profile.structureScanRange, bOverrideProfile);
        ai.leashRange = ResolveChampionAITuningParam(
            ai.tuning.leashRange, profile.leashRange, bOverrideProfile);

        ai.retreatHpRatio = ResolveChampionAITuningParam(
            ai.tuning.retreatHpRatio, 0.10f, bOverrideProfile);
        ai.reengageHpRatio = ResolveChampionAITuningParam(
            ai.tuning.reengageHpRatio, 0.25f, bOverrideProfile);
        ai.fChampionScoreMargin = ResolveChampionAITuningParam(
            ai.tuning.championScoreMargin, 0.10f, bOverrideProfile);
        ai.fTurretDangerThreshold = ResolveChampionAITuningParam(
            ai.tuning.turretDangerThreshold, 0.85f, bOverrideProfile);
        ai.fPostComboBASelfHpMinRatio = ResolveChampionAITuningParam(
            ai.tuning.postComboBASelfHpMinRatio, ai.retreatHpRatio, bOverrideProfile);
        ai.fPostComboBAEnemyHpMargin = ResolveChampionAITuningParam(
            ai.tuning.postComboBAEnemyHpMargin, 0.f, bOverrideProfile);
        ai.fPostComboBAWindow = ResolveChampionAITuningParam(
            ai.tuning.postComboBAWindow, 0.80f, bOverrideProfile);
        ai.fLowHpExecuteThreshold = ResolveChampionAITuningParam(
            ai.tuning.lowHpExecuteThreshold, 0.10f, bOverrideProfile);
        ai.fDiveScanRange = ResolveChampionAITuningParam(
            ai.tuning.diveScanRange, 11.f, bOverrideProfile);
        ai.fDiveExtraBAWindow = ResolveChampionAITuningParam(
            ai.tuning.diveExtraBAWindow, 1.80f, bOverrideProfile);
    }

    void UpdateChampionAIDecisionEvidence(
        ChampionAIComponent& ai,
        const TickContext& tc,
        const ChampionAIContext& ctx)
    {
        ai.fDecisionSelfHpRatio = ctx.selfHpRatio;
        ai.fDecisionEnemyHpRatio = ctx.enemyHpRatio;
        ai.fDecisionEnemyDistance = ctx.enemyDistance;
        ai.fDecisionAttackRange = ctx.attackRange;
        ai.fDecisionTurretDanger = ctx.turretDanger;
        ai.lowHpEnemyChampion = ctx.lowHpEnemyChampion;
        ai.fDecisionLowHpEnemyRatio = ctx.lowHpEnemyRatio;
        ai.fDecisionLowHpEnemyDistance = ctx.lowHpEnemyDistance;
        ai.fDecisionChampionScanRange = ai.championScanRange;
        ai.fDecisionDiveScanRange = ai.fDiveScanRange;
        ai.fDecisionFlashRange =
            GameplayDefinitionQuery::ResolveSummonerSpellRange(
                tc,
                ChampionScoreComponent::kSummonerSpellFlash);
        if (ctx.enemyChampion == NULL_ENTITY && ctx.lowHpEnemyChampion == NULL_ENTITY)
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
        ai.bCanAttackChampion = CanAttackChampion(ai, ctx);

        ChampionAIValuation::ValueInput vin{};
        vin.selfHpRatio = ctx.selfHpRatio;
        vin.enemyHpRatio = ctx.enemyHpRatio;
        vin.selfGold = ctx.selfGold;
        vin.enemyGold = ctx.enemyGold;
        vin.selfLevel = ctx.selfLevel;
        vin.enemyLevel = ctx.enemyLevel;
        vin.enemyDistance = ctx.enemyDistance;
        vin.attackRange = ctx.attackRange;
        vin.turretDanger = ctx.turretDanger;
        vin.retreatHpRatio = ai.retreatHpRatio;
        vin.bAlliedWaveNearby = ctx.bAlliedWaveNearby;
        vin.bEnemyMinionInRange = (ctx.enemyMinion != NULL_ENTITY);
        vin.bStructureExposed =
            (ctx.enemyChampion == NULL_ENTITY &&
                ctx.enemyStructure != NULL_ENTITY &&
                ctx.alliedWave != NULL_ENTITY &&
                ctx.bStructureWaveTanking);

        ai.fFarmDecisionScore = ChampionAIValuation::MinionFarmValue(vin);
        ai.fStructureDecisionScore = ChampionAIValuation::StructureValue(vin);

        if (!ai.bCanAttackChampion)
        {
            ai.fChampionDecisionScore = 0.f;
            return;
        }

        f32_t championScore = ChampionAIValuation::ChampionFightValue(vin);

        if (ai.fPostComboBATimer > 0.f)
        {
            ai.bPostComboBAAllowed = ShouldContinueBasicAttackAfterCombo(ai, ctx);
            if (ai.bPostComboBAAllowed)
                championScore = 1.f;
        }

        ai.fChampionDecisionScore = WintersMath::Clamp01(championScore);
    }

    void SampleLaneCombatIntent(
        const TickContext& tc,
        ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        // 결정 입력만 평탄화해서 brain에 위임한다.
        // 룰 자체는 ChampionAIBrain.cpp의 brain 구현에 있다.
        ChampionAIBrainInput input{};
        input.fDt = tc.fDt;
        input.bCanAttackChampion = CanAttackChampion(ai, ctx);
        input.bPostComboBAWindow =
            ai.fPostComboBATimer > 0.f && ai.bPostComboBAAllowed;
        input.fChampionScore = ai.fChampionDecisionScore;
        input.fFarmScore = ai.fFarmDecisionScore;
        input.fStructureScore = ai.fStructureDecisionScore;

        ai.intent = ResolveChampionAIBrain(ai.brainType)
            .DecideLaneCombatIntent(ai, input);
    }

    bool_t ShouldAttackChampion(
        const TickContext& tc,
        ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        SampleLaneCombatIntent(tc, ai, ctx);
        return ai.intent == eChampionAIIntent::AttackChampion &&
            CanAttackChampion(ai, ctx);
    }

    bool_t TryExecuteYasuoUltimate(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::YASUO ||
            ctx.airborneChampion == NULL_ENTITY ||
            !IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::R)))
        {
            return false;
        }

        return EmitSkillCommand(
            world,
            tc,
            self,
            ai,
            champion.id,
            selfPos,
            ctx.airborneChampion,
            static_cast<u8_t>(eSkillSlot::R),
            "yasuo-airborne-r",
            outCommands);
    }

    bool_t TryExecuteYasuoChampionCombat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::YASUO)
            return false;

        if (TryExecuteYasuoUltimate(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return true;

        const EntityID target = ctx.enemyChampion;
        if (target == NULL_ENTITY)
            return false;

        if (!CanAttackChampion(ai, ctx))
            return false;

        const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
        const u8_t eSlot = static_cast<u8_t>(eSkillSlot::E);
        const f32_t qRange = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            self,
            tc,
            champion.id,
            qSlot);
        const f32_t eRange = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            self,
            tc,
            champion.id,
            eSlot);

        if (ctx.bYasuoEActive &&
            IsSkillReady(world, self, qSlot) &&
            (qRange <= 0.f || ctx.enemyDistance <= qRange))
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, qSlot, "yasuo-eq-after-dash", outCommands);
        }

        if (IsSkillReady(world, self, eSlot) &&
            eRange > 0.f &&
            ctx.enemyDistance <= eRange &&
            ctx.enemyDistance > ctx.attackRange + 0.25f)
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, eSlot, "yasuo-e-engage-champion", outCommands);
        }

        if (IsSkillReady(world, self, qSlot) &&
            (qRange <= 0.f || ctx.enemyDistance <= qRange))
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, qSlot, "yasuo-q-champion", outCommands);
        }

        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            target, eChampionAIAction::AttackChampion, "yasuo-ba-champion", outCommands))
        {
            return true;
        }

        if (IsSkillReady(world, self, eSlot) &&
            ctx.yasuoDashMinion != NULL_ENTITY)
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.yasuoDashMinion, eSlot, "yasuo-e-minion-gapclose", outCommands);
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            targetPos, eChampionAIAction::AttackChampion,
            "yasuo-move-champion", outCommands);
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

        if (ai.fPostComboBATimer > 0.f)
        {
            if (!ai.bPostComboBAAllowed)
                return false;

            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "post-combo-attack-champion-ba", outCommands))
            {
                return true;
            }

            Vec3 targetPos{};
            if (!TryGetPosition(world, target, targetPos))
                return false;

            return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
                targetPos, eChampionAIAction::AttackChampion,
                "post-combo-attack-champion-move", outCommands);
        }

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
        RecordChampionAICommandDebug(ai, NULL_ENTITY, recall.kind, recall.slot, selfPos);
        PushChampionAIDecisionTrace(ai, tc, NULL_ENTITY);
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
            ctx.alliedWave == NULL_ENTITY ||
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
        return ShouldAttackChampion(tc, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands);
    }

    bool_t TryExecuteJaxDive(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::JAX)
            return false;

        EntityID target = ai.diveTarget;
        const bool_t bDiveActive =
            target != NULL_ENTITY &&
            ai.divePhase != eChampionAIDivePhase::None;
        if (!bDiveActive)
        {
            if (ctx.lowHpEnemyChampion == NULL_ENTITY)
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
                return false;
            }
            if (!IsFlashReady(world, self))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::FlashNotReady);
                return false;
            }

            target = ctx.lowHpEnemyChampion;
            ai.diveTarget = target;
            ai.divePhase = eChampionAIDivePhase::EngageQ;
            ai.diveExtraBACount = 0u;
            ai.fDiveExtraBATimer = ai.fDiveExtraBAWindow;
        }

        const bool_t bTargetAlive = IsAliveTarget(world, target);
        Vec3 targetPos = selfPos;
        if (bTargetAlive && !TryGetPosition(world, target, targetPos))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
            return false;
        }

        SetChampionAIState(ai, eChampionAIState::Diving);
        SetChampionAIIntent(ai, eChampionAIIntent::ExecuteDive, true);
        ai.lockedChampion = target;

        const f32_t targetHp = bTargetAlive ? HealthRatio(world, target) : 0.f;
        const f32_t distance =
            std::sqrt(std::max(0.f, WintersMath::DistanceSqXZ(selfPos, targetPos)));
        if (!bTargetAlive || targetHp <= 0.f)
            ai.divePhase = eChampionAIDivePhase::FlashExit;

        if (ai.divePhase == eChampionAIDivePhase::EngageQ)
        {
            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, static_cast<u8_t>(eSkillSlot::Q),
                "jax-dive-q", outCommands))
            {
                ai.divePhase = eChampionAIDivePhase::ArmW;
                return true;
            }

            if (distance > ctx.attackRange + 0.25f)
            {
                return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
                    targetPos, eChampionAIAction::AttackChampion,
                    "jax-dive-chase", outCommands);
            }

            ai.divePhase = eChampionAIDivePhase::ArmW;
        }

        if (ai.divePhase == eChampionAIDivePhase::ArmW)
        {
            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, static_cast<u8_t>(eSkillSlot::W),
                "jax-dive-w", outCommands))
            {
                ai.divePhase = eChampionAIDivePhase::BasicAttack;
                return true;
            }

            ai.divePhase = eChampionAIDivePhase::BasicAttack;
        }

        if (ai.divePhase == eChampionAIDivePhase::BasicAttack)
        {
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "jax-dive-ba", outCommands))
            {
                ai.divePhase = eChampionAIDivePhase::ExtraBasicAttack;
                return true;
            }
        }

        if (ai.divePhase == eChampionAIDivePhase::ExtraBasicAttack)
        {
            if (bTargetAlive &&
                targetHp <= ai.fLowHpExecuteThreshold &&
                ai.diveExtraBACount < 2u &&
                ai.fDiveExtraBATimer > 0.f)
            {
                if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                    target, eChampionAIAction::AttackChampion,
                    "jax-dive-extra-ba", outCommands))
                {
                    ++ai.diveExtraBACount;
                    return true;
                }
            }

            ai.divePhase = eChampionAIDivePhase::FlashExit;
        }

        if (ai.divePhase == eChampionAIDivePhase::FlashExit)
        {
            if (EmitFlashCommand(world, tc, self, ai, champion.id, selfPos,
                ai.safeAnchor, "jax-dive-flash-exit", outCommands))
            {
                ai.diveTarget = NULL_ENTITY;
                ai.divePhase = eChampionAIDivePhase::ExitMove;
                return true;
            }

            ai.divePhase = eChampionAIDivePhase::ExitMove;
        }

        if (ai.divePhase == eChampionAIDivePhase::ExitMove)
        {
            ai.diveTarget = NULL_ENTITY;
            ai.divePhase = eChampionAIDivePhase::None;
            return EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
        }

        return false;
    }

    bool_t TryExecuteYasuoMinionFarm(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::YASUO ||
            ctx.enemyMinion == NULL_ENTITY)
        {
            return false;
        }

        Vec3 minionPos{};
        if (!TryGetPosition(world, ctx.enemyMinion, minionPos))
            return false;

        const f32_t minionDistance =
            std::sqrt(std::max(0.f, WintersMath::DistanceSqXZ(selfPos, minionPos)));
        const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
        const f32_t qRange = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            self,
            tc,
            champion.id,
            qSlot);
        const f32_t minionHpRatio = HealthRatio(world, ctx.enemyMinion);

        if (IsSkillReady(world, self, qSlot) &&
            minionHpRatio <= 0.65f &&
            (qRange <= 0.f || minionDistance <= qRange))
        {
            return EmitSkillCommand(
                world,
                tc,
                self,
                ai,
                champion.id,
                selfPos,
                ctx.enemyMinion,
                qSlot,
                "yasuo-q-farm-minion",
                outCommands,
                0u,
                static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity),
                eChampionAIAction::AttackMinion);
        }

        return false;
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

    // Champion-specific combat tactics stay behind one function-pointer seam.
    bool_t TryExecuteYoneChampionCombat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::YONE)
            return false;

        const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
        const u8_t wSlot = static_cast<u8_t>(eSkillSlot::W);
        const u8_t eSlot = static_cast<u8_t>(eSkillSlot::E);
        const u8_t rSlot = static_cast<u8_t>(eSkillSlot::R);

        const YoneSimComponent* pYoneState = world.HasComponent<YoneSimComponent>(self)
            ? &world.GetComponent<YoneSimComponent>(self)
            : nullptr;
        const bool_t bSoulOut =
            pYoneState != nullptr &&
            pYoneState->bSoulUnboundActive &&
            !pYoneState->bReturning;

        // E recast is a stage-2 command. It must bypass the AI cooldown gate
        // and let CommandExecutor validate the active stage window.
        if (bSoulOut)
        {
            const bool_t bShouldReturn =
                pYoneState->soulTimerSec <= 0.75f ||
                ctx.selfHpRatio <= ai.reengageHpRatio ||
                ctx.bInsideEnemyTurretDanger ||
                ctx.enemyChampion == NULL_ENTITY ||
                ctx.selfHpRatio + ai.fChampionScoreMargin < ctx.enemyHpRatio;
            if (bShouldReturn)
            {
                ai.lastAction = eChampionAIAction::Retreat;
                GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
                cmd.slot = eSlot;
                cmd.itemId = 2u;
                cmd.direction = WintersMath::DirectionXZ(selfPos, pYoneState->anchorPosition);
                cmd.groundPos = pYoneState->anchorPosition;
                outCommands.push_back(cmd);
                RecordChampionAICommandDebug(ai, NULL_ENTITY, cmd.kind, cmd.slot, pYoneState->anchorPosition);
                PushChampionAIDecisionTrace(ai, tc, NULL_ENTITY);
                LogChampionAICommand("yone-e-soul-return", tc, self, ai, champion.id,
                    selfPos, pYoneState->anchorPosition, NULL_ENTITY, cmd.kind, cmd.slot);
                return true;
            }
        }

        const EntityID target = ctx.enemyChampion;
        if (target == NULL_ENTITY || !CanAttackChampion(ai, ctx))
            return false;

        if ((ctx.enemyHpRatio <= 0.5f || bSoulOut) &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rSlot, "yone-r-champion", outCommands))
        {
            return true;
        }

        if (!bSoulOut &&
            ctx.enemyDistance > ctx.attackRange + 0.25f &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                eSlot, "yone-e-soul-engage", outCommands))
        {
            return true;
        }

        if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            qSlot, "yone-q-champion", outCommands))
        {
            return true;
        }

        if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            wSlot, "yone-w-champion", outCommands))
        {
            return true;
        }

        return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos, target,
            eChampionAIAction::AttackChampion, "yone-ba-champion", outCommands);
    }

    using ChampionCombatTacticsFn = bool_t (*)(
        CWorld&, const TickContext&, EntityID, ChampionAIComponent&,
        ChampionComponent&, const Vec3&, const ChampionAIContext&,
        std::vector<GameCommand>&);

    struct ChampionCombatTacticsEntry
    {
        eChampion champion = eChampion::END;
        ChampionCombatTacticsFn fn = nullptr;
    };

    constexpr ChampionCombatTacticsEntry kChampionCombatTacticsRegistry[] =
    {
        { eChampion::YASUO, &TryExecuteYasuoChampionCombat },
        { eChampion::YONE, &TryExecuteYoneChampionCombat },
    };

    ChampionCombatTacticsFn ResolveChampionCombatTactics(eChampion champion)
    {
        for (const ChampionCombatTacticsEntry& entry : kChampionCombatTacticsRegistry)
        {
            if (entry.champion == champion)
                return entry.fn;
        }

        return nullptr;
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
        SetChampionAIState(ai, eChampionAIState::LaneCombat);
        ai.lockedChampion = ctx.enemyChampion;
        ai.targetMinion = ctx.enemyMinion;
        ai.targetStructure = ctx.enemyStructure;
        ai.alliedWave = ctx.alliedWave;
        ai.bStructureWaveTanking = ctx.bStructureWaveTanking;
        ai.bInsideEnemyTurretDanger = ctx.bInsideEnemyTurretDanger;

        if (TryExecuteJaxDive(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        const bool_t bSelfLowHp = ctx.selfHpRatio <= ai.retreatHpRatio;
        const bool_t bChampionInsideTurret =
            ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY;
        const bool_t bTooMuchTurretDanger =
            ctx.turretDanger > ai.fTurretDangerThreshold && !ctx.bStructureWaveTanking;
        if (bSelfLowHp || bChampionInsideTurret || bTooMuchTurretDanger)
        {
            ai.diveTarget = NULL_ENTITY;
            ai.divePhase = eChampionAIDivePhase::None;
            SetChampionAIBlockReason(ai,
                bSelfLowHp
                    ? eChampionAIDecisionBlockReason::SelfLowHp
                    : eChampionAIDecisionBlockReason::TurretDanger);
            EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
            return;
        }

        const eChampionAIBehaviorStatus comboStatus =
            TickActiveChampionCombo(world, tc, self, ai, champion, selfPos, ctx, outCommands);
        if (comboStatus != eChampionAIBehaviorStatus::Failure)
            return;

        if (TryExecuteStructureAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (ChampionCombatTacticsFn tactics = ResolveChampionCombatTactics(champion.id))
        {
            if (tactics(world, tc, self, ai, champion, selfPos, ctx, outCommands))
                return;
        }

        if (TryStartChampionAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        SetChampionAIIntent(ai, eChampionAIIntent::FarmMinion);
        ClearChampionAICombo(ai);

        if (TryExecuteYasuoMinionFarm(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

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
        case eChampionAIAction::UseFlashEscape:
            ai.state = eChampionAIState::Diving;
            SetChampionAIIntent(ai, eChampionAIIntent::ExecuteDive, true);
            return EmitFlashCommand(world, tc, self, ai, champion.id, selfPos,
                ai.safeAnchor, "debug-flash-escape", outCommands);
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
            ai.fPostComboBATimer = std::max(0.f, ai.fPostComboBATimer - tc.fDt);
            if (ai.fPostComboBATimer <= 0.f)
                ai.bPostComboBAAllowed = false;
            ai.fDiveExtraBATimer = std::max(0.f, ai.fDiveExtraBATimer - tc.fDt);

            const Vec3 selfPos = selfTf.GetPosition();
            const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
            ai.champion = champion.id;
            ai.team = champion.team;
            ApplyChampionAIProfileAndTuning(ai, profile);

            const ChampionAIContext ctx =
                BuildChampionAIContext(world, self, ai, champion, tc, selfPos);

            UpdateChampionAIDecisionEvidence(ai, tc, ctx);
            ai.debugAvailableActionMask =
                BuildChampionAIAvailableActionMask(world, self, ai, ctx);
            ai.debugAvailableSkillMask =
                BuildChampionAIAvailableSkillMask(world, self, champion.id, tc, profile, ctx);

            if (HasActiveRecall(world, self))
            {
                ExecuteRecalling(world, tc, self, ai, champion, selfPos, outCommands);
                return;
            }

            const bool_t bHasDebugOverride = ai.debugForcedDecisionCount > 0;
            if (ai.decisionTimer > 0.f && !bHasDebugOverride)
                return;

            if (IsChampionAIActionLocked(world, self, champion.id, tc))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::ActionLocked);
                PushChampionAIDecisionTrace(ai, tc, ai.debugLastCommandTarget);
                return;
            }

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
