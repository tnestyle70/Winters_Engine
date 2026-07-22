#include "GameObject/Champion/Irelia/Irelia_Skills.h"

#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kIrelia_Q_OnCastAccepted =
        MakeHookId(eChampion::IRELIA, HookVariant::Q_OnCastAccepted);
    constexpr u32_t kIrelia_W_OnCastAccepted =
        MakeHookId(eChampion::IRELIA, HookVariant::W_OnCastAccepted);
    constexpr u32_t kIrelia_E_OnCastAccepted =
        MakeHookId(eChampion::IRELIA, HookVariant::E_OnCastAccepted);
    constexpr u32_t kIrelia_R_OnCastAccepted =
        MakeHookId(eChampion::IRELIA, HookVariant::R_OnCastAccepted);

    struct IreliaAutoRegister
    {
        IreliaAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::IRELIA;
            cd.animPrefix = "irelia_";
            cd.idleAnimKey = "idle_01";
            cd.runAnimKey = "run_base";
            cd.basicAttackKey = "attack_01";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Texture/Character/Irelia/irelia_fixed.wmesh";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath =
                L"Texture/Character/Irelia/irelia_base_tx_cm.png";
            cd.texturePath[0] =
                L"Texture/Character/Irelia/irelia_base_blades_tx_cm.png";
            cd.texturePath[1] =
                L"Texture/Character/Irelia/irelia_base_tx_cm.png";
            cd.texturePath[2] =
                L"Texture/Character/Irelia/irelia_base_tx_cm.png";
            cd.texturePath[3] =
                L"Texture/Character/Irelia/irelia_base_blades_tx_cm.png";
            cd.spawnPosition = { 24.f, 1.f, -6.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Irelia";
            CChampionRegistry::Instance().Add(eChampion::IRELIA, cd);

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 0;
                s.animKey = "attack_01";
                s.bOneShot = true;
                CSkillRegistry::Instance().Add(eChampion::IRELIA, 0, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 1;
                s.animKey = "spell1";
                s.bOneShot = true;
                s.endTransitionIdleAnim = "spell1_to_idle";
                s.endTransitionRunAnim = "spell1_into_runbase";
                s.endTransitionDuration = 0.05f;
                s.onCastAcceptedHookId = kIrelia_Q_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::IRELIA, 1, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 2;
                s.animKey = "spell2";
                s.bOneShot = true;
                s.stage2AnimKey = "spell2_2";
                s.endTransitionRunAnim = "spell2_to_run";
                s.endTransitionDuration = 0.10f;
                s.onCastAcceptedHookId = kIrelia_W_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::IRELIA, 2, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 3;
                s.animKey = "spell3";
                s.bOneShot = true;
                s.stage2AnimKey = "spell3_b";
                s.endTransitionIdleAnim = "spell3_to_idle";
                s.endTransitionRunAnim = "spell3_run";
                s.endTransitionDuration = 0.05f;
                s.onCastAcceptedHookId = kIrelia_E_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::IRELIA, 3, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 4;
                s.animKey = "spell4";
                s.bOneShot = true;
                s.endTransitionIdleAnim = "spell4_idle";
                s.endTransitionRunAnim = "spell4_to_run";
                s.endTransitionDuration = 0.5f;
                s.onCastAcceptedHookId = kIrelia_R_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::IRELIA, 4, s);
            }

            CSkillHookRegistry::Instance().Register(
                kIrelia_Q_OnCastAccepted, &Irelia::OnCastAccepted_Q);
            CSkillHookRegistry::Instance().Register(
                kIrelia_W_OnCastAccepted, &Irelia::OnCastAccepted_W);
            CVisualHookRegistry::Instance().Register(
                kIrelia_Q_OnCastAccepted, &Irelia::Visual::OnCastAccepted_Q_Visual);
            CVisualHookRegistry::Instance().Register(
                kIrelia_W_OnCastAccepted, &Irelia::Visual::OnCastAccepted_W_Visual);
            CVisualHookRegistry::Instance().Register(
                kIrelia_E_OnCastAccepted, &Irelia::Visual::OnCastAccepted_E_Visual);
            CVisualHookRegistry::Instance().Register(
                kIrelia_R_OnCastAccepted, &Irelia::Visual::OnCastAccepted_R_Visual);
        }
    };

    static IreliaAutoRegister s_register;
}

void Irelia_KeepAlive()
{
    (void)&s_register;
    Irelia::ResetLocalState();
}
