#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Fiora/Fiora_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kFio_BA_Cast = MakeHookId(eChampion::FIORA, HookVariant::BA_CastFrame);
    constexpr u32_t kFio_Q_Cast  = MakeHookId(eChampion::FIORA, HookVariant::Q_CastFrame);
    constexpr u32_t kFio_W_Cast  = MakeHookId(eChampion::FIORA, HookVariant::W_CastFrame);
    constexpr u32_t kFio_E_Cast  = MakeHookId(eChampion::FIORA, HookVariant::E_CastFrame);
    constexpr u32_t kFio_R_Cast  = MakeHookId(eChampion::FIORA, HookVariant::R_CastFrame);

    struct FioraAutoRegister
    {
        FioraAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::FIORA;
            cd.animPrefix    = "fiora_";
            cd.idleAnimKey   = "idle1";
            cd.runAnimKey    = "run";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Texture/Character/Fiora/fiora.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            const wchar_t* fioraBaseTexture =
                L"Texture/Character/Fiora/fiora_base_tx_cm.png";
            cd.defaultTexturePath = fioraBaseTexture;
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = fioraBaseTexture;
            cd.spawnPosition = { 30.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Fiora";
            CChampionRegistry::Instance().Add(eChampion::FIORA, cd);

            // BA
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f; s.rangeMax = 1.5f; s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 1.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.visualCastFrame = 6.f; s.visualRecoveryFrame = 14.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kFio_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 0, s);
            }
            // Q
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 1;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 1.5f; s.rangeMax = 4.0f; s.manaCost = 0.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 4.f; s.visualRecoveryFrame = 10.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kFio_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 1, s);
            }
            // W
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 2;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 12.f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell2";
                s.lockDurationSec = 1.5f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.visualCastFrame = 1.f; s.visualRecoveryFrame = 18.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kFio_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 2, s);
            }
            // E
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 13.f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.4f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.visualCastFrame = 1.f; s.visualRecoveryFrame = 8.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kFio_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 3, s);
            }
            // R
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 4;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 80.f; s.rangeMax = 5.0f; s.manaCost = 100.f;
                s.animKey = "channel";
                s.lockDurationSec = 2.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.visualCastFrame = 18.f; s.visualRecoveryFrame = 36.f; s.visualPlaySpeed = 1.f;
                s.castHookId = kFio_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 4, s);
            }

            // Fiora gameplay is now handled by the shared hook path below.
            // Registering the same cast hook into CSkillHookRegistry makes
            // Scene_InGame dispatch damage twice: GameplayHook -> SkillHook.

            CGameplayHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Visual::OnCastFrame_R_Visual);

        }
    };

    static FioraAutoRegister s_register;
}

void Fiora_KeepAlive()
{
    (void)&s_register;
}
