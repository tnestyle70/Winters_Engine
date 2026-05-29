#include "GameObject/Champion/LeeSin/LeeSin_Skills.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GamePlay/VisualHookRegistry.h"
#include "WintersMath.h"

namespace
{
    Vec3 ResolveCasterPosition(VisualHookContext& ctx)
    {
        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        }

        return ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
    }

    Vec3 ResolveForward(VisualHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 vCommandDir = WintersMath::NormalizeXZOrZero(ctx.pCommand->direction);
            if (vCommandDir.x != 0.f || vCommandDir.z != 0.f)
                return vCommandDir;
        }

        if (ctx.pWorld && ctx.pCommand &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pCommand->targetEntityId != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity) &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
        {
            const Vec3 vCaster =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 vTarget =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
            const Vec3 vToTarget{ vTarget.x - vCaster.x, 0.f, vTarget.z - vCaster.z };
            const Vec3 vTargetDir = WintersMath::NormalizeXZOrZero(vToTarget);
            if (vTargetDir.x != 0.f || vTargetDir.z != 0.f)
                return vTargetDir;
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

    bool_t ResolveTargetPosition(VisualHookContext& ctx, Vec3& vOutPosition)
    {
        if (!ctx.pWorld ||
            !ctx.pCommand ||
            ctx.pCommand->targetEntityId == NULL_ENTITY ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
        {
            return false;
        }

        vOutPosition =
            ctx.pWorld->GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
        return true;
    }

    void PlayLeeSinCue(VisualHookContext& ctx, const char* pszCueName, bool_t bAttachToCaster)
    {
        if (!ctx.pWorld)
            return;

        FxCueContext fx{};
        fx.vWorldPos = ResolveCasterPosition(ctx);
        fx.vForward = ResolveForward(ctx);
        fx.attachTo = bAttachToCaster ? ctx.casterEntity : NULL_ENTITY;
        fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
        CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx);
    }

    void PlayLeeSinCueBetweenCasterAndTarget(VisualHookContext& ctx, const char* pszCueName)
    {
        if (!ctx.pWorld)
            return;

        FxCueContext fx{};
        fx.vWorldPos = ResolveCasterPosition(ctx);
        fx.vForward = ResolveForward(ctx);
        fx.pFxMeshRenderer = ctx.pFxMeshRenderer;

        Vec3 vTarget{};
        if (ResolveTargetPosition(ctx, vTarget))
        {
            fx.vEndWorldPos = vTarget;
            fx.bOverrideEndWorldPos = true;
        }

        CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx);
    }
}

namespace LeeSin
{
    namespace Visual
    {
        void OnQCastFrame(VisualHookContext& ctx)
        {
            if (ctx.skillStage >= 2u)
                PlayLeeSinCue(ctx, "LeeSin.Q2.Dash", true);
            else
                PlayLeeSinCue(ctx, "LeeSin.Q.Cast", false);
        }

        void OnWCastFrame(VisualHookContext& ctx)
        {
            if (ctx.skillStage >= 2u)
                PlayLeeSinCue(ctx, "LeeSin.W2.Cast", true);
            else
                PlayLeeSinCue(ctx, "LeeSin.W1.Cast", true);
        }

        void OnECastFrame(VisualHookContext& ctx)
        {
            if (ctx.skillStage >= 2u)
                PlayLeeSinCue(ctx, "LeeSin.E2.Cast", true);
            else
                PlayLeeSinCue(ctx, "LeeSin.E1.Cast", true);
        }

        void OnRCastFrame(VisualHookContext& ctx)
        {
            PlayLeeSinCueBetweenCasterAndTarget(ctx, "LeeSin.R.Cast");
        }
    }
}
