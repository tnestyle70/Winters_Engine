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
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Irelia/irelia_fixed.wmesh";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath =
                L"Client/Bin/Resource/Texture/Character/Irelia/irelia_base_tx_cm.png";
            cd.texturePath[0] =
                L"Client/Bin/Resource/Texture/Character/Irelia/irelia_base_blades_tx_cm.png";
            cd.texturePath[1] =
                L"Client/Bin/Resource/Texture/Character/Irelia/irelia_base_tx_cm.png";
            cd.texturePath[2] =
                L"Client/Bin/Resource/Texture/Character/Irelia/irelia_base_tx_cm.png";
            cd.texturePath[3] =
                L"Client/Bin/Resource/Texture/Character/Irelia/irelia_base_blades_tx_cm.png";
            cd.spawnPosition = { 24.f, 1.f, -6.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Irelia";
            CChampionRegistry::Instance().Add(eChampion::IRELIA, cd);

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f;
                s.rangeMax = 1.5f;
                s.manaCost = 0.f;
                s.animKey = "attack_01";
                s.lockDurationSec = 1.f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f;
                s.recoveryFrame = 14.f;
                CSkillRegistry::Instance().Add(eChampion::IRELIA, 0, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 1;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f;
                s.rangeMax = 6.0f;
                s.manaCost = 25.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.5f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 8.f;
                s.recoveryFrame = 18.f;
                s.animPlaySpeed = 1.f;
                s.stage2PlaySpeed = 1.f;
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
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 0.6f;
                s.rangeMax = 0.f;
                s.manaCost = 40.f;
                s.animKey = "spell2";
                s.lockDurationSec = 5.f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::Direction;
                s.stage2AnimKey = "spell2_2";
                s.stage2LockSec = 0.4f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stageWindowSec = 4.f;
                s.castFrame = 0.f;
                s.recoveryFrame = 7.f;
                s.stage2CastFrame = 6.f;
                s.stage2RecoveryFrame = 14.f;
                s.animPlaySpeed = 1.f;
                s.stage2PlaySpeed = 1.f;
                s.endTransitionRunAnim = "spell2_to_run";
                s.endTransitionDuration = 0.10f;
                s.onCastAcceptedHookId = kIrelia_W_OnCastAccepted;
                CSkillRegistry::Instance().Add(eChampion::IRELIA, 2, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::IRELIA;
                s.slot = 3;
                s.targetMode = eTargetMode::GroundTarget;
                s.cooldownSec = 0.6f;
                s.rangeMax = 9.0f;
                s.manaCost = 80.f;
                s.animKey = "spell3";
                s.lockDurationSec = 1.f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::GroundTarget;
                s.stage2AnimKey = "spell3_b";
                s.stage2LockSec = 0.45f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stageWindowSec = 3.5f;
                s.castFrame = 8.f;
                s.recoveryFrame = 18.f;
                s.stage2CastFrame = 5.f;
                s.stage2RecoveryFrame = 13.f;
                s.animPlaySpeed = 1.f;
                s.stage2PlaySpeed = 1.05f;
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
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 0.6f;
                s.rangeMax = 12.f;
                s.manaCost = 100.f;
                s.animKey = "spell4";
                s.lockDurationSec = 0.65f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 7.f;
                s.recoveryFrame = 30.f;
                s.animPlaySpeed = 1.00f;
                s.stage2PlaySpeed = 1.f;
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
