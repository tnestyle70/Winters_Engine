#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"

#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <cmath>
#include <functional>
#include <iostream>

namespace
{
    constexpr f32_t kIreliaRRange = 8.f;
    constexpr f32_t kIreliaRWidth = 7.5f;
    constexpr f32_t kIreliaRHalfWidth = kIreliaRWidth * 0.5f;
    constexpr f32_t kIreliaQDashDurationSec = 0.25f;
    constexpr f32_t kIreliaWRange = 6.0f;
    constexpr f32_t kIreliaWHalfWidth = 2.2f;
    constexpr f32_t kIreliaWBaseDamage = 30.f;
    constexpr f32_t kIreliaWDamagePerRank = 40.f;

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    void EnqueuePhysicalDamage(CWorld& world, EntityID source, EntityID target, eTeam team, f32_t amount, u16_t skillId, u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = team;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = skillId;
        request.rank = rank;
        EnqueueDamageRequest(world, request);
    }

    IreliaSimComponent& GetIreliaState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<IreliaSimComponent>(caster))
            world.AddComponent<IreliaSimComponent>(caster, IreliaSimComponent{});

        return world.GetComponent<IreliaSimComponent>(caster);
    }

    Vec3 Lerp(const Vec3& a, const Vec3& b, f32_t t)
    {
        return Vec3{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    Vec3 ResolveRForward(CWorld& world, EntityID caster, const GameCommand* pCommand)
    {
        if (pCommand)
        {
            const f32_t fLenSq =
                pCommand->direction.x * pCommand->direction.x +
                pCommand->direction.z * pCommand->direction.z;
            if (fLenSq > 0.0001f)
            {
                const f32_t fInvLen = 1.f / std::sqrtf(fLenSq);
                return Vec3{
                    pCommand->direction.x * fInvLen,
                    0.f,
                    pCommand->direction.z * fInvLen
                };
            }
        }

        if (world.HasComponent<TransformComponent>(caster))
        {
            const f32_t fYaw =
                world.GetComponent<TransformComponent>(caster).GetRotation().y -
                GetDefaultChampionVisualYawOffset(eChampion::IRELIA);
            return Vec3{ std::sinf(fYaw), 0.f, std::cosf(fYaw) };
        }

        return Vec3{ 0.f, 0.f, -1.f };
    }

    void EmitIreliaRHitVisual(CWorld& world, const GameplayHookContext& ctx,
        EntityID target, const Vec3& vHitPos, const Vec3& vForward)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = ctx.casterEntity;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::R_OnCastAccepted);
        event.slot = static_cast<u8_t>(eSkillSlot::R);
        event.rank = ctx.skillRank;
        event.flags = static_cast<u16_t>(
            (2u << 12) |
            (static_cast<u16_t>(ctx.skillRank & 0x0fu) << 8) |
            static_cast<u16_t>(eSkillSlot::R));
        event.position = vHitPos;
        event.direction = vForward;
        event.durationMs = 900;
        event.startTick = ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0;

        EnqueueReplicatedEvent(world, event);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        CWorld& world = *ctx.pWorld;
        const GameCommand& cmd = *ctx.pCommand;

        if (!world.HasComponent<TransformComponent>(ctx.casterEntity) ||
            cmd.targetEntity == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(cmd.targetEntity))
        {
            return;
        }

        auto& casterTf = world.GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 casterPos = casterTf.GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(cmd.targetEntity).GetPosition();

        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{ targetPos.x - casterPos.x, 0.f, targetPos.z - casterPos.z });
        const f32_t gap = 1.35f;

        Vec3 endPos{
            targetPos.x - dir.x * gap,
            casterPos.y,
            targetPos.z - dir.z * gap
        };

        IreliaSimComponent& state = GetIreliaState(world, ctx.casterEntity);
        state.dashStartPos = casterPos;
        state.dashEndPos = endPos;
        state.dashElapsedSec = 0.f;
        state.dashDurationSec = kIreliaQDashDurationSec;
        state.dashTarget = cmd.targetEntity;
        state.bDashActive = true;

        ClearMove(world, ctx.casterEntity);

        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            cmd.targetEntity,
            ctx.casterTeam,
            45.f + 25.f * static_cast<f32_t>(ctx.skillRank),
            static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u),
            ctx.skillRank);

        std::cout << "[IreliaSim] Q dash caster=" << ctx.casterEntity
            << " target=" << cmd.targetEntity
            << " start=(" << casterPos.x << "," << casterPos.y << "," << casterPos.z << ")"
            << " end=(" << endPos.x << "," << endPos.y << "," << endPos.z << ")\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        const bool_t bStage2 = ctx.pCommand != nullptr && ctx.pCommand->itemId == 2u;
        std::cout << "[IreliaSim] W " << (bStage2 ? "release" : "hold")
            << " caster=" << ctx.casterEntity << "\n";

        if (!bStage2 || !ctx.pWorld || !ctx.pCommand ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        const Vec3 origin =
            world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 forward = ResolveRForward(world, ctx.casterEntity, ctx.pCommand);
        const f32_t damage =
            kIreliaWBaseDamage + kIreliaWDamagePerRank * static_cast<f32_t>(ctx.skillRank);

        auto tryHit = [&](EntityID target, eTeam team, const Vec3& targetPos)
        {
            if (target == ctx.casterEntity || team == ctx.casterTeam)
                return;

            const Vec3 delta{ targetPos.x - origin.x, 0.f, targetPos.z - origin.z };
            const f32_t along = delta.x * forward.x + delta.z * forward.z;
            if (along < 0.f || along > kIreliaWRange)
                return;

            const Vec3 perp{
                delta.x - forward.x * along,
                0.f,
                delta.z - forward.z * along
            };
            if ((perp.x * perp.x + perp.z * perp.z) >
                kIreliaWHalfWidth * kIreliaWHalfWidth)
            {
                return;
            }

            EnqueuePhysicalDamage(
                world,
                ctx.casterEntity,
                target,
                ctx.casterTeam,
                damage,
                static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 2u),
                ctx.skillRank);
        };

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& tf)
                {
                    tryHit(entity, champion.team, tf.GetPosition());
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID entity, MinionComponent& minion, TransformComponent& tf)
                {
                    tryHit(entity, minion.team, tf.GetPosition());
                }));
    }

    void OnE(GameplayHookContext& ctx)
    {
        CWorld& world = *ctx.pWorld;
        const GameCommand& cmd = *ctx.pCommand;
        IreliaSimComponent& state = GetIreliaState(world, ctx.casterEntity);

        const bool_t bStage2 = cmd.itemId == 2u;
        if (!bStage2)
        {
            state.blade1Pos = cmd.groundPos;
            state.blade1Tick = ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0;
            state.bHasBlade1 = true;

            std::cout << "[IreliaSim] E blade1 pos=("
                << state.blade1Pos.x << "," << state.blade1Pos.y << "," << state.blade1Pos.z << ")\n";
            return;
        }

        state.blade2Pos = cmd.groundPos;
        state.blade2Tick = ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0;
        state.bHasBlade2 = true;

        if (!state.bHasBlade1)
            return;

        const Vec3 a = state.blade1Pos;
        const Vec3 b = state.blade2Pos;
        const f32_t dx = b.x - a.x;
        const f32_t dz = b.z - a.z;
        const f32_t segLenSq = dx * dx + dz * dz + 0.000001f;
        constexpr f32_t kBeamRadius = 1.5f;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& tf)
                {
                    if (champion.team == ctx.casterTeam)
                        return;

                    const Vec3 pos = tf.GetPosition();
                    f32_t u = ((pos.x - a.x) * dx + (pos.z - a.z) * dz) / segLenSq;
                    if (u < 0.f) u = 0.f;
                    if (u > 1.f) u = 1.f;

                    const Vec3 closest{ a.x + dx * u, pos.y, a.z + dz * u };
                    if (WintersMath::DistanceSqXZ(pos, closest) > kBeamRadius * kBeamRadius)
                        return;

                    StunComponent stun{};
                    stun.fRemaining = 0.75f;
                    stun.sourceEntity = ctx.casterEntity;

                    if (world.HasComponent<StunComponent>(target))
                        world.GetComponent<StunComponent>(target) = stun;
                    else
                        world.AddComponent<StunComponent>(target, stun);

                    EnqueuePhysicalDamage(
                        world,
                        ctx.casterEntity,
                        target,
                        ctx.casterTeam,
                        70.f + 30.f * static_cast<f32_t>(ctx.skillRank),
                        static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 3u),
                        ctx.skillRank);
                }));

        state.blade1Pos = {};
        state.blade2Pos = {};
        state.blade1Tick = 0;
        state.blade2Tick = 0;
        state.bHasBlade1 = false;
        state.bHasBlade2 = false;

        std::cout << "[IreliaSim] E beam resolved\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 vOrigin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 vForward = ResolveRForward(world, ctx.casterEntity, ctx.pCommand);

        EntityID bestTarget = NULL_ENTITY;
        Vec3 vBestHitPos{};
        //bestalong의 의미가 뭐임?
        f32_t fBestAlong = kIreliaRRange + 1.f;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& tf)
                {
                    if (target == ctx.casterEntity || champion.team == ctx.casterTeam)
                        return;

                    const Vec3 vPos = tf.GetPosition();
                    //best along의 의미가  정확히 뭐임?
                    const f32_t dx = vPos.x - vOrigin.x;
                    const f32_t dz = vPos.z - vOrigin.z;
                    const f32_t fAlong = dx * vForward.x + dz * vForward.z;
                    if (fAlong < 0.f || fAlong > kIreliaRRange || fAlong >= fBestAlong)
                        return;

                    const f32_t fPerp = dx * (-vForward.z) + dz * vForward.x;
                    if (std::fabs(fPerp) > kIreliaRHalfWidth)
                        return;

                    bestTarget = target;
                    vBestHitPos = vPos;
                    fBestAlong = fAlong;

                    //along의 역할이 뭔지 모르겠어
                }));
        if (bestTarget != NULL_ENTITY)
        {
            EmitIreliaRHitVisual(world, ctx, bestTarget, vBestHitPos, vForward);
            std::cout << "[IreliaSim] R hit caster=" << ctx.casterEntity
                << " target=" << bestTarget
                << " along=" << fBestAlong << "\n";
            return;
        }

        std::cout << "[IreliaSim] R accepted caster=" << ctx.casterEntity << " miss\n";
    }
}

namespace IreliaGameSim
{
    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<IreliaSimComponent, TransformComponent>(
            std::function<void(EntityID, IreliaSimComponent&, TransformComponent&)>(
                [&](EntityID entity, IreliaSimComponent& state, TransformComponent& transform)
                {
                    if (!state.bDashActive)
                        return;

                    ClearMove(world, entity);

                    const f32_t duration =
                        state.dashDurationSec > 0.01f ? state.dashDurationSec : kIreliaQDashDurationSec;
                    state.dashElapsedSec += tc.fDt;

                    f32_t t = state.dashElapsedSec / duration;
                    if (t < 0.f) t = 0.f;
                    if (t > 1.f) t = 1.f;

                    const Vec3 desiredPos = Lerp(state.dashStartPos, state.dashEndPos, t);
                    Vec3 guardedPos = desiredPos;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable)
                    {
                        const Vec3 currentPos = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(currentPos, desiredPos, 0.5f, guardedPos))
                        {
                            guardedPos = currentPos;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(guardedPos, desiredPos) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }
                    }

                    transform.SetPosition(guardedPos);

                    const Vec3 dir = WintersMath::NormalizeXZ(Vec3{
                        state.dashEndPos.x - state.dashStartPos.x,
                        0.f,
                        state.dashEndPos.z - state.dashStartPos.z
                    });
                    if (dir.x * dir.x + dir.z * dir.z > 0.0001f)
                    {
                        Vec3 rot = transform.GetRotation();
                        transform.SetRotation({
                            rot.x,
                            ResolveChampionVisualYawNear(eChampion::IRELIA, dir, rot.y),
                            rot.z });
                    }

                    if (t >= 1.f || bDashBlocked)
                    {
                        state.bDashActive = false;
                        state.dashElapsedSec = 0.f;
                        state.dashDurationSec = 0.f;
                        state.dashTarget = NULL_ENTITY;
                    }
                }));
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::Q_OnCastAccepted), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::W_OnCastAccepted), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::E_OnCastAccepted), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::R_OnCastAccepted), &OnR);

        s_bRegistered = true;
        std::cout << "[IreliaSim] hooks registered\n";
    }
}
