#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

enum class eFxMaterialStyleMode : u32_t
{
	LegacyTint = 0,
	BrushRim = 1,
	ToonCell = 2,
	Gradient = 3,
	MagicSurface = 4,
	VolumetricSmoke = 16,
	MotionTrail = 17,
};

struct FxMaterialDesc
{
	Vec4 vTint = { 1.f, 1.f, 1.f, 1.f };
	Vec4 vUVRect = { 0.f, 0.f, 1.f, 1.f };
	Vec2 vUVScroll = { 0.f, 0.f };
	f32_t fAlphaClip = 0.05f;
	f32_t fErodeThreshold = 0.f;

	u32_t iStyleMode = static_cast<u32_t>(eFxMaterialStyleMode::LegacyTint);

	Vec4 vStyleColorA = { 1.f, 1.f, 1.f, 1.f };
	Vec4 vStyleColorB = { 0.f, 0.f, 0.f, 1.f };
	Vec4 vRimColor = { 1.f, 1.f, 1.f, 0.f };

	f32_t fRimPower = 3.f;
	f32_t fCellLow = 0.f;
	f32_t fCellHigh = 0.5f;

	Vec4 vMagicScrollA = { 0.f, 0.5f, 0.1f, 0.05f };
	Vec4 vMagicShape = { 2.5f, 0.06f, 1.0f, 0.035f };
	Vec4 vMagicCore = { 2.0f, 1.0f, 2.0f, 0.f };

	f32_t fMaterialRandom = 0.f;
};

inline void FxSetMaterialDrawFields(
	FxMaterialDesc& material,
	const Vec4& vTint,
	const Vec4& vUVRect,
	const Vec2& vUVScroll,
	f32_t fAlphaClip,
	f32_t fErodeThreshold)
{
	material.vTint = vTint;
	material.vUVRect = vUVRect;
	material.vUVScroll = vUVScroll;
	material.fAlphaClip = fAlphaClip;
	material.fErodeThreshold = fErodeThreshold;
}
