#include "Game/ServerProjectileAuthority.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ProjectileBarrierComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <cmath>
#include <cstdio>

namespace
{
    struct ExactTickHistory final : ILagCompensationQuery
    {
        EntityHandle hEntity{};
        u64_t uAvailableTick = 0u;
        Vec3 vPosition{};
        mutable u64_t uLastRequestedTick = 0u;
        mutable u32_t uQueryCount = 0u;

        bool_t TryGetHistoricalStateAtTick(
            EntityHandle hRequested,
            u64_t uExpectedTick,
            LagCompensatedEntityState& outState) const override
        {
            ++uQueryCount;
            uLastRequestedTick = uExpectedTick;
            if (hRequested != hEntity || uExpectedTick != uAvailableTick)
                return false;
            outState.vPosition = vPosition;
            outState.fHp = 100.f;
            outState.bIsDead = false;
            return true;
        }
    };

    EntityID SpawnChampionTarget(
        CWorld& world,
        eTeam team,
        const Vec3& position)
    {
        const EntityID entity = world.CreateEntity();
        TransformComponent transform{};
        transform.SetPosition(position);
        world.AddComponent<TransformComponent>(entity, transform);
        world.AddComponent<HealthComponent>(entity, HealthComponent{});
        ChampionComponent champion{};
        champion.id = eChampion::EZREAL;
        champion.team = team;
        world.AddComponent<ChampionComponent>(entity, champion);
        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Character;
        spatial.team = static_cast<u8_t>(team);
        spatial.radius = 0.65f;
        world.AddComponent<SpatialAgentComponent>(entity, spatial);
        world.AddComponent<TargetableTag>(entity, TargetableTag{});
        return entity;
    }

    SkillProjectileComponent MakeProjectile(CWorld& world, EntityID source)
    {
        SkillProjectileComponent projectile{};
        projectile.sourceEntity = source;
        projectile.sourceHandle = world.GetEntityHandle(source);
        projectile.sourceTeam = eTeam::Blue;
        projectile.kind = eProjectileKind::MysticShot;
        projectile.unitHitPolicy = eProjectileUnitHitPolicy::Destroy;
        projectile.targetKindMask = ProjectileTarget_Champion;
        projectile.hitRadius = 0.25f;
        projectile.bBlockedByProjectileBarriers = true;
        return projectile;
    }

    bool CheckMovingTargetRelativeCcdAndDiscontinuity()
    {
        CWorld world;
        const EntityID source = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -5.f, 0.f, 0.f });
        const EntityID target = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 0.5f, 0.f, 2.f });
        SkillProjectileComponent projectile = MakeProjectile(world, source);

        ExactTickHistory history{};
        history.hEntity = world.GetEntityHandle(target);
        history.uAvailableTick = 9u;
        history.vPosition = Vec3{ 0.5f, 0.f, -2.f };

        Vec3 hitPos{};
        f32_t hitT = 0.f;
        const EntityID movingHit =
            CServerProjectileAuthority::FindSkillProjectileHitTarget(
                world,
                projectile,
                &history,
                10u,
                Vec3{ 0.f, 0.f, 0.f },
                Vec3{ 1.f, 0.f, 0.f },
                hitPos,
                hitT);
        if (movingHit != target ||
            history.uLastRequestedTick != 9u ||
            hitT <= 0.f || hitT >= 1.f)
        {
            return false;
        }

        PositionDiscontinuityComponent discontinuity{};
        discontinuity.uTick = 10u;
        world.AddComponent<PositionDiscontinuityComponent>(
            target, discontinuity);
        hitPos = {};
        hitT = 0.f;
        const EntityID blinkHit =
            CServerProjectileAuthority::FindSkillProjectileHitTarget(
                world,
                projectile,
                &history,
                10u,
                Vec3{ 0.f, 0.f, 0.f },
                Vec3{ 1.f, 0.f, 0.f },
                hitPos,
                hitT);
        return blinkHit == NULL_ENTITY;
    }

    bool CheckExactPreviousTickFallback()
    {
        CWorld world;
        const EntityID source = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -5.f, 0.f, 0.f });
        const EntityID target = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 0.5f, 0.f, 2.f });
        SkillProjectileComponent projectile = MakeProjectile(world, source);

        ExactTickHistory history{};
        history.hEntity = world.GetEntityHandle(target);
        history.uAvailableTick = 8u;
        history.vPosition = Vec3{ 0.5f, 0.f, -2.f };

        Vec3 hitPos{};
        f32_t hitT = 0.f;
        const EntityID hit =
            CServerProjectileAuthority::FindSkillProjectileHitTarget(
                world,
                projectile,
                &history,
                10u,
                Vec3{ 0.f, 0.f, 0.f },
                Vec3{ 1.f, 0.f, 0.f },
                hitPos,
                hitT);
        return hit == NULL_ENTITY &&
            history.uQueryCount == 1u &&
            history.uLastRequestedTick == 9u;
    }

    bool CheckSourceAndTargetGenerationSafety()
    {
        CWorld world;
        const EntityID source = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -5.f, 0.f, 0.f });
        const EntityID target = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 1.f, 0.f, 0.f });

        SkillProjectileComponent projectile = MakeProjectile(world, source);
        projectile.targetEntity = target;
        projectile.targetHandle = world.GetEntityHandle(target);
        if (!projectile.sourceHandle.IsValid() ||
            !projectile.targetHandle.IsValid())
        {
            return false;
        }

        world.DestroyEntity(source);
        const EntityID replacementSource = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -4.f, 0.f, 0.f });
        if (replacementSource != source ||
            world.ResolveEntity(projectile.sourceHandle) != NULL_ENTITY ||
            world.GetEntityHandle(replacementSource) == projectile.sourceHandle)
        {
            return false;
        }

        world.DestroyEntity(target);
        const EntityID replacementTarget = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 2.f, 0.f, 0.f });
        return replacementTarget == target &&
            world.ResolveEntity(projectile.targetHandle) == NULL_ENTITY &&
            world.GetEntityHandle(replacementTarget) != projectile.targetHandle;
    }

    bool CheckStructureProjectileGenerationSafety()
    {
        CWorld world;
        const EntityID source = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -5.f, 0.f, 0.f });
        const EntityID target = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 1.f, 0.f, 0.f });

        StructureProjectileComponent projectile{};
        projectile.sourceEntity = source;
        projectile.targetEntity = target;
        projectile.sourceHandle = world.GetEntityHandle(source);
        projectile.targetHandle = world.GetEntityHandle(target);

        world.DestroyEntity(target);
        const EntityID replacementTarget = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 2.f, 0.f, 0.f });
        if (replacementTarget != target ||
            world.ResolveEntity(projectile.targetHandle) != NULL_ENTITY ||
            world.GetEntityHandle(replacementTarget) == projectile.targetHandle)
        {
            return false;
        }

        world.DestroyEntity(source);
        const EntityID replacementSource = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -4.f, 0.f, 0.f });
        return replacementSource == source &&
            world.ResolveEntity(projectile.sourceHandle) == NULL_ENTITY &&
            world.GetEntityHandle(replacementSource) != projectile.sourceHandle;
    }

    bool CheckDeterministicTargetTieBreakAndGenerationSafety()
    {
        CWorld world;
        const EntityID source = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -5.f, 0.f, 0.f });
        const EntityID first = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 1.f, 0.f, 0.f });
        const EntityID second = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 1.f, 0.f, 0.f });
        SkillProjectileComponent projectile = MakeProjectile(world, source);
        projectile.unitHitPolicy = eProjectileUnitHitPolicy::Pierce;

        Vec3 hitPos{};
        f32_t hitT = 0.f;
        const EntityID tiedHit =
            CServerProjectileAuthority::FindSkillProjectileHitTarget(
                world,
                projectile,
                nullptr,
                1u,
                Vec3{},
                Vec3{ 2.f, 0.f, 0.f },
                hitPos,
                hitT);
        if (tiedHit != first)
            return false;

        projectile.hitEntities[0] = world.GetEntityHandle(first);
        projectile.hitEntityCount = 1u;
        world.DestroyEntity(first);
        const EntityID replacement = SpawnChampionTarget(
            world, eTeam::Red, Vec3{ 0.5f, 0.f, 0.f });
        if (replacement != first)
            return false;

        world.GetComponent<TransformComponent>(second).SetPosition(
            Vec3{ 5.f, 0.f, 5.f });
        hitPos = {};
        hitT = 0.f;
        const EntityID replacementHit =
            CServerProjectileAuthority::FindSkillProjectileHitTarget(
                world,
                projectile,
                nullptr,
                2u,
                Vec3{},
                Vec3{ 2.f, 0.f, 0.f },
                hitPos,
                hitT);
        return replacementHit == replacement &&
            world.GetEntityHandle(replacement) != projectile.hitEntities[0];
    }

    bool CheckBarrierSweepAndOptOut()
    {
        CWorld world;
        const EntityID source = SpawnChampionTarget(
            world, eTeam::Blue, Vec3{ -5.f, 0.f, 0.f });
        const EntityID barrierEntity = world.CreateEntity();
        ProjectileBarrierComponent barrier{};
        barrier.sourceTeam = eTeam::Red;
        barrier.previousCenter = Vec3{};
        barrier.center = Vec3{};
        barrier.direction = Vec3{ 0.f, 0.f, 1.f };
        barrier.halfLength = 1.6f;
        barrier.halfThickness = 0.1f;
        world.AddComponent<ProjectileBarrierComponent>(barrierEntity, barrier);

        SkillProjectileComponent projectile = MakeProjectile(world, source);
        Vec3 hitPos{};
        f32_t hitT = 0.f;
        if (!CServerProjectileAuthority::FindProjectileBarrierHit(
                world,
                projectile,
                Vec3{ 0.f, 0.f, -2.f },
                Vec3{ 0.f, 0.f, 2.f },
                hitPos,
                hitT) ||
            hitT <= 0.f || hitT >= 1.f)
        {
            return false;
        }

        projectile.bBlockedByProjectileBarriers = false;
        return !CServerProjectileAuthority::FindProjectileBarrierHit(
            world,
            projectile,
            Vec3{ 0.f, 0.f, -2.f },
            Vec3{ 0.f, 0.f, 2.f },
            hitPos,
            hitT);
    }

    bool CheckQuantizationAndTypedContact()
    {
        const u32_t quantized =
            CServerProjectileAuthority::QuantizeContactT(0.33333334f);
        const f32_t dequantized =
            CServerProjectileAuthority::DequantizeContactT(quantized);
        if (!std::isfinite(dequantized) ||
            dequantized < 0.f || dequantized > 1.f ||
            CServerProjectileAuthority::QuantizeContactT(dequantized) !=
                quantized)
        {
            return false;
        }

        const ReplicatedEventComponent event =
            CServerProjectileAuthority::BuildProjectileHitEvent(
                1u,
                2u,
                3u,
                44u,
                Vec3{ 4.f, 5.f, 6.f },
                10u,
                ProjectileContactReason::Barrier,
                7u,
                false);
        return event.kind == eReplicatedEventKind::ProjectileHit &&
            event.sourceEntity == 1u &&
            event.targetEntity == 2u &&
            event.projectileEntity == 3u &&
            event.eContactReason == ProjectileContactReason::Barrier &&
            event.uContactOrdinal == 7u &&
            !event.bDestroyed;
    }
}

int main()
{
    const bool_t pass =
        CheckMovingTargetRelativeCcdAndDiscontinuity() &&
        CheckExactPreviousTickFallback() &&
        CheckSourceAndTargetGenerationSafety() &&
        CheckStructureProjectileGenerationSafety() &&
        CheckDeterministicTargetTieBreakAndGenerationSafety() &&
        CheckBarrierSweepAndOptOut() &&
        CheckQuantizationAndTypedContact();

    std::printf(
        "[ServerProjectileAuthorityContract] %s: relative CCD, blink discontinuity, exact T-1, skill/structure lifetime identity, barrier, typed contact\n",
        pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
