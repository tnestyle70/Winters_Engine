#include "GameObject/Champion/Ezreal/Ezreal_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "WintersMath.h"

namespace
{
	constexpr const char* kCueBACast = "Ezreal.BA.Cast";
	constexpr const char* kCueBAProjectile = "Ezreal.BA.Projectile";
	constexpr const char* kCueQCast = "Ezreal.Q.Cast";
	constexpr const char* kCueQProjectile = "Ezreal.Q.Projectile";
	constexpr const char* kCueWCast = "Ezreal.W.Cast";
	constexpr const char* kCueWProjectile = "Ezreal.W.Projectile";
	constexpr const char* kCueEFlash = "Ezreal.E.BlinkFlash";
	constexpr const char* kCueRCast = "Ezreal.R.Cast";
	constexpr const char* kCueRMissile = "Ezreal.R.Missile";

	Vec3 NormalizeForward(const Vec3& dir)
	{
		Vec3 n = WintersMath::NormalizeXZOrZero(dir);
		if (n.x == 0.f && n.z == 0.f)
			n = { 0.f, 0.f, 1.f };
		return n;
	}

	Vec3 ResolveEntityPos(CWorld& world, EntityID entity, const Vec3& fallback)
	{
		if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
			return world.GetComponent<TransformComponent>(entity).GetPosition();
		return fallback;
	}

	void PlayAttachedCue(CWorld& world,
		const char* cueName,
		Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner,
		const Vec3& origin,
		const Vec3& dir,
		f32_t lifetimeOverride = 0.f)
	{
		FxCueContext cue{};
		cue.attachTo = owner;
		cue.vWorldPos = ResolveEntityPos(world, owner, origin);
		cue.vForward = NormalizeForward(dir);
		cue.pFxMeshRenderer = pRenderer;
		if (lifetimeOverride > 0.f)
		{
			cue.bOverrideLifetime = true;
			cue.fLifetimeOverride = lifetimeOverride;
		}

		CFxCuePlayer::PlayAll(world, cueName, cue, nullptr);
	}

	void PlayMovingCue(CWorld& world,
		const char* cueName,
		Engine::CFxStaticMeshRenderer* pRenderer,
		const Vec3& origin,
		const Vec3& dir,
		f32_t lifetime,
		f32_t speed)
	{
		const Vec3 n = NormalizeForward(dir);

		FxCueContext cue{};
		cue.vWorldPos = origin;
		cue.vForward = n;
		cue.vVelocity = { n.x * speed, 0.f, n.z * speed };
		cue.bOverrideVelocity = true;
		cue.pFxMeshRenderer = pRenderer;
		if (lifetime > 0.f)
		{
			cue.bOverrideLifetime = true;
			cue.fLifetimeOverride = lifetime;
		}

		CFxCuePlayer::PlayAll(world, cueName, cue, nullptr);
	}

	void PlaySegmentCue(CWorld& world,
		const char* cueName,
		const Vec3& origin,
		const Vec3& dest,
		f32_t lifetime)
	{
		const Vec3 dir{ dest.x - origin.x, 0.f, dest.z - origin.z };

		FxCueContext cue{};
		cue.vWorldPos = origin;
		cue.vEndWorldPos = dest;
		cue.vForward = NormalizeForward(dir);
		cue.bOverrideEndWorldPos = true;
		if (lifetime > 0.f)
		{
			cue.bOverrideLifetime = true;
			cue.fLifetimeOverride = lifetime;
		}

		CFxCuePlayer::PlayAll(world, cueName, cue, nullptr);
	}
}

namespace Ezreal::Fx
{
	void SpawnBAProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime)
	{
		PlayAttachedCue(world, kCueBACast, nullptr, owner, origin, dir);
		PlayMovingCue(world, kCueBAProjectile, nullptr, origin, dir, fLifetime, 20.f);
	}

	void SpawnQProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed)
	{
		PlayAttachedCue(world, kCueQCast, pRenderer, owner, origin, dir);
		PlayMovingCue(world, kCueQProjectile, pRenderer, origin, dir, fLifetime, fSpeed);
	}

	void SpawnWProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed)
	{
		PlayAttachedCue(world, kCueWCast, pRenderer, owner, origin, dir);
		PlayMovingCue(world, kCueWProjectile, pRenderer, origin, dir, fLifetime, fSpeed);
	}

	void SpawnEFlash(CWorld& world, const Vec3& origin, const Vec3& dest,
		f32_t fLifetime)
	{
		PlaySegmentCue(world, kCueEFlash, origin, dest, fLifetime);
	}

	void SpawnRBow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, f32_t fLifetime)
	{
		PlayAttachedCue(world, kCueRCast, pRenderer, owner, {}, { 0.f, 0.f, 1.f },
			fLifetime);
	}

	void SpawnRProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed)
	{
		(void)owner;
		PlayMovingCue(world, kCueRMissile, pRenderer, origin, dir, fLifetime, fSpeed);
	}
}
