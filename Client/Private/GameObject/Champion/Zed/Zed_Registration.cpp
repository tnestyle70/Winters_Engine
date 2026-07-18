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

    SkillDef MakeZedClientSkill(u8_t slot)
    {
        SkillDef def{};
        def.champ = eChampion::ZED;
        def.slot = slot;
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q:
            def.animKey = "zed_spell1";
            break;
        case eSkillSlot::W:
            def.animKey = "zed_spell2";
            def.stage2AnimKey = "zed_spell2";
            break;
        case eSkillSlot::E:
            def.animKey = "zed_spell3";
            break;
        case eSkillSlot::R:
            def.animKey = "zed_spell4";
            def.stage2AnimKey = "zed_spell4";
            break;
        default:
            def.animKey = "zed_attack1";
            break;
        }
        return def;
    }

    void DispatchZedVisualHook(VisualHookContext& visualCtx)
    {
        if (!visualCtx.bAuthoritativeEvent &&
            visualCtx.pDef &&
            ((visualCtx.pDef->slot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
                    visualCtx.skillStage >= 2u) ||
                visualCtx.pDef->slot == static_cast<u8_t>(eSkillSlot::E) ||
                visualCtx.pDef->slot == static_cast<u8_t>(eSkillSlot::R)))
        {
            return;
        }

        SkillHookContext skillCtx{};
        skillCtx.pWorld = visualCtx.pWorld;
        skillCtx.casterEntity = visualCtx.casterEntity;
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.fEffectLifetimeSec = visualCtx.fEffectLifetimeSec;
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
                SkillDef s = MakeZedClientSkill(slot);
                s.castHookId = ResolveZedCastHook(slot);
                if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
                {
                    s.stage2AnimKey = "skinned_mesh_zed_attack_passive";
                }
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
