#include "GameObject/Champion/Yasuo/Yasuo_Skills.h"

#include "GameObject/SkillDef.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kYas_Q_KeySwap = MakeHookId(eChampion::YASUO, HookVariant::Q_KeySwap);
    constexpr u32_t kYas_Q_OnCastAccepted = MakeHookId(eChampion::YASUO, HookVariant::Q_OnCastAccepted);
    constexpr u32_t kYas_W_OnCastAccepted = MakeHookId(eChampion::YASUO, HookVariant::W_OnCastAccepted);
    constexpr u32_t kYas_E_OnCastAccepted = MakeHookId(eChampion::YASUO, HookVariant::E_OnCastAccepted);
    constexpr u32_t kYas_R_OnCastAccepted = MakeHookId(eChampion::YASUO, HookVariant::R_OnCastAccepted);

    struct YasuoAutoRegister
    {
        YasuoAutoRegister()
        {
            {
                SkillDef s{};
                s.champ = eChampion::YASUO;
                s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f;
                s.rangeMax = 2.5f;
                s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.5f;
                s.animPlaySpeed = 0.85f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                CSkillRegistry::Instance().Add(eChampion::YASUO, 0, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::YASUO;
                s.slot = 1;
                s.targetMode = eTargetMode::Conditional;
                s.cooldownSec = 0.6f;
                s.rangeMax = 5.f;
                s.manaCost = 0.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.5f;
                s.animPlaySpeed = 0.85f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.keySwapHookId = kYas_Q_KeySwap;
                s.onCastAcceptedHookId = kYas_Q_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::YASUO, 1, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::YASUO;
                s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 0.6f;
                s.rangeMax = 4.f;
                s.manaCost = 0.f;
                s.animKey = "spell2";
                s.lockDurationSec = 0.25f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.onCastAcceptedHookId = kYas_W_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::YASUO, 2, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::YASUO;
                s.slot = 3;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f;
                s.rangeMax = 4.75f;
                s.manaCost = 0.f;
                s.animKey = "spell3";
                s.lockDurationSec = 0.4f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.onCastAcceptedHookId = kYas_E_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::YASUO, 3, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::YASUO;
                s.slot = 4;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 0.6f;
                s.rangeMax = 14.f;
                s.manaCost = 0.f;
                s.animKey = "spell4";
                s.lockDurationSec = 0.6f;
                s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.onCastAcceptedHookId = kYas_R_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::YASUO, 4, s);
            }

            CVisualHookRegistry::Instance().Register(
                kYas_Q_OnCastAccepted,
                &Yasuo::Visual::OnCastAccepted_Q_Visual);
            CVisualHookRegistry::Instance().Register(
                kYas_W_OnCastAccepted,
                &Yasuo::Visual::OnCastAccepted_W_Visual);
            CVisualHookRegistry::Instance().Register(
                kYas_E_OnCastAccepted,
                &Yasuo::Visual::OnCastAccepted_E_Visual);
            CVisualHookRegistry::Instance().Register(
                kYas_R_OnCastAccepted,
                &Yasuo::Visual::OnCastAccepted_R_Visual);

            CVisualHookRegistry::Instance().Register(kYas_Q_KeySwap, &Yasuo::Visual::OnKeySwap_Q);
            CSkillHookRegistry::Instance().Register(kYas_Q_OnCastAccepted, &Yasuo::OnCastAccepted_Q);
            CSkillHookRegistry::Instance().Register(kYas_W_OnCastAccepted, &Yasuo::OnCastAccepted_W);
            CSkillHookRegistry::Instance().Register(kYas_E_OnCastAccepted, &Yasuo::OnCastAccepted_E);
            CSkillHookRegistry::Instance().Register(kYas_R_OnCastAccepted, &Yasuo::OnCastAccepted_R);

        }
    };

    static YasuoAutoRegister s_register;
}

void Yasuo_KeepAlive()
{
    (void)&s_register;
}
