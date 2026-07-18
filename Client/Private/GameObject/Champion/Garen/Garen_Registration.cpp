#include "GameObject/Champion/Garen/Garen_Skills.h"

#include "GameObject/SkillDef.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"

#include <Windows.h>

namespace
{
    u32_t ResolveGarenCastHook(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q:
            return MakeHookId(eChampion::GAREN, HookVariant::Q_CastFrame);
        case eSkillSlot::W:
            return MakeHookId(eChampion::GAREN, HookVariant::W_CastFrame);
        case eSkillSlot::E:
            return MakeHookId(eChampion::GAREN, HookVariant::E_CastFrame);
        case eSkillSlot::R:
            return MakeHookId(eChampion::GAREN, HookVariant::R_CastFrame);
        default:
            return MakeHookId(eChampion::GAREN, HookVariant::BA_CastFrame);
        }
    }

    SkillDef MakeGarenClientSkill(u8_t slot)
    {
        SkillDef def{};
        def.champ = eChampion::GAREN;
        def.slot = slot;
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q:
            def.animKey = "garen_2013_spell1";
            break;
        case eSkillSlot::W:
            def.animKey = "garen_2013_channel";
            break;
        case eSkillSlot::E:
            def.animKey = "garen_base_spell3_0";
            break;
        case eSkillSlot::R:
            def.animKey = "garen_2013_spell4";
            break;
        default:
            def.animKey = "garen_2013_attack_01";
            break;
        }
        return def;
    }

    //?낆쓣 留뚮뱶??寃??쒕쾭??蹂대궪 臾댁뼵媛瑜??꾩넚?쒕떎???살씤 嫄멸퉴?
    void DispatchGarenVisualHook(VisualHookContext& visualCtx)
    {
        SkillHookContext skillCtx{};
        skillCtx.pWorld = visualCtx.pWorld;
        skillCtx.casterEntity = visualCtx.casterEntity;
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.pFxMeshRenderer = visualCtx.pFxMeshRenderer;

        Garen::OnCastFrame(skillCtx);
    }

    struct GarenAutoRegister
    {
        GarenAutoRegister()
        {
            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                SkillDef s = MakeGarenClientSkill(slot);
                s.castHookId = ResolveGarenCastHook(slot);
                CSkillRegistry::Instance().Add(eChampion::GAREN, slot, s);
            }

            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                const u32_t hookId = ResolveGarenCastHook(slot);
                CVisualHookRegistry::Instance().Register(
                    hookId,
                    [](VisualHookContext& ctx)
                    {
                        DispatchGarenVisualHook(ctx);
                    }
                );

                CSkillHookRegistry::Instance().Register(hookId, &Garen::OnCastFrame);
            }
        }
    };

    static GarenAutoRegister s_register;
}

void Garen_KeepAlive()
{
    (void)&s_register;
}
