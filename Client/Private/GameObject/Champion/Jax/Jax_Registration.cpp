#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Jax/Jax_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kJax_BA_Cast = MakeHookId(eChampion::JAX, HookVariant::BA_CastFrame);
    constexpr u32_t kJax_Q_Cast = MakeHookId(eChampion::JAX, HookVariant::Q_CastFrame);
    constexpr u32_t kJax_W_Cast = MakeHookId(eChampion::JAX, HookVariant::W_CastFrame);
    constexpr u32_t kJax_E_Cast = MakeHookId(eChampion::JAX, HookVariant::E_CastFrame);
    constexpr u32_t kJax_R_Cast = MakeHookId(eChampion::JAX, HookVariant::R_CastFrame);

    struct JaxAutoRegister
    {
        JaxAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::JAX;
            cd.animPrefix = "";
            cd.idleAnimKey = "idle1_v04";
            cd.runAnimKey = "jax_run2";
            cd.basicAttackKey = "attack_1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Jax/jax.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";

            const wchar_t* jaxBodyTexture =
                L"Client/Bin/Resource/Texture/Character/Jax/jax_base_body_tx_cm.png";
            const wchar_t* jaxFishTexture =
                L"Client/Bin/Resource/Texture/Character/Jax/jax_base_fish_tx_cm.png";
            const wchar_t* jaxWeaponTexture =
                L"Client/Bin/Resource/Texture/Character/Jax/jax_base_weapon_tx_cm.png";

            cd.defaultTexturePath = jaxBodyTexture;
            cd.texturePath[0] = jaxBodyTexture;
            cd.texturePath[1] = jaxBodyTexture;
            cd.texturePath[2] = jaxBodyTexture;
            cd.texturePath[3] = jaxFishTexture;
            cd.texturePath[4] = jaxWeaponTexture;
            cd.texturePath[5] = jaxBodyTexture;
            cd.texturePath[6] = jaxBodyTexture;
            cd.texturePath[7] = jaxBodyTexture;
            cd.spawnPosition = { 33.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Jax";
            CChampionRegistry::Instance().Add(eChampion::JAX, cd);

            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f; s.rangeMax = 1.5f; s.manaCost = 0.f;
                s.animKey = "attack_1";
                s.lockDurationSec = 1.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f; s.recoveryFrame = 14.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 0, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 1;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f; s.rangeMax = 7.0f; s.manaCost = 0.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 1, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 2;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 0.6f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell2_v03";
                s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 1.f; s.recoveryFrame = 8.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 2, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 0.6f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell3_idle_cycle";
                s.lockDurationSec = 2.0f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::Self;
                s.stage2AnimKey = "spell3_attack1";
                s.stage2LockSec = 0.7f;
                s.stage2Rotate = eRotateMode::None;
                s.stageWindowSec = 2.0f;
                s.castFrame = 1.f; s.recoveryFrame = 48.f; s.animPlaySpeed = 1.f;
                s.stage2CastFrame = 6.f; s.stage2RecoveryFrame = 14.f; s.stage2PlaySpeed = 1.f;
                s.castFrameHookId = kJax_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 3, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 4;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 0.6f; s.rangeMax = 0.f; s.manaCost = 100.f;
                s.animKey = "spell4_idle";
                s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 4.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kJax_BA_Cast, &Jax::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kJax_Q_Cast, &Jax::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kJax_W_Cast, &Jax::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kJax_E_Cast, &Jax::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kJax_R_Cast, &Jax::OnCastFrame_R);

            CGameplayHookRegistry::Instance().Register(kJax_BA_Cast, &Jax::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kJax_Q_Cast, &Jax::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kJax_W_Cast, &Jax::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kJax_E_Cast, &Jax::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kJax_R_Cast, &Jax::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kJax_BA_Cast, &Jax::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kJax_Q_Cast, &Jax::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kJax_W_Cast, &Jax::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kJax_E_Cast, &Jax::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kJax_R_Cast, &Jax::Visual::OnCastFrame_R_Visual);

        }
    };

    static JaxAutoRegister s_register;
}

void Jax_KeepAlive()
{
    (void)&s_register;
}
