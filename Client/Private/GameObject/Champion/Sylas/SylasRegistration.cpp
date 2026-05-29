#include "GameObject/Champion/Sylas/SylasSkills.h"

#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"

#include <Windows.h>

namespace
{
	constexpr u32_t kSylasBACast = MakeHookId(eChampion::SYLAS, HookVariant::BA_CastFrame);
	constexpr u32_t kSylasQCast = MakeHookId(eChampion::SYLAS, HookVariant::Q_CastFrame);
	constexpr u32_t kSylasWCast = MakeHookId(eChampion::SYLAS, HookVariant::W_CastFrame);
	constexpr u32_t kSylasECast = MakeHookId(eChampion::SYLAS, HookVariant::E_CastFrame);
	constexpr u32_t kSylasRCast = MakeHookId(eChampion::SYLAS, HookVariant::R_CastFrame);
	//사거리
	f32_t ResolveRange(u8_t slot)
	{
		switch (static_cast<eSkillSlot>(slot))
		{
		case eSkillSlot::BasicAttack: return 1.5f;
		case eSkillSlot::Q: return 4.f;
		case eSkillSlot::W: return 5.f;
		case eSkillSlot::E: return 6.f;
		case eSkillSlot::R: return 10.f;
		default: return 0.f;
		}
	}

	void RegisterSkill(u8_t slot, eTargetMode targetMode, const char* animKey, u32_t hookId)
	{
		SkillDef s{};
		s.champ = eChampion::SYLAS;
		s.slot = slot;
		s.targetMode = targetMode;
		s.cooldownSec = 0.6f;
		s.rangeMax = ResolveRange(slot);
		s.animKey = animKey;
		s.lockDurationSec = 0.55f;
		s.bOneShot = true;
		s.rotate = targetMode == eTargetMode::Self ? eRotateMode::None : eRotateMode::TowardsCursor;
		s.castFrame = 4.f;
		s.recoveryFrame = 12.f;
		s.animPlaySpeed = 1.f;
		s.castFrameHookId = hookId;
		//사일러스 E 사슬 투사체 
		if (slot == static_cast<u8_t>(eSkillSlot::E))
		{
			s.stageCount = 2;
			s.stage2TargetMode = eTargetMode::UnitTarget;
			s.stage2AnimKey = "skinned_mesh_sylas_spell3_bhit_cast";
			s.stage2LockSec = 0.5f;
			s.stage2Rotate = eRotateMode::TowardsTarget;
			s.stageWindowSec = 3.f;
			s.stage2CastFrame = 4.f;
			s.stage2RecoveryFrame = 12.f;
			s.stage2PlaySpeed = 1.f;
		}
		CSkillRegistry::Instance().Add(eChampion::SYLAS, slot, s);
	}

	struct SylasAutoRegister
	{
		SylasAutoRegister()
		{
			ChampionDef cd{};
			cd.id = eChampion::SYLAS;
			cd.animPrefix = "";
			cd.idleAnimKey = "skinned_mesh_sylas_idle";
			cd.runAnimKey = "skinned_mesh_sylas_run";
			cd.basicAttackKey = "skinned_mesh_sylas_attack_01";
			cd.basicAttackRange = 1.5f;
			cd.fbxPath = "Client/Bin/Resource/Texture/Character/Sylas/sylas.wmesh";
			cd.shaderPath = L"Shaders/Mesh3D.hlsl";
			cd.defaultTexturePath = L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_tx_cm.png";
			for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
				cd.texturePath[i] = cd.defaultTexturePath;
			cd.texturePath[2] = L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png";
			cd.texturePath[3] = L"Client/Bin/Resource/Texture/Character/Sylas/sylas_base_chain_lock_tx_cm.png";
			cd.spawnPosition = { -27.f, 1.f, 6.f };
			cd.spawnScale = 0.01f;
			cd.displayName = "Sylas";
			CChampionRegistry::Instance().Add(eChampion::SYLAS, cd);

			RegisterSkill(0, eTargetMode::UnitTarget, "skinned_mesh_sylas_attack_01", kSylasBACast);
			RegisterSkill(1, eTargetMode::GroundTarget, "skinned_mesh_sylas_spell1_cast", kSylasQCast);
			RegisterSkill(2, eTargetMode::UnitTarget, "skinned_mesh_sylas_spell2", kSylasWCast);
			RegisterSkill(3, eTargetMode::Direction, "skinned_mesh_sylas_spell3_dash", kSylasECast);
			RegisterSkill(4, eTargetMode::UnitTarget, "skinned_mesh_sylas_spell4_cast", kSylasRCast);

			CVisualHookRegistry::Instance().Register(kSylasQCast, &Sylas::Visual::OnQCastFrame);
			CVisualHookRegistry::Instance().Register(kSylasWCast, &Sylas::Visual::OnWCastFrame);
			CVisualHookRegistry::Instance().Register(kSylasECast, &Sylas::Visual::OnECastFrame);
			CVisualHookRegistry::Instance().Register(kSylasRCast, &Sylas::Visual::OnRCastFrame);

			OutputDebugStringA("[Sylas] Registration complete\n");
		}
	};

	static SylasAutoRegister s_register;
}

void SylasKeepAlive()
{
	(void)&s_register;
}
