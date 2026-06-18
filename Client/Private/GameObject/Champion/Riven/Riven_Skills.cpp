#include "GameObject/Champion/Riven/Riven_Skills.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/Champion/Riven/RivenFxPresets.h"
#include "GamePlay/VisualHookRegistry.h"

#include <Windows.h>
#include <cstdio>

namespace Riven
{
    void OnCastAccepted_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand ||
            !ctx.pWorld->HasComponent<RivenStateComponent>(ctx.casterEntity))
        {
            return;
        }

        auto& rs = ctx.pWorld->GetComponent<RivenStateComponent>(ctx.casterEntity);
        const u8_t stackIdx = rs.qStackCount;
        RivenFx::SpawnQSlash(*ctx.pWorld, ctx.casterEntity, stackIdx, 0.4f,
            ctx.pFxMeshRenderer);

        if (ctx.pCommand->targetEntityId != NULL_ENTITY && ctx.applyTargetDamage)
            ctx.applyTargetDamage(ctx.pCommand->targetEntityId,
                40.f + 15.f * static_cast<f32_t>(stackIdx));

        if (stackIdx >= 2)
        {
            rs.qStackCount = 0;
            rs.qStackTimer = 0.f;
        }
        else
        {
            rs.qStackCount += 1;
            rs.qStackTimer = 1.5f;
        }
    }

    void OnCastFrame(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pDef)
            return;

        RivenStateComponent* pRivenState = ctx.pWorld->HasComponent<RivenStateComponent>(ctx.casterEntity)
            ? &ctx.pWorld->GetComponent<RivenStateComponent>(ctx.casterEntity)
            : nullptr;

        const u8_t slot = ctx.pDef->slot;
        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            const f32_t damage = (pRivenState && pRivenState->bUlted) ? 75.f : 50.f;
            if (ctx.pCommand && ctx.pCommand->targetEntityId != NULL_ENTITY && ctx.applyTargetDamage)
                ctx.applyTargetDamage(ctx.pCommand->targetEntityId, damage);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::W))
        {
            RivenFx::SpawnWNova(*ctx.pWorld, ctx.casterEntity, 0.6f,
                ctx.pFxMeshRenderer);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            RivenFx::SpawnEShield(*ctx.pWorld, ctx.casterEntity, 1.5f,
                ctx.pFxMeshRenderer);
            if (pRivenState)
            {
                pRivenState->fShieldRemaining = 70.f;
                pRivenState->fShieldTimer = 1.5f;
            }
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::R))
        {
            if (pRivenState && pRivenState->bUlted)
            {
                RivenFx::SpawnRWindSlash(*ctx.pWorld, ctx.casterEntity, 0.7f,
                    ctx.pFxMeshRenderer);
                return;
            }

            RivenFx::SpawnRActivate(*ctx.pWorld, ctx.casterEntity, 0.8f,
                ctx.pFxMeshRenderer);
            if (pRivenState)
            {
                pRivenState->bUlted = true;
                pRivenState->fUltTimer = 15.f;
            }
            if (ctx.setLocalLoopAnimations)
                ctx.setLocalLoopAnimations("riven_idle1_ult", "riven_run_ult", true);
        }
    }
}

namespace Riven::Visual
{
    void OnCastAccepted_Q_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        u8_t stackIdx = 0;
        if (ctx.pWorld->HasComponent<RivenStateComponent>(ctx.casterEntity))
            stackIdx = ctx.pWorld->GetComponent<RivenStateComponent>(ctx.casterEntity).qStackCount;

        RivenFx::SpawnQSlash(*ctx.pWorld, ctx.casterEntity, stackIdx, 0.4f,
            ctx.pFxMeshRenderer);
    }

    void OnCastFrame_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pDef)
            return;

        const u8_t slot = ctx.pDef->slot;
        if (slot == static_cast<u8_t>(eSkillSlot::W))
        {
            RivenFx::SpawnWNova(*ctx.pWorld, ctx.casterEntity, 0.6f,
                ctx.pFxMeshRenderer);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            RivenFx::SpawnEShield(*ctx.pWorld, ctx.casterEntity, 1.5f,
                ctx.pFxMeshRenderer);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::R))
        {
            if (ctx.pWorld->HasComponent<RivenStateComponent>(ctx.casterEntity) &&
                ctx.pWorld->GetComponent<RivenStateComponent>(ctx.casterEntity).bUlted)
            {
                RivenFx::SpawnRWindSlash(*ctx.pWorld, ctx.casterEntity, 0.7f,
                    ctx.pFxMeshRenderer);
            }
            else
            {
                RivenFx::SpawnRActivate(*ctx.pWorld, ctx.casterEntity, 0.8f,
                    ctx.pFxMeshRenderer);
            }
        }
    }

    void OnKeySwap_Q(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pKeyOut)
            return;
        if (!ctx.pWorld->HasComponent<RivenStateComponent>(ctx.casterEntity))
            return;

        const auto& rs = ctx.pWorld->GetComponent<RivenStateComponent>(ctx.casterEntity);
        if (rs.qStackCount >= 2)
            *ctx.pKeyOut = "spell1c";
        else if (rs.qStackCount == 1)
            *ctx.pKeyOut = "spell1b";
        else
            *ctx.pKeyOut = "spell1a";

        char dbg[96]{};
        sprintf_s(dbg, "[Riven Q Anim] stack=%u key=%s\n",
            static_cast<u32_t>(rs.qStackCount), ctx.pKeyOut->c_str());
    }
}
