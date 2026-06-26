#include "GameObject/Champion/Zed/Zed_Skills.h"

#include "GameObject/SkillDef.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kZed_BA_Cast =
        MakeHookId(eChampion::ZED, HookVariant::BA_CastFrame);
    constexpr u32_t kZed_Q_Cast =
        MakeHookId(eChampion::ZED, HookVariant::Q_CastFrame);
    constexpr u32_t kZed_W_Cast =
        MakeHookId(eChampion::ZED, HookVariant::W_CastFrame);
    constexpr u32_t kZed_E_Cast =
        MakeHookId(eChampion::ZED, HookVariant::E_CastFrame);
    constexpr u32_t kZed_R_Cast =
        MakeHookId(eChampion::ZED, HookVariant::R_CastFrame);

    u32_t ResolveZedCastHook(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q:
            return kZed_Q_Cast;
        case eSkillSlot::W:
            return kZed_W_Cast;
        case eSkillSlot::E:
            return kZed_E_Cast;
        case eSkillSlot::R:
            return kZed_R_Cast;
        default:
            return kZed_BA_Cast;
        }
    }

    void DispatchZedVisualHook(VisualHookContext& visualCtx)
    {
        SkillHookContext skillCtx{};
        skillCtx.pWorld = visualCtx.pWorld;
        skillCtx.casterEntity = visualCtx.casterEntity;
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.pKeyOut = visualCtx.pKeyOut;
        skillCtx.pFxMeshRenderer = visualCtx.pFxMeshRenderer;

        Zed::OnCastFrame(skillCtx);
    }

    struct ZedAutoRegister
    {
        ZedAutoRegister()
        {
            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                const SkillDef* legacy = FindSkillDef(eChampion::ZED, slot);
                if (!legacy)
                    continue;

                SkillDef s = *legacy;
                s.castHookId = ResolveZedCastHook(slot);
                CSkillRegistry::Instance().Add(eChampion::ZED, slot, s);
            }

            const u32_t hooks[] =
            {
                kZed_BA_Cast,
                kZed_Q_Cast,
                kZed_W_Cast,
                kZed_E_Cast,
                kZed_R_Cast
            };

            for (const u32_t hook : hooks)
            {
                CSkillHookRegistry::Instance().Register(hook, &Zed::OnCastFrame);
                CVisualHookRegistry::Instance().Register(
                    hook,
                    [](VisualHookContext& ctx)
                    {
                        DispatchZedVisualHook(ctx);
                    });
            }

        }
    };

    static ZedAutoRegister s_register;
}

void Zed_KeepAlive()
{
    (void)&s_register;
}
