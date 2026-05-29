#include "GameObject/Champion/Garen/Garen_Skills.h"

#include "GameObject/Champion/Garen/GarenFxPresets.h"

namespace Garen
{
    void OnCastFrame(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pDef)
            return;

        const u8_t slot = ctx.pDef->slot;
        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            if (ctx.pCommand && ctx.pCommand->targetEntityId != NULL_ENTITY && ctx.applyTargetDamage)
                ctx.applyTargetDamage(ctx.pCommand->targetEntityId, 60.f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::Q))
        {
            GarenFx::SpawnQTrail(*ctx.pWorld, ctx.casterEntity, 0.5f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::W))
        {
            GarenFx::SpawnWShield(*ctx.pWorld, ctx.casterEntity, 1.5f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            GarenFx::SpawnESpinBlade(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, 3.0f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::R))
        {
            if (!ctx.pCommand || ctx.pCommand->targetEntityId == NULL_ENTITY)
                return;

            GarenFx::SpawnRSword(*ctx.pWorld, ctx.pCommand->targetEntityId, 1.0f);
            if (ctx.applyTargetDamage)
                ctx.applyTargetDamage(ctx.pCommand->targetEntityId, 250.f);
        }
    }
}
