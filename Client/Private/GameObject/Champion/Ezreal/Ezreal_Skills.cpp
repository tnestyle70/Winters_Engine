#include "GameObject/Champion/Ezreal/Ezreal_Skills.h"
#include "GameObject/Champion/Ezreal/Ezreal_Components.h"
#include "GameObject/Champion/Ezreal/Ezreal_FxPresets.h"
#include "GameObject/Champion/Yasuo/PendingHitSystem.h"
#include "GameObject/Projectile/ProjectileKind.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace Ezreal
{
	namespace
	{
		Vec3 GetMuzzlePos(CWorld& world, EntityID entity)
		{
			Vec3 pos{};
			if (world.HasComponent<TransformComponent>(entity))
			{
				auto& tf = world.GetComponent<TransformComponent>(entity);
				pos = tf.GetWorldPosition();
				pos.y += 1.0f;
			}
			return pos;
		}

		void ApplyBasicAttack(CWorld& world, EntityID caster, eTeam casterTeam,
			EntityID target)
		{
			if (target == NULL_ENTITY)
				return;

			ApplyDamage(world, caster, casterTeam, target, 55.f);
		}

		void ScheduleMysticShot(CWorld& world, EntityID caster, eTeam casterTeam,
			const Vec3& direction)
		{
			const Vec3 dir = WintersMath::NormalizeXZ(direction);
			CPendingHitSystem::Schedule(world,
				caster, casterTeam, dir,
				0.f, eProjectileKind::MysticShot,
				30.f, 30.f, 0.6f, 70.f, 0.f);
		}

		void ScheduleEssenceFlux(CWorld& world, EntityID caster, eTeam casterTeam,
			const Vec3& direction)
		{
			const Vec3 dir = WintersMath::NormalizeXZ(direction);
			CPendingHitSystem::Schedule(world,
				caster, casterTeam, dir,
				0.f, eProjectileKind::EssenceFlux,
				18.f, 27.f, 0.8f, 65.f, 0.f);
		}

		void ScheduleTrueshotBarrage(CWorld& world, EntityID caster, eTeam casterTeam,
			const Vec3& direction)
		{
			if (!world.HasComponent<EzrealStateComponent>(caster))
				return;

			auto& es = world.GetComponent<EzrealStateComponent>(caster);
			const Vec3 dir = WintersMath::NormalizeXZ(direction);
			CPendingHitSystem::Schedule(world,
				caster, casterTeam, dir,
				0.f, eProjectileKind::GlobalBeam,
				es.fGlobalSpeed, es.fGlobalSpeed * es.fGlobalLifetime, 1.2f,
				250.f, 0.f);
		}

		bool ResolveArcaneShift(CWorld& world, EntityID caster, const Vec3& direction,
			Vec3& outOrigin, Vec3& outDest)
		{
			if (!world.HasComponent<TransformComponent>(caster))
				return false;
			if (!world.HasComponent<EzrealStateComponent>(caster))
				return false;

			auto& tf = world.GetComponent<TransformComponent>(caster);
			auto& es = world.GetComponent<EzrealStateComponent>(caster);
			const Vec3 dir = WintersMath::NormalizeXZ(direction);

			outOrigin = tf.GetWorldPosition();
			outDest = {
				outOrigin.x + dir.x * es.fTeleportDistance,
				outOrigin.y,
				outOrigin.z + dir.z * es.fTeleportDistance
			};
			return true;
		}

		bool ResolveArcaneShiftVisual(CWorld& world, EntityID caster, const Vec3& direction,
			Vec3& outOrigin, Vec3& outDest)
		{
			if (!world.HasComponent<TransformComponent>(caster))
				return false;

			auto& tf = world.GetComponent<TransformComponent>(caster);
			const Vec3 dir = WintersMath::NormalizeXZ(direction);
			f32_t fDistance = 4.75f;
			if (world.HasComponent<EzrealStateComponent>(caster))
				fDistance = world.GetComponent<EzrealStateComponent>(caster).fTeleportDistance;

			outDest = tf.GetWorldPosition();
			outOrigin = {
				outDest.x - dir.x * fDistance,
				outDest.y,
				outDest.z - dir.z * fDistance
			};
			return true;
		}

		void ApplyArcaneShift(CWorld& world, EntityID caster, const Vec3& direction)
		{
			Vec3 origin{};
			Vec3 dest{};
			if (!ResolveArcaneShift(world, caster, direction, origin, dest))
				return;

			auto& tf = world.GetComponent<TransformComponent>(caster);
			tf.SetPosition(dest);
			tf.m_bLocalDirty = true;
			tf.m_bWorldDirty = true;

			char dbg[160]{};
			sprintf_s(dbg, "[Ezreal E TP] from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f)\n",
				origin.x, origin.y, origin.z, dest.x, dest.y, dest.z);
			OutputDebugStringA(dbg);
		}

		void SpawnBasicAttackVisual(CWorld& world, EntityID caster,
			const CastSkillCommand& command)
		{
			Fx::SpawnBAProjectile(world, caster,
				GetMuzzlePos(world, caster),
				command.direction, 0.4f);
		}

		void SpawnMysticShotVisual(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
			EntityID caster, const CastSkillCommand& command)
		{
			const Vec3 origin = GetMuzzlePos(world, caster);
			const Vec3 dir = WintersMath::NormalizeXZ(command.direction);
			Fx::SpawnQProjectile(world, pRenderer, caster,
				origin, dir, 1.0f, 30.f);
		}

		void SpawnEssenceFluxVisual(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
			EntityID caster, const CastSkillCommand& command)
		{
			const Vec3 origin = GetMuzzlePos(world, caster);
			const Vec3 dir = WintersMath::NormalizeXZ(command.direction);
			Fx::SpawnWProjectile(world, pRenderer, caster,
				origin, dir, 1.5f, 18.f);
		}

		void SpawnArcaneShiftVisual(CWorld& world, EntityID caster,
			const CastSkillCommand& command)
		{
			Vec3 origin{};
			Vec3 dest{};
			if (!ResolveArcaneShiftVisual(world, caster, command.direction, origin, dest))
				return;

			Fx::SpawnEFlash(world, origin, dest, 0.4f);
		}

		void SpawnTrueshotBarrageVisual(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
			EntityID caster, const CastSkillCommand& command)
		{
			if (!world.HasComponent<EzrealStateComponent>(caster))
				return;

			auto& es = world.GetComponent<EzrealStateComponent>(caster);
			const Vec3 origin = GetMuzzlePos(world, caster);
			const Vec3 dir = WintersMath::NormalizeXZ(command.direction);

			Fx::SpawnRBow(world, pRenderer, caster, 0.4f);
			Fx::SpawnRProjectile(world, pRenderer, caster,
				origin, dir, es.fGlobalLifetime, es.fGlobalSpeed);
		}

		void ApplyKeySwapE(std::string& key, const Vec3& direction)
		{
			const Vec3 dir = WintersMath::NormalizeXZ(direction);
			const f32_t yawDeg = std::atan2f(dir.x, dir.z) * 57.2958f;

			if (yawDeg < -135.f || yawDeg > 135.f)
				key = "spell3_180";
			else if (yawDeg < -45.f)
				key = "spell3_-90";
			else if (yawDeg < 45.f)
				key = "spell3_generic";
			else
				key = "spell3_90";
		}
	}

	void OnCastFrame_BA(SkillHookContext& ctx)
	{
		if (!ctx.pWorld || !ctx.pCommand)
			return;

		ApplyBasicAttack(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
			ctx.pCommand->targetEntityId);
		SpawnBasicAttackVisual(*ctx.pWorld, ctx.casterEntity, *ctx.pCommand);
	}

	void OnCastFrame_Q(SkillHookContext& ctx)
	{
		if (!ctx.pWorld || !ctx.pCommand)
			return;

		ScheduleMysticShot(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
			ctx.pCommand->direction);
		SpawnMysticShotVisual(*ctx.pWorld, ctx.pFxMeshRenderer,
			ctx.casterEntity, *ctx.pCommand);
	}

	void OnCastFrame_W(SkillHookContext& ctx)
	{
		if (!ctx.pWorld || !ctx.pCommand)
			return;

		ScheduleEssenceFlux(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
			ctx.pCommand->direction);
		SpawnEssenceFluxVisual(*ctx.pWorld, ctx.pFxMeshRenderer,
			ctx.casterEntity, *ctx.pCommand);
	}

	void OnCastFrame_R(SkillHookContext& ctx)
	{
		if (!ctx.pWorld || !ctx.pCommand)
			return;

		ScheduleTrueshotBarrage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
			ctx.pCommand->direction);
		SpawnTrueshotBarrageVisual(*ctx.pWorld, ctx.pFxMeshRenderer,
			ctx.casterEntity, *ctx.pCommand);
	}

	void OnCastAccepted_E(SkillHookContext& ctx)
	{
		if (!ctx.pWorld || !ctx.pCommand)
			return;

		ApplyArcaneShift(*ctx.pWorld, ctx.casterEntity, ctx.pCommand->direction);
		SpawnArcaneShiftVisual(*ctx.pWorld, ctx.casterEntity, *ctx.pCommand);
	}

	void OnKeySwap_E(SkillHookContext& ctx)
	{
		if (!ctx.pKeyOut || !ctx.pCommand)
			return;

		ApplyKeySwapE(*ctx.pKeyOut, ctx.pCommand->direction);
	}

	namespace Gameplay
	{
		void OnCastFrame_BA(GameplayHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand) return;
			ApplyBasicAttack(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
				ctx.pCommand->targetEntity);
		}

		void OnCastFrame_Q(GameplayHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand) return;
			ScheduleMysticShot(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
				ctx.pCommand->direction);
		}

		void OnCastFrame_W(GameplayHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand) return;
			ScheduleEssenceFlux(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
				ctx.pCommand->direction);
		}

		void OnCastFrame_R(GameplayHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand) return;
			ScheduleTrueshotBarrage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
				ctx.pCommand->direction);
		}

		void OnCastAccepted_E(GameplayHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand) return;
			ApplyArcaneShift(*ctx.pWorld, ctx.casterEntity,
				ctx.pCommand->direction);
		}
	}

	namespace Visual
	{
		void OnKeySwap_E(VisualHookContext& ctx)
		{
			if (!ctx.pKeyOut || !ctx.pCommand)
				return;

			ApplyKeySwapE(*ctx.pKeyOut, ctx.pCommand->direction);
		}

		void OnCastAccepted_E_Visual(VisualHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand)
				return;

			SpawnArcaneShiftVisual(*ctx.pWorld, ctx.casterEntity, *ctx.pCommand);
		}

		void OnCastFrame_BA_Visual(VisualHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand)
				return;

			SpawnBasicAttackVisual(*ctx.pWorld, ctx.casterEntity, *ctx.pCommand);
		}

		void OnCastFrame_Q_Visual(VisualHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand)
				return;

			SpawnMysticShotVisual(*ctx.pWorld, ctx.pFxMeshRenderer,
				ctx.casterEntity, *ctx.pCommand);
		}

		void OnCastFrame_W_Visual(VisualHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand)
				return;

			SpawnEssenceFluxVisual(*ctx.pWorld, ctx.pFxMeshRenderer,
				ctx.casterEntity, *ctx.pCommand);
		}

		void OnCastFrame_R_Visual(VisualHookContext& ctx)
		{
			if (!ctx.pWorld || !ctx.pCommand)
				return;

			SpawnTrueshotBarrageVisual(*ctx.pWorld, ctx.pFxMeshRenderer,
				ctx.casterEntity, *ctx.pCommand);
		}
	}
}
