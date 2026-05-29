#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "WintersMath.h"

#include <vector>

class CWorld;

namespace Engine
{
	class CFxStaticMeshRenderer;
}

struct FxCueContext
{
	Vec3 vWorldPos{};
	Vec3 vForward{ 0.f, 0.f, 1.f };
	Vec3 vVelocity{};
	Vec3 vEndWorldPos{};
	EntityID attachTo = NULL_ENTITY;
	Engine::CFxStaticMeshRenderer* pFxMeshRenderer = nullptr;
	bool_t bOverrideVelocity = false;
	bool_t bOverrideLifetime = false;
	bool_t bOverrideEndWorldPos = false;
	bool_t bOverrideSize = false;
	f32_t fLifetimeOverride = 0.f;
	f32_t fWidthOverride = 1.f;
	f32_t fHeightOverride = 1.f;
};

class CFxCuePlayer final
{
public:
	static u32_t PreloadDirectory(const wchar_t* wszDirectoryPath);
	static FxAssetHandle FindCue(const char* pszCueName);
	static EntityID Play(CWorld& world, const char* pszCueName, const FxCueContext& ctx);
	static EntityID PlayAll(CWorld& world, const char* pszCueName, const FxCueContext& ctx,
		std::vector<EntityID>* pOutSpawned);

private:
	CFxCuePlayer() = delete;
};
