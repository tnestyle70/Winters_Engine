#include "GameObject/Champion/Yone/Yone_Registration.h"
#include "GameObject/Champion/Yone/Yone_Skills.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kYon_BA_Cast = MakeHookId(eChampion::YONE, HookVariant::BA_CastFrame);
    constexpr u32_t kYon_Q_Cast = MakeHookId(eChampion::YONE, HookVariant::Q_CastFrame);
    constexpr u32_t kYon_W_Cast = MakeHookId(eChampion::YONE, HookVariant::W_CastFrame);
    constexpr u32_t kYon_E_Cast = MakeHookId(eChampion::YONE, HookVariant::E_CastFrame);
    constexpr u32_t kYon_R_Cast = MakeHookId(eChampion::YONE, HookVariant::R_CastFrame);

    struct YoneAutoRegister
    {
        YoneAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::YONE;
            cd.animPrefix = "yone_";
            cd.idleAnimKey = "idle1";
            cd.runAnimKey = "run1";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Texture/Character/Yone/yone.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath = L"Texture/Character/Yone/yone_base_tx_cm.png";
            cd.texturePath[0] = L"Texture/Character/Yone/yone_base_tx_cm.png";
            cd.texturePath[1] = L"Texture/Character/Yone/yone_base_swords_tx_cm.png";
            cd.texturePath[2] = L"Texture/Character/Yone/yone_base_swords_tx_cm.png";
            cd.texturePath[3] = L"Texture/Character/Yone/yone_base_props_tx_cm.png";
            cd.texturePath[4] = L"Texture/Character/Yone/yone_base_tx_cm.png";
            cd.texturePath[5] = L"Texture/Character/Yone/yone_base_tx_cm.png";
            cd.texturePath[6] = L"Texture/Character/Yone/yone_base_tx_cm.png";
            cd.texturePath[7] = L"Texture/Character/Yone/yone_base_tx_cm.png";
            cd.spawnPosition = { 45.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Yone";
            CChampionRegistry::Instance().Add(eChampion::YONE, cd);

            {
                SkillDef s{};
                s.champ = eChampion::YONE; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.75f; s.rangeMax = 1.5f; s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.9f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.visualCastFrame = 5.f; s.visualRecoveryFrame = 12.f; s.visualPlaySpeed = 0.85f;
                s.endTransitionIdleAnim = "attack1_toidle1";
                s.endTransitionRunAnim = "attack1_towalk1";
                s.endTransitionDuration = 0.18f;
                s.castHookId = kYon_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::YONE, 0, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::YONE; s.slot = 1;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 4.f; s.rangeMax = 4.75f; s.manaCost = 0.f;
                s.animKey = "spell1_a1";
                s.lockDurationSec = 0.9f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 4.f; s.visualRecoveryFrame = 10.f; s.visualPlaySpeed = 0.85f;
                s.endTransitionRunAnim = "spell1a_towalk1";
                s.endTransitionDuration = 0.16f;
                s.castHookId = kYon_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::YONE, 1, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::YONE; s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 16.f; s.rangeMax = 6.f; s.manaCost = 0.f;
                s.animKey = "spell2";
                s.lockDurationSec = 0.65f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 5.f; s.visualRecoveryFrame = 12.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kYon_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::YONE, 2, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::YONE; s.slot = 3;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 22.f; s.rangeMax = 4.f; s.manaCost = 0.f;
                s.animKey = "spell3_bodyin";
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::Direction;
                s.stage2AnimKey = "spell3_out";
                s.stage2LockSec = 0.50f;
                s.stage2VisualPlaySpeed = 1.f;
                s.lockDurationSec = 0.75f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 6.f; s.visualRecoveryFrame = 14.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kYon_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::YONE, 3, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::YONE; s.slot = 4;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 120.f; s.rangeMax = 10.f; s.manaCost = 0.f;
                s.animKey = "spell4_in";
                s.lockDurationSec = 1.2f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 12.f; s.visualRecoveryFrame = 24.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kYon_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::YONE, 4, s);
            }

            // Yone gameplay/state mutation is handled by the shared hook path.
            // Keep cast execution single-source; VisualHook stays client-only.

            CGameplayHookRegistry::Instance().Register(kYon_BA_Cast, &Yone::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kYon_Q_Cast, &Yone::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kYon_W_Cast, &Yone::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kYon_E_Cast, &Yone::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kYon_R_Cast, &Yone::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kYon_BA_Cast, &Yone::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kYon_Q_Cast, &Yone::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kYon_W_Cast, &Yone::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kYon_E_Cast, &Yone::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kYon_R_Cast, &Yone::Visual::OnCastFrame_R_Visual);

        }
    };

    static YoneAutoRegister s_register;
}

void Yone_KeepAlive()
{
    (void)&s_register;
}
