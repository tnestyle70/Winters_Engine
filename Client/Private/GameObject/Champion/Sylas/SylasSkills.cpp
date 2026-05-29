#include "GameObject/Champion/Sylas/SylasSkills.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GamePlay/VisualHookRegistry.h"
#include "WintersMath.h"

#include <cmath>

namespace
{
	bool_t IsMeaningfulPosition(const Vec3& v)
	{
		return std::fabs(v.x) > 0.001f ||
			std::fabs(v.y) > 0.001f ||
			std::fabs(v.z) > 0.001f;
	}

	Vec3 ResolveCasterPosition(VisualHookContext& ctx)
	{
		if (ctx.pWorld && ctx.casterEntity != NULL_ENTITY &&
			ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
		{
			return ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
		}

		return ctx.pCommand ? ctx.pCommand->groundPos : Vec3{};
	}

	Vec3 ResolveEffectPosition(VisualHookContext& ctx)
	{
		if (ctx.pCommand && IsMeaningfulPosition(ctx.pCommand->groundPos))
			return ctx.pCommand->groundPos;

		return ResolveCasterPosition(ctx);
	}

	Vec3 ResolveTargetOrEffectPosition(VisualHookContext& ctx)
	{
		if (ctx.pWorld && ctx.pCommand &&
			ctx.pCommand->targetEntityId != NULL_ENTITY &&
			ctx.pWorld->HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
		{
			return ctx.pWorld->GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
		}

		return ResolveEffectPosition(ctx);
	}

	Vec3 ResolveForward(VisualHookContext& ctx)
	{
		if (ctx.pCommand)
		{
			const Vec3 vCommandDir = WintersMath::NormalizeXZOrZero(ctx.pCommand->direction);
			if (vCommandDir.x != 0.f || vCommandDir.z != 0.f)
				return vCommandDir;	
		}

		if (ctx.pWorld && ctx.pCommand &&
			ctx.casterEntity != NULL_ENTITY &&
			ctx.pCommand->targetEntityId != NULL_ENTITY &&
			ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity) &&
			ctx.pWorld->HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
		{
			const Vec3 vCaster =
				ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
			const Vec3 vTarget =
				ctx.pWorld->GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
			const Vec3 vToTarget{ vTarget.x - vCaster.x, 0.f, vTarget.z - vCaster.z };
			const Vec3 vTargetDir = WintersMath::NormalizeXZOrZero(vToTarget);
			if (vTargetDir.x != 0.f || vTargetDir.z != 0.f)
				return vTargetDir;
		}

		if (ctx.pWorld &&
			ctx.casterEntity != NULL_ENTITY &&
			ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
		{
			const f32_t yaw = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetRotation().y;
			return WintersMath::DirectionFromYawXZ(yaw);
		}
		
		return { 0.f, 0.f, 1.f };
	}
	//sylas fx cue play
	void PlaySylasCueAt(VisualHookContext& ctx, const char* pszCueName,
		const Vec3& vWorldPos, bool_t bAttachToCaster)
	{
		if (!ctx.pWorld)
			return;

		FxCueContext fx{};
		fx.vWorldPos = vWorldPos;
		fx.vForward = ResolveForward(ctx);
		fx.attachTo = bAttachToCaster ? ctx.casterEntity : NULL_ENTITY;
		fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
		CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx);
	}

	void PlaySylasCueSegment(VisualHookContext& ctx, const char* pszCueName,
		const Vec3& vStartWorldPos, const Vec3& vEndWorldPos)
	{
		if (!ctx.pWorld)
			return;

		FxCueContext fx{};
		fx.vWorldPos = vStartWorldPos;
		fx.vEndWorldPos = vEndWorldPos;
		fx.bOverrideEndWorldPos = true;
		fx.vForward = ResolveForward(ctx);
		fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
		CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx);
	}

	void PlaySylasCue(VisualHookContext& ctx, const char* pszCueName, bool_t bAttachToCaster)
	{
		PlaySylasCueAt(ctx, pszCueName, ResolveCasterPosition(ctx), bAttachToCaster);
	}
}

namespace Sylas
{
	namespace Visual
	{
		void OnQCastFrame(VisualHookContext& ctx)
		{
			PlaySylasCue(ctx, "Sylas.Q.Cast", true);
			PlaySylasCueAt(ctx, "Sylas.Q.Explosion", ResolveEffectPosition(ctx), false);
		}

		void OnWCastFrame(VisualHookContext& ctx)
		{
			PlaySylasCueAt(ctx, "Sylas.W.Cast", ResolveTargetOrEffectPosition(ctx), false);
		}

		void OnECastFrame(VisualHookContext& ctx)
		{
			if (ctx.skillStage >= 2u)
			{
				Vec3 vStart = ResolveCasterPosition(ctx);
				Vec3 vEnd = ResolveTargetOrEffectPosition(ctx);
				if (WintersMath::DistanceSqXZ(vStart, vEnd) < 0.01f)
				{
					const Vec3 vForward = ResolveForward(ctx);
					const f32_t fRange = (ctx.pDef && ctx.pDef->rangeMax > 0.f)
						? ctx.pDef->rangeMax
						: 6.f;
					vEnd = Vec3{
						vStart.x + vForward.x * fRange,
						vStart.y,
						vStart.z + vForward.z * fRange
					};
				}
				vStart.y += 1.1f;
				vEnd.y += 1.0f;
				PlaySylasCueSegment(ctx, "Sylas.E2.Chain", vStart, vEnd);
			}
			else
			{
				PlaySylasCue(ctx, "Sylas.E1.Dash", true);
			}
		}

		void OnRCastFrame(VisualHookContext& ctx)
		{
			Vec3 vStart = ResolveCasterPosition(ctx);
			Vec3 vEnd = ResolveTargetOrEffectPosition(ctx);
			vStart.y += 1.35f;
			vEnd.y += 1.25f;
			PlaySylasCueSegment(ctx, "Sylas.R.Cast", vStart, vEnd);
		}
	}
}
