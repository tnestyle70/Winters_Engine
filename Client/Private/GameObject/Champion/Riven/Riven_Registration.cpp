#include "GameObject/Champion/Riven/Riven_Skills.h"

#include "GameObject/SkillDef.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kRiven_Q_OnCastAccepted =
        MakeHookId(eChampion::RIVEN, HookVariant::Q_OnCastAccepted);
    constexpr u32_t kRiven_Q_KeySwap =
        MakeHookId(eChampion::RIVEN, HookVariant::Q_KeySwap);

    u32_t ResolveRivenCastHook(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::W:
            return MakeHookId(eChampion::RIVEN, HookVariant::W_CastFrame);
        case eSkillSlot::E:
            return MakeHookId(eChampion::RIVEN, HookVariant::E_CastFrame);
        case eSkillSlot::R:
            return MakeHookId(eChampion::RIVEN, HookVariant::R_CastFrame);
        default:
            return MakeHookId(eChampion::RIVEN, HookVariant::BA_CastFrame);
        }
    }

    SkillDef MakeRivenClientSkill(u8_t slot)
    {
        SkillDef def{};
        def.champ = eChampion::RIVEN;
        def.slot = slot;
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q:
            def.animKey = "spell1";
            break;
        case eSkillSlot::W:
            def.animKey = "spell2";
            break;
        case eSkillSlot::E:
            def.animKey = "spell3";
            break;
        case eSkillSlot::R:
            def.animKey = "spell4a";
            break;
        default:
            def.animKey = "attack1";
            def.stage2AnimKey = "attack1_ult";
            break;
        }
        return def;
    }

    struct RivenAutoRegister
    {
        RivenAutoRegister()
        {
            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                SkillDef s = MakeRivenClientSkill(slot);
                if (slot == static_cast<u8_t>(eSkillSlot::Q))
                {
                    s.keySwapHookId = kRiven_Q_KeySwap;
                    s.onCastAcceptedHookId = kRiven_Q_OnCastAccepted;
                }
                else
                {
                    if (slot == static_cast<u8_t>(eSkillSlot::R))
                    {
                        s.stage2AnimKey = "spell4b";
                    }
                    s.castHookId = ResolveRivenCastHook(slot);
                }

                CSkillRegistry::Instance().Add(eChampion::RIVEN, slot, s);
            }

            CVisualHookRegistry::Instance().Register(
                kRiven_Q_OnCastAccepted,
                &Riven::Visual::OnCastAccepted_Q_Visual);
            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                if (slot == static_cast<u8_t>(eSkillSlot::Q))
                    continue;

                CVisualHookRegistry::Instance().Register(
                    ResolveRivenCastHook(slot),
                    &Riven::Visual::OnCastFrame_Visual);
            }

            CSkillHookRegistry::Instance().Register(
                kRiven_Q_OnCastAccepted, &Riven::OnCastAccepted_Q);
            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                if (slot == static_cast<u8_t>(eSkillSlot::Q))
                    continue;

                CSkillHookRegistry::Instance().Register(
                    ResolveRivenCastHook(slot), &Riven::OnCastFrame);
            }
            CVisualHookRegistry::Instance().Register(
                kRiven_Q_KeySwap, &Riven::Visual::OnKeySwap_Q);
        }
    };

    static RivenAutoRegister s_register;
}

void Riven_KeepAlive()
{
    (void)&s_register;
}
