#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr f32_t kSylasE1DashDistance = 3.25f;
    constexpr f32_t kSylasE1DashDurationSec = 0.16f;
    constexpr f32_t kSylasE2ChainSpeed = 26.f;
    constexpr f32_t kSylasE2ChainHitRadius = 0.55f;
    constexpr f32_t kSylasE2DashGap = 0.85f;
    constexpr f32_t kSylasE2DashDurationSec = 0.22f;
    constexpr f32_t kSylasE2AirborneDurationSec = 0.75f;
    constexpr f32_t kSylasE2BaseDamage = 65.f;
    constexpr f32_t kSylasE2DamagePerRank = 25.f;

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<MoveTargetComponent>(entity))
            return;

        auto& move = world.GetComponent<MoveTargetComponent>(entity);
        move.bHasTarget = false;
        move.pathCount = 0;
        move.pathIndex = 0;
    }

    Vec3 ResolveCommandDirection(const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction, Vec3{}, 0.0001f);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }
        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            const f32_t yaw = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetRotation().y;
            return WintersMath::DirectionFromYawXZ(yaw);
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    void RotateToward(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;

        const Vec3 dir = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::SYLAS, dir, rot.y),
            rot.z
            });
    }

    void StartDash(CWorld& world, EntityID caster, const Vec3& end, f32_t duration)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();

        SylasDashComponent dash{};
        dash.vStart = start;
        dash.vEnd = end;
        dash.fDurationSec = duration;

        if (world.HasComponent<SylasDashComponent>(caster))
            world.GetComponent<SylasDashComponent>(caster) = dash;
        else
            world.AddComponent<SylasDashComponent>(caster, dash);

        RotateToward(world, caster, Vec3{ end.x - start.x, 0.f, end.z - start.z });
        ClearMove(world, caster);
    }

    void StartDirectionalDash(CWorld& world, EntityID caster, const Vec3& direction)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        const Vec3 dir = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 end{
            start.x + dir.x * kSylasE1DashDistance,
            start.y,
            start.z + dir.z * kSylasE1DashDistance
        };

        StartDash(world, caster, end, kSylasE1DashDurationSec);
    }

    void StartTargetDash(CWorld& world, EntityID caster, EntityID target)
    {
        if (!world.HasComponent<TransformComponent>(caster) ||
            target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{
            targetPos.x - start.x,
            0.f,
            targetPos.z - start.z
            }, Vec3{}, 0.0001f);

        if (dir.x == 0.f && dir.z == 0.f)
            return;

        const f32_t dx = targetPos.x - start.x;
        const f32_t dz = targetPos.z - start.z;
        const f32_t dist = std::sqrt(dx * dx + dz * dz);
        const f32_t moveDist = std::max(0.f, dist - kSylasE2DashGap);

        const Vec3 end{
            start.x + dir.x * moveDist,
            start.y,
            start.z + dir.z * moveDist
        };

        StartDash(world, caster, end, kSylasE2DashDurationSec);
    }

    void SpawnChainProjectile(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand ||
            ctx.casterEntity == NULL_ENTITY ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        origin.y += 1.f;

        const Vec3 dir = ResolveCommandDirection(ctx);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        f32_t range = ChampionGameDataDB::ResolveSkillRange(eChampion::SYLAS,
            static_cast<u8_t>(eSkillSlot::E));
        if (range <= 0.f)
            range = 6.f;

        const u8_t rank = ctx.skillRank > 0u ? ctx.skillRank : static_cast<u8_t>(1u);
        const f32_t rankBonus = static_cast<f32_t>(rank - 1u);

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = ctx.casterEntity;
        projectile.sourceTeam = ctx.casterTeam;
        projectile.kind = eProjectileKind::SylasChain;
        projectile.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::SYLAS) << 8) |
            static_cast<u8_t>(eSkillSlot::E));
        projectile.rank = rank;
        projectile.currentPos = origin;
        projectile.direction = dir;
        projectile.speed = kSylasE2ChainSpeed;
        projectile.maxDistance = range;
        projectile.hitRadius = kSylasE2ChainHitRadius;
        projectile.damage = kSylasE2BaseDamage + rankBonus * kSylasE2DamagePerRank;

        const EntityID projectileEntity = ctx.pWorld->CreateEntity();
        ctx.pWorld->AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        ctx.pWorld->AddComponent<TransformComponent>(projectileEntity, transform);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        if (ctx.pCommand->itemId == 2u)
            SpawnChainProjectile(ctx);
        else
            StartDirectionalDash(*ctx.pWorld, ctx.casterEntity, ResolveCommandDirection(ctx));
    }

    bool_t IsValidChampion(eChampion champion)
    {
        return champion != eChampion::NONE && champion != eChampion::END;
    }

    bool_t IsAliveChampion(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<ChampionComponent>(entity))
        {
            return false;
        }

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }

        return true;
    }

    eChampion ResolveHijackSourceChampion(CWorld& world, EntityID target)
    {
        if (!world.HasComponent<ChampionComponent>(target))
            return eChampion::END;

        return world.GetComponent<ChampionComponent>(target).id;
    }

    u8_t ResolveHijackRank(CWorld& world, EntityID caster, EntityID target)
    {
        const u8_t rSlot = static_cast<u8_t>(eSkillSlot::R);
        if (world.HasComponent<SkillRankComponent>(caster))
        {
            const auto& casterRanks = world.GetComponent<SkillRankComponent>(caster);
            if (casterRanks.ranks[rSlot] > 0u)
                return casterRanks.ranks[rSlot];
        }
        if (world.HasComponent<SkillRankComponent>(target))
        {
            const auto& targetRanks = world.GetComponent<SkillRankComponent>(target);
            if (targetRanks.ranks[rSlot] > 0u)
                return targetRanks.ranks[rSlot];
        }
        return 1u;
    }

    bool_t CanHijackUltimateInternal(CWorld& world, EntityID caster, EntityID target)
    {
        if (!IsAliveChampion(world, caster) || !IsAliveChampion(world, target))
            return false;
        if (!world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
        if (!GameplayStateQuery::CanBeTargetedBy(world, caster, target))
            return false;

        const auto& casterChampion = world.GetComponent<ChampionComponent>(caster);
        const auto& targetChampion = world.GetComponent<ChampionComponent>(target);
        if (casterChampion.id != eChampion::SYLAS)
            return false;
        if (casterChampion.team == targetChampion.team &&
            casterChampion.team != eTeam::Neutral)
        {
            return false;
        }

        const eChampion stolenChampion = ResolveHijackSourceChampion(world, target);
        if (!IsValidChampion(stolenChampion) || stolenChampion == eChampion::SYLAS)
            return false;

        f32_t range = ChampionGameDataDB::ResolveSkillRange(
            eChampion::SYLAS,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            range = 10.f;
        const f32_t effectiveRange =
            range +
            GameplayStateQuery::ResolveGameplayRadius(world, caster) +
            GameplayStateQuery::ResolveGameplayRadius(world, target);
        return WintersMath::DistanceSqXZ(
            world.GetComponent<TransformComponent>(caster).GetPosition(),
            world.GetComponent<TransformComponent>(target).GetPosition()) <=
            effectiveRange * effectiveRange;
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID caster = ctx.casterEntity;
        const EntityID target = ctx.pCommand->targetEntity;

        if (world.HasComponent<SpellbookOverrideComponent>(caster))
            return;
        if (!CanHijackUltimateInternal(world, caster, target))
            return;

        SpellbookOverrideComponent spellbook{};
        spellbook.sourceChampion = ResolveHijackSourceChampion(world, target);
        spellbook.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.localSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.sourceRank = ResolveHijackRank(world, caster, target);
        spellbook.fRemainingSec = 45.f;
        spellbook.bActive = true;

        world.AddComponent<SpellbookOverrideComponent>(caster, spellbook);

#if defined(_DEBUG)
        char msg[192]{};
        sprintf_s(msg,
            "[SylasHijack] caster=%u target=%u stolenChampion=%u rank=%u\n",
            static_cast<u32_t>(caster),
            static_cast<u32_t>(target),
            static_cast<u32_t>(spellbook.sourceChampion),
            static_cast<u32_t>(spellbook.sourceRank));
        WintersOutputAIDebugStringA(msg);
#endif
    }
}

namespace SylasGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<SylasDashComponent, TransformComponent>(
            std::function<void(EntityID, SylasDashComponent&, TransformComponent&)>(
                [&](EntityID entity, SylasDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

                    dash.fElapsedSec += tc.fDt;
                    f32_t t = dash.fDurationSec > 0.01f
                        ? dash.fElapsedSec / dash.fDurationSec
                        : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }

                    const Vec3 desired{
                        dash.vStart.x + (dash.vEnd.x - dash.vStart.x) * t,
                        dash.vStart.y + (dash.vEnd.y - dash.vStart.y) * t,
                        dash.vStart.z + (dash.vEnd.z - dash.vStart.z) * t
                    };

                    Vec3 guarded = desired;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable)
                    {
                        const Vec3 current = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(current, desired, 0.5f, guarded))
                        {
                            guarded = current;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(guarded, desired) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }

                        f32_t surfaceY = 0.f;
                        if (tc.pWalkable->TrySampleHeight(guarded.x, guarded.z, surfaceY))
                            guarded.y = surfaceY;
                    }

                    transform.SetPosition(guarded);
                    if (bDashBlocked && t < 1.f)
                        finishedDashes.push_back(entity);
                }));

        for (EntityID entity : finishedDashes)
            world.RemoveComponent<SylasDashComponent>(entity);
    }

    bool_t CanHijackUltimate(CWorld& world, EntityID caster, EntityID target)
    {
        return CanHijackUltimateInternal(world, caster, target);
    }

    void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
    {
        if (source == NULL_ENTITY ||
            target == NULL_ENTITY ||
            !world.IsAlive(source) ||
            !world.IsAlive(target))
        {
            return;
        }

        StartTargetDash(world, source, target);
        GameplayStatus::ApplyAirborne(
            world,
            tc,
            target,
            source,
            eChampion::SYLAS,
            eSkillSlot::E,
            kSylasE2AirborneDurationSec);
    }
}
