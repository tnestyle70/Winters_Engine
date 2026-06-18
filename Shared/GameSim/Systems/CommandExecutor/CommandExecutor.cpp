#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/KalistaPassiveDashComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"

#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"

#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"
//Viego Spawn
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
    constexpr f32_t kMaxMoveCommandAbs = 10000.f;
    constexpr f32_t kAttackChaseArriveSlack = 0.05f;
    constexpr u16_t kNetAnimFlagLoop = 0x0800u;

    void OutputCommandDebug(const char* pText)
    {
        if (!pText)
            return;

        WintersOutputAIDebugStringA(pText);
    }

    eTeam ResolveTeam(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).team;
        if (world.HasComponent<MinionComponent>(entity))
            return world.GetComponent<MinionComponent>(entity).team;
        if (world.HasComponent<StructureComponent>(entity))
            return world.GetComponent<StructureComponent>(entity).team;
        return eTeam::Neutral;
    }

    eChampion ResolveChampion(CWorld& world, EntityID entity);

    f32_t DistanceSqXZ(CWorld& world, EntityID lhs, EntityID rhs)
    {
        if (!world.HasComponent<TransformComponent>(lhs) ||
            !world.HasComponent<TransformComponent>(rhs))
        {
            return 3.402823466e+38F;
        }

        const Vec3 a = world.GetComponent<TransformComponent>(lhs).GetLocalPosition();
        const Vec3 b = world.GetComponent<TransformComponent>(rhs).GetLocalPosition();
        return WintersMath::DistanceSqXZ(a, b);
    }

    void ClearMoveTarget(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
    }

    void ClearAttackChase(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<AttackChaseComponent>(entity))
            world.RemoveComponent<AttackChaseComponent>(entity);
    }

    void ClearCombatAction(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<CombatActionComponent>(entity))
            world.RemoveComponent<CombatActionComponent>(entity);
    }

    void CancelRecall(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<RecallComponent>(entity))
            world.RemoveComponent<RecallComponent>(entity);
    }

    void ClearMoveFacingOverride(MoveTargetComponent& moveTarget)
    {
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }

    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        ClearMoveFacingOverride(moveTarget);
    }

    void SetMoveFacingOverride(
        MoveTargetComponent& moveTarget,
        const Vec3& facingTarget,
        const Vec3& facingDirection,
        u32_t sequenceNum)
    {
        const Vec3 normalizedFacingDirection =
            WintersMath::NormalizeXZ(facingDirection, Vec3{}, 0.0001f);
        if (normalizedFacingDirection.x == 0.f && normalizedFacingDirection.z == 0.f)
        {
            ClearMoveFacingOverride(moveTarget);
            return;
        }

        moveTarget.facingTarget = facingTarget;
        moveTarget.facingDirection = normalizedFacingDirection;
        moveTarget.facingSequenceNum = sequenceNum;
        moveTarget.facingLockTicks = kMoveFacingIntentLockTicks;
        moveTarget.bHasFacingTarget = true;
    }

    Vec3 ResolveAttackChaseFacingDirection(
        const Vec3& selfPos,
        const Vec3& targetPos,
        const Vec3& commandDirection)
    {
        const Vec3 targetDirection =
            WintersMath::DirectionXZ(selfPos, targetPos, Vec3{});
        const Vec3 clientDirection =
            WintersMath::NormalizeXZ(commandDirection, Vec3{}, 0.0001f);

        if (clientDirection.x != 0.f || clientDirection.z != 0.f)
        {
            if (targetDirection.x == 0.f && targetDirection.z == 0.f)
                return clientDirection;

            const f32_t dot =
                clientDirection.x * targetDirection.x +
                clientDirection.z * targetDirection.z;
            if (dot > -0.10f)
                return clientDirection;
        }

        return targetDirection;
    }

    bool_t TryAssignGridMovePath(
        const TickContext& tc,
        const Vec3& from,
        Vec3& ioTarget,
        MoveTargetComponent& moveTarget)
    {
        ClearMovePath(moveTarget);
        if (!tc.pWalkable)
            return true;

        Vec3 waypoints[kMovePathMaxWaypoints]{};
        u16_t waypointCount = 0;
        Vec3 resolved = ioTarget;
        if (!tc.pWalkable->TryBuildMovePath(
            from,
            ioTarget,
            waypoints,
            kMovePathMaxWaypoints,
            waypointCount,
            resolved))
        {
            ClearMovePath(moveTarget);
            return false;
        }

        ioTarget = resolved;
        moveTarget.pathCount = waypointCount;
        moveTarget.pathIndex = 0;
        for (u16_t i = 0; i < waypointCount; ++i)
            moveTarget.pathWaypoints[i] = waypoints[i];

        return true;
    }

    void StartAttackChase(CWorld& world, const TickContext& tc,
        const GameCommand& cmd, f32_t effectiveRange)
    {
        auto& chase = world.HasComponent<AttackChaseComponent>(cmd.issuerEntity)
            ? world.GetComponent<AttackChaseComponent>(cmd.issuerEntity)
            : world.AddComponent<AttackChaseComponent>(cmd.issuerEntity, AttackChaseComponent{});

        chase.target = cmd.targetEntity;
        chase.sequenceNum = cmd.sequenceNum;
        chase.commandKind = static_cast<u8_t>(cmd.kind);
        chase.slot = cmd.slot;
        chase.itemId = cmd.itemId;
        chase.effectiveRange = effectiveRange;
        chase.groundPos = cmd.groundPos;
        chase.direction = cmd.direction;
        chase.repathTimer = 0.f;
        chase.bActive = true;

        if (world.HasComponent<TransformComponent>(cmd.targetEntity))
        {
            Vec3 goal = world.GetComponent<TransformComponent>(cmd.targetEntity).GetLocalPosition();

            auto& moveTarget = world.HasComponent<MoveTargetComponent>(cmd.issuerEntity)
                ? world.GetComponent<MoveTargetComponent>(cmd.issuerEntity)
                : world.AddComponent<MoveTargetComponent>(cmd.issuerEntity, MoveTargetComponent{});

            moveTarget.arriveRadius =
                std::max(MoveTargetComponent{}.arriveRadius,
                    effectiveRange - kAttackChaseArriveSlack);
            if (world.HasComponent<TransformComponent>(cmd.issuerEntity))
            {
                const Vec3 selfPos =
                    world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition();
                if (!TryAssignGridMovePath(tc, selfPos, goal, moveTarget))
                {
                    moveTarget.bHasTarget = false;
                    return;
                }

                const Vec3 facingDirection =
                    ResolveAttackChaseFacingDirection(selfPos, goal, cmd.direction);
                const Vec3 facingTarget{
                    selfPos.x + facingDirection.x,
                    selfPos.y,
                    selfPos.z + facingDirection.z
                };
                SetMoveFacingOverride(
                    moveTarget,
                    facingTarget,
                    facingDirection,
                    cmd.sequenceNum);
            }
            else
            {
                ClearMovePath(moveTarget);
            }

            moveTarget.target = goal;
            moveTarget.bHasTarget = true;
        }

        static u32_t s_chaseLogCount = 0;
        if (s_chaseLogCount < 512u)
        {
            const bool_t bHasIssuerTransform =
                world.HasComponent<TransformComponent>(cmd.issuerEntity);
            const bool_t bHasTargetTransform =
                world.HasComponent<TransformComponent>(cmd.targetEntity);
            const Vec3 selfPos = bHasIssuerTransform
                ? world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition()
                : Vec3{};
            const Vec3 targetPos = bHasTargetTransform
                ? world.GetComponent<TransformComponent>(cmd.targetEntity).GetLocalPosition()
                : Vec3{};
            const Vec3 targetDir =
                WintersMath::DirectionXZ(selfPos, targetPos, Vec3{});
            const Vec3 clientDir =
                WintersMath::NormalizeXZ(cmd.direction, Vec3{}, 0.0001f);
            const Vec3 facingDir =
                ResolveAttackChaseFacingDirection(selfPos, targetPos, cmd.direction);
            const bool_t bHasMoveTarget =
                world.HasComponent<MoveTargetComponent>(cmd.issuerEntity);
            const MoveTargetComponent* pMoveTarget = bHasMoveTarget
                ? &world.GetComponent<MoveTargetComponent>(cmd.issuerEntity)
                : nullptr;
            const Vec3 firstWaypoint =
                (pMoveTarget && pMoveTarget->pathCount > 0)
                    ? pMoveTarget->pathWaypoints[0]
                    : Vec3{};
            const Vec3 path0Dir =
                (pMoveTarget && pMoveTarget->pathCount > 0)
                    ? WintersMath::DirectionXZ(selfPos, firstWaypoint, Vec3{})
                    : Vec3{};
            const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
            const f32_t yawBefore = bHasIssuerTransform
                ? world.GetComponent<TransformComponent>(cmd.issuerEntity).GetRotation().y
                : 0.f;
            const f32_t chaseYaw =
                (facingDir.x != 0.f || facingDir.z != 0.f)
                    ? ResolveChampionVisualYawFromDirection(champion, facingDir)
                    : yawBefore;
            const f32_t targetYaw =
                (targetDir.x != 0.f || targetDir.z != 0.f)
                    ? ResolveChampionVisualYawFromDirection(champion, targetDir)
                    : yawBefore;
            const f32_t path0Yaw =
                (path0Dir.x != 0.f || path0Dir.z != 0.f)
                    ? ResolveChampionVisualYawFromDirection(champion, path0Dir)
                    : yawBefore;
            const f32_t clientVsTargetDot =
                clientDir.x * targetDir.x + clientDir.z * targetDir.z;
            const f32_t path0VsTargetDot =
                path0Dir.x * targetDir.x + path0Dir.z * targetDir.z;
            const f32_t chaseVsPrev =
                NormalizeChampionVisualYaw(chaseYaw - yawBefore);
            char msg[1280]{};
            sprintf_s(msg,
                "[YawTrace][AttackChaseStart] tick=%llu issuer=%u target=%u seq=%u champion=%u self=(%.3f,%.3f,%.3f) targetPos=(%.3f,%.3f,%.3f) cmdGround=(%.3f,%.3f,%.3f) cmdDir=(%.3f,%.3f) targetDir=(%.3f,%.3f) facingDir=(%.3f,%.3f) path0=(%.3f,%.3f,%.3f) path0Dir=(%.3f,%.3f) pathCount=%u lockTicks=%u yawBefore=%.4f chaseYaw=%.4f targetYaw=%.4f path0Yaw=%.4f chaseVsPrev=%.4f clientVsTargetDot=%.4f path0VsTargetDot=%.4f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                static_cast<u32_t>(cmd.issuerEntity),
                static_cast<u32_t>(cmd.targetEntity),
                cmd.sequenceNum,
                static_cast<u32_t>(champion),
                selfPos.x,
                selfPos.y,
                selfPos.z,
                targetPos.x,
                targetPos.y,
                targetPos.z,
                cmd.groundPos.x,
                cmd.groundPos.y,
                cmd.groundPos.z,
                clientDir.x,
                clientDir.z,
                targetDir.x,
                targetDir.z,
                facingDir.x,
                facingDir.z,
                firstWaypoint.x,
                firstWaypoint.y,
                firstWaypoint.z,
                path0Dir.x,
                path0Dir.z,
                pMoveTarget ? static_cast<u32_t>(pMoveTarget->pathCount) : 0u,
                pMoveTarget ? static_cast<u32_t>(pMoveTarget->facingLockTicks) : 0u,
                yawBefore,
                chaseYaw,
                targetYaw,
                path0Yaw,
                chaseVsPrev,
                clientVsTargetDot,
                path0VsTargetDot);
            OutputCommandDebug(msg);
            ++s_chaseLogCount;
        }
    }

    bool_t IsAliveForCommand(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    bool_t IsFiniteMoveTarget(const Vec3& v)
    {
        return std::isfinite(v.x) &&
            std::isfinite(v.y) &&
            std::isfinite(v.z) &&
            std::fabs(v.x) <= kMaxMoveCommandAbs &&
            std::fabs(v.y) <= kMaxMoveCommandAbs &&
            std::fabs(v.z) <= kMaxMoveCommandAbs;
    }

    void StartNetAnimation(
        CWorld& world,
        EntityID entity,
        eNetAnimId animId,
        const TickContext& tc,
        u16_t playbackRateQ8,
        u8_t skillStage = 1,
        bool_t bLoop = false)
    {
        auto& anim = world.HasComponent<NetAnimationComponent>(entity)
            ? world.GetComponent<NetAnimationComponent>(entity)
            : world.AddComponent<NetAnimationComponent>(entity, NetAnimationComponent{});

        const u8_t sanitizedStage =
            (skillStage == 0u) ? 1u : static_cast<u8_t>(skillStage & 0x0fu);
        ++anim.actionSeq;
        anim.animId = static_cast<u16_t>(animId);
        anim.animPhaseFrame = 0;
        anim.animStartTick = tc.tickIndex;
        anim.playbackRateQ8 = playbackRateQ8 != 0 ? playbackRateQ8 : 256;
        anim.flags = static_cast<u16_t>(
            (static_cast<u16_t>(sanitizedStage) << 12) |
            (bLoop ? kNetAnimFlagLoop : 0u));
        anim.priority = 0;
    }

    eNetAnimId SkillSlotToAnim(u8_t slot)
    {
        switch (slot)
        {
        case 1: return eNetAnimId::SkillQ;
        case 2: return eNetAnimId::SkillW;
        case 3: return eNetAnimId::SkillE;
        case 4: return eNetAnimId::SkillR;
        default: return eNetAnimId::BasicAttack;
        }
    }

    bool_t ShouldSuppressCastNetAnimation(eChampion champion, u8_t slot, u8_t stage)
    {
        return champion == eChampion::JAX &&
            slot == static_cast<u8_t>(eSkillSlot::W) &&
            stage == 1u;
    }

    bool_t ShouldLoopSkillNetAnimation(eChampion champion, u8_t slot, u8_t stage)
    {
        return champion == eChampion::JAX &&
            slot == static_cast<u8_t>(eSkillSlot::E) &&
            stage == 1u;
    }

    Vec3 ResolveEventPosition(CWorld& world, EntityID source, EntityID target, const Vec3& fallback)
    {
        if (target != NULL_ENTITY && world.HasComponent<TransformComponent>(target))
            return world.GetComponent<TransformComponent>(target).GetPosition();
        if (fallback.x != 0.f || fallback.y != 0.f || fallback.z != 0.f)
            return fallback;
        if (source != NULL_ENTITY && world.HasComponent<TransformComponent>(source))
            return world.GetComponent<TransformComponent>(source).GetPosition();
        return Vec3{};
    }

    u8_t ResolveSkillRank(CWorld& world, EntityID entity, u8_t slot)
    {
        if (!world.HasComponent<SkillRankComponent>(entity) ||
            slot >= SkillRankComponent::kSlotCount)
        {
            return 0;
        }

        return world.GetComponent<SkillRankComponent>(entity).ranks[slot];
    }

    bool_t IsSkillLearned(CWorld& world, EntityID entity, u8_t slot)
    {
        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
            return true;

        return ResolveSkillRank(world, entity, slot) > 0;
    }

    u32_t BuildGenericEffectId(CWorld& world, EntityID entity, u8_t slot)
    {
        u32_t champion = 0;
        if (world.HasComponent<ChampionComponent>(entity))
            champion = static_cast<u32_t>(world.GetComponent<ChampionComponent>(entity).id);
        return (champion << 8) | static_cast<u32_t>(slot);
    }

    u16_t ResolveSkillEffectDurationMs(eChampion champion, u8_t slot)
    {
        if (champion == eChampion::KINDRED &&
            slot == static_cast<u8_t>(eSkillSlot::R))
        {
            return static_cast<u16_t>(
                KindredGameSim::GetUltimateDurationSec() * 1000.f + 0.5f);
        }

        return 700;
    }

    eChampion ResolveChampion(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).id;
        if (world.HasComponent<StatComponent>(entity))
            return world.GetComponent<StatComponent>(entity).championId;
        return eChampion::NONE;
    }

    f32_t ResolveCooldownReduction(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<StatComponent>(entity))
            return 0.f;

        const f32_t cdr = world.GetComponent<StatComponent>(entity).cdr;
        if (!std::isfinite(cdr))
            return 0.f;

        return std::clamp(cdr, 0.f, 0.4f);
    }

    f32_t ResolveCastSkillCooldown(CWorld& world, EntityID entity, eChampion champion, u8_t slot)
    {
        f32_t cooldown = ChampionGameDataDB::ResolveSkillCooldown(champion, slot);
        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
            return std::max(0.f, cooldown);

        if (world.HasComponent<StatComponent>(entity))
        {
            const auto& stat = world.GetComponent<StatComponent>(entity);
            if (stat.abilityHaste > 0.f)
                return std::max(0.f, CCombatFormula::ResolveAbilityCooldown(cooldown, stat.abilityHaste));

            cooldown *= (1.f - ResolveCooldownReduction(world, entity));
        }

        return std::max(0.f, cooldown);
    }

    f32_t ResolveBasicAttackCooldown(CWorld& world, EntityID entity, eChampion champion)
    {
        if (world.HasComponent<StatComponent>(entity))
        {
            const f32_t attackSpeed = world.GetComponent<StatComponent>(entity).attackSpeed;
            if (std::isfinite(attackSpeed) && attackSpeed > 0.001f)
                return std::clamp(1.f / attackSpeed, 0.333f, 5.f);
        }

        return std::max(0.333f, ChampionGameDataDB::ResolveSkillCooldown(
            champion,
            static_cast<u8_t>(eSkillSlot::BasicAttack)));
    }

    f32_t ResolveBasicAttackAnimSpeedScale(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<StatComponent>(entity))
            return 1.f;

        const auto& stat = world.GetComponent<StatComponent>(entity);
        const f32_t baseAttackSpeed = stat.baseAttackSpeed > 0.001f
            ? stat.baseAttackSpeed
            : stat.attackSpeedRatio;

        if (!std::isfinite(stat.attackSpeed) ||
            !std::isfinite(baseAttackSpeed) ||
            baseAttackSpeed <= 0.001f)
        {
            return 1.f;
        }

        return std::clamp(stat.attackSpeed / baseAttackSpeed, 0.2f, 4.f);
    }

    u64_t ResolveScaledBasicAttackActionTicks(f32_t actionDurationSec, f32_t speedScale)
    {
        if (!std::isfinite(actionDurationSec) || actionDurationSec <= 0.01f)
            actionDurationSec = 0.6f;
        if (!std::isfinite(speedScale) || speedScale <= 0.01f)
            speedScale = 1.f;

        const f32_t scaledSec = std::clamp(
            actionDurationSec / speedScale,
            DeterministicTime::kFixedDt,
            5.f);

        const u64_t ticks = static_cast<u64_t>(std::ceil(
            static_cast<f64_t>(scaledSec) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));

        return ticks > 0 ? ticks : 1u;
    }

    eCombatActionMovePolicy ResolveBasicAttackMovePolicy(eChampion champion)
    {
        if (champion == eChampion::KALISTA)
            return eCombatActionMovePolicy::QueueMoveUntilImpact;

        return eCombatActionMovePolicy::CancelBeforeImpactMoveAfterImpact;
    }

    bool_t HasActiveBasicAttackAction(CWorld& world, EntityID entity)
    {
        return entity != NULL_ENTITY &&
            world.HasComponent<CombatActionComponent>(entity) &&
            world.GetComponent<CombatActionComponent>(entity).eKind ==
                eCombatActionKind::BasicAttack;
    }

    bool_t ConsumeMoveForCombatAction(CWorld& world, const TickContext& tc, const GameCommand& cmd)
    {
        if (!HasActiveBasicAttackAction(world, cmd.issuerEntity))
            return false;

        auto& action = world.GetComponent<CombatActionComponent>(cmd.issuerEntity);
        const bool_t bImpactDue = tc.tickIndex >= action.uImpactTick;
        if (!action.bImpactIssued && bImpactDue)
        {
            action.bQueuedMove = true;
            action.vQueuedMoveTarget = cmd.groundPos;
            action.vQueuedMoveDirection = cmd.direction;
            ClearMoveTarget(world, cmd.issuerEntity);
            return true;
        }

        if (!action.bImpactIssued)
        {
            if (action.eMovePolicy == eCombatActionMovePolicy::LockUntilEnd &&
                tc.tickIndex < action.uEndTick)
            {
                return true;
            }

            if (action.eMovePolicy == eCombatActionMovePolicy::QueueMoveUntilImpact)
            {
                action.bQueuedMove = true;
                action.vQueuedMoveTarget = cmd.groundPos;
                action.vQueuedMoveDirection = cmd.direction;
                ClearMoveTarget(world, cmd.issuerEntity);
                return true;
            }

            ClearCombatAction(world, cmd.issuerEntity);
            return false;
        }

        ClearCombatAction(world, cmd.issuerEntity);
        return false;
    }

    bool_t IsKalistaPassiveDashAction(eNetAnimId animId)
    {
        return animId == eNetAnimId::BasicAttack || animId == eNetAnimId::SkillQ;
    }

    u8_t SlotFromKalistaPassiveDashAction(eNetAnimId animId)
    {
        return animId == eNetAnimId::SkillQ
            ? static_cast<u8_t>(eSkillSlot::Q)
            : static_cast<u8_t>(eSkillSlot::BasicAttack);
    }

    f32_t ResolveEntityVisualYawOffset(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<JungleComponent>(entity))
            return WintersMath::kPi;

        return ChampionGameDataDB::ResolveVisualYawOffset(ResolveChampion(world, entity));
    }

    f32_t ResolveVisualYawFromDirection(const Vec3& direction, f32_t visualYawOffset)
    {
        return WintersMath::NormalizeRadians(
            WintersMath::YawFromDirectionXZ(direction, visualYawOffset));
    }

    f32_t ResolveVisualYawNear(
        const Vec3& direction,
        f32_t referenceYaw,
        f32_t visualYawOffset)
    {
        return MakeChampionVisualYawNear(
            ResolveVisualYawFromDirection(direction, visualYawOffset),
            referenceYaw);
    }

    Vec3 ForwardFromYaw(f32_t yaw, f32_t visualYawOffset)
    {
        const f32_t gameplayYaw = yaw - visualYawOffset;
        return WintersMath::DirectionFromYawXZ(gameplayYaw);
    }

    Vec3 ForwardFromYaw(f32_t yaw, eChampion champion)
    {
        return ForwardFromYaw(
            yaw,
            ChampionGameDataDB::ResolveVisualYawOffset(champion));
    }

    void ArmKalistaPassiveDashWindow(
        CWorld& world,
        EntityID entity,
        u8_t slot,
        const Vec3& direction,
        bool_t bStartPending = false,
        u64_t triggerTick = 0)
    {
        if (ResolveChampion(world, entity) != eChampion::KALISTA)
            return;
        if (!world.HasComponent<TransformComponent>(entity) ||
            !world.HasComponent<NetAnimationComponent>(entity))
        {
            return;
        }

        auto& anim = world.GetComponent<NetAnimationComponent>(entity);
        const u64_t lockTicks = ChampionGameDataDB::ResolveSkillActionLockTicks(eChampion::KALISTA, slot);
        const f32_t dashDistance =
            ChampionGameDataDB::ResolvePassiveDashDistance(eChampion::KALISTA);
        const f32_t dashDurationSec =
            ChampionGameDataDB::ResolvePassiveDashDurationSec(eChampion::KALISTA);
        const Vec3 fallback = ForwardFromYaw(
            world.GetComponent<TransformComponent>(entity).GetRotation().y,
            eChampion::KALISTA);

        auto& dash = world.HasComponent<KalistaPassiveDashComponent>(entity)
            ? world.GetComponent<KalistaPassiveDashComponent>(entity)
            : world.AddComponent<KalistaPassiveDashComponent>(entity, KalistaPassiveDashComponent{});

        if (dash.bActive)
            return;

        dash.bPending = bStartPending;
        dash.bActive = false;
        dash.slot = slot;
        dash.sourceActionSeq = anim.actionSeq;
        dash.triggerTick = bStartPending ? triggerTick : anim.animStartTick + lockTicks;
        dash.direction = WintersMath::NormalizeXZ(direction, fallback, 0.0001f);
        dash.bHasQueuedMove = false;
        dash.queuedMoveTarget = {};
        dash.distance = dashDistance;
        dash.durationSec = dashDurationSec;
        dash.elapsedSec = 0.f;
    }

    bool_t TryConsumeKalistaPassiveDashMove(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd)
    {
        if (ResolveChampion(world, cmd.issuerEntity) != eChampion::KALISTA)
            return false;

        KalistaPassiveDashComponent* pExistingDash = nullptr;
        if (world.HasComponent<KalistaPassiveDashComponent>(cmd.issuerEntity))
        {
            pExistingDash = &world.GetComponent<KalistaPassiveDashComponent>(cmd.issuerEntity);
            if (pExistingDash->bActive || pExistingDash->bPending)
            {
                if (cmd.kind == eCommandKind::Move && IsFiniteMoveTarget(cmd.groundPos))
                {
                    pExistingDash->queuedMoveTarget = cmd.groundPos;
                    pExistingDash->bHasQueuedMove = true;
                }
                ClearMoveTarget(world, cmd.issuerEntity);
                return true;
            }
        }

        if (!world.HasComponent<TransformComponent>(cmd.issuerEntity) ||
            !world.HasComponent<NetAnimationComponent>(cmd.issuerEntity))
        {
            return false;
        }

        auto& anim = world.GetComponent<NetAnimationComponent>(cmd.issuerEntity);
        const auto animId = static_cast<eNetAnimId>(anim.animId);
        if (!IsKalistaPassiveDashAction(animId))
        {
            if (pExistingDash)
                world.RemoveComponent<KalistaPassiveDashComponent>(cmd.issuerEntity);
            return false;
        }
        if (tc.tickIndex < anim.animStartTick)
            return false;

        const u8_t slot = SlotFromKalistaPassiveDashAction(animId);
        const u64_t lockTicks = ChampionGameDataDB::ResolveSkillActionLockTicks(eChampion::KALISTA, slot);
        const u64_t triggerTick = anim.animStartTick + lockTicks;
        const u64_t inputGraceTicks =
            ChampionGameDataDB::ResolvePassiveDashInputGraceTicks(eChampion::KALISTA);
        const u64_t expireTick = triggerTick + inputGraceTicks;
        if (tc.tickIndex >= expireTick)
        {
            if (pExistingDash)
                world.RemoveComponent<KalistaPassiveDashComponent>(cmd.issuerEntity);
            return false;
        }
        if (pExistingDash &&
            pExistingDash->sourceActionSeq != 0u &&
            pExistingDash->sourceActionSeq != anim.actionSeq)
        {
            world.RemoveComponent<KalistaPassiveDashComponent>(cmd.issuerEntity);
            pExistingDash = nullptr;
        }

        ClearMoveTarget(world, cmd.issuerEntity);

        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const Vec3 pos = transform.GetLocalPosition();
        Vec3 dashDir{};
        if (cmd.kind == eCommandKind::Move &&
            (cmd.direction.x != 0.f || cmd.direction.z != 0.f))
        {
            dashDir = Vec3{ cmd.direction.x, 0.f, cmd.direction.z };
        }
        else if (cmd.kind == eCommandKind::Move)
        {
            dashDir = Vec3{
                cmd.groundPos.x - pos.x,
                0.f,
                cmd.groundPos.z - pos.z
            };
        }
        else if (cmd.direction.x != 0.f || cmd.direction.z != 0.f)
        {
            dashDir = Vec3{ cmd.direction.x, 0.f, cmd.direction.z };
        }
        else if (cmd.kind == eCommandKind::BasicAttack &&
            cmd.targetEntity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(cmd.targetEntity))
        {
            const Vec3 targetPos = world.GetComponent<TransformComponent>(cmd.targetEntity).GetPosition();
            dashDir = Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z };
        }
        else if (pExistingDash &&
            pExistingDash->sourceActionSeq == anim.actionSeq)
        {
            dashDir = pExistingDash->direction;
        }
        else
        {
            dashDir = Vec3{
                cmd.groundPos.x - pos.x,
                0.f,
                cmd.groundPos.z - pos.z
            };
        }
        const f32_t dirLenSq = dashDir.x * dashDir.x + dashDir.z * dashDir.z;
        if (dirLenSq > 0.0001f)
        {
            const f32_t invLen = 1.f / std::sqrt(dirLenSq);
            dashDir.x *= invLen;
            dashDir.z *= invLen;
        }
        else
        {
            dashDir = ForwardFromYaw(transform.GetRotation().y, ResolveChampion(world, cmd.issuerEntity));
        }

        auto& dash = pExistingDash
            ? *pExistingDash
            : world.AddComponent<KalistaPassiveDashComponent>(cmd.issuerEntity, KalistaPassiveDashComponent{});

        dash.bPending = true;
        dash.slot = slot;
        dash.sourceActionSeq = anim.actionSeq;
        dash.triggerTick = (tc.tickIndex > triggerTick) ? tc.tickIndex : triggerTick;
        dash.direction = dashDir;
        dash.bHasQueuedMove = false;
        dash.queuedMoveTarget = {};
        dash.distance = ChampionGameDataDB::ResolvePassiveDashDistance(eChampion::KALISTA);
        dash.durationSec = ChampionGameDataDB::ResolvePassiveDashDurationSec(eChampion::KALISTA);
        dash.elapsedSec = 0.f;

        char msg[256]{};
        sprintf_s(msg,
            "[KalistaPassive] server dash queued issuer=%u seq=%u slot=%u dir=(%.2f,%.2f) dist=%.2f trigger=%llu dur=%.2f\n",
            static_cast<u32_t>(cmd.issuerEntity),
            cmd.sequenceNum,
            static_cast<u32_t>(slot),
            dashDir.x,
            dashDir.z,
            dash.distance,
            static_cast<unsigned long long>(dash.triggerTick),
            dash.durationSec);
        OutputCommandDebug(msg);
        return true;
    }

    u16_t CastFrameVariantForSlot(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q: return GameplayHookVariant::Q_CastFrame;
        case eSkillSlot::W: return GameplayHookVariant::W_CastFrame;
        case eSkillSlot::E: return GameplayHookVariant::E_CastFrame;
        case eSkillSlot::R: return GameplayHookVariant::R_CastFrame;
        default: return GameplayHookVariant::BA_CastFrame;
        }
    }

    u16_t AcceptedVariantForSlot(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q: return GameplayHookVariant::Q_OnCastAccepted;
        case eSkillSlot::W: return GameplayHookVariant::W_OnCastAccepted;
        case eSkillSlot::E: return GameplayHookVariant::E_OnCastAccepted;
        case eSkillSlot::R: return GameplayHookVariant::R_OnCastAccepted;
        default: return GameplayHookVariant::BA_OnCastAccepted;
        }
    }

    u32_t BuildPrimarySkillHookId(eChampion champion, u8_t slot)
    {
        if (champion == eChampion::NONE || champion == eChampion::END)
            return 0;

        switch (champion)
        {
        case eChampion::ANNIE:
        case eChampion::IRELIA:
        case eChampion::YASUO:
            return (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
                ? 0
                : MakeGameplayHookId(champion, AcceptedVariantForSlot(slot));
        case eChampion::EZREAL:
            return (slot == static_cast<u8_t>(eSkillSlot::E))
                ? MakeGameplayHookId(champion, GameplayHookVariant::E_OnCastAccepted)
                : MakeGameplayHookId(champion, CastFrameVariantForSlot(slot));
        case eChampion::KALISTA:
            return (slot == static_cast<u8_t>(eSkillSlot::E))
                ? MakeGameplayHookId(champion, GameplayHookVariant::E_OnCastAccepted)
                : MakeGameplayHookId(champion, CastFrameVariantForSlot(slot));
        case eChampion::RIVEN:
            return (slot == static_cast<u8_t>(eSkillSlot::Q))
                ? MakeGameplayHookId(champion, GameplayHookVariant::Q_OnCastAccepted)
                : MakeGameplayHookId(champion, CastFrameVariantForSlot(slot));
        case eChampion::GAREN:
            return MakeGameplayHookId(champion, CastFrameVariantForSlot(slot));
        case eChampion::ZED:
            return MakeGameplayHookId(champion, CastFrameVariantForSlot(slot));
        default:
            return MakeGameplayHookId(champion, CastFrameVariantForSlot(slot));
        }
    }

    bool_t DispatchGameplayHookIfAvailable(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd,
        u32_t hookId,
        eChampion champion,
        u8_t rank)
    {
        if (hookId == 0 || !CGameplayHookRegistry::Instance().Has(hookId))
            return false;

        SkillDef def{};
        def.champ = champion;
        def.slot = cmd.slot;
        def.skillId = static_cast<u16_t>((static_cast<u32_t>(champion) << 8) | cmd.slot);

        GameplayHookContext ctx{};
        ctx.pWorld = &world;
        ctx.casterEntity = cmd.issuerEntity;
        ctx.casterTeam = ResolveTeam(world, cmd.issuerEntity);
        ctx.casterChampion = champion;
        ctx.skillRank = rank;
        ctx.pDef = &def;
        ctx.pCommand = &cmd;
        ctx.pTickCtx = &tc;
        return CGameplayHookRegistry::Instance().Dispatch(hookId, ctx);
    }

    void EnqueueFallbackSkillDamage(
        CWorld& world,
        const GameCommand& cmd,
        eChampion champion,
        u8_t rank)
    {
        if (cmd.targetEntity == NULL_ENTITY ||
            !IsAliveForCommand(world, cmd.targetEntity) ||
            !world.HasComponent<HealthComponent>(cmd.targetEntity))
        {
            return;
        }

        const eTeam sourceTeam = ResolveTeam(world, cmd.issuerEntity);
        const eTeam targetTeam = ResolveTeam(world, cmd.targetEntity);
        if (sourceTeam == targetTeam && sourceTeam != eTeam::Neutral)
            return;

        f32_t range = ChampionGameDataDB::ResolveSkillRange(champion, cmd.slot);
        if (range <= 0.f)
            range = 12.f;
        if (world.HasComponent<StatComponent>(cmd.issuerEntity))
        {
            const auto& stat = world.GetComponent<StatComponent>(cmd.issuerEntity);
            if (cmd.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
                stat.attackRange > 0.f)
            {
                range = stat.attackRange;
            }
        }

        if (DistanceSqXZ(world, cmd.issuerEntity, cmd.targetEntity) > range * range)
            return;

        DamageRequest request{};
        request.source = cmd.issuerEntity;
        request.target = cmd.targetEntity;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Magic;
        request.flatAmount = 45.f + static_cast<f32_t>(rank) * 25.f;
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(champion) << 8) | cmd.slot);
        request.rank = rank;
        EnqueueDamageRequest(world, request);
    }

    Vec3 ResolveProjectileDirection(CWorld& world, EntityID source,
        EntityID target, const Vec3& requestedDir)
    {
        Vec3 dir = WintersMath::Normalize3D(
            requestedDir,
            Vec3{},
            std::numeric_limits<f32_t>::epsilon());
        if (dir.x != 0.f || dir.y != 0.f || dir.z != 0.f)
            return dir;

        if (source != NULL_ENTITY &&
            target != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(source) &&
            world.HasComponent<TransformComponent>(target))
        {
            const Vec3 a = world.GetComponent<TransformComponent>(source).GetPosition();
            const Vec3 b = world.GetComponent<TransformComponent>(target).GetPosition();
            dir = WintersMath::Normalize3D(
                Vec3{ b.x - a.x, b.y - a.y, b.z - a.z },
                Vec3{},
                std::numeric_limits<f32_t>::epsilon());
            if (dir.x != 0.f || dir.y != 0.f || dir.z != 0.f)
                return dir;
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    Vec3 ResolveCastFacingDirection(CWorld& world, const GameCommand& cmd)
    {
        Vec3 dir = WintersMath::Normalize3D(
            cmd.direction,
            Vec3{},
            std::numeric_limits<f32_t>::epsilon());
        if (dir.x != 0.f || dir.y != 0.f || dir.z != 0.f)
            return dir;

        if (cmd.issuerEntity != NULL_ENTITY &&
            cmd.targetEntity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(cmd.issuerEntity) &&
            world.HasComponent<TransformComponent>(cmd.targetEntity))
        {
            const Vec3 a = world.GetComponent<TransformComponent>(cmd.issuerEntity).GetPosition();
            const Vec3 b = world.GetComponent<TransformComponent>(cmd.targetEntity).GetPosition();
            dir = WintersMath::Normalize3D(
                Vec3{ b.x - a.x, 0.f, b.z - a.z },
                Vec3{},
                std::numeric_limits<f32_t>::epsilon());
        }

        return dir;
    }

    void RotateEntityTowardDirection(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (entity == NULL_ENTITY || !world.HasComponent<TransformComponent>(entity))
            return;

        const f32_t lenSq = direction.x * direction.x + direction.z * direction.z;
        if (lenSq <= 0.0001f)
            return;

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        const eChampion champion = ResolveChampion(world, entity);
        const f32_t visualYawOffset = ResolveEntityVisualYawOffset(world, entity);
        const f32_t resolvedYaw =
            ResolveVisualYawNear(direction, rot.y, visualYawOffset);
        transform.SetRotation({
            rot.x,
            resolvedYaw,
            rot.z
            });

        static u32_t s_rotateYawTraceCount = 0;
        const f32_t yawDelta = NormalizeChampionVisualYaw(resolvedYaw - rot.y);
        if ((std::fabs(yawDelta) > 0.75f ||
                std::fabs(std::fabs(yawDelta) - WintersMath::kPi) <= 0.35f) &&
            s_rotateYawTraceCount < 512u)
        {
            const Vec3 prevForward = ForwardFromYaw(rot.y, visualYawOffset);
            const Vec3 nextForward = ForwardFromYaw(resolvedYaw, visualYawOffset);
            const Vec3 normalizedDirection =
                WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);
            const f32_t prevVsDirDot =
                prevForward.x * normalizedDirection.x + prevForward.z * normalizedDirection.z;
            const f32_t nextVsDirDot =
                nextForward.x * normalizedDirection.x + nextForward.z * normalizedDirection.z;
            char msg[640]{};
            sprintf_s(
                msg,
                "[YawTrace][ServerRotate] entity=%u champion=%u dir=(%.3f,%.3f) prevYaw=%.4f newYaw=%.4f yawDelta=%.4f halfTurn=%u prevF=(%.3f,%.3f) newF=(%.3f,%.3f) prevVsDirDot=%.4f newVsDirDot=%.4f offset=%.4f\n",
                static_cast<u32_t>(entity),
                static_cast<u32_t>(champion),
                normalizedDirection.x,
                normalizedDirection.z,
                rot.y,
                resolvedYaw,
                yawDelta,
                (std::fabs(std::fabs(yawDelta) - WintersMath::kPi) <= 0.35f) ? 1u : 0u,
                prevForward.x,
                prevForward.z,
                nextForward.x,
                nextForward.z,
                prevVsDirDot,
                nextVsDirDot,
                visualYawOffset);
            OutputCommandDebug(msg);
            ++s_rotateYawTraceCount;
        }
    }

    bool_t IsFacingCandIDateOpposedToIntent(
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

    Vec3 ResolveMoveFacingTarget(
        const Vec3& origin,
        const Vec3& rawTarget,
        const Vec3& resolvedTarget)
    {
        if (WintersMath::DistanceSqXZ(origin, rawTarget) > 0.0001f)
            return rawTarget;

        return resolvedTarget;
    }

    Vec3 ResolveMoveFacingDirection(const Vec3& commandDirection)
    {
        const Vec3 clientIntentDirection =
            WintersMath::NormalizeXZ(commandDirection, Vec3{}, 0.0001f);
        return clientIntentDirection;
    }

    bool_t IsServerProjectileSkill(eChampion champion, u8_t slot)
    {
        return (champion == eChampion::EZREAL ||
            champion == eChampion::LEESIN) &&
            slot == static_cast<u8_t>(eSkillSlot::Q);
    }

    GameCommand ResolveServerOwnedSkillCommand(CWorld& world, const GameCommand& cmd,
        eChampion champion)
    {
        GameCommand resolved = cmd;

        if ((champion != eChampion::EZREAL && champion != eChampion::LEESIN) ||
            cmd.slot != static_cast<u8_t>(eSkillSlot::Q))
        {
            return resolved;
        }

        if (champion == eChampion::LEESIN && cmd.itemId == 2u)
            return resolved;

        if (!world.HasComponent<TransformComponent>(cmd.issuerEntity))
            return resolved;

        const Vec3 direction = ResolveProjectileDirection(
            world, cmd.issuerEntity, NULL_ENTITY, cmd.direction);

        resolved.direction = direction;
        resolved.targetEntity = NULL_ENTITY;

        return resolved;
    }

    EntityID SpawnServerSkillProjectile(
        CWorld& world,
        const GameCommand& cmd,
        eChampion champion,
        u8_t rank)
    {
        if (!IsServerProjectileSkill(champion, cmd.slot))
            return NULL_ENTITY;
        if (!world.HasComponent<TransformComponent>(cmd.issuerEntity))
            return NULL_ENTITY;

        Vec3 origin = world.GetComponent<TransformComponent>(cmd.issuerEntity).GetPosition();
        origin.y += 1.0f;

        f32_t range = ChampionGameDataDB::ResolveSkillRange(champion, cmd.slot);
        if (range <= 0.f)
            range = 11.f;

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = cmd.issuerEntity;
        projectile.sourceTeam = ResolveTeam(world, cmd.issuerEntity);
        projectile.kind = champion == eChampion::LEESIN
            ? eProjectileKind::LeeSinQ
            : eProjectileKind::MysticShot;
        projectile.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(champion) << 8) | cmd.slot);
        projectile.rank = rank;
        projectile.currentPos = origin;
        projectile.direction = ResolveProjectileDirection(
            world, cmd.issuerEntity, NULL_ENTITY, cmd.direction);
        projectile.speed = champion == eChampion::LEESIN ? 24.f : 30.f;
        projectile.maxDistance = range;
        projectile.hitRadius = champion == eChampion::LEESIN ? 0.55f : 0.65f;
        projectile.damage = champion == eChampion::LEESIN
            ? 55.f + static_cast<f32_t>(rank - 1u) * 25.f
            : 70.f + static_cast<f32_t>(rank - 1u) * 25.f;

        const EntityID projectileEntity = world.CreateEntity();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);

        static u32_t s_projectileLogCount = 0;
        if (s_projectileLogCount < 64u)
        {
            char msg[256]{};
            sprintf_s(msg,
                "[SkillProjectile] queued kind=%u source=%u projectile=%u slot=%u damage=%.2f range=%.2f\n",
                static_cast<u32_t>(projectile.kind),
                static_cast<u32_t>(cmd.issuerEntity),
                static_cast<u32_t>(projectileEntity),
                static_cast<u32_t>(cmd.slot),
                projectile.damage,
                projectile.maxDistance);
            OutputCommandDebug(msg);
            ++s_projectileLogCount;
        }

        return projectileEntity;
    }

    void ApplyServerOwnedImmediateSkill(CWorld& world, const TickContext& tc, GameCommand& cmd,
        eChampion champion)
    {
        if (champion != eChampion::EZREAL ||
            cmd.slot != static_cast<u8_t>(eSkillSlot::E))
        {
            return;
        }

        if (!world.HasComponent<TransformComponent>(cmd.issuerEntity))
            return;

        const Vec3 direction = ResolveProjectileDirection(
            world, cmd.issuerEntity, NULL_ENTITY, cmd.direction);
        f32_t range = ChampionGameDataDB::ResolveSkillRange(champion, cmd.slot);
        if (range <= 0.f)
            range = 4.75f;

        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const Vec3 origin = transform.GetPosition();
        Vec3 dest{
            origin.x + direction.x * range,
            origin.y,
            origin.z + direction.z * range
        };
        if (tc.pWalkable)
        {
            Vec3 guardedDest = dest;
            if (tc.pWalkable->TryClampMoveSegmentXZ(origin, dest, GameplayStateQuery::ResolveGameplayRadius(world, cmd.issuerEntity), guardedDest))
            {
                f32_t surfaceY = 0.f;
                if (tc.pWalkable->TrySampleHeight(guardedDest.x, guardedDest.z, surfaceY))
                    guardedDest.y = surfaceY;
                dest = guardedDest;
            }
            else
            {
                dest = origin;
            }
        }

        transform.SetPosition(dest);
        transform.m_bLocalDirty = true;
        transform.m_bWorldDirty = true;

        cmd.direction = direction;
        cmd.groundPos = dest;
    }

    void LogBasicAttackReject(const char* reason, const GameCommand& cmd)
    {
        static u32_t s_rejectLogCount = 0;
        if (s_rejectLogCount >= 32u)
            return;

        char msg[192]{};
        sprintf_s(msg,
            "[Command] basic-attack reject reason=%s issuer=%u target=%u seq=%u\n",
            reason ? reason : "-",
            static_cast<u32_t>(cmd.issuerEntity),
            static_cast<u32_t>(cmd.targetEntity),
            cmd.sequenceNum);
        OutputCommandDebug(msg);
        ++s_rejectLogCount;
    }

    void LogBasicAttackAccept(const GameCommand& cmd, f32_t damage, f32_t cooldown)
    {
        static u32_t s_acceptLogCount = 0;
        if (s_acceptLogCount >= 64u)
            return;

        char msg[192]{};
        sprintf_s(msg,
            "[Command] basic-attack accept issuer=%u target=%u seq=%u damage=%.2f cooldown=%.2f\n",
            static_cast<u32_t>(cmd.issuerEntity),
            static_cast<u32_t>(cmd.targetEntity),
            cmd.sequenceNum,
            damage,
            cooldown);
        OutputCommandDebug(msg);
        ++s_acceptLogCount;
    }

    void LogCastSkill(const char* state, const char* reason, const GameCommand& cmd,
        eChampion champion, f32_t cooldown)
    {
        static u32_t s_castLogCount = 0;
        if (s_castLogCount >= 64u)
            return;

        char msg[240]{};
        sprintf_s(msg,
            "[Command] cast-skill %s reason=%s champion=%u issuer=%u target=%u slot=%u seq=%u cooldown=%.2f\n",
            state ? state : "-",
            reason ? reason : "-",
            static_cast<u32_t>(champion),
            static_cast<u32_t>(cmd.issuerEntity),
            static_cast<u32_t>(cmd.targetEntity),
            static_cast<u32_t>(cmd.slot),
            cmd.sequenceNum,
            cooldown);
        OutputCommandDebug(msg);
        ++s_castLogCount;
    }
    //Viego Getting Soul Component
    bool_t TryHandleViegoSoulBasicAttack(CWorld& world, const TickContext& tc,
        const GameCommand& cmd)
    {
        if (!world.HasComponent<ViegoSoulComponent>(cmd.targetEntity))
            return false;

        const auto& soul = world.GetComponent<ViegoSoulComponent>(cmd.targetEntity);
        if (!world.HasComponent<ChampionComponent>(cmd.issuerEntity))
        {
            MSG_BOX("비에고 소울 없음!");
            return true;
        }

        const auto& issuerChampion = world.GetComponent<ChampionComponent>(cmd.issuerEntity);
        if (issuerChampion.id != eChampion::VIEGO || issuerChampion.team != soul.eligibleTeam)
        {
            MSG_BOX("비에고 소울 없음!");
            return true;
        }

        constexpr f32_t kConsumeRange = 2.25f;

        const f32_t effectiveRange =
            kConsumeRange + GameplayStateQuery::ResolveGameplayRadius(world, cmd.issuerEntity) +
            GameplayStateQuery::ResolveGameplayRadius(world, cmd.targetEntity);
        //Argument로 entity를 받기 때문에 winters math를 못 씀!! -> 따로 빼야 하는 걸까?
        if (DistanceSqXZ(world, cmd.issuerEntity, cmd.targetEntity) >
            effectiveRange * effectiveRange)
        {
            StartAttackChase(world, tc, cmd, effectiveRange);
            return true;
        }

        ClearAttackChase(world, cmd.issuerEntity);
        ClearMoveTarget(world, cmd.issuerEntity);
        ClearCombatAction(world, cmd.issuerEntity);

        auto& viego = world.HasComponent<ViegoSimComponent>(cmd.issuerEntity)
            ? world.GetComponent<ViegoSimComponent>(cmd.issuerEntity)
            : world.AddComponent<ViegoSimComponent>(cmd.issuerEntity, ViegoSimComponent{});
        viego.bPossessionActive = true;
        viego.possessedTarget = soul.deadChampion;
        viego.possessionDurationSec = 5.f;
        viego.possessionTimerSec = 5.f;

        FormOverrideComponent form{};
        form.visualChampion = soul.champion;
        form.skillChampion = soul.champion;
        form.skillSlotMask = FormOverrideComponent{}.skillSlotMask;
        form.fRemainingSec = 5.f;
        form.bActive = true;

        if (world.HasComponent<FormOverrideComponent>(cmd.issuerEntity))
            world.GetComponent<FormOverrideComponent>(cmd.issuerEntity) = form;
        else
            world.AddComponent<FormOverrideComponent>(cmd.issuerEntity, form);

        StartNetAnimation(world, cmd.issuerEntity,
            eNetAnimId::ViegoConsumeSoul, tc, EncodeSkillPlaybackRateQ8(1.f));

        world.DestroyEntity(cmd.targetEntity);
        return true;
    }

    ChampionAITuningParam* ResolveChampionAITuningParamById(
        ChampionAIComponent& ai,
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

    void ApplyChampionAITuningOverride(
        ChampionAIComponent& ai,
        eChampionAITuningId tuningId,
        f32_t value)
    {
        ChampionAITuningParam* pParam =
            ResolveChampionAITuningParamById(ai, tuningId);
        if (!pParam)
            return;

        pParam->fCurrent = std::clamp(value, pParam->fMin, pParam->fMax);
        pParam->bOverride = true;
        ai.tuning.bOverrideProfile = true;
    }
}

std::unique_ptr<CDefaultCommandExecutor> CDefaultCommandExecutor::Create()
{
    return std::unique_ptr<CDefaultCommandExecutor>(new CDefaultCommandExecutor());
}

void CDefaultCommandExecutor::ExecuteCommand(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (cmd.issuerEntity == NULL_ENTITY || !world.IsAlive(cmd.issuerEntity))
        return;

    if (!IsAliveForCommand(world, cmd.issuerEntity))
        return;

    switch (cmd.kind)
    {
    case eCommandKind::Move:
        HandleMove(world, tc, cmd);
        break;
    case eCommandKind::CastSkill:
        HandleCastSkill(world, tc, cmd);
        break;
    case eCommandKind::BasicAttack:
        HandleBasicAttack(world, tc, cmd);
        break;
    case eCommandKind::LevelSkill:
        HandleLevelSkill(world, tc, cmd);
        break;
    case eCommandKind::BuyItem:
        HandleBuyItem(world, tc, cmd);
        break;
    case eCommandKind::Recall:
        HandleRecall(world, tc, cmd);
        break;
    case eCommandKind::RecallCancel:
        HandleRecallCancel(world, tc, cmd);
        break;
    case eCommandKind::AIDebugControl:
        HandleAIDebugControl(world, tc, cmd);
        break;
    case eCommandKind::Flash:
        HandleFlash(world, tc, cmd);
        break;
    default:
        break;
    }
}

void CDefaultCommandExecutor::HandleMove(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (!IsFiniteMoveTarget(cmd.groundPos))
    {
        static u32_t s_rejectLogCount = 0;
        if (s_rejectLogCount < 16u)
        {
            char msg[160]{};
            sprintf_s(msg,
                "[Command] move reject reason=invalid-pos issuer=%u seq=%u\n",
                static_cast<u32_t>(cmd.issuerEntity),
                cmd.sequenceNum);
            OutputCommandDebug(msg);
            ++s_rejectLogCount;
        }
        return;
    }

    if (!GameplayStateQuery::CanMove(world, cmd.issuerEntity))
        return;

    CancelRecall(world, cmd.issuerEntity);

    ClearAttackChase(world, cmd.issuerEntity);

    if (TryConsumeKalistaPassiveDashMove(world, tc, cmd))
        return;

    if (ConsumeMoveForCombatAction(world, tc, cmd))
        return;

    if (!world.HasComponent<MoveTargetComponent>(cmd.issuerEntity))
        world.AddComponent<MoveTargetComponent>(cmd.issuerEntity, MoveTargetComponent{});

    auto& moveTarget = world.GetComponent<MoveTargetComponent>(cmd.issuerEntity);
    moveTarget.arriveRadius = MoveTargetComponent{}.arriveRadius;
    Vec3 target = cmd.groundPos;

    if (world.HasComponent<TransformComponent>(cmd.issuerEntity))
    {
        const Vec3 pos = world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition();
        target.y = pos.y;

        if (!TryAssignGridMovePath(tc, pos, target, moveTarget))
        {
            static u32_t s_navRejectLogCount = 0;
            if (s_navRejectLogCount < 32u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[Command] move reject reason=no-grid-path issuer=%u seq=%u target=(%.2f,%.2f,%.2f)\n",
                    static_cast<u32_t>(cmd.issuerEntity),
                    cmd.sequenceNum,
                    target.x,
                    target.y,
                    target.z);
                OutputCommandDebug(msg);
                ++s_navRejectLogCount;
            }
            moveTarget.bHasTarget = false;
            return;
        }

        if (WintersMath::DistanceSqXZ(pos, target) <= moveTarget.arriveRadius * moveTarget.arriveRadius)
        {
            moveTarget.bHasTarget = false;
            ClearMovePath(moveTarget);
            return;
        }

        const Vec3 facingTarget = ResolveMoveFacingTarget(
            pos,
            cmd.groundPos,
            target);
        const Vec3 facingDirection =
            ResolveMoveFacingDirection(cmd.direction);
        SetMoveFacingOverride(
            moveTarget,
            facingTarget,
            facingDirection,
            cmd.sequenceNum);
        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const f32_t yawBefore = transform.GetRotation().y;
        const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
        const f32_t yawOffset = ChampionGameDataDB::ResolveVisualYawOffset(champion);
        const bool_t bHasFacingDirection =
            facingDirection.x != 0.f || facingDirection.z != 0.f;
        const f32_t yawFromFacing = bHasFacingDirection
            ? ResolveChampionVisualYawFromDirection(champion, facingDirection)
            : yawBefore;
        const Vec3 clientIntentDirection =
            WintersMath::NormalizeXZ(cmd.direction, Vec3{}, 0.0001f);
        const bool_t bHasClientIntentDirection =
            clientIntentDirection.x != 0.f || clientIntentDirection.z != 0.f;
        const Vec3 rawDirection =
            WintersMath::DirectionXZ(pos, cmd.groundPos, Vec3{});
        const Vec3 resolvedDirection =
            WintersMath::DirectionXZ(pos, target, Vec3{});
        const Vec3 firstWaypoint = moveTarget.pathCount > 0
            ? moveTarget.pathWaypoints[0]
            : Vec3{};
        const Vec3 path0Direction = moveTarget.pathCount > 0
            ? WintersMath::DirectionXZ(pos, firstWaypoint, Vec3{})
            : Vec3{};
        const f32_t rawYaw = (rawDirection.x != 0.f || rawDirection.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, rawDirection)
            : yawBefore;
        const f32_t resolvedYaw = (resolvedDirection.x != 0.f || resolvedDirection.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, resolvedDirection)
            : yawBefore;
        const f32_t path0Yaw = (path0Direction.x != 0.f || path0Direction.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, path0Direction)
            : yawBefore;
        const f32_t cmdVsRawDot =
            clientIntentDirection.x * rawDirection.x +
            clientIntentDirection.z * rawDirection.z;
        const f32_t resolvedVsRawDot =
            resolvedDirection.x * rawDirection.x +
            resolvedDirection.z * rawDirection.z;
        const f32_t path0VsRawDot =
            path0Direction.x * rawDirection.x +
            path0Direction.z * rawDirection.z;
        const bool_t bCommandOpposesRaw =
            (clientIntentDirection.x != 0.f || clientIntentDirection.z != 0.f) &&
            (rawDirection.x != 0.f || rawDirection.z != 0.f) &&
            cmdVsRawDot < -0.10f;
        const bool_t bResolvedOpposesRaw =
            (resolvedDirection.x != 0.f || resolvedDirection.z != 0.f) &&
            (rawDirection.x != 0.f || rawDirection.z != 0.f) &&
            resolvedVsRawDot < -0.10f;
        const bool_t bPath0OpposesRaw =
            (path0Direction.x != 0.f || path0Direction.z != 0.f) &&
            (rawDirection.x != 0.f || rawDirection.z != 0.f) &&
            path0VsRawDot < -0.10f;
        const bool_t bFirstWaypointOpposed =
            moveTarget.pathCount > 0 &&
            IsFacingCandIDateOpposedToIntent(pos, cmd.groundPos, firstWaypoint);
        static u32_t s_moveYawTraceCount = 0;
        if (s_moveYawTraceCount < 512u)
        {
            const f32_t yawAfter = transform.GetRotation().y;
            const f32_t yawDelta = NormalizeChampionVisualYaw(yawAfter - yawBefore);
            const f32_t calcVsPrev = NormalizeChampionVisualYaw(yawFromFacing - yawBefore);
            const Vec3 prevForward = ForwardFromYaw(yawBefore, champion);
            const f32_t prevVsFacingDot =
                prevForward.x * facingDirection.x + prevForward.z * facingDirection.z;
            const bool_t bCalcHalfTurn =
                std::fabs(std::fabs(calcVsPrev) - WintersMath::kPi) <= 0.35f;
            char msg[1536]{};
            sprintf_s(
                msg,
                "[YawTrace][ServerCommand] tick=%llu seq=%u entity=%u champion=%u yawOffset=%.4f facingSource=%s cmdDir=(%.3f,%.3f) facingDir=(%.3f,%.3f) lockTicks=%u pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) pathCount=%u path0=(%.3f,%.3f,%.3f) path0Opposed=%u facing=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) resolvedDir=(%.3f,%.3f) path0Dir=(%.3f,%.3f) yawBefore=%.4f yawCalc=%.4f rawYaw=%.4f resolvedYaw=%.4f path0Yaw=%.4f yawAfter=%.4f yawDelta=%.4f calcVsPrev=%.4f calcHalfTurn=%u prevVsFacingDot=%.4f calcVsRaw=%.4f calcVsResolved=%.4f calcVsPath0=%.4f cmdVsRawDot=%.4f resolvedVsRawDot=%.4f path0VsRawDot=%.4f cmdOppRaw=%u resolvedOppRaw=%u path0OppRaw=%u\n",
                static_cast<unsigned long long>(tc.tickIndex),
                cmd.sequenceNum,
                static_cast<u32_t>(cmd.issuerEntity),
                static_cast<u32_t>(champion),
                yawOffset,
                bHasClientIntentDirection ? "client-dir" : "server-pos",
                clientIntentDirection.x,
                clientIntentDirection.z,
                facingDirection.x,
                facingDirection.z,
                static_cast<u32_t>(moveTarget.facingLockTicks),
                pos.x,
                pos.y,
                pos.z,
                cmd.groundPos.x,
                cmd.groundPos.y,
                cmd.groundPos.z,
                target.x,
                target.y,
                target.z,
                static_cast<u32_t>(moveTarget.pathCount),
                firstWaypoint.x,
                firstWaypoint.y,
                firstWaypoint.z,
                bFirstWaypointOpposed ? 1u : 0u,
                facingTarget.x,
                facingTarget.y,
                facingTarget.z,
                rawDirection.x,
                rawDirection.z,
                resolvedDirection.x,
                resolvedDirection.z,
                path0Direction.x,
                path0Direction.z,
                yawBefore,
                yawFromFacing,
                rawYaw,
                resolvedYaw,
                path0Yaw,
                yawAfter,
                yawDelta,
                calcVsPrev,
                bCalcHalfTurn ? 1u : 0u,
                prevVsFacingDot,
                NormalizeChampionVisualYaw(yawFromFacing - rawYaw),
                NormalizeChampionVisualYaw(yawFromFacing - resolvedYaw),
                NormalizeChampionVisualYaw(yawFromFacing - path0Yaw),
                cmdVsRawDot,
                resolvedVsRawDot,
                path0VsRawDot,
                bCommandOpposesRaw ? 1u : 0u,
                bResolvedOpposesRaw ? 1u : 0u,
                bPath0OpposesRaw ? 1u : 0u);
            OutputCommandDebug(msg);
            ++s_moveYawTraceCount;
        }
    }

    moveTarget.target = target;
    moveTarget.bHasTarget = true;
}

void CDefaultCommandExecutor::HandleCastSkill(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (!world.HasComponent<SkillStateComponent>(cmd.issuerEntity))
    {
        LogCastSkill("reject", "no-skill-state", cmd, ResolveChampion(world, cmd.issuerEntity), 0.f);
        return;
    }
    if (cmd.slot >= 5)
    {
        LogCastSkill("reject", "invalid-slot", cmd, ResolveChampion(world, cmd.issuerEntity), 0.f);
        return;
    }
    if (!GameplayStateQuery::CanCast(world, cmd.issuerEntity))
    {
        LogCastSkill("reject", "state-blocked", cmd, ResolveChampion(world, cmd.issuerEntity), 0.f);
        return;
    }
    if (cmd.targetEntity != NULL_ENTITY &&
        !IsAliveForCommand(world, cmd.targetEntity))
    {
        LogCastSkill("reject", "dead-target", cmd, ResolveChampion(world, cmd.issuerEntity), 0.f);
        return;
    }
    if (cmd.targetEntity != NULL_ENTITY &&
        !GameplayStateQuery::CanBeTargetedBy(world, cmd.issuerEntity, cmd.targetEntity))
    {
        LogCastSkill("reject", "untargetable", cmd, ResolveChampion(world, cmd.issuerEntity), 0.f);
        return;
    }

    auto& skillState = world.GetComponent<SkillStateComponent>(cmd.issuerEntity);
    const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
    const SkillOverrideResolveResult skillIdentity =
        CSpellbookFormOverrideSystem::ResolveSkill(
            world,
            cmd.issuerEntity,
            champion,
            cmd.slot);
    auto& slot = skillState.slots[skillIdentity.localSlot];
    const eChampion hookChampion = skillIdentity.hookChampion;
    const u8_t hookSlot = skillIdentity.hookSlot;
    const bool_t bRequestedStage2 = cmd.itemId == 2u;

    if (!IsSkillLearned(world, cmd.issuerEntity, skillIdentity.localSlot))
    {
        LogCastSkill("reject", "unlearned", cmd, hookChampion, 0.f);
        return;
    }

    const bool_t bStage2 =
        bRequestedStage2 &&
        slot.currentStage == 1 &&
        slot.stageWindow > 0.f &&
        ChampionGameDataDB::IsSkillTwoStage(hookChampion, hookSlot);
    u8_t skillStage = bStage2 ? 2u : 1u;
    if (!bStage2 &&
        hookChampion == eChampion::YASUO &&
        hookSlot == static_cast<u8_t>(eSkillSlot::Q))
    {
        skillStage = YasuoGameSim::ResolveQVariantStage(world, cmd.issuerEntity);
    }
    if (!bStage2 &&
        hookChampion == eChampion::YONE &&
        hookSlot == static_cast<u8_t>(eSkillSlot::E))
    {
        skillStage = YoneGameSim::ResolveEStage(world, cmd.issuerEntity);
    }

    GameCommand effectiveCmd = cmd;
    if (!bStage2 &&
        hookChampion == eChampion::YASUO &&
        hookSlot == static_cast<u8_t>(eSkillSlot::R))
    {
        const f32_t searchRadius = ChampionGameDataDB::ResolveSkillRange(hookChampion, hookSlot);
        const EntityID airborneTarget = YasuoGameSim::FindAirborneTarget(
            world,
            cmd.issuerEntity,
            ResolveTeam(world, cmd.issuerEntity),
            searchRadius > 0.f ? searchRadius : 14.f);
        if (airborneTarget == NULL_ENTITY)
        {
            LogCastSkill("reject", "no-airborne", cmd, hookChampion, 0.f);
            return;
        }
        if (!GameplayStateQuery::CanBeTargetedBy(world, cmd.issuerEntity, airborneTarget))
        {
            LogCastSkill("reject", "untargetable", cmd, hookChampion, 0.f);
            return;
        }

        effectiveCmd.targetEntity = airborneTarget;
    }

    if (bRequestedStage2 && !bStage2)
    {
        LogCastSkill("reject", "stage2-window", cmd, hookChampion, slot.stageWindow);
        return;
    }

    const bool_t bSylasHijackCapture =
        champion == eChampion::SYLAS &&
        cmd.slot == static_cast<u8_t>(eSkillSlot::R) &&
        hookChampion == eChampion::SYLAS &&
        hookSlot == static_cast<u8_t>(eSkillSlot::R) &&
        !world.HasComponent<SpellbookOverrideComponent>(cmd.issuerEntity);
    if (bSylasHijackCapture &&
        !SylasGameSim::CanHijackUltimate(world, cmd.issuerEntity, effectiveCmd.targetEntity))
    {
        LogCastSkill("reject", "no-hijack-target", cmd, hookChampion, 0.f);
        return;
    }

    if (!bStage2 && slot.cooldownRemaining > 0.f)
    {
        LogCastSkill("reject", "cooldown", cmd, hookChampion, slot.cooldownRemaining);
        return;
    }

    if (!bStage2 &&
        hookChampion == eChampion::ANNIE &&
        hookSlot == static_cast<u8_t>(eSkillSlot::Q) &&
        effectiveCmd.targetEntity != NULL_ENTITY &&
        world.HasComponent<TransformComponent>(effectiveCmd.issuerEntity) &&
        world.HasComponent<TransformComponent>(effectiveCmd.targetEntity))
    {
        f32_t range = ChampionGameDataDB::ResolveSkillRange(hookChampion, hookSlot);
        if (range <= 0.f)
            range = 6.25f;

        const f32_t effectiveRange =
            range +
            GameplayStateQuery::ResolveGameplayRadius(world, effectiveCmd.issuerEntity) +
            GameplayStateQuery::ResolveGameplayRadius(world, effectiveCmd.targetEntity);
        if (DistanceSqXZ(world, effectiveCmd.issuerEntity, effectiveCmd.targetEntity) >
            effectiveRange * effectiveRange)
        {
            GameCommand chaseCmd = effectiveCmd;
            chaseCmd.slot = hookSlot;
            StartAttackChase(world, tc, chaseCmd, effectiveRange);
            return;
        }
    }

    ClearAttackChase(world, cmd.issuerEntity);

    const f32_t cooldown = ResolveCastSkillCooldown(
        world,
        cmd.issuerEntity,
        skillIdentity.cooldownChampion,
        skillIdentity.cooldownSlot);
    if (bStage2)
    {
        slot.currentStage = 0;
        slot.stageWindow = 0.f;
    }
    else if (!bSylasHijackCapture)
    {
        slot.cooldownRemaining = cooldown;
        slot.cooldownDuration = cooldown;
        if (ChampionGameDataDB::IsSkillTwoStage(hookChampion, hookSlot))
        {
            slot.currentStage = 1;
            slot.stageWindow = ChampionGameDataDB::ResolveSkillStageWindowSec(hookChampion, hookSlot);
        }
    }
    else
    {
        slot.cooldownRemaining = 0.f;
        slot.cooldownDuration = 0.f;
    }

    if (skillIdentity.bConsumeSpellbookOnAccept)
    {
        CSpellbookFormOverrideSystem::ConsumeSpellbookOverride(
            world,
            cmd.issuerEntity,
            skillIdentity.localSlot);
    }

    LogCastSkill("accept", bStage2 ? "stage2" : "ok", effectiveCmd, hookChampion, cooldown);
    ClearMoveTarget(world, effectiveCmd.issuerEntity);

    const ChampionSkillTimingDefaults timing =
        ChampionGameDataDB::ResolveSkillTiming(hookChampion, hookSlot, skillStage);
    if (!ShouldSuppressCastNetAnimation(hookChampion, hookSlot, skillStage))
    {
        StartNetAnimation(
            world,
            effectiveCmd.issuerEntity,
            SkillSlotToAnim(effectiveCmd.slot),
            tc,
            EncodeSkillPlaybackRateQ8(timing.animPlaySpeed),
            skillStage,
            ShouldLoopSkillNetAnimation(hookChampion, hookSlot, skillStage));
    }

    u8_t rank = ResolveSkillRank(world, effectiveCmd.issuerEntity, skillIdentity.localSlot);
    if (skillIdentity.sourceRank > 0u)
        rank = skillIdentity.sourceRank;
    GameCommand hookCmd = effectiveCmd;
    hookCmd.slot = hookSlot;
    GameCommand resolvedCmd = ResolveServerOwnedSkillCommand(world, hookCmd, hookChampion);
    ApplyServerOwnedImmediateSkill(world, tc, resolvedCmd, hookChampion);
    const Vec3 facingDir = ResolveCastFacingDirection(world, resolvedCmd);
    if (facingDir.x != 0.f || facingDir.z != 0.f)
    {
        RotateEntityTowardDirection(world, resolvedCmd.issuerEntity, facingDir);
        if (resolvedCmd.direction.x == 0.f &&
            resolvedCmd.direction.y == 0.f &&
            resolvedCmd.direction.z == 0.f)
        {
            resolvedCmd.direction = facingDir;
        }
    }
    if (hookChampion == eChampion::KALISTA &&
        resolvedCmd.slot == static_cast<u8_t>(eSkillSlot::Q))
    {
        ArmKalistaPassiveDashWindow(
            world,
            resolvedCmd.issuerEntity,
            resolvedCmd.slot,
            resolvedCmd.direction);
    }
    const u32_t primaryHookId = BuildPrimarySkillHookId(hookChampion, hookSlot);
    const u32_t effectId = (primaryHookId != 0)
        ? primaryHookId
        : BuildGenericEffectId(world, cmd.issuerEntity, cmd.slot);
    const Vec3 eventPos = ResolveEventPosition(
        world, resolvedCmd.issuerEntity, resolvedCmd.targetEntity, resolvedCmd.groundPos);

    const bool_t bServerProjectileSkill =
        IsServerProjectileSkill(hookChampion, resolvedCmd.slot) &&
        !(hookChampion == eChampion::LEESIN &&
            resolvedCmd.slot == static_cast<u8_t>(eSkillSlot::Q) &&
            skillStage >= 2u);
    const bool_t bGameplayHookHandled =
        !bServerProjectileSkill &&
        DispatchGameplayHookIfAvailable(
            world, tc, resolvedCmd, primaryHookId, hookChampion, rank);
    if (!bServerProjectileSkill && !bGameplayHookHandled)
        EnqueueFallbackSkillDamage(world, resolvedCmd, hookChampion, rank);

    ReplicatedEventComponent castEvent{};
    castEvent.kind = eReplicatedEventKind::SkillCast;
    castEvent.sourceEntity = resolvedCmd.issuerEntity;
    castEvent.targetEntity = resolvedCmd.targetEntity;
    castEvent.slot = skillIdentity.localSlot;
    castEvent.rank = rank;
    castEvent.position = eventPos;
    castEvent.direction = resolvedCmd.direction;
    castEvent.startTick = tc.tickIndex;
    EnqueueReplicatedEvent(world, castEvent);

    ReplicatedEventComponent effectEvent{};
    effectEvent.kind = eReplicatedEventKind::EffectTrigger;
    effectEvent.sourceEntity = resolvedCmd.issuerEntity;
    effectEvent.targetEntity = resolvedCmd.targetEntity;
    effectEvent.effectId = effectId;
    effectEvent.slot = skillIdentity.localSlot;
    effectEvent.rank = rank;
    effectEvent.flags = static_cast<u16_t>(
        (static_cast<u16_t>(skillStage & 0x0fu) << 12) |
        (static_cast<u16_t>(rank & 0x0fu) << 8) |
        static_cast<u16_t>(skillIdentity.localSlot));
    effectEvent.position = eventPos;
    effectEvent.direction = resolvedCmd.direction;
    effectEvent.durationMs = ResolveSkillEffectDurationMs(hookChampion, hookSlot);
    effectEvent.startTick = tc.tickIndex;
    EnqueueReplicatedEvent(world, effectEvent);

#if defined(_DEBUG)
    if (hookChampion == eChampion::IRELIA)
    {
        static u32_t s_ireliaCueTraceCount = 0;
        if (s_ireliaCueTraceCount < 64u)
        {
            char msg[320]{};
            sprintf_s(msg,
                "[IreliaReplayCue][Server] slot=%u stage=%u rank=%u effect=0x%08X issuer=%u target=%u pos=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f)\n",
                static_cast<u32_t>(hookSlot),
                static_cast<u32_t>(skillStage),
                static_cast<u32_t>(rank),
                effectId,
                static_cast<u32_t>(resolvedCmd.issuerEntity),
                static_cast<u32_t>(resolvedCmd.targetEntity),
                effectEvent.position.x,
                effectEvent.position.y,
                effectEvent.position.z,
                effectEvent.direction.x,
                effectEvent.direction.y,
                effectEvent.direction.z);
            OutputCommandDebug(msg);
            ++s_ireliaCueTraceCount;
        }
    }
#endif

    if (bServerProjectileSkill)
        SpawnServerSkillProjectile(world, resolvedCmd, hookChampion, rank);

    const bool_t bShouldEmitGenericProjectile =
        resolvedCmd.slot != static_cast<u8_t>(eSkillSlot::BasicAttack) &&
        primaryHookId == 0 &&
        !bServerProjectileSkill;

    if (bShouldEmitGenericProjectile)
    {
        Vec3 projectilePos = ResolveEventPosition(
            world, resolvedCmd.issuerEntity, NULL_ENTITY, resolvedCmd.groundPos);
        projectilePos.y += 1.2f;

        ReplicatedEventComponent projectileEvent{};
        projectileEvent.kind = eReplicatedEventKind::ProjectileSpawn;
        projectileEvent.sourceEntity = resolvedCmd.issuerEntity;
        projectileEvent.targetEntity = resolvedCmd.targetEntity;
        projectileEvent.projectileKind = static_cast<u16_t>(effectId & 0xffffu);
        projectileEvent.position = projectilePos;
        projectileEvent.direction = ResolveProjectileDirection(
            world, resolvedCmd.issuerEntity, resolvedCmd.targetEntity, resolvedCmd.direction);
        projectileEvent.speed = 18.f;
        projectileEvent.maxDistance = ChampionGameDataDB::ResolveSkillRange(hookChampion, resolvedCmd.slot);
        if (projectileEvent.maxDistance <= 0.f)
            projectileEvent.maxDistance = 12.f;
        projectileEvent.startTick = tc.tickIndex;
        EnqueueReplicatedEvent(world, projectileEvent);
    }
}

void CDefaultCommandExecutor::HandleBasicAttack(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (!GameplayStateQuery::CanAttack(world, cmd.issuerEntity))
    {
        LogBasicAttackReject("state-blocked", cmd);
        return;
    }

    if (TryConsumeKalistaPassiveDashMove(world, tc, cmd))
        return;

    if (cmd.targetEntity == NULL_ENTITY || !world.IsAlive(cmd.targetEntity))
    {
        LogBasicAttackReject("invalid-target", cmd);
        return;
    }
    //Viego Soul Gain
    if (TryHandleViegoSoulBasicAttack(world, tc, cmd))
        return;

    if (!world.HasComponent<HealthComponent>(cmd.targetEntity))
    {
        LogBasicAttackReject("target-has-no-health", cmd);
        return;
    }
    if (!IsAliveForCommand(world, cmd.targetEntity))
    {
        LogBasicAttackReject("dead-target", cmd);
        return;
    }
    if (!GameplayStateQuery::CanBeTargetedBy(world, cmd.issuerEntity, cmd.targetEntity))
    {
        LogBasicAttackReject("untargetable", cmd);
        return;
    }

    const eTeam sourceTeam = ResolveTeam(world, cmd.issuerEntity);
    const eTeam targetTeam = ResolveTeam(world, cmd.targetEntity);

    if (sourceTeam == targetTeam && sourceTeam != eTeam::Neutral)
    {
        LogBasicAttackReject("same-team", cmd);
        return;
    }

    if (world.HasComponent<SkillStateComponent>(cmd.issuerEntity))
    {
        const auto& skillState = world.GetComponent<SkillStateComponent>(cmd.issuerEntity);
        if (skillState.slots[0].cooldownRemaining > 0.f)
        {
            LogBasicAttackReject("cooldown", cmd);
            return;
        }
    }

    f32_t range = 5.5f;
    f32_t damage = 55.f;

    if (world.HasComponent<StatComponent>(cmd.issuerEntity))
    {
        const auto& stat = world.GetComponent<StatComponent>(cmd.issuerEntity);
        if (stat.attackRange > 0.f)
            range = stat.attackRange;
        if (stat.ad > 0.f)
            damage = stat.ad;
    }

    if (!world.HasComponent<TransformComponent>(cmd.issuerEntity) ||
        !world.HasComponent<TransformComponent>(cmd.targetEntity))
    {
        LogBasicAttackReject("missing-transform", cmd);
        return;
    }

    const f32_t effectiveRange =
        range +
        GameplayStateQuery::ResolveGameplayRadius(world, cmd.issuerEntity) +
        GameplayStateQuery::ResolveGameplayRadius(world, cmd.targetEntity);

    const f32_t rangeSq = effectiveRange * effectiveRange;
    if (DistanceSqXZ(world, cmd.issuerEntity, cmd.targetEntity) > rangeSq)
    {
        StartAttackChase(world, tc, cmd, effectiveRange);
        return;
    }

    const eChampion champion = ResolveChampion(world, cmd.issuerEntity);

    ClearAttackChase(world, cmd.issuerEntity);
    ClearMoveTarget(world, cmd.issuerEntity);

    auto& action = world.HasComponent<CombatActionComponent>(cmd.issuerEntity)
        ? world.GetComponent<CombatActionComponent>(cmd.issuerEntity)
        : world.AddComponent<CombatActionComponent>(cmd.issuerEntity, CombatActionComponent{});
    const bool_t bJaxEmpowerAttack =
        champion == eChampion::JAX &&
        JaxGameSim::TryConsumeEmpowerForBasicAttack(world, cmd.issuerEntity);
    const f32_t attackSpeedScale =
        ResolveBasicAttackAnimSpeedScale(world, cmd.issuerEntity);
    const ChampionBasicAttackTimingDefaults attackTiming =
        ChampionGameDataDB::ResolveBasicAttackTiming(champion);

    eNetAnimId attackAnimId = eNetAnimId::BasicAttack;
    f32_t attackActionDurationSec = attackTiming.fActionDurationSec;
    f32_t attackAnimPlaySpeed = attackTiming.fAnimPlaySpeed;

    if (bJaxEmpowerAttack)
    {
        const ChampionSkillTimingDefaults wTiming =
            ChampionGameDataDB::ResolveSkillTiming(champion, static_cast<u8_t>(eSkillSlot::W));
        attackAnimId = eNetAnimId::SkillW;
        attackActionDurationSec = wTiming.lockDurationSec;
        attackAnimPlaySpeed = wTiming.animPlaySpeed;
    }

    const u64_t actionTicks =
        ResolveScaledBasicAttackActionTicks(attackActionDurationSec, attackSpeedScale);
    const u16_t attackPlaybackRate =
        EncodeSkillPlaybackRateQ8(attackAnimPlaySpeed);

    action.eKind = eCombatActionKind::BasicAttack;
    action.eMovePolicy = ResolveBasicAttackMovePolicy(champion);
    action.uSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    action.uStage = 1;
    action.uFlags = bJaxEmpowerAttack ? CombatActionFlags::JaxEmpower : 0u;
    action.entityTarget = cmd.targetEntity;
    action.uSequenceNum = cmd.sequenceNum;
    action.uStartTick = tc.tickIndex;
    action.uImpactTick = tc.tickIndex + actionTicks;
    action.uEndTick = action.uImpactTick;
    action.bImpactIssued = false;
    action.bQueuedMove = false;
    action.vQueuedMoveTarget = {};
    action.vQueuedMoveDirection = {};

    RotateEntityTowardDirection(
        world,
        cmd.issuerEntity,
        ResolveCastFacingDirection(world, cmd));

    const f32_t cooldown = ResolveBasicAttackCooldown(
        world,
        cmd.issuerEntity,
        champion);

    if (world.HasComponent<SkillStateComponent>(cmd.issuerEntity))
    {
        auto& basicAttackSlot = world.GetComponent<SkillStateComponent>(cmd.issuerEntity).slots[0];
        basicAttackSlot.cooldownRemaining = cooldown;
        basicAttackSlot.cooldownDuration = cooldown;
    }

    LogBasicAttackAccept(cmd, damage, cooldown);

    const Vec3 attackDirection = ResolveProjectileDirection(
        world, cmd.issuerEntity, cmd.targetEntity, cmd.direction);

    StartNetAnimation(
        world,
        cmd.issuerEntity,
        attackAnimId,
        tc,
        attackPlaybackRate);

    ArmKalistaPassiveDashWindow(
        world,
        cmd.issuerEntity,
        static_cast<u8_t>(eSkillSlot::BasicAttack),
        attackDirection);
}

void CDefaultCommandExecutor::HandleLevelSkill(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;

    if (!world.HasComponent<SkillRankComponent>(cmd.issuerEntity))
        return;
    if (cmd.slot >= SkillRankComponent::kSlotCount)
        return;

    auto& rank = world.GetComponent<SkillRankComponent>(cmd.issuerEntity);
    const u8_t maxRank = (cmd.slot == 4)
        ? 3
        : ((cmd.slot >= 1 && cmd.slot <= 3) ? 5 : 0);
    if (maxRank == 0)
        return;
    if (rank.pointsAvailable == 0)
        return;
    if (rank.ranks[cmd.slot] >= maxRank)
        return;
    const u8_t championLevel = world.HasComponent<ChampionComponent>(cmd.issuerEntity)
        ? world.GetComponent<ChampionComponent>(cmd.issuerEntity).level
        : 1;
    const u8_t requiredLevel = (cmd.slot == 4)
        ? static_cast<u8_t>((rank.ranks[cmd.slot] == 0) ? 6 : ((rank.ranks[cmd.slot] == 1) ? 11 : 16))
        : static_cast<u8_t>(1 + rank.ranks[cmd.slot] * 2);
    if (championLevel < requiredLevel)
        return;

    ++rank.ranks[cmd.slot];
    --rank.pointsAvailable;
}

void CDefaultCommandExecutor::HandleBuyItem(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;

    if (cmd.issuerEntity == NULL_ENTITY || cmd.itemId == 0)
        return;

    if (!world.HasComponent<GoldComponent>(cmd.issuerEntity) ||
        !world.HasComponent<InventoryComponent>(cmd.issuerEntity) ||
        !world.HasComponent<StatComponent>(cmd.issuerEntity))
        return;

    const ItemDef* pItem = CItemRegistry::Instance().Find(cmd.itemId);
    if (!pItem)
        return;

    GoldComponent& gold = world.GetComponent<GoldComponent>(cmd.issuerEntity);
    InventoryComponent& inventory = world.GetComponent<InventoryComponent>(cmd.issuerEntity);
    StatComponent& stat = world.GetComponent<StatComponent>(cmd.issuerEntity);

    if (inventory.count >= InventoryComponent::kMaxSlots)
        return;
    if (gold.amount < pItem->price)
        return;

    gold.amount -= pItem->price;
    inventory.itemIds[inventory.count++] = pItem->itemId;
    stat.itemMaskHash ^= static_cast<u32_t>(pItem->itemId) * 16777619u;
    stat.bDirty = true;
}

void CDefaultCommandExecutor::HandleRecall(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (!world.HasComponent<RespawnComponent>(cmd.issuerEntity) ||
        !world.HasComponent<TransformComponent>(cmd.issuerEntity))
        return;

    if (world.HasComponent<HealthComponent>(cmd.issuerEntity))
    {
        const auto& health = world.GetComponent<HealthComponent>(cmd.issuerEntity);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return;
    }

    ClearMoveTarget(world, cmd.issuerEntity);
    ClearAttackChase(world, cmd.issuerEntity);

    RecallComponent recall{};
    recall.fDurationSec = kRecallDurationSec;
    recall.fRemainingSec = recall.fDurationSec;
    recall.bActive = true;

    if (world.HasComponent<RecallComponent>(cmd.issuerEntity))
        world.GetComponent<RecallComponent>(cmd.issuerEntity) = recall;
    else
        world.AddComponent<RecallComponent>(cmd.issuerEntity, recall);

    StartNetAnimation(
        world,
        cmd.issuerEntity,
        eNetAnimId::Recall,
        tc,
        256);
}

void CDefaultCommandExecutor::HandleRecallCancel(CWorld& world,
    const TickContext& tc, const GameCommand& cmd)
{
    const bool_t bHadRecall = world.HasComponent<RecallComponent>(cmd.issuerEntity);
    CancelRecall(world, cmd.issuerEntity);

    if (bHadRecall)
        StartNetAnimation(world, cmd.issuerEntity, eNetAnimId::Idle, tc, 256);
}

void CDefaultCommandExecutor::HandleFlash(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (!world.HasComponent<TransformComponent>(cmd.issuerEntity))
        return;
    if (!GameplayStateQuery::CanMove(world, cmd.issuerEntity))
        return;

    auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
    const Vec3 origin = transform.GetLocalPosition();
    Vec3 rawTarget = cmd.groundPos;
    rawTarget.y = origin.y;

    const f32_t dx = rawTarget.x - origin.x;
    const f32_t dz = rawTarget.z - origin.z;
    const f32_t lenSq = dx * dx + dz * dz;
    if (lenSq <= 0.001f)
        return;

    u8_t flashSlot = 0u;
    bool_t bHasFlash = true;
    if (world.HasComponent<ChampionScoreComponent>(cmd.issuerEntity))
    {
        bHasFlash = false;
        const auto& score = world.GetComponent<ChampionScoreComponent>(cmd.issuerEntity);
        for (u8_t i = 0; i < ChampionScoreComponent::kSummonerSpellSlotCount; ++i)
        {
            if (score.iSummonerSpellIds[i] == ChampionScoreComponent::kSummonerSpellFlash)
            {
                flashSlot = i;
                bHasFlash = true;
                break;
            }
        }
    }
    if (!bHasFlash)
        return;

    auto& spells = world.HasComponent<SummonerSpellStateComponent>(cmd.issuerEntity)
        ? world.GetComponent<SummonerSpellStateComponent>(cmd.issuerEntity)
        : world.AddComponent<SummonerSpellStateComponent>(
            cmd.issuerEntity,
            SummonerSpellStateComponent{});
    if (spells.cooldownRemaining[flashSlot] > 0.f)
        return;

    const f32_t range =
        ChampionGameDataDB::ResolveSummonerSpellRange(ChampionScoreComponent::kSummonerSpellFlash);
    const f32_t cooldown =
        ChampionGameDataDB::ResolveSummonerSpellCooldown(ChampionScoreComponent::kSummonerSpellFlash);
    if (range <= 0.f || cooldown <= 0.f)
        return;

    const f32_t len = std::sqrt(lenSq);
    const f32_t useLen = std::min(len, range);
    const f32_t nx = dx / len;
    const f32_t nz = dz / len;
    Vec3 dest{ origin.x + nx * useLen, origin.y, origin.z + nz * useLen };

    if (tc.pWalkable)
    {
        Vec3 resolved{};
        if (!tc.pWalkable->TryResolveMoveTarget(origin, dest, resolved))
            return;
        resolved.y = origin.y;
        dest = resolved;
    }

    ClearMoveTarget(world, cmd.issuerEntity);
    ClearAttackChase(world, cmd.issuerEntity);
    ClearCombatAction(world, cmd.issuerEntity);
    CancelRecall(world, cmd.issuerEntity);

    transform.SetPosition(dest);
    spells.cooldownRemaining[flashSlot] = cooldown;
    spells.cooldownDuration[flashSlot] = cooldown;

    ReplicatedEventComponent flashEvent{};
    flashEvent.kind = eReplicatedEventKind::EffectTrigger;
    flashEvent.sourceEntity = cmd.issuerEntity;
    flashEvent.effectId = kGlobalEffectFlashBlink;
    flashEvent.position = origin;
    flashEvent.direction = Vec3{ dest.x - origin.x, 0.f, dest.z - origin.z };
    flashEvent.durationMs = 400u;
    flashEvent.startTick = tc.tickIndex;
    EnqueueReplicatedEvent(world, flashEvent);
}

void CDefaultCommandExecutor::HandleAIDebugControl(CWorld& world,
    const TickContext& tc, const GameCommand& cmd)
{
    (void)tc;

#if defined(_DEBUG)
    if (cmd.targetEntity == NULL_ENTITY ||
        !world.HasComponent<ChampionAIComponent>(cmd.targetEntity))
    {
        return;
    }

    auto& ai = world.GetComponent<ChampionAIComponent>(cmd.targetEntity);
    if (cmd.itemId == kChampionAIDebugTuneRuntimeItemId)
    {
        const auto tuningId = static_cast<eChampionAITuningId>(cmd.slot);
        if (static_cast<u8_t>(tuningId) >= static_cast<u8_t>(eChampionAITuningId::Count))
            return;

        ApplyChampionAITuningOverride(ai, tuningId, cmd.groundPos.x);
        ai.decisionTimer = 0.f;
        return;
    }

    if (cmd.itemId == kChampionAIDebugResetTuningItemId)
    {
        ai.tuning = ChampionAITuning{};
        ai.decisionTimer = 0.f;
        return;
    }

    if (cmd.itemId == kChampionAIDebugClearOverrideItemId)
    {
        ai.debugForcedDecisionCount = 0;
        ai.debugForcedSkillSlot = 0;
        ai.bDebugForceAction = false;
        return;
    }

    const auto action = static_cast<eChampionAIAction>(cmd.itemId);
    if (static_cast<u8_t>(action) > static_cast<u8_t>(eChampionAIAction::Recall))
        return;

    const bool_t bForceAction = cmd.slot == kChampionAIDebugForceActionSkillSlot;
    ai.debugForcedAction = action;
    ai.debugForcedSkillSlot = bForceAction ? 0u : cmd.slot;
    ai.bDebugForceAction = bForceAction;
    ai.debugForcedDecisionCount = bForceAction
        ? kChampionAIDebugForceDecisionCount
        : kChampionAIDebugSingleDecisionCount;
    ai.decisionTimer = 0.f;
#else
    (void)world;
    (void)cmd;
#endif
}

GameCommand BuildServerCommand(const GameCommandWire& wire,
    uint32_t sessionId, EntityID controlledEntity,
    const EntityIdMap& map)
{
    (void)sessionId;

    GameCommand cmd{};
    cmd.kind = wire.kind;
    cmd.issuerEntity = controlledEntity;
    cmd.issuedAtTick = 0;
    cmd.sequenceNum = wire.sequenceNum;
    cmd.slot = wire.slot;
    cmd.targetEntity = (wire.targetNet != NULL_NET_ENTITY)
        ? map.FromNet(wire.targetNet)
        : NULL_ENTITY;
    cmd.groundPos = wire.groundPos;
    cmd.direction = wire.direction;
    cmd.itemId = wire.itemId;
    return cmd;
}
