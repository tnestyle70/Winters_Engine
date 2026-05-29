Session - 제드 Q/W/E/R을 현재 폴더형 GameSim 구조와 Data/LoL/FX WFX 루트로 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ProjectileKindComponent.h

기존 코드:

```cpp
    LeeSinQ = 20,
    KindredArrow = 21,
    PROJECTILE_END
```

아래로 교체:

```cpp
    LeeSinQ = 20,
    KindredArrow = 21,
    ZedShuriken = 22,
    PROJECTILE_END
```

1-2. C:/Users/user/Desktop/Winters/Engine/Public/ECS/Components/GameplayComponents.h

기존 코드:

```cpp
inline constexpr u32_t kGameplayStateInvisibleFlag = 1u << 3;
inline constexpr u32_t kGameplayStateUntargetableFlag = 1u << 4;
inline constexpr u32_t kGameplayStateCannotMoveFlag = 1u << 5;
inline constexpr u32_t kGameplayStateCannotAttackFlag = 1u << 6;
inline constexpr u32_t kGameplayStateCannotCastFlag = 1u << 7;
```

아래로 교체:

```cpp
inline constexpr u32_t kGameplayStateInvisibleFlag = 1u << 3;
inline constexpr u32_t kGameplayStateUntargetableFlag = 1u << 4;
inline constexpr u32_t kGameplayStateCannotMoveFlag = 1u << 5;
inline constexpr u32_t kGameplayStateCannotAttackFlag = 1u << 6;
inline constexpr u32_t kGameplayStateCannotCastFlag = 1u << 7;
inline constexpr u32_t kGameplayStateRenderHiddenFlag = 1u << 8;
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SnapshotStateFlags.h

기존 코드:

```cpp
inline constexpr u32_t kSnapshotStateDeadFlag = 1u << 0;
inline constexpr u32_t kSnapshotStateMovingFlag = 1u << 1;
inline constexpr u32_t kSnapshotStateAttackFlag = 1u << 2;
inline constexpr u32_t kSnapshotStateInvisibleFlag = 1u << 3;
inline constexpr u32_t kSnapshotStateViegoSoulFlag = 1u << 4;
```

아래로 교체:

```cpp
inline constexpr u32_t kSnapshotStateDeadFlag = 1u << 0;
inline constexpr u32_t kSnapshotStateMovingFlag = 1u << 1;
inline constexpr u32_t kSnapshotStateAttackFlag = 1u << 2;
inline constexpr u32_t kSnapshotStateInvisibleFlag = 1u << 3;
inline constexpr u32_t kSnapshotStateViegoSoulFlag = 1u << 4;
inline constexpr u32_t kSnapshotStateRenderHiddenFlag = 1u << 5;
inline constexpr u32_t kSnapshotStateUntargetableFlag = 1u << 6;
```

1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

```cpp
        if (world.HasComponent<GameplayStateComponent>(entity) &&
            (world.GetComponent<GameplayStateComponent>(entity).stateFlags &
                kGameplayStateInvisibleFlag) != 0u)
        {
            stateFlags |= kSnapshotStateInvisibleFlag;
        }
```

아래로 교체:

```cpp
        if (world.HasComponent<GameplayStateComponent>(entity))
        {
            const u32_t gameplayFlags =
                world.GetComponent<GameplayStateComponent>(entity).stateFlags;

            if ((gameplayFlags & kGameplayStateInvisibleFlag) != 0u)
                stateFlags |= kSnapshotStateInvisibleFlag;
            if ((gameplayFlags & kGameplayStateRenderHiddenFlag) != 0u)
                stateFlags |= kSnapshotStateRenderHiddenFlag;
            if ((gameplayFlags & kGameplayStateUntargetableFlag) != 0u)
                stateFlags |= kSnapshotStateUntargetableFlag;
        }
```

1-5. C:/Users/user/Desktop/Winters/Client/Public/Scene/RenderVisibilityFilter.h

기존 코드:

```cpp
        if (!world.HasComponent<SpatialAgentComponent>(entity))
            return true;

        const SpatialAgentComponent& agent = world.GetComponent<SpatialAgentComponent>(entity);
        if (agent.team == localTeam)
            return true;
```

아래로 교체:

```cpp
        if (!world.HasComponent<SpatialAgentComponent>(entity))
            return true;

        if (world.HasComponent<ReplicatedStateComponent>(entity) &&
            (world.GetComponent<ReplicatedStateComponent>(entity).stateFlags &
                kSnapshotStateRenderHiddenFlag) != 0u)
        {
            return false;
        }

        const SpatialAgentComponent& agent = world.GetComponent<SpatialAgentComponent>(entity);
        if (agent.team == localTeam)
            return true;
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/GameplayQuery.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include "Shared/GameSim/Components/ViegoSoulComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"

#include "Shared/GameSim/Components/ViegoSoulComponent.h"
```

기존 코드:

```cpp
    if (world.HasComponent<HealthComponent>(target))
    {
        const auto& health = world.GetComponent<HealthComponent>(target);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return false;
    }

    if (world.HasComponent<ViegoSoulComponent>(target))
```

아래로 교체:

```cpp
    if (world.HasComponent<HealthComponent>(target))
    {
        const auto& health = world.GetComponent<HealthComponent>(target);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return false;
    }

    if (world.HasComponent<ReplicatedStateComponent>(target))
    {
        const u32_t flags = world.GetComponent<ReplicatedStateComponent>(target).stateFlags;
        if ((flags & (kSnapshotStateInvisibleFlag |
            kSnapshotStateRenderHiddenFlag |
            kSnapshotStateUntargetableFlag)) != 0u)
        {
            return false;
        }
    }

    if (world.HasComponent<ViegoSoulComponent>(target))
```

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ZedSimComponent.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "GameContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct ZedShadowState
{
    Vec3 vPosition{};
    Vec3 vDirection{ 0.f, 0.f, 1.f };
    f32_t fRemainingSec = 0.f;
    bool_t bActive = false;
};

struct ZedDeathMarkState
{
    EntityID target = NULL_ENTITY;
    eTeam sourceTeam = eTeam::Neutral;
    u8_t rank = 1;
    f32_t fRemainingSec = 0.f;
    bool_t bActive = false;
};

struct ZedSimComponent
{
    ZedShadowState shadow{};
    ZedDeathMarkState deathMark{};
};
```

1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Zed/ZedGameSim.h

기존 파일 전체가 비어 있음. 아래로 교체:

```cpp
#pragma once

class CWorld;
struct TickContext;

namespace ZedGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
```

1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Zed/ZedGameSim.cpp

기존 파일 전체가 비어 있음. 아래로 교체:

```cpp
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/ZedSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace
{
    constexpr f32_t kZedShadowDurationSec = 4.0f;
    constexpr f32_t kZedRHiddenDurationSec = 0.65f;
    constexpr f32_t kZedRMarkDelaySec = 3.0f;
    constexpr f32_t kZedQSpeed = 26.0f;
    constexpr f32_t kZedQRange = 9.0f;
    constexpr f32_t kZedQHitRadius = 0.55f;
    constexpr f32_t kZedERadius = 2.55f;
    constexpr f32_t kZedWRange = 6.5f;

    u16_t MakeZedSkillId(u8_t slot)
    {
        return static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::ZED) << 8) |
            static_cast<u32_t>(slot));
    }

    u16_t MakeEffectFlags(u8_t slot, u8_t rank, u8_t stage)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (static_cast<u16_t>(rank & 0x0fu) << 8) |
            static_cast<u16_t>(slot));
    }

    f32_t Ranked(f32_t base, f32_t perRank, u8_t rank)
    {
        const u8_t clampedRank = std::max<u8_t>(1u, rank);
        return base + perRank * static_cast<f32_t>(clampedRank - 1u);
    }

    ZedSimComponent& EnsureZedState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<ZedSimComponent>(caster))
            world.AddComponent<ZedSimComponent>(caster, ZedSimComponent{});
        return world.GetComponent<ZedSimComponent>(caster);
    }

    Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();
        return Vec3{};
    }

    Vec3 ResolveCommandDirection(const GameplayHookContext& ctx, const Vec3& origin)
    {
        if (ctx.pCommand)
        {
            const Vec3 direct = WintersMath::NormalizeXZOrZero(ctx.pCommand->direction);
            if (direct.x != 0.f || direct.z != 0.f)
                return direct;

            const Vec3 toGround = WintersMath::DirectionXZ(origin, ctx.pCommand->groundPos, Vec3{});
            if (toGround.x != 0.f || toGround.z != 0.f)
                return toGround;
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    Vec3 ResolveGroundTarget(const GameplayHookContext& ctx, const Vec3& origin, const Vec3& direction)
    {
        Vec3 desired = ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
        if (WintersMath::DistanceSqXZ(origin, desired) <= 0.0001f)
        {
            desired = Vec3{
                origin.x + direction.x * kZedWRange,
                origin.y,
                origin.z + direction.z * kZedWRange
            };
        }

        const f32_t dx = desired.x - origin.x;
        const f32_t dz = desired.z - origin.z;
        const f32_t distSq = dx * dx + dz * dz;
        const f32_t rangeSq = kZedWRange * kZedWRange;
        if (distSq > rangeSq && distSq > 0.0001f)
        {
            const f32_t scale = kZedWRange / std::sqrt(distSq);
            desired.x = origin.x + dx * scale;
            desired.z = origin.z + dz * scale;
        }

        if (ctx.pTickCtx && ctx.pTickCtx->pWalkable)
        {
            Vec3 clamped = desired;
            if (ctx.pTickCtx->pWalkable->TryClampMoveSegmentXZ(origin, desired, 0.45f, clamped))
                desired = clamped;
        }

        return desired;
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    void RotateToward(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;

        const Vec3 dir = WintersMath::NormalizeXZOrZero(direction);
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawFromDirection(eChampion::ZED, dir),
            rot.z });
    }

    void EmitZedEffect(CWorld& world, const GameplayHookContext& ctx,
        EntityID target, u8_t slot, u8_t stage, const Vec3& position,
        const Vec3& direction, u16_t durationMs)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = ctx.casterEntity;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::BA_CastFrame + slot);
        event.slot = slot;
        event.rank = ctx.skillRank;
        event.flags = MakeEffectFlags(slot, ctx.skillRank, stage);
        event.position = position;
        event.direction = direction;
        event.durationMs = durationMs;
        event.startTick = ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0;
        EnqueueReplicatedEvent(world, event);
    }

    EntityID SpawnQProjectile(CWorld& world, EntityID caster, eTeam casterTeam,
        const Vec3& origin, const Vec3& direction, u8_t rank)
    {
        const Vec3 dir = WintersMath::NormalizeXZ(direction);
        const Vec3 start{ origin.x, origin.y + 1.1f, origin.z };

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = caster;
        projectile.sourceTeam = casterTeam;
        projectile.kind = eProjectileKind::ZedShuriken;
        projectile.skillID = MakeZedSkillId(static_cast<u8_t>(eSkillSlot::Q));
        projectile.rank = rank;
        projectile.currentPos = start;
        projectile.direction = dir;
        projectile.speed = kZedQSpeed;
        projectile.maxDistance = kZedQRange;
        projectile.hitRadius = kZedQHitRadius;
        projectile.damage = Ranked(75.f, 25.f, rank);

        const EntityID projectileEntity = world.CreateEntity();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(start);
        world.AddComponent<TransformComponent>(projectileEntity, transform);
        return projectileEntity;
    }

    void EnqueuePhysicalDamage(CWorld& world, EntityID source, EntityID target,
        eTeam sourceTeam, f32_t amount, u8_t slot, u8_t rank)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            !GameplayStateQuery::CanReceiveDamage(world, source, target))
        {
            return;
        }

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = MakeZedSkillId(slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    void EnqueueAreaDamage(CWorld& world, EntityID caster, eTeam casterTeam,
        const Vec3& origin, f32_t radius, f32_t amount, u8_t slot, u8_t rank)
    {
        const f32_t radiusSq = radius * radius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == caster || champion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > radiusSq)
                        return;
                    EnqueuePhysicalDamage(world, caster, target, casterTeam, amount, slot, rank);
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID target, MinionComponent& minion, TransformComponent& transform)
                {
                    if (minion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > radiusSq)
                        return;
                    EnqueuePhysicalDamage(world, caster, target, casterTeam, amount, slot, rank);
                }));
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 origin = ResolveEntityPosition(world, ctx.casterEntity);
        const Vec3 dir = ResolveCommandDirection(ctx, origin);
        ZedSimComponent& state = EnsureZedState(world, ctx.casterEntity);

        SpawnQProjectile(world, ctx.casterEntity, ctx.casterTeam, origin, dir, ctx.skillRank);
        if (state.shadow.bActive && state.shadow.fRemainingSec > 0.f)
            SpawnQProjectile(world, ctx.casterEntity, ctx.casterTeam, state.shadow.vPosition, dir, ctx.skillRank);
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        auto& transform = world.GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 origin = transform.GetPosition();
        const Vec3 dir = ResolveCommandDirection(ctx, origin);
        ZedSimComponent& state = EnsureZedState(world, ctx.casterEntity);

        if (ctx.pCommand->itemId == 2u && state.shadow.bActive)
        {
            const Vec3 oldCasterPos = origin;
            transform.SetPosition(state.shadow.vPosition);
            state.shadow.vPosition = oldCasterPos;
            state.shadow.vDirection = dir;
            state.shadow.fRemainingSec = kZedShadowDurationSec;
            state.shadow.bActive = true;
            ClearMove(world, ctx.casterEntity);
            RotateToward(world, ctx.casterEntity, dir);
            EmitZedEffect(world, ctx, NULL_ENTITY, static_cast<u8_t>(eSkillSlot::W),
                2u, oldCasterPos, dir, 650u);
            return;
        }

        state.shadow.vPosition = ResolveGroundTarget(ctx, origin, dir);
        state.shadow.vDirection = dir;
        state.shadow.fRemainingSec = kZedShadowDurationSec;
        state.shadow.bActive = true;
        RotateToward(world, ctx.casterEntity, dir);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        CWorld& world = *ctx.pWorld;
        ZedSimComponent& state = EnsureZedState(world, ctx.casterEntity);
        const Vec3 origin = ResolveEntityPosition(world, ctx.casterEntity);
        const f32_t damage = Ranked(65.f, 25.f, ctx.skillRank);

        EnqueueAreaDamage(world, ctx.casterEntity, ctx.casterTeam, origin,
            kZedERadius, damage, static_cast<u8_t>(eSkillSlot::E), ctx.skillRank);

        if (state.shadow.bActive && state.shadow.fRemainingSec > 0.f)
        {
            EnqueueAreaDamage(world, ctx.casterEntity, ctx.casterTeam, state.shadow.vPosition,
                kZedERadius, damage, static_cast<u8_t>(eSkillSlot::E), ctx.skillRank);
            EmitZedEffect(world, ctx, NULL_ENTITY, static_cast<u8_t>(eSkillSlot::E),
                1u, state.shadow.vPosition, state.shadow.vDirection, 500u);
        }
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || ctx.pCommand->targetEntity == NULL_ENTITY)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (!world.IsAlive(target) || !world.HasComponent<HealthComponent>(target))
            return;

        ZedSimComponent& state = EnsureZedState(world, ctx.casterEntity);

        StatusEffectApplyDesc hidden{};
        hidden.effectId = eStatusEffectId::ZedDeathMark;
        hidden.sourceEntity = ctx.casterEntity;
        hidden.stackGroup = MakeZedSkillId(static_cast<u8_t>(eSkillSlot::R));
        hidden.stateFlags =
            kGameplayStateInvisibleFlag |
            kGameplayStateUntargetableFlag |
            kGameplayStateRenderHiddenFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        hidden.fDurationSec = kZedRHiddenDurationSec;
        GameplayStatus::ApplyStatusEffect(world, ctx.casterEntity, hidden);

        StatusEffectApplyDesc mark{};
        mark.effectId = eStatusEffectId::ZedDeathMark;
        mark.sourceEntity = ctx.casterEntity;
        mark.stackGroup = MakeZedSkillId(static_cast<u8_t>(eSkillSlot::R));
        mark.fDurationSec = kZedRMarkDelaySec;
        GameplayStatus::ApplyStatusEffect(world, target, mark);

        state.deathMark.target = target;
        state.deathMark.sourceTeam = ctx.casterTeam;
        state.deathMark.rank = ctx.skillRank;
        state.deathMark.fRemainingSec = kZedRMarkDelaySec;
        state.deathMark.bActive = true;
    }
}

namespace ZedGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[ZedSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<ZedSimComponent>(
            std::function<void(EntityID, ZedSimComponent&)>(
                [&](EntityID caster, ZedSimComponent& state)
                {
                    if (state.shadow.bActive)
                    {
                        state.shadow.fRemainingSec -= tc.fDt;
                        if (state.shadow.fRemainingSec <= 0.f)
                            state.shadow = ZedShadowState{};
                    }

                    if (!state.deathMark.bActive)
                        return;

                    state.deathMark.fRemainingSec -= tc.fDt;
                    if (state.deathMark.fRemainingSec > 0.f)
                        return;

                    const EntityID target = state.deathMark.target;
                    const u8_t rank = state.deathMark.rank;
                    const eTeam sourceTeam = state.deathMark.sourceTeam;
                    state.deathMark = ZedDeathMarkState{};

                    if (target == NULL_ENTITY ||
                        !world.IsAlive(target) ||
                        !world.HasComponent<HealthComponent>(target) ||
                        !GameplayStateQuery::CanReceiveDamage(world, caster, target))
                    {
                        return;
                    }

                    DamageRequest request{};
                    request.source = caster;
                    request.target = target;
                    request.sourceTeam = sourceTeam;
                    request.type = eDamageType::Physical;
                    request.skillId = MakeZedSkillId(static_cast<u8_t>(eSkillSlot::R));
                    request.rank = rank;
                    request.targetMissingHpRatioOverride = 0.25f + 0.05f * static_cast<f32_t>(rank - 1u);
                    EnqueueDamageRequest(world, request);

                    GameplayHookContext fxCtx{};
                    fxCtx.pWorld = &world;
                    fxCtx.casterEntity = caster;
                    fxCtx.casterTeam = sourceTeam;
                    fxCtx.casterChampion = eChampion::ZED;
                    fxCtx.skillRank = rank;
                    fxCtx.pTickCtx = &tc;

                    EmitZedEffect(world, fxCtx, target, static_cast<u8_t>(eSkillSlot::R),
                        2u, ResolveEntityPosition(world, target), Vec3{ 0.f, 0.f, 1.f }, 700u);
                }));
    }
}
```

1-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

`kChampionSkillTimingTable`의 Zed W stage1 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        { eChampion::ZED, 2, 1, 0.50f, 1.00f },
```

아래에 추가:

```cpp
        { eChampion::ZED, 2, 2, 0.35f, 1.00f },
```

`kChampionSkillStageTable`의 LeeSin stage rows 아래에 추가:

기존 코드:

```cpp
        { eChampion::LEESIN, 1, 2, 3.00f },
        { eChampion::LEESIN, 2, 2, 3.00f },
        { eChampion::LEESIN, 3, 2, 3.00f },
```

아래에 추가:

```cpp
        { eChampion::ZED, 2, 2, 4.00f },
```

1-11. C:/Users/user/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

Zed W 기존 코드:

```cpp
    1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
```

아래로 교체:

```cpp
    2, eTargetMode::Self, "zed_spell2_cast", 0.35f, eRotateMode::None, 4.f,
```

1-12. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
```

기존 코드:

```cpp
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Components/ZedSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
```

기존 코드:

```cpp
    ViegoGameSim::Tick(m_world, tc);
    YoneGameSim::Tick(m_world, tc);
    YasuoGameSim::Tick(m_world, tc);
```

아래로 교체:

```cpp
    ViegoGameSim::Tick(m_world, tc);
    YoneGameSim::Tick(m_world, tc);
    YasuoGameSim::Tick(m_world, tc);
    ZedGameSim::Tick(m_world, tc);
```

기존 코드:

```cpp
    ViegoGameSim::RegisterHooks();
    YoneGameSim::RegisterHooks();
    YasuoGameSim::RegisterHooks();
```

아래로 교체:

```cpp
    ViegoGameSim::RegisterHooks();
    YoneGameSim::RegisterHooks();
    YasuoGameSim::RegisterHooks();
    ZedGameSim::RegisterHooks();
```

기존 코드:

```cpp
    if (slot.champion == eChampion::MASTERYI)
        m_world.AddComponent<MasterYiSimComponent>(entity, MasterYiSimComponent{});
```

아래에 추가:

```cpp
    if (slot.champion == eChampion::ZED)
        m_world.AddComponent<ZedSimComponent>(entity, ZedSimComponent{});
```

1-13. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
        case eProjectileKind::MysticShot:
        case eProjectileKind::LeeSinQ:
            return true;
```

아래로 교체:

```cpp
        case eProjectileKind::MysticShot:
        case eProjectileKind::LeeSinQ:
        case eProjectileKind::ZedShuriken:
            return true;
```

기존 코드:

```cpp
        case eProjectileKind::LeeSinQ:
            return "LeeSin.Q.Projectile";
```

아래에 추가:

```cpp
        case eProjectileKind::ZedShuriken:
            return "Zed.Q.Projectile";
```

기존 코드:

```cpp
        case eProjectileKind::LeeSinQ:
            return "LeeSin.Q.Hit";
```

아래에 추가:

```cpp
        case eProjectileKind::ZedShuriken:
            return "Zed.Q.Hit";
```

기존 코드:

```cpp
    const bool_t bShouldSpawnGenericProjectile =
        !bChampionProjectileVisual ||
        (static_cast<eProjectileKind>(ev->kind()) == eProjectileKind::LeeSinQ && !bPlayedProjectileWfxCue);
```

아래로 교체:

```cpp
    const eProjectileKind projectileKind = static_cast<eProjectileKind>(ev->kind());
    const bool_t bShouldSpawnGenericProjectile =
        !bChampionProjectileVisual ||
        ((projectileKind == eProjectileKind::LeeSinQ ||
            projectileKind == eProjectileKind::ZedShuriken) &&
            !bPlayedProjectileWfxCue);
```

1-14. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Zed/ZedFxPresets.h

기존 코드:

```cpp
namespace ZedFx
{
    void SpawnQRazor(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWShadow(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fDuration);
    void SpawnESlash(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnRMark(CWorld& world, EntityID target, f32_t fLifetime);
}
```

아래로 교체:

```cpp
namespace ZedFx
{
    void SpawnQRazor(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWShadow(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fDuration);
    void SpawnWShadowSwap(CWorld& world, const Vec3& groundPos, const Vec3& dir, f32_t fLifetime);
    void SpawnESlash(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnESlashAt(CWorld& world, const Vec3& groundPos, f32_t fLifetime);
    void SpawnRMark(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnRPopAt(CWorld& world, const Vec3& groundPos, f32_t fLifetime);
}
```

1-15. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Zed/Zed_Skills.cpp

기존 파일 전체를 아래로 교체:

```cpp
#include "GameObject/Champion/Zed/Zed_Skills.h"

#include "ECS/Components/TransformComponent.h"
#include "GameObject/Champion/Zed/ZedFxPresets.h"

#include <cmath>

namespace
{
    bool_t HasEventPosition(const Vec3& pos)
    {
        return (std::fabs(pos.x) + std::fabs(pos.y) + std::fabs(pos.z)) > 0.001f;
    }

    bool_t IsAwayFromCaster(CWorld& world, EntityID caster, const Vec3& pos)
    {
        if (caster == NULL_ENTITY || !world.HasComponent<TransformComponent>(caster))
            return false;
        const Vec3 casterPos = world.GetComponent<TransformComponent>(caster).GetPosition();
        return WintersMath::DistanceSqXZ(casterPos, pos) > 0.04f;
    }
}

namespace Zed
{
    void OnCastFrame(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pDef)
            return;

        const u8_t slot = ctx.pDef->slot;
        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            if (ctx.pCommand && ctx.pCommand->targetEntityId != NULL_ENTITY && ctx.applyTargetDamage)
                ctx.applyTargetDamage(ctx.pCommand->targetEntityId, 55.f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::Q))
        {
            if (ctx.applyTargetDamage)
            {
                const Vec3 dir = ctx.pCommand ? ctx.pCommand->direction : Vec3{ 0.f, 0.f, -1.f };
                ZedFx::SpawnQRazor(*ctx.pWorld, ctx.casterEntity, dir, 0.6f);
            }
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::W))
        {
            const Vec3 groundPos = ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
            const Vec3 dir = ctx.pCommand ? ctx.pCommand->direction : Vec3{ 0.f, 0.f, 1.f };
            if (ctx.skillStage >= 2u)
                ZedFx::SpawnWShadowSwap(*ctx.pWorld, groundPos, dir, 0.65f);
            else
                ZedFx::SpawnWShadow(*ctx.pWorld, ctx.casterEntity, groundPos, 4.f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            const Vec3 groundPos = ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
            if (HasEventPosition(groundPos) && IsAwayFromCaster(*ctx.pWorld, ctx.casterEntity, groundPos))
                ZedFx::SpawnESlashAt(*ctx.pWorld, groundPos, 0.5f);
            else
                ZedFx::SpawnESlash(*ctx.pWorld, ctx.casterEntity, 0.5f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::R))
        {
            const Vec3 groundPos = ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
            if (ctx.skillStage >= 2u)
            {
                ZedFx::SpawnRPopAt(*ctx.pWorld, groundPos, 0.7f);
                return;
            }

            if (!ctx.pCommand || ctx.pCommand->targetEntityId == NULL_ENTITY)
                return;

            ZedFx::SpawnRMark(*ctx.pWorld, ctx.pCommand->targetEntityId, 3.0f);
            if (ctx.applyTargetDamage)
                ctx.applyTargetDamage(ctx.pCommand->targetEntityId, 240.f);
        }
    }
}
```

1-16. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Zed/ZedFxPresets.cpp

기존 파일 전체를 아래로 교체:

```cpp
#include "GameObject/Champion/Zed/ZedFxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxSystem.h"

#include <cmath>

namespace
{
    constexpr const wchar_t* kPathQRazorTex =
        L"Client/Bin/Resource/Texture/Character/Zed/particles/zed_shuriken_tx.png";
    constexpr const wchar_t* kPathWShadowTex =
        L"Client/Bin/Resource/Texture/Character/Zed/particles/zed_w_groundshadow_proj.png";
    constexpr const wchar_t* kPathWShadowWispsTex =
        L"Client/Bin/Resource/Texture/Character/Zed/particles/zed_shadowwisps.png";
    constexpr const wchar_t* kPathESlashTex =
        L"Client/Bin/Resource/Texture/Character/Zed/particles/zed_e_slash.png";
    constexpr const wchar_t* kPathRMarkTex =
        L"Client/Bin/Resource/Texture/Character/Zed/particles/zed_r_marker.png";

    Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return Vec3{};
    }

    bool_t PlayCue(CWorld& world, const char* cue, const Vec3& pos,
        const Vec3& dir, EntityID attachTo, f32_t lifetime)
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = WintersMath::NormalizeXZ(dir);
        fx.attachTo = attachTo;
        fx.bOverrideLifetime = lifetime > 0.f;
        fx.fLifetimeOverride = lifetime;
        return CFxCuePlayer::Play(world, cue, fx) != NULL_ENTITY;
    }

    bool_t HasGroundPos(const Vec3& groundPos)
    {
        return (std::fabs(groundPos.x) +
            std::fabs(groundPos.y) +
            std::fabs(groundPos.z)) > 0.001f;
    }
}

void ZedFx::SpawnQRazor(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime)
{
    if (owner == NULL_ENTITY)
        return;

    const Vec3 ownerPos = ResolveEntityPosition(world, owner);
    const Vec3 forward = WintersMath::NormalizeXZ(dir);
    const Vec3 spawnPos{
        ownerPos.x + forward.x * 0.8f,
        ownerPos.y + 1.25f,
        ownerPos.z + forward.z * 0.8f
    };

    FxCueContext cue{};
    cue.vWorldPos = spawnPos;
    cue.vForward = forward;
    cue.vVelocity = { forward.x * 22.f, 0.f, forward.z * 22.f };
    cue.bOverrideVelocity = true;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    if (CFxCuePlayer::Play(world, "Zed.Q.Projectile", cue) != NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.vWorldPos = spawnPos;
    fx.vVelocity = cue.vVelocity;
    fx.texturePath = kPathQRazorTex;
    fx.fWidth = 1.2f;
    fx.fHeight = 1.2f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.f, 1.f, 1.f, 1.f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.4f;
    fx.bBlockableByWindWall = true;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnWShadow(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fDuration)
{
    if (owner == NULL_ENTITY)
        return;

    const Vec3 ownerPos = ResolveEntityPosition(world, owner);
    const Vec3 spawnPos = HasGroundPos(groundPos) ? groundPos : ownerPos;
    if (PlayCue(world, "Zed.W.Shadow", spawnPos, Vec3{ 0.f, 0.f, 1.f }, NULL_ENTITY, fDuration))
        return;

    FxBillboardComponent ground{};
    ground.vWorldPos = { spawnPos.x, spawnPos.y + 0.06f, spawnPos.z };
    ground.texturePath = kPathWShadowTex;
    ground.renderType = eFxRenderType::GroundDecal;
    ground.fWidth = 3.8f;
    ground.fHeight = 3.8f;
    ground.bBillboard = false;
    ground.fLifetime = fDuration;
    ground.vColor = { 1.f, 1.f, 1.f, 0.85f };
    ground.blendMode = eBlendPreset::AlphaBlend;
    ground.fFadeIn = 0.05f;
    ground.fFadeOut = 0.25f;
    CFxSystem::Spawn(world, ground);

    FxBillboardComponent wisps{};
    wisps.vWorldPos = { spawnPos.x, spawnPos.y + 1.2f, spawnPos.z };
    wisps.texturePath = kPathWShadowWispsTex;
    wisps.fWidth = 2.0f;
    wisps.fHeight = 2.0f;
    wisps.bBillboard = true;
    wisps.fLifetime = fDuration;
    wisps.vColor = { 0.55f, 0.65f, 1.f, 0.95f };
    wisps.blendMode = eBlendPreset::Additive;
    wisps.fFadeIn = 0.08f;
    wisps.fFadeOut = 0.35f;
    CFxSystem::Spawn(world, wisps);
}

void ZedFx::SpawnWShadowSwap(CWorld& world, const Vec3& groundPos, const Vec3& dir, f32_t fLifetime)
{
    PlayCue(world, "Zed.W.Swap", groundPos, dir, NULL_ENTITY, fLifetime);
}

void ZedFx::SpawnESlash(CWorld& world, EntityID owner, f32_t fLifetime)
{
    if (owner == NULL_ENTITY)
        return;

    const Vec3 pos = ResolveEntityPosition(world, owner);
    if (PlayCue(world, "Zed.E.Slash", pos, Vec3{ 0.f, 0.f, 1.f }, owner, fLifetime))
        return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathESlashTex;
    fx.fWidth = 2.5f;
    fx.fHeight = 2.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.0f, 0.4f, 0.4f, 1.0f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fLifetime * 0.3f;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnESlashAt(CWorld& world, const Vec3& groundPos, f32_t fLifetime)
{
    if (PlayCue(world, "Zed.E.Slash", groundPos, Vec3{ 0.f, 0.f, 1.f }, NULL_ENTITY, fLifetime))
        return;

    FxBillboardComponent fx{};
    fx.vWorldPos = { groundPos.x, groundPos.y + 1.0f, groundPos.z };
    fx.texturePath = kPathESlashTex;
    fx.fWidth = 2.5f;
    fx.fHeight = 2.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.0f, 0.4f, 0.4f, 1.0f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fLifetime * 0.3f;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnRMark(CWorld& world, EntityID target, f32_t fLifetime)
{
    if (target == NULL_ENTITY)
        return;

    const Vec3 pos = ResolveEntityPosition(world, target);
    if (PlayCue(world, "Zed.R.Mark", pos, Vec3{ 0.f, 0.f, 1.f }, target, fLifetime))
        return;

    FxBillboardComponent fx{};
    fx.attachTo = target;
    fx.vAttachOffset = { 0.f, 2.5f, 0.f };
    fx.texturePath = kPathRMarkTex;
    fx.fWidth = 1.5f;
    fx.fHeight = 1.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 0.9f, 0.2f, 0.3f, 1.0f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.5f;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnRPopAt(CWorld& world, const Vec3& groundPos, f32_t fLifetime)
{
    PlayCue(world, "Zed.R.Pop", groundPos, Vec3{ 0.f, 0.f, 1.f }, NULL_ENTITY, fLifetime);
}
```

1-17. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/q_projectile.wfx

새 파일:

```json
{
  "name": "Zed.Q.Projectile",
  "emitters": [
    {
      "name": "q_shuriken_core",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_shuriken_tx.png",
      "lifetime": 0.65,
      "width": 1.15,
      "height": 1.15,
      "color": [0.92, 0.96, 1.0, 0.96],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.16,
      "billboard": true,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_shadow_trail",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_base_z_redwisp.png",
      "lifetime": 0.65,
      "width": 1.35,
      "height": 0.48,
      "color": [0.65, 0.08, 0.12, 0.68],
      "attach_offset": [0.0, 0.0, -0.45],
      "fade_in": 0.02,
      "fade_out": 0.22,
      "billboard": true,
      "blockable_by_wind_wall": true
    }
  ]
}
```

1-18. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/q_hit.wfx

새 파일:

```json
{
  "name": "Zed.Q.Hit",
  "emitters": [
    {
      "name": "q_hit_slash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_e_hitslash.png",
      "lifetime": 0.32,
      "width": 1.8,
      "height": 1.8,
      "color": [1.0, 0.28, 0.32, 0.92],
      "attach_offset": [0.0, 0.75, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.18,
      "billboard": true
    }
  ]
}
```

1-19. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/w_shadow.wfx

새 파일:

```json
{
  "name": "Zed.W.Shadow",
  "emitters": [
    {
      "name": "w_ground_shadow",
      "render_type": "GroundDecal",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_w_groundshadow_proj.png",
      "lifetime": 4.0,
      "width": 3.8,
      "height": 3.8,
      "color": [0.32, 0.38, 0.62, 0.78],
      "attach_offset": [0.0, 0.06, 0.0],
      "fade_in": 0.06,
      "fade_out": 0.35,
      "billboard": false
    },
    {
      "name": "w_shadow_wisps",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_shadowwisps.png",
      "lifetime": 4.0,
      "width": 2.1,
      "height": 2.1,
      "color": [0.48, 0.6, 1.0, 0.82],
      "attach_offset": [0.0, 1.15, 0.0],
      "fade_in": 0.08,
      "fade_out": 0.35,
      "billboard": true
    }
  ]
}
```

1-20. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/w_swap.wfx

새 파일:

```json
{
  "name": "Zed.W.Swap",
  "emitters": [
    {
      "name": "w_swap_pulse",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_base_w_team_indicator_blue_mult.png",
      "lifetime": 0.55,
      "start_radius": 0.45,
      "end_radius": 2.1,
      "thickness": 0.18,
      "width": 3.4,
      "height": 3.4,
      "color": [0.48, 0.58, 1.0, 0.72],
      "attach_offset": [0.0, 0.08, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.32,
      "billboard": false
    },
    {
      "name": "w_swap_wisp",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_shadowswirls.png",
      "lifetime": 0.65,
      "width": 2.2,
      "height": 2.2,
      "color": [0.68, 0.72, 1.0, 0.82],
      "attach_offset": [0.0, 1.05, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.34,
      "billboard": true
    }
  ]
}
```

1-21. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/e_slash.wfx

새 파일:

```json
{
  "name": "Zed.E.Slash",
  "emitters": [
    {
      "name": "e_ground_ring",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_base_w_team_indicator_red.png",
      "lifetime": 0.5,
      "start_radius": 0.9,
      "end_radius": 2.7,
      "thickness": 0.24,
      "width": 5.2,
      "height": 5.2,
      "color": [0.9, 0.12, 0.16, 0.62],
      "attach_offset": [0.0, 0.08, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "billboard": false
    },
    {
      "name": "e_slash_body",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_e_slash.png",
      "lifetime": 0.5,
      "width": 3.0,
      "height": 3.0,
      "color": [1.0, 0.34, 0.36, 0.9],
      "attach_offset": [0.0, 1.05, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.2,
      "billboard": true
    }
  ]
}
```

1-22. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/r_mark.wfx

새 파일:

```json
{
  "name": "Zed.R.Mark",
  "emitters": [
    {
      "name": "r_marker",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_r_marker.png",
      "lifetime": 3.0,
      "width": 1.7,
      "height": 1.7,
      "color": [1.0, 0.18, 0.22, 0.95],
      "attach_offset": [0.0, 2.45, 0.0],
      "fade_in": 0.08,
      "fade_out": 0.45,
      "billboard": true
    },
    {
      "name": "r_preimpact_floor",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_base_r_preimpactmarker.png",
      "lifetime": 3.0,
      "width": 2.8,
      "height": 2.8,
      "color": [0.95, 0.08, 0.1, 0.5],
      "attach_offset": [0.0, 0.08, 0.0],
      "fade_in": 0.1,
      "fade_out": 0.5,
      "billboard": false
    }
  ]
}
```

1-23. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/r_pop.wfx

새 파일:

```json
{
  "name": "Zed.R.Pop",
  "emitters": [
    {
      "name": "r_pop_burst",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_basicatk_slashult.png",
      "lifetime": 0.62,
      "width": 3.4,
      "height": 3.4,
      "color": [1.0, 0.22, 0.24, 0.92],
      "attach_offset": [0.0, 1.05, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.34,
      "billboard": true
    },
    {
      "name": "r_pop_ground",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_base_r_preimpactmarker_slice.png",
      "lifetime": 0.7,
      "start_radius": 0.6,
      "end_radius": 2.4,
      "thickness": 0.2,
      "width": 4.0,
      "height": 4.0,
      "color": [0.9, 0.08, 0.12, 0.7],
      "attach_offset": [0.0, 0.08, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.42,
      "billboard": false
    }
  ]
}
```

1-24. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Champions\Yone\YoneGameSim.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Champions\Yasuo\YasuoGameSim.cpp" />
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Champions\Zed\ZedGameSim.cpp" />
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Champions\Yone\YoneGameSim.h" />
    <ClInclude Include="..\..\Shared\GameSim\Champions\Yasuo\YasuoGameSim.h" />
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Champions\Zed\ZedGameSim.h" />
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\YoneSimComponent.h" />
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\ZedSimComponent.h" />
```

1-25. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Champions\Yasuo\YasuoGameSim.cpp">
      <Filter>04. Shared\GameSim\Champions</Filter>
    </ClCompile>
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Champions\Zed\ZedGameSim.cpp">
      <Filter>04. Shared\GameSim\Champions</Filter>
    </ClCompile>
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Champions\Yasuo\YasuoGameSim.h">
      <Filter>04. Shared\GameSim\Champions</Filter>
    </ClInclude>
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Champions\Zed\ZedGameSim.h">
      <Filter>04. Shared\GameSim\Champions</Filter>
    </ClInclude>
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\YoneSimComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\ZedSimComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
```

2. 검증

검증 명령:

```powershell
git diff --check
MSBuild.exe Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

수동 확인:

```text
- Data/LoL/FX/Champions/Zed/*.wfx가 CFxCuePlayer 기본 루트 preload로 잡히는지 확인.
- Zed Q가 ZedShuriken projectile spawn event 1회당 Zed.Q.Projectile WFX 1회만 재생되는지 확인.
- W stage1 후 Q/E가 caster와 shadow 위치 양쪽에서 나가는지 확인.
- W stage2 재시전 시 caster와 shadow 위치가 swap되고 이전 caster 위치에 Zed.W.Swap WFX가 재생되는지 확인.
- R 시 caster가 render hidden + untargetable 상태가 되고, target mark가 3초 유지된 뒤 Zed.R.Pop과 잃은 체력 비례 피해가 들어가는지 확인.
```

확인 필요:

```text
- W 그림자에 실제 Zed FBX 검은 셰이더 복제체를 넣으려면 별도 renderer/material override 설계가 필요하다. 현재 shaderPath는 custom black shader path가 아니라 공유 Mesh/PBR 선택 경로다.
- Engine public header 변경 후 EngineSDK/inc 동기화가 필요한 정책이면 UpdateLib.bat 실행 필요.
```
