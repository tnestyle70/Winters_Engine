#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"

#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr u8_t kEzrealBasicAttackSlot =
        static_cast<u8_t>(eSkillSlot::BasicAttack);
    constexpr u8_t kEzrealQSlot = static_cast<u8_t>(eSkillSlot::Q);
    constexpr u8_t kEzrealWSlot = static_cast<u8_t>(eSkillSlot::W);
    constexpr u8_t kEzrealESlot = static_cast<u8_t>(eSkillSlot::E);
    constexpr u8_t kEzrealRSlot = static_cast<u8_t>(eSkillSlot::R);

    const bool_t s_bEzrealKeyframeComponentsRegistered = []()
    {
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<EzrealPendingCastComponent>("EzrealPendingCastComponent");
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<EzrealEssenceFluxMarkComponent>(
                "EzrealEssenceFluxMarkComponent");
        return true;
    }();

    u8_t SanitizeRank(u8_t rank)
    {
        return rank == 0u ? 1u : rank;
    }

    u16_t MakeEzrealSkillId(u8_t slot)
    {
        return static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::EZREAL) << 8) | slot);
    }

    u64_t SecondsToTicksCeil(f32_t seconds)
    {
        if (!std::isfinite(seconds) || seconds <= 0.f)
            return 0u;

        return static_cast<u64_t>(std::ceil(
            static_cast<f64_t>(seconds) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
    }

    f32_t ResolveEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t slot,
        eSkillEffectParamId param,
        f32_t fallback)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            source,
            tc,
            eChampion::EZREAL,
            slot,
            param,
            fallback);
    }

    f32_t ResolveRankedValue(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t slot,
        u8_t rank,
        eSkillEffectParamId baseParam,
        eSkillEffectParamId perRankParam,
        f32_t fallbackBase,
        f32_t fallbackPerRank)
    {
        const f32_t base = ResolveEffectParam(
            world, tc, source, slot, baseParam, fallbackBase);
        const f32_t perRank = ResolveEffectParam(
            world, tc, source, slot, perRankParam, fallbackPerRank);
        return base + perRank * static_cast<f32_t>(SanitizeRank(rank) - 1u);
    }

    eTeam ResolveSourceTeam(CWorld& world, EntityID source)
    {
        const eTeam team = GameplayStateQuery::ResolveEntityTeam(world, source);
        return team == eTeam::TEAM_END ? eTeam::Neutral : team;
    }

    bool_t IsAliveCastSource(CWorld& world, EntityID source)
    {
        if (source == NULL_ENTITY || !world.IsAlive(source))
            return false;
        if (!world.HasComponent<HealthComponent>(source))
            return true;

        const HealthComponent& health = world.GetComponent<HealthComponent>(source);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    Vec3 ResolveCastDirection(
        CWorld& world,
        EntityID caster,
        const Vec3& requestedDirection)
    {
        const Vec3 commandDirection = WintersMath::NormalizeXZ(
            requestedDirection,
            Vec3{},
            0.0001f);
        if (commandDirection.x != 0.f || commandDirection.z != 0.f)
            return commandDirection;

        if (world.HasComponent<TransformComponent>(caster))
        {
            return WintersMath::DirectionFromYawXZ(
                world.GetComponent<TransformComponent>(caster).GetRotation().y);
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    EntityID SpawnEzrealProjectile(
        CWorld& world,
        EntityID source,
        EntityID target,
        eProjectileKind kind,
        u8_t slot,
        u8_t rank,
        const Vec3& origin,
        const Vec3& direction,
        f32_t speed,
        f32_t maxDistance,
        f32_t hitRadius,
        f32_t damage,
        f32_t totalAdRatio,
        f32_t bonusAdRatio,
        f32_t apRatio,
        eDamageType damageType,
        eDamageSourceKind damageSourceKind,
        u32_t damageFlags,
        eProjectileUnitHitPolicy unitHitPolicy,
        u8_t targetKindMask,
        u16_t maxUniqueHits,
        bool_t bEpicMonstersOnly,
        bool_t bApplyDamageOnHit,
        f32_t paidManaCost = -1.f)
    {
        if (source == NULL_ENTITY || !world.IsAlive(source))
            return NULL_ENTITY;
        if (!std::isfinite(speed) || speed <= 0.f ||
            !std::isfinite(maxDistance) || maxDistance <= 0.f)
        {
            return NULL_ENTITY;
        }

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = source;
        projectile.targetEntity = target;
        projectile.sourceHandle = world.GetEntityHandle(source);
        projectile.targetHandle = target != NULL_ENTITY
            ? world.GetEntityHandle(target)
            : NULL_ENTITY_HANDLE;
        projectile.sourceTeam = ResolveSourceTeam(world, source);
        projectile.kind = kind;
        projectile.unitHitPolicy = unitHitPolicy;
        projectile.targetKindMask = targetKindMask;
        projectile.maxUniqueHits = maxUniqueHits;
        projectile.bEpicMonstersOnly = bEpicMonstersOnly;
        projectile.bCollidesWithTerrain = false;
        projectile.bPersistAfterSourceDeath = true;
        projectile.bApplyDamageOnHit = bApplyDamageOnHit;
        projectile.skillId = MakeEzrealSkillId(slot);
        projectile.rank = SanitizeRank(rank);
        projectile.currentPos = origin;
        projectile.direction = ResolveCastDirection(world, source, direction);
        projectile.speed = speed;
        projectile.maxDistance = maxDistance;
        projectile.hitRadius = (std::max)(0.05f, hitRadius);
        projectile.damage = damage;
        projectile.paidManaCost = paidManaCost;
        projectile.totalAdRatio = totalAdRatio;
        projectile.bonusAdRatio = bonusAdRatio;
        projectile.apRatio = apRatio;
        projectile.damageType = damageType;
        projectile.damageSourceKind = damageSourceKind;
        projectile.sourceSlot = slot;
        projectile.damageFlags = damageFlags;

        const EntityHandle hProjectile = world.CreateEntityHandle();
        if (!hProjectile.IsValid())
            return NULL_ENTITY;

        const EntityID projectileEntity = hProjectile.GetIndex();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);
        return projectileEntity;
    }

    Vec3 ResolveMuzzlePosition(CWorld& world, EntityID source)
    {
        if (!world.HasComponent<TransformComponent>(source))
            return {};

        Vec3 origin = world.GetComponent<TransformComponent>(source).GetPosition();
        origin.y += 1.f;
        return origin;
    }

    void EmitEffectEvent(
        CWorld& world,
        const TickContext& tc,
        u32_t effectId,
        EntityID source,
        EntityID target,
        u8_t slot,
        u8_t rank,
        f32_t durationSec,
        u32_t sourceNetOverride = 0u,
        u32_t targetNetOverride = 0u)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.effectId = effectId;
        event.sourceEntity = source;
        event.targetEntity = target;
        event.sourceNetOverride = sourceNetOverride;
        event.targetNetOverride = targetNetOverride;
        event.slot = slot;
        event.rank = rank;
        event.startTick = tc.tickIndex;
        const f32_t durationMs = (std::max)(0.f, durationSec) * 1000.f;
        event.durationMs = static_cast<u16_t>((std::min)(65535.f, durationMs));
        if (target != NULL_ENTITY && world.HasComponent<TransformComponent>(target))
            event.position = world.GetComponent<TransformComponent>(target).GetPosition();
        EnqueueReplicatedEvent(world, event);
    }

    void EmitArcaneShiftEvent(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        const Vec3& origin,
        const Vec3& destination)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.effectId = kEzrealEffectArcaneShiftBlink;
        event.sourceEntity = source;
        event.slot = kEzrealESlot;
        event.startTick = tc.tickIndex;
        event.durationMs = 400u;
        event.position = origin;
        event.direction = Vec3{
            destination.x - origin.x,
            destination.y - origin.y,
            destination.z - origin.z };
        EnqueueReplicatedEvent(world, event);
    }

    EntityID FindEssenceFluxMarkRelation(
        CWorld& world,
        EntityID source,
        EntityID target)
    {
        if (source == NULL_ENTITY || target == NULL_ENTITY)
            return NULL_ENTITY;

        const EntityHandle hSource = world.GetEntityHandle(source);
        const EntityHandle hTarget = world.GetEntityHandle(target);
        if (!hSource.IsValid() || !hTarget.IsValid())
            return NULL_ENTITY;

        const auto relations =
            DeterministicEntityIterator<EzrealEssenceFluxMarkComponent>::CollectSorted(world);
        for (EntityID relationEntity : relations)
        {
            const EzrealEssenceFluxMarkComponent& mark =
                world.GetComponent<EzrealEssenceFluxMarkComponent>(relationEntity);
            if (mark.hSource == hSource && mark.hTarget == hTarget)
                return relationEntity;
        }
        return NULL_ENTITY;
    }

    void AttachOrRefreshEssenceFluxMark(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        u8_t rank)
    {
        const EntityHandle hSource = world.GetEntityHandle(source);
        const EntityHandle hTarget = world.GetEntityHandle(target);
        if (!hSource.IsValid() || !hTarget.IsValid())
            return;

        const f32_t durationSec = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealWSlot,
            eSkillEffectParamId::MarkDurationSec,
            4.f);
        const u64_t durationTicks = (std::max)(1ull, SecondsToTicksCeil(durationSec));
        const u32_t sourceNet = tc.pEntityMap
            ? tc.pEntityMap->ToNet(source)
            : 0u;
        const u32_t targetNet = tc.pEntityMap
            ? tc.pEntityMap->ToNet(target)
            : 0u;

        const EntityID existing = FindEssenceFluxMarkRelation(world, source, target);
        if (existing != NULL_ENTITY)
        {
            EzrealEssenceFluxMarkComponent& mark =
                world.GetComponent<EzrealEssenceFluxMarkComponent>(existing);
            mark.uExpireTick = tc.tickIndex + durationTicks;
            mark.uRank = SanitizeRank(rank);
            mark.uSourceNet = sourceNet;
            mark.uTargetNet = targetNet;
        }
        else
        {
            EzrealEssenceFluxMarkComponent mark{};
            mark.hSource = hSource;
            mark.hTarget = hTarget;
            mark.uSourceNet = sourceNet;
            mark.uTargetNet = targetNet;
            mark.uExpireTick = tc.tickIndex + durationTicks;
            mark.uRank = SanitizeRank(rank);

            const EntityHandle hRelation = world.CreateEntityHandle();
            if (!hRelation.IsValid())
                return;
            world.AddComponent<EzrealEssenceFluxMarkComponent>(
                hRelation.GetIndex(),
                mark);
        }

        EmitEffectEvent(
            world,
            tc,
            kEzrealEffectEssenceFluxMark,
            source,
            target,
            kEzrealWSlot,
            SanitizeRank(rank),
            durationSec,
            sourceNet,
            targetNet);
    }

    bool_t TryConsumeEssenceFluxMark(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        u8_t& outRank)
    {
        const EntityID relationEntity =
            FindEssenceFluxMarkRelation(world, source, target);
        if (relationEntity == NULL_ENTITY)
            return false;

        const EzrealEssenceFluxMarkComponent mark =
            world.GetComponent<EzrealEssenceFluxMarkComponent>(relationEntity);
        if (tc.tickIndex >= mark.uExpireTick)
            return false;

        outRank = SanitizeRank(mark.uRank);
        world.DestroyEntity(relationEntity);
        EmitEffectEvent(
            world,
            tc,
            kEzrealEffectEssenceFluxDetonate,
            source,
            target,
            kEzrealWSlot,
            outRank,
            0.4f,
            mark.uSourceNet,
            mark.uTargetNet);
        return true;
    }

    void TickEssenceFluxMarks(CWorld& world, const TickContext& tc)
    {
        const auto relations =
            DeterministicEntityIterator<EzrealEssenceFluxMarkComponent>::CollectSorted(world);
        for (EntityID relationEntity : relations)
        {
            if (!world.IsAlive(relationEntity) ||
                !world.HasComponent<EzrealEssenceFluxMarkComponent>(relationEntity))
            {
                continue;
            }

            const EzrealEssenceFluxMarkComponent mark =
                world.GetComponent<EzrealEssenceFluxMarkComponent>(relationEntity);
            const EntityID source = world.ResolveEntity(mark.hSource);
            const EntityID target = world.ResolveEntity(mark.hTarget);
            const bool_t bExpired = tc.tickIndex >= mark.uExpireTick;
            const bool_t bStaleEndpoint = source == NULL_ENTITY || target == NULL_ENTITY;
            if (!bExpired && !bStaleEndpoint)
                continue;

            EmitEffectEvent(
                world,
                tc,
                kEzrealEffectEssenceFluxClear,
                source,
                target,
                kEzrealWSlot,
                SanitizeRank(mark.uRank),
                0.f,
                mark.uSourceNet,
                mark.uTargetNet);
            world.DestroyEntity(relationEntity);
        }
    }

    DamageRequest BuildProjectileDamageRequest(
        const SkillProjectileComponent& projectile,
        EntityID target)
    {
        DamageRequest request{};
        request.source = projectile.sourceEntity;
        request.target = target;
        request.sourceTeam = projectile.sourceTeam;
        request.type = projectile.damageType;
        request.flatAmount = projectile.damage;
        request.skillId = projectile.skillId;
        request.rank = projectile.rank;
        request.iSourceSlot = projectile.sourceSlot;
        request.eSourceKind = projectile.damageSourceKind;
        request.adRatioOverride = projectile.totalAdRatio;
        request.bonusAdRatioOverride = projectile.bonusAdRatio;
        request.apRatioOverride = projectile.apRatio;
        request.flags = projectile.damageFlags;
        return request;
    }

    bool_t IsEpicJungleMonster(CWorld& world, EntityID target)
    {
        if (!world.HasComponent<JungleComponent>(target))
            return false;
        const u32_t subKind = world.GetComponent<JungleComponent>(target).subKind;
        return subKind == 0u || subKind == 1u;
    }

    void ApplyTrueshotNonEpicReduction(
        CWorld& world,
        const TickContext& tc,
        const SkillProjectileComponent& projectile,
        EntityID target,
        DamageRequest& request)
    {
        const auto targetKind = GameplayStateQuery::ResolveTargetKind(world, target);
        if (targetKind == GameplayStateQuery::eGameplayTargetKind::Champion ||
            (targetKind == GameplayStateQuery::eGameplayTargetKind::JungleMonster &&
                IsEpicJungleMonster(world, target)))
        {
            return;
        }

        const f32_t explicitBase = ResolveEffectParam(
            world,
            tc,
            projectile.sourceEntity,
            kEzrealRSlot,
            eSkillEffectParamId::NonEpicBaseDamage,
            -1.f);
        const f32_t explicitPerRank = ResolveEffectParam(
            world,
            tc,
            projectile.sourceEntity,
            kEzrealRSlot,
            eSkillEffectParamId::NonEpicDamagePerRank,
            -1.f);
        if (explicitBase >= 0.f && explicitPerRank >= 0.f)
        {
            request.flatAmount = explicitBase + explicitPerRank *
                static_cast<f32_t>(SanitizeRank(projectile.rank) - 1u);
        }
        else
        {
            request.flatAmount = 150.f + 75.f *
                static_cast<f32_t>(SanitizeRank(projectile.rank) - 1u);
        }
    }

    DamageRequest BuildEssenceFluxDetonationDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        u8_t rank)
    {
        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = ResolveSourceTeam(world, source);
        request.type = eDamageType::Magic;
        request.flatAmount = ResolveRankedValue(
            world,
            tc,
            source,
            kEzrealWSlot,
            rank,
            eSkillEffectParamId::BaseDamage,
            eSkillEffectParamId::DamagePerRank,
            80.f,
            55.f);
        request.skillId = MakeEzrealSkillId(kEzrealWSlot);
        request.rank = SanitizeRank(rank);
        request.iSourceSlot = kEzrealWSlot;
        request.eSourceKind = eDamageSourceKind::Skill;
        request.bonusAdRatioOverride = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealWSlot,
            eSkillEffectParamId::BonusAdRatio,
            1.f);
        request.apRatioOverride = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealWSlot,
            eSkillEffectParamId::ApRatio,
            0.9f);
        return request;
    }

    void RestoreEssenceFluxMana(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t triggerSlot,
        f32_t paidManaCost)
    {
        if (triggerSlot == kEzrealBasicAttackSlot)
            return;
        if (!world.HasComponent<ChampionComponent>(source))
            return;

        const f32_t flatRestore = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealWSlot,
            eSkillEffectParamId::ManaRestoreFlat,
            60.f);
        const f32_t manaRestore =
            (std::max)(0.f, flatRestore) +
            (std::max)(0.f, paidManaCost);
        ChampionComponent& champion = world.GetComponent<ChampionComponent>(source);
        champion.mana = (std::min)(
            champion.maxMana,
            champion.mana + (std::max)(0.f, manaRestore));
    }

    void ReduceEzrealCooldownsOnMysticShotHit(
        CWorld& world,
        const TickContext& tc,
        EntityID source)
    {
        if (!world.HasComponent<SkillStateComponent>(source))
            return;

        const f32_t refundSec = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealQSlot,
            eSkillEffectParamId::CooldownRefundSec,
            1.5f);
        if (refundSec <= 0.f)
            return;

        SkillStateComponent& skills = world.GetComponent<SkillStateComponent>(source);
        for (u8_t slot = kEzrealQSlot; slot <= kEzrealRSlot; ++slot)
        {
            skills.slots[slot].cooldownRemaining = (std::max)(
                0.f,
                skills.slots[slot].cooldownRemaining - refundSec);
        }
    }

    bool_t IsEssenceFluxTriggerProjectile(eProjectileKind kind)
    {
        return kind == eProjectileKind::EzrealBasicAttack ||
            kind == eProjectileKind::MysticShot ||
            kind == eProjectileKind::ArcaneShiftBolt ||
            kind == eProjectileKind::GlobalBeam;
    }

    bool_t IsEzrealProjectile(eProjectileKind kind)
    {
        return kind == eProjectileKind::EzrealBasicAttack ||
            kind == eProjectileKind::MysticShot ||
            kind == eProjectileKind::EssenceFlux ||
            kind == eProjectileKind::ArcaneShiftBolt ||
            kind == eProjectileKind::GlobalBeam;
    }

    void RegisterRisingSpellForceHit(
        CWorld& world,
        const TickContext& tc,
        EntityID source)
    {
        if (source == NULL_ENTITY ||
            !world.IsAlive(source) ||
            !world.HasComponent<ChampionComponent>(source) ||
            world.GetComponent<ChampionComponent>(source).id != eChampion::EZREAL)
        {
            return;
        }

        const f32_t fStackWindowSec = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealBasicAttackSlot,
            eSkillEffectParamId::StackWindowSec,
            6.f);
        const f32_t fMaxStacks = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealBasicAttackSlot,
            eSkillEffectParamId::MaxStacks,
            5.f);
        if (!std::isfinite(fStackWindowSec) ||
            fStackWindowSec <= 0.f ||
            !std::isfinite(fMaxStacks))
        {
            return;
        }
        const u8_t uMaxStacks = static_cast<u8_t>((std::clamp)(
            static_cast<i32_t>(std::lround(fMaxStacks)),
            1,
            255));

        BuffComponent& buffs = world.HasComponent<BuffComponent>(source)
            ? world.GetComponent<BuffComponent>(source)
            : world.AddComponent<BuffComponent>(source, BuffComponent{});

        BuffInstance passive{};
        passive.buffDefId = kEzrealRisingSpellForceBuffDefId;
        passive.source = source;
        passive.fDurationRemaining = (std::max)(0.f, fStackWindowSec);
        passive.uExpireTick =
            tc.tickIndex + SecondsToTicksCeil(fStackWindowSec);
        passive.stackCount = 1u;
        passive.bonusAttackSpeedPerStack = (std::max)(
            0.f,
            ResolveEffectParam(
                world,
                tc,
                source,
                kEzrealBasicAttackSlot,
                eSkillEffectParamId::BonusAttackSpeed,
                0.10f));

        for (u8_t i = 0u;
            i < buffs.count && i < BuffComponent::kMaxBuffs;
            ++i)
        {
            const BuffInstance& existing = buffs.buffs[i];
            if (existing.buffDefId == kEzrealRisingSpellForceBuffDefId &&
                existing.source == source)
            {
                passive.stackCount = static_cast<u8_t>((std::min)(
                    static_cast<u16_t>(
                        static_cast<u16_t>(existing.stackCount) + 1u),
                    static_cast<u16_t>(uMaxStacks)));
                break;
            }
        }

        if (CBuffSystem::AddOrRefresh(buffs, passive) &&
            world.HasComponent<StatComponent>(source))
        {
            world.GetComponent<StatComponent>(source).bDirty = true;
        }
    }

    void LaunchMysticShot(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& direction,
        f32_t fPaidManaCost)
    {
        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world, source, tc, eChampion::EZREAL, kEzrealQSlot);
        if (range <= 0.f)
            range = 12.f;

        SpawnEzrealProjectile(
            world,
            source,
            NULL_ENTITY,
            eProjectileKind::MysticShot,
            kEzrealQSlot,
            rank,
            ResolveMuzzlePosition(world, source),
            direction,
            ResolveEffectParam(world, tc, source, kEzrealQSlot,
                eSkillEffectParamId::Speed, 20.f),
            range,
            ResolveEffectParam(world, tc, source, kEzrealQSlot,
                eSkillEffectParamId::HalfWidth, 0.6f),
            ResolveRankedValue(world, tc, source, kEzrealQSlot, rank,
                eSkillEffectParamId::BaseDamage,
                eSkillEffectParamId::DamagePerRank,
                20.f, 25.f),
            ResolveEffectParam(world, tc, source, kEzrealQSlot,
                eSkillEffectParamId::TotalAdRatio, 1.3f),
            0.f,
            ResolveEffectParam(world, tc, source, kEzrealQSlot,
                eSkillEffectParamId::ApRatio, 0.4f),
            eDamageType::Physical,
            eDamageSourceKind::Skill,
            DamageFlag_OnHit,
            eProjectileUnitHitPolicy::Destroy,
            kProjectileTargetMobileUnits,
            1u,
            false,
            true,
            fPaidManaCost);
    }

    void LaunchEssenceFlux(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& direction)
    {
        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world, source, tc, eChampion::EZREAL, kEzrealWSlot);
        if (range <= 0.f)
            range = 12.f;

        SpawnEzrealProjectile(
            world,
            source,
            NULL_ENTITY,
            eProjectileKind::EssenceFlux,
            kEzrealWSlot,
            rank,
            ResolveMuzzlePosition(world, source),
            direction,
            ResolveEffectParam(world, tc, source, kEzrealWSlot,
                eSkillEffectParamId::Speed, 17.f),
            range,
            ResolveEffectParam(world, tc, source, kEzrealWSlot,
                eSkillEffectParamId::HalfWidth, 0.8f),
            0.f,
            0.f,
            0.f,
            0.f,
            eDamageType::Magic,
            eDamageSourceKind::Skill,
            DamageFlag_None,
            eProjectileUnitHitPolicy::Destroy,
            ProjectileTarget_Champion |
                ProjectileTarget_JungleMonster |
                ProjectileTarget_Structure,
            1u,
            true,
            false);
    }

    bool_t IsEligibleArcaneShiftTarget(
        CWorld& world,
        EntityID source,
        EntityID target,
        const Vec3& origin,
        f32_t rangeSq)
    {
        if (target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target) ||
            !GameplayStateQuery::CanReceiveEnemyAbilityHit(world, source, target))
        {
            return false;
        }

        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        return WintersMath::DistanceSqXZ(origin, targetPos) <= rangeSq;
    }

    bool_t IsEligibleMarkedArcaneShiftTarget(
        CWorld& world,
        EntityID source,
        EntityID target,
        const Vec3& origin,
        f32_t rangeSq)
    {
        if (IsEligibleArcaneShiftTarget(
            world, source, target, origin, rangeSq))
        {
            return true;
        }
        if (target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target) ||
            GameplayStateQuery::ResolveTargetKind(world, target) !=
                GameplayStateQuery::eGameplayTargetKind::Structure ||
            !GameplayStateQuery::CanReceiveDamage(world, source, target))
        {
            return false;
        }

        const eTeam sourceTeam =
            GameplayStateQuery::ResolveEntityTeam(world, source);
        const eTeam targetTeam =
            GameplayStateQuery::ResolveEntityTeam(world, target);
        if (sourceTeam != eTeam::TEAM_END &&
            targetTeam != eTeam::TEAM_END &&
            sourceTeam == targetTeam)
        {
            return false;
        }

        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        return WintersMath::DistanceSqXZ(origin, targetPos) <= rangeSq;
    }

    EntityID FindArcaneShiftTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        const Vec3& origin,
        f32_t range)
    {
        const f32_t rangeSq = range * range;
        EntityID bestTarget = NULL_ENTITY;
        f32_t bestDistanceSq = std::numeric_limits<f32_t>::max();

        const auto marks =
            DeterministicEntityIterator<EzrealEssenceFluxMarkComponent>::CollectSorted(world);
        const EntityHandle hSource = world.GetEntityHandle(source);
        for (EntityID relationEntity : marks)
        {
            const EzrealEssenceFluxMarkComponent& mark =
                world.GetComponent<EzrealEssenceFluxMarkComponent>(relationEntity);
            if (mark.hSource != hSource)
                continue;
            if (tc.tickIndex >= mark.uExpireTick)
                continue;

            const EntityID target = world.ResolveEntity(mark.hTarget);
            if (!IsEligibleMarkedArcaneShiftTarget(
                world, source, target, origin, rangeSq))
                continue;

            const f32_t distanceSq = WintersMath::DistanceSqXZ(
                origin,
                world.GetComponent<TransformComponent>(target).GetPosition());
            if (bestTarget == NULL_ENTITY ||
                distanceSq < bestDistanceSq ||
                (distanceSq == bestDistanceSq && target < bestTarget))
            {
                bestTarget = target;
                bestDistanceSq = distanceSq;
            }
        }
        if (bestTarget != NULL_ENTITY)
            return bestTarget;

        const auto candidates =
            DeterministicEntityIterator<TransformComponent>::CollectSorted(world);
        for (EntityID target : candidates)
        {
            if (!IsEligibleArcaneShiftTarget(world, source, target, origin, rangeSq))
                continue;

            const f32_t distanceSq = WintersMath::DistanceSqXZ(
                origin,
                world.GetComponent<TransformComponent>(target).GetPosition());
            if (bestTarget == NULL_ENTITY ||
                distanceSq < bestDistanceSq ||
                (distanceSq == bestDistanceSq && target < bestTarget))
            {
                bestTarget = target;
                bestDistanceSq = distanceSq;
            }
        }
        return bestTarget;
    }

    Vec3 ResolveArcaneShiftDestination(
        const TickContext& tc,
        const Vec3& origin,
        const Vec3& direction,
        f32_t distance)
    {
        const Vec3 desired{
            origin.x + direction.x * distance,
            origin.y,
            origin.z + direction.z * distance };
        if (!tc.pWalkable)
            return desired;

        Vec3 destination = origin;
        constexpr u32_t kLandingSearchSteps = 32u;
        for (u32_t step = 0u; step <= kLandingSearchSteps; ++step)
        {
            const f32_t t = 1.f -
                static_cast<f32_t>(step) /
                static_cast<f32_t>(kLandingSearchSteps);
            Vec3 candidate{
                origin.x + (desired.x - origin.x) * t,
                origin.y,
                origin.z + (desired.z - origin.z) * t };
            if (!tc.pWalkable->IsWalkableXZ(candidate))
                continue;

            f32_t surfaceY = candidate.y;
            if (tc.pWalkable->TrySampleHeight(candidate.x, candidate.z, surfaceY))
                candidate.y = surfaceY;
            destination = candidate;
            break;
        }
        return destination;
    }

    void LaunchArcaneShift(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& groundTarget,
        bool_t bHasGroundTarget,
        const Vec3& direction,
        f32_t fPaidManaCost)
    {
        if (!world.HasComponent<TransformComponent>(source))
            return;

        TransformComponent& transform =
            world.GetComponent<TransformComponent>(source);
        const Vec3 origin = transform.GetPosition();
        f32_t blinkDistance = GameplayDefinitionQuery::ResolveSkillRange(
            world, source, tc, eChampion::EZREAL, kEzrealESlot);
        if (blinkDistance <= 0.f)
            blinkDistance = 4.75f;

        Vec3 blinkDirection = direction;
        f32_t resolvedBlinkDistance = blinkDistance;
        if (bHasGroundTarget)
        {
            const Vec3 groundDelta{
                groundTarget.x - origin.x,
                0.f,
                groundTarget.z - origin.z };
            const f32_t requestedDistance = std::sqrt(
                groundDelta.x * groundDelta.x + groundDelta.z * groundDelta.z);
            resolvedBlinkDistance = (std::min)(blinkDistance, requestedDistance);
            if (requestedDistance > 0.0001f)
            {
                blinkDirection = Vec3{
                    groundDelta.x / requestedDistance,
                    0.f,
                    groundDelta.z / requestedDistance };
            }
            else
            {
                blinkDirection = Vec3{};
            }
        }

        const Vec3 destination = ResolveArcaneShiftDestination(
            tc,
            origin,
            blinkDirection,
            resolvedBlinkDistance);
        transform.SetPosition(destination);
        PositionDiscontinuityComponent& discontinuity =
            world.HasComponent<PositionDiscontinuityComponent>(source)
                ? world.GetComponent<PositionDiscontinuityComponent>(source)
                : world.AddComponent<PositionDiscontinuityComponent>(
                    source,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = tc.tickIndex;
        transform.m_bLocalDirty = true;
        transform.m_bWorldDirty = true;
        EmitArcaneShiftEvent(world, tc, source, origin, destination);

        const f32_t boltSearchRadius = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealESlot,
            eSkillEffectParamId::Radius,
            7.5f);
        const EntityID target = FindArcaneShiftTarget(
            world,
            tc,
            source,
            destination,
            (std::max)(0.f, boltSearchRadius));
        if (target == NULL_ENTITY)
            return;

        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 boltDirection = WintersMath::DirectionXZ(
            destination,
            targetPos,
            direction);
        Vec3 boltOrigin = destination;
        boltOrigin.y += 1.f;
        SpawnEzrealProjectile(
            world,
            source,
            target,
            eProjectileKind::ArcaneShiftBolt,
            kEzrealESlot,
            rank,
            boltOrigin,
            boltDirection,
            ResolveEffectParam(world, tc, source, kEzrealESlot,
                eSkillEffectParamId::Speed, 20.f),
            (std::max)(24.f, boltSearchRadius + 8.f),
            ResolveEffectParam(world, tc, source, kEzrealESlot,
                eSkillEffectParamId::HalfWidth, 0.5f),
            ResolveRankedValue(world, tc, source, kEzrealESlot, rank,
                eSkillEffectParamId::BaseDamage,
                eSkillEffectParamId::DamagePerRank,
                80.f, 50.f),
            ResolveEffectParam(world, tc, source, kEzrealESlot,
                eSkillEffectParamId::TotalAdRatio, 0.f),
            ResolveEffectParam(world, tc, source, kEzrealESlot,
                eSkillEffectParamId::BonusAdRatio, 0.f),
            ResolveEffectParam(world, tc, source, kEzrealESlot,
                eSkillEffectParamId::ApRatio, 0.75f),
            eDamageType::Magic,
            eDamageSourceKind::Skill,
            DamageFlag_None,
            eProjectileUnitHitPolicy::Destroy,
            kProjectileTargetMobileUnits,
            1u,
            false,
            true,
            fPaidManaCost);
    }

    void LaunchTrueshotBarrage(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& origin,
        const Vec3& direction,
        f32_t fPaidManaCost)
    {
        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world, source, tc, eChampion::EZREAL, kEzrealRSlot);
        if (range <= 0.f)
            range = 250.f;

        SpawnEzrealProjectile(
            world,
            source,
            NULL_ENTITY,
            eProjectileKind::GlobalBeam,
            kEzrealRSlot,
            rank,
            origin,
            direction,
            ResolveEffectParam(world, tc, source, kEzrealRSlot,
                eSkillEffectParamId::Speed, 20.f),
            range,
            ResolveEffectParam(world, tc, source, kEzrealRSlot,
                eSkillEffectParamId::HalfWidth, 1.6f),
            ResolveRankedValue(world, tc, source, kEzrealRSlot, rank,
                eSkillEffectParamId::BaseDamage,
                eSkillEffectParamId::DamagePerRank,
                350.f, 200.f),
            ResolveEffectParam(world, tc, source, kEzrealRSlot,
                eSkillEffectParamId::TotalAdRatio, 0.f),
            ResolveEffectParam(world, tc, source, kEzrealRSlot,
                eSkillEffectParamId::BonusAdRatio, 1.f),
            ResolveEffectParam(world, tc, source, kEzrealRSlot,
                eSkillEffectParamId::ApRatio, 1.1f),
            eDamageType::Magic,
            eDamageSourceKind::Skill,
            DamageFlag_None,
            eProjectileUnitHitPolicy::Pierce,
            kProjectileTargetMobileUnits,
            kMaxPiercingProjectileHits,
            false,
            true,
            fPaidManaCost);
    }

    void QueuePendingCast(GameplayHookContext& ctx, u8_t slot, f32_t fallbackCastTimeSec)
    {
        if (!ctx.pWorld || !ctx.pTickCtx || !ctx.pCommand ||
            !ctx.pWorld->IsAlive(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        EzrealPendingCastComponent pending{};
        pending.hCaster = world.GetEntityHandle(ctx.casterEntity);
        if (!pending.hCaster.IsValid())
            return;
        pending.vOrigin = ResolveMuzzlePosition(world, ctx.casterEntity);
        pending.vGroundTarget = ctx.pCommand->groundPos;
        pending.vDirection = ResolveCastDirection(
            world,
            ctx.casterEntity,
            ctx.pCommand->direction);
        pending.uSlot = slot;
        pending.uRank = SanitizeRank(ctx.skillRank);
        pending.fPaidManaCost = ctx.fPaidManaCost;
        pending.bHasGroundTarget = slot == kEzrealESlot;
        const f32_t castTimeSec = ResolveEffectParam(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            eSkillEffectParamId::CastTimeSec,
            fallbackCastTimeSec);
        pending.uLaunchTick = ctx.pTickCtx->tickIndex +
            SecondsToTicksCeil(castTimeSec);

        if (world.HasComponent<EzrealPendingCastComponent>(ctx.casterEntity))
            world.GetComponent<EzrealPendingCastComponent>(ctx.casterEntity) = pending;
        else
            world.AddComponent<EzrealPendingCastComponent>(ctx.casterEntity, pending);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        QueuePendingCast(ctx, kEzrealQSlot, 0.25f);
    }

    void OnW(GameplayHookContext& ctx)
    {
        QueuePendingCast(ctx, kEzrealWSlot, 0.25f);
    }

    void OnE(GameplayHookContext& ctx)
    {
        QueuePendingCast(ctx, kEzrealESlot, 0.25f);
    }

    void OnR(GameplayHookContext& ctx)
    {
        QueuePendingCast(ctx, kEzrealRSlot, 1.f);
    }

    void TickPendingCasts(CWorld& world, const TickContext& tc)
    {
        const auto casters =
            DeterministicEntityIterator<EzrealPendingCastComponent>::CollectSorted(world);
        for (EntityID casterEntity : casters)
        {
            if (!world.IsAlive(casterEntity) ||
                !world.HasComponent<EzrealPendingCastComponent>(casterEntity))
            {
                continue;
            }

            const EzrealPendingCastComponent pending =
                world.GetComponent<EzrealPendingCastComponent>(casterEntity);
            if (tc.tickIndex < pending.uLaunchTick)
                continue;

            world.RemoveComponent<EzrealPendingCastComponent>(casterEntity);

            const EntityID resolvedCaster = world.ResolveEntity(pending.hCaster);
            if (resolvedCaster != casterEntity ||
                !IsAliveCastSource(world, resolvedCaster) ||
                !GameplayStateQuery::CanCast(world, resolvedCaster))
            {
                continue;
            }

            switch (pending.uSlot)
            {
            case kEzrealQSlot:
                LaunchMysticShot(
                    world,
                    tc,
                    resolvedCaster,
                    pending.uRank,
                    pending.vDirection,
                    pending.fPaidManaCost);
                break;
            case kEzrealWSlot:
                LaunchEssenceFlux(
                    world, tc, resolvedCaster, pending.uRank, pending.vDirection);
                break;
            case kEzrealESlot:
                LaunchArcaneShift(
                    world,
                    tc,
                    resolvedCaster,
                    pending.uRank,
                    pending.vGroundTarget,
                    pending.bHasGroundTarget,
                    pending.vDirection,
                    pending.fPaidManaCost);
                break;
            case kEzrealRSlot:
                LaunchTrueshotBarrage(
                    world,
                    tc,
                    resolvedCaster,
                    pending.uRank,
                    pending.vOrigin,
                    pending.vDirection,
                    pending.fPaidManaCost);
                break;
            default:
                break;
            }
        }
    }
}

namespace EzrealGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::Q_CastFrame),
            &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::W_CastFrame),
            &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::E_OnCastAccepted),
            &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::R_CastFrame),
            &OnR);
        s_bRegistered = true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        TickEssenceFluxMarks(world, tc);
        TickPendingCasts(world, tc);
    }

    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest)
    {
        if (!world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !world.HasComponent<StatComponent>(attacker) ||
            world.GetComponent<StatComponent>(attacker).championId !=
                eChampion::EZREAL ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        const Vec3 origin = ResolveMuzzlePosition(world, attacker);
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 direction = WintersMath::DirectionXZ(
            origin,
            targetPos,
            Vec3{ 0.f, 0.f, 1.f });
        const f32_t initialDistance = std::sqrt(
            WintersMath::DistanceSqXZ(origin, targetPos));
        const f32_t damage = damageRequest.flatAmount != 0.f
            ? damageRequest.flatAmount
            : damageRequest.amount;

        return SpawnEzrealProjectile(
            world,
            attacker,
            target,
            eProjectileKind::EzrealBasicAttack,
            kEzrealBasicAttackSlot,
            damageRequest.rank,
            origin,
            direction,
            ResolveEffectParam(world, tc, attacker, kEzrealBasicAttackSlot,
                eSkillEffectParamId::Speed, 20.f),
            (std::max)(48.f, initialDistance + 12.f),
            ResolveEffectParam(world, tc, attacker, kEzrealBasicAttackSlot,
                eSkillEffectParamId::Radius, 0.35f),
            damage,
            damageRequest.adRatioOverride,
            damageRequest.bonusAdRatioOverride,
            damageRequest.apRatioOverride,
            damageRequest.type,
            eDamageSourceKind::BasicAttack,
            damageRequest.flags,
            eProjectileUnitHitPolicy::Destroy,
            ProjectileTarget_Champion |
                ProjectileTarget_MinionOrSummon |
                ProjectileTarget_JungleMonster |
                ProjectileTarget_Structure,
            1u,
            false,
            true) != NULL_ENTITY;
    }

    bool_t HandleProjectileHit(
        CWorld& world,
        const TickContext& tc,
        const SkillProjectileComponent& projectile,
        EntityID target,
        DamageRequest& outDamage,
        bool_t& outEnqueue)
    {
        if (!IsEzrealProjectile(projectile.kind))
            return false;

        outDamage = {};
        outEnqueue = false;
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return true;

        if (projectile.kind != eProjectileKind::EzrealBasicAttack)
        {
            RegisterRisingSpellForceHit(
                world,
                tc,
                projectile.sourceEntity);
        }

        if (projectile.kind == eProjectileKind::EssenceFlux)
        {
            AttachOrRefreshEssenceFluxMark(
                world,
                tc,
                projectile.sourceEntity,
                target,
                projectile.rank);
            return true;
        }


        DamageRequest triggerDamage =
            BuildProjectileDamageRequest(projectile, target);
        if (projectile.kind == eProjectileKind::GlobalBeam)
        {
            ApplyTrueshotNonEpicReduction(
                world,
                tc,
                projectile,
                target,
                triggerDamage);
        }

        if (projectile.kind == eProjectileKind::MysticShot)
        {
            ReduceEzrealCooldownsOnMysticShotHit(
                world,
                tc,
                projectile.sourceEntity);
        }

        u8_t markRank = 1u;
        if (IsEssenceFluxTriggerProjectile(projectile.kind) &&
            TryConsumeEssenceFluxMark(
                world,
                tc,
                projectile.sourceEntity,
                target,
                markRank))
        {
            if (projectile.bApplyDamageOnHit)
                EnqueueDamageRequest(world, triggerDamage);

            outDamage = BuildEssenceFluxDetonationDamage(
                world,
                tc,
                projectile.sourceEntity,
                target,
                markRank);
            outEnqueue = true;
            RestoreEssenceFluxMana(
                world,
                tc,
                projectile.sourceEntity,
                projectile.sourceSlot,
                projectile.paidManaCost);
            return true;
        }

        outDamage = triggerDamage;
        outEnqueue = projectile.bApplyDamageOnHit;
        return true;
    }
}
