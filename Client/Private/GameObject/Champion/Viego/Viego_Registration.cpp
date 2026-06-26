#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Viego/Viego_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kViego_BA_Cast = MakeHookId(eChampion::VIEGO, HookVariant::BA_CastFrame);
    constexpr u32_t kViego_Q_Cast = MakeHookId(eChampion::VIEGO, HookVariant::Q_CastFrame);
    constexpr u32_t kViego_W_Cast = MakeHookId(eChampion::VIEGO, HookVariant::W_CastFrame);
    constexpr u32_t kViego_E_Cast = MakeHookId(eChampion::VIEGO, HookVariant::E_CastFrame);
    constexpr u32_t kViego_R_Cast = MakeHookId(eChampion::VIEGO, HookVariant::R_CastFrame);

    struct ViegoAutoRegister
    {
        ViegoAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::VIEGO;
            cd.animPrefix = "viego_";
            cd.idleAnimKey = "idle1";
            cd.runAnimKey = "run";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Texture/Character/Viego/viego_fixed.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";

            cd.defaultTexturePath =
                L"Texture/Character/Viego/viego_base_body_tx_cm.png";
            cd.texturePath[3] =
                L"Texture/Character/Viego/viego_base_crown_sword_tx_cm.png";
            cd.texturePath[4] =
                L"Texture/Character/Viego/viego_base_crown_sword_tx_cm.png";
            cd.texturePath[5] =
                L"Texture/Character/Viego/viego_base_wraith_tx_cm.png";
            cd.spawnPosition = { -30.f, 1.f, 6.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Viego";
            CChampionRegistry::Instance().Add(eChampion::VIEGO, cd);
            //?닿굅 cooldown sec?섎? ?녿뒗 嫄?留욎븘? ?꾨? ?쒕쾭 沅뚯쐞濡??뚮━怨??덉쑝?덇퉴
            //Server Sim 湲곗??쇰줈 ?숈옉?섎뒗 嫄?留욎븘?
            {
                SkillDef s{};
                s.champ = eChampion::VIEGO; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f; s.rangeMax = 1.5f; s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.75f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.visualCastFrame = 6.f; s.visualRecoveryFrame = 14.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kViego_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::VIEGO, 0, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::VIEGO; s.slot = 1;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 4.f; s.rangeMax = 5.5f; s.manaCost = 0.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 5.f; s.visualRecoveryFrame = 11.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kViego_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::VIEGO, 1, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::VIEGO; s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 8.f; s.rangeMax = 4.f; s.manaCost = 0.f;
                s.animKey = "spell2";
                s.lockDurationSec = 4.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::Direction;
                s.stage2AnimKey = "spell2_dash";
                s.stage2LockSec = 0.30f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stageWindowSec = 4.0f;
                s.visualCastFrame = 0.f; s.visualRecoveryFrame = 7.f;
                s.stage2VisualCastFrame = 4.f; s.stage2VisualRecoveryFrame = 10.f;
                s.visualPlaySpeed = 1.f;
                s.stage2VisualPlaySpeed = 1.f;
                s.castHookId = kViego_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::VIEGO, 2, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::VIEGO; s.slot = 3;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 14.f; s.rangeMax = 6.0f; s.manaCost = 0.f;
                s.animKey = "spell3";
                s.lockDurationSec = 0.75f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 4.f; s.visualRecoveryFrame = 14.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kViego_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::VIEGO, 3, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::VIEGO; s.slot = 4;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 100.f; s.rangeMax = 6.0f; s.manaCost = 0.f;
                s.animKey = "spell4";
                s.lockDurationSec = 0.8f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 10.f; s.visualRecoveryFrame = 18.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kViego_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::VIEGO, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kViego_BA_Cast, &Viego::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kViego_Q_Cast, &Viego::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kViego_W_Cast, &Viego::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kViego_E_Cast, &Viego::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kViego_R_Cast, &Viego::OnCastFrame_R);

            CGameplayHookRegistry::Instance().Register(kViego_BA_Cast, &Viego::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kViego_Q_Cast, &Viego::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kViego_W_Cast, &Viego::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kViego_E_Cast, &Viego::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kViego_R_Cast, &Viego::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kViego_BA_Cast, &Viego::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kViego_Q_Cast, &Viego::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kViego_W_Cast, &Viego::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kViego_E_Cast, &Viego::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kViego_R_Cast, &Viego::Visual::OnCastFrame_R_Visual);

        }
    };

    static ViegoAutoRegister s_register;
}

void Viego_KeepAlive()
{
    (void)&s_register;
}
