#include "GameObject/Champion/Annie/Annie_Skills.h"

#include "GameObject/Champion/Annie/Annie_Components.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "WintersMath.h"

namespace
{
    Vec3 ResolveCasterPosition(const VisualHookContext& ctx)
    {
        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        }

        return ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
    }

    Vec3 ResolveTargetPosition(const VisualHookContext& ctx, const Vec3& fallback)
    {
        if (ctx.pWorld &&
            ctx.pCommand &&
            ctx.pCommand->targetEntityId != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
        {
            return ctx.pWorld->GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
        }

        return fallback;
    }

    Vec3 ResolveForward(const VisualHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 commandDir = WintersMath::NormalizeXZOrZero(ctx.pCommand->direction);
            if (commandDir.x != 0.f || commandDir.z != 0.f)
                return commandDir;
        }

        if (ctx.pWorld &&
            ctx.pCommand &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pCommand->targetEntityId != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity) &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
        {
            const Vec3 caster =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 target =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
            const Vec3 targetDir = WintersMath::NormalizeXZOrZero(
                Vec3{ target.x - caster.x, 0.f, target.z - caster.z });
            if (targetDir.x != 0.f || targetDir.z != 0.f)
                return targetDir;
        }

        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            const f32_t yaw =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetRotation().y;
            return WintersMath::DirectionFromYawXZ(yaw);
        }

        return { 0.f, 0.f, 1.f };
    }

    bool_t PlayAnnieCue(
        VisualHookContext& ctx,
        const char* pszCueName,
        const Vec3& worldPos,
        const Vec3& forward,
        EntityID attachTo,
        const Vec3* pEndWorldPos = nullptr)
    {
        if (!ctx.pWorld)
            return false;

        FxCueContext fx{};
        fx.vWorldPos = worldPos;
        fx.vForward = forward;
        fx.attachTo = attachTo;
        fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
        if (pEndWorldPos)
        {
            fx.vEndWorldPos = *pEndWorldPos;
            fx.bOverrideEndWorldPos = true;
        }

        return CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx) != NULL_ENTITY;
    }

    AnnieStateComponent* FindVisualState(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || ctx.casterEntity == NULL_ENTITY)
            return nullptr;

        if (!ctx.pWorld->HasComponent<AnnieStateComponent>(ctx.casterEntity))
            ctx.pWorld->AddComponent<AnnieStateComponent>(ctx.casterEntity, AnnieStateComponent{});

        return &ctx.pWorld->GetComponent<AnnieStateComponent>(ctx.casterEntity);
    }

    void PlayStunReadyCue(VisualHookContext& ctx)
    {
        PlayAnnieCue(ctx,
            "Annie.Stun.Ready",
            ResolveCasterPosition(ctx),
            ResolveForward(ctx),
            ctx.casterEntity);
    }

    void AdvanceVisualStunStack(VisualHookContext& ctx, bool_t bConsumesReady)
    {
        AnnieStateComponent* pState = FindVisualState(ctx);
        if (!pState)
            return;

        if (bConsumesReady && pState->bNextStunReady)
        {
            pState->bNextStunReady = false;
            pState->stunStacks = 0;
        }

        if (pState->bNextStunReady)
            return;

        ++pState->stunStacks;
        if (pState->stunStacks >= pState->stunThreshold)
        {
            pState->stunStacks = 0;
            pState->bNextStunReady = true;
            PlayStunReadyCue(ctx);
        }
    }
}

namespace Annie
{
    void OnCastFrame_BA(SkillHookContext& ctx) { (void)ctx; }
    void OnCastFrame_Q(SkillHookContext& ctx) { (void)ctx; }
    void OnCastFrame_W(SkillHookContext& ctx) { (void)ctx; }
    void OnCastFrame_E(SkillHookContext& ctx) { (void)ctx; }
    void OnCastFrame_R(SkillHookContext& ctx) { (void)ctx; }

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const EntityID target = ctx.pCommand->targetEntityId;
            if (target != NULL_ENTITY)
            {
                const Vec3 casterPos = ResolveCasterPosition(ctx);
                const Vec3 forward = ResolveForward(ctx);
                const Vec3 targetPos = ResolveTargetPosition(ctx, casterPos);
                Vec3 endPos = targetPos;
                endPos.y = casterPos.y;

                PlayAnnieCue(ctx, "Annie.BA.Projectile", casterPos, forward, NULL_ENTITY, &endPos);
                PlayAnnieCue(ctx, "Annie.BA.Hit", targetPos, forward, target);
            }
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 casterPos = ResolveCasterPosition(ctx);
            const Vec3 forward = ResolveForward(ctx);
            Vec3 endPos = ResolveTargetPosition(ctx, {
                casterPos.x + forward.x * 5.0f,
                casterPos.y,
                casterPos.z + forward.z * 5.0f
            });
            endPos.y = casterPos.y;
            PlayAnnieCue(ctx, "Annie.Q.Fireball", casterPos, forward, NULL_ENTITY, &endPos);
            AdvanceVisualStunStack(ctx, true);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 casterPos = ResolveCasterPosition(ctx);
            const Vec3 forward = ResolveForward(ctx);
            PlayAnnieCue(ctx, "Annie.W.Cone", casterPos, forward, NULL_ENTITY);
            AdvanceVisualStunStack(ctx, true);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const Vec3 casterPos = ResolveCasterPosition(ctx);
            const Vec3 forward = ResolveForward(ctx);
            PlayAnnieCue(ctx, "Annie.E.Shield", casterPos, forward, ctx.casterEntity);
            AdvanceVisualStunStack(ctx, false);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 forward = ResolveForward(ctx);
            PlayAnnieCue(ctx, "Annie.R.Summon", ctx.pCommand->groundPos, forward, NULL_ENTITY);
            AdvanceVisualStunStack(ctx, true);
        }
    }
}
