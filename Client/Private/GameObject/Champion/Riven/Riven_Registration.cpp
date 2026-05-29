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

    struct RivenAutoRegister
    {
        RivenAutoRegister()
        {
            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                const SkillDef* legacy = FindSkillDef(eChampion::RIVEN, slot);
                if (!legacy)
                    continue;

                SkillDef s = *legacy;
                if (slot == static_cast<u8_t>(eSkillSlot::Q))
                {
                    s.keySwapHookId = kRiven_Q_KeySwap;
                    s.onCastAcceptedHookId = kRiven_Q_OnCastAccepted;
                }
                else
                {
                    s.castFrameHookId = ResolveRivenCastHook(slot);
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
            OutputDebugStringA("[Riven] Registration complete\n");
        }
    };

    static RivenAutoRegister s_register;
}

void Riven_KeepAlive()
{
    (void)&s_register;
}
