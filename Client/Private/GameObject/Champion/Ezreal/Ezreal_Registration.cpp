#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Ezreal/Ezreal_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
	constexpr f32_t kEzrealDebugSkillCooldownSec = 0.2f;

	constexpr u32_t kEz_BA_Cast = MakeHookId(eChampion::EZREAL, HookVariant::BA_CastFrame);
	constexpr u32_t kEz_Q_Cast = MakeHookId(eChampion::EZREAL, HookVariant::Q_CastFrame);
	constexpr u32_t kEz_W_Cast = MakeHookId(eChampion::EZREAL, HookVariant::W_CastFrame);
	constexpr u32_t kEz_E_OnAccept = MakeHookId(eChampion::EZREAL, HookVariant::E_OnCastAccepted);
	constexpr u32_t kEz_E_KeySwap = MakeHookId(eChampion::EZREAL, HookVariant::E_KeySwap);
	constexpr u32_t kEz_R_Cast = MakeHookId(eChampion::EZREAL, HookVariant::R_CastFrame);

	struct EzrealAutoRegister
	{
		EzrealAutoRegister()
		{
			ChampionDef cd{};
			cd.id = eChampion::EZREAL;
			cd.animPrefix = "ezreal_";
			cd.idleAnimKey = "idle";
			cd.runAnimKey = "run";
			cd.basicAttackKey = "attack1";
			cd.basicAttackRange = 5.5f;
			cd.fbxPath = "Texture/Character/Ezreal/ezreal.fbx";
			cd.shaderPath = L"Shaders/Mesh3D.hlsl";
			const wchar_t* ezrealBaseTexture =
				L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
			cd.defaultTexturePath = ezrealBaseTexture;
			for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
				cd.texturePath[i] = ezrealBaseTexture;
			cd.spawnPosition = { 27.f, 1.f, 0.f };
			cd.spawnScale = 0.01f;
			cd.displayName = "Ezreal";
			CChampionRegistry::Instance().Add(eChampion::EZREAL, cd);

			{
				SkillDef s{};
				s.champ = eChampion::EZREAL; s.slot = 0;
				s.animKey = "attack1";
				s.bOneShot = true;
				s.castHookId = kEz_BA_Cast;
				CSkillRegistry::Instance().Add(eChampion::EZREAL, 0, s);
			}
			{
				SkillDef s{};
				s.champ = eChampion::EZREAL; s.slot = 1;
				s.animKey = "spell1"; s.bOneShot = true;
				s.castHookId = kEz_Q_Cast;
				CSkillRegistry::Instance().Add(eChampion::EZREAL, 1, s);
			}
			{
				SkillDef s{};
				s.champ = eChampion::EZREAL; s.slot = 2;
				s.animKey = "spell2"; s.bOneShot = true;
				s.castHookId = kEz_W_Cast;
				CSkillRegistry::Instance().Add(eChampion::EZREAL, 2, s);
			}
			{
				SkillDef s{};
				s.champ = eChampion::EZREAL; s.slot = 3;
				s.animKey = "spell3_generic"; s.bOneShot = true;
				s.onCastAcceptedHookId = kEz_E_OnAccept;
				s.keySwapHookId = kEz_E_KeySwap;
				CSkillRegistry::Instance().Add(eChampion::EZREAL, 3, s);
			}
			{
				SkillDef s{};
				s.champ = eChampion::EZREAL; s.slot = 4;
				s.animKey = "spell4"; s.bOneShot = true;
				s.castHookId = kEz_R_Cast;
				CSkillRegistry::Instance().Add(eChampion::EZREAL, 4, s);
			}


			//Shared Gameplay hook
			CGameplayHookRegistry::Instance().Register(kEz_BA_Cast,
				&Ezreal::Gameplay::OnCastFrame_BA);
			CGameplayHookRegistry::Instance().Register(kEz_Q_Cast,
				&Ezreal::Gameplay::OnCastFrame_Q);
			CGameplayHookRegistry::Instance().Register(kEz_W_Cast,
				&Ezreal::Gameplay::OnCastFrame_W);
			CGameplayHookRegistry::Instance().Register(kEz_R_Cast,
				&Ezreal::Gameplay::OnCastFrame_R);
			CGameplayHookRegistry::Instance().Register(kEz_E_OnAccept,
				&Ezreal::Gameplay::OnCastAccepted_E);

			// ?? Client Visual hook (FX/anim) ??
			CVisualHookRegistry::Instance().Register(kEz_E_KeySwap, 
				&Ezreal::Visual::OnKeySwap_E);
			CVisualHookRegistry::Instance().Register(kEz_E_OnAccept,
				&Ezreal::Visual::OnCastAccepted_E_Visual);  // FX flash
			CVisualHookRegistry::Instance().Register(kEz_BA_Cast,
				&Ezreal::Visual::OnCastFrame_BA_Visual);
			CVisualHookRegistry::Instance().Register(kEz_Q_Cast,
				&Ezreal::Visual::OnCastFrame_Q_Visual);
			CVisualHookRegistry::Instance().Register(kEz_W_Cast,
				&Ezreal::Visual::OnCastFrame_W_Visual);
			CVisualHookRegistry::Instance().Register(kEz_R_Cast, 
				&Ezreal::Visual::OnCastFrame_R_Visual);
		}
	};

	static EzrealAutoRegister s_register;
}

void Ezreal_KeepAlive()
{
	(void)&s_register;
}
