#pragma once

#include "GameObject/FX/FxBillboardComponent.h"

namespace FxPresetStyle
{
	inline void ApplyLoLMagicSurface(
		FxBillboardComponent& fx,
		const Vec4& vHotCore,
		const Vec4& vEdge,
		const Vec4& vRim,
		const Vec4& vScroll,
		const Vec4& vShape,
		const Vec4& vCore,
		f32_t fRandom,
		eFxDepthMode depthMode = eFxDepthMode::DepthTestWriteOff)
	{
		FxMaterialDesc material = fx.material;
		material.vTint = fx.vColor;
		material.vUVScroll = { fx.fUvScrollU, fx.fUvScrollV };
		material.fAlphaClip = fx.fAlphaClip;
		material.fErodeThreshold = fx.fErodeThreshold;
		material.iStyleMode = static_cast<u32_t>(eFxMaterialStyleMode::MagicSurface);
		material.vStyleColorA = vHotCore;
		material.vStyleColorB = vEdge;
		material.vRimColor = vRim;
		material.vMagicScrollA = vScroll;
		material.vMagicShape = vShape;
		material.vMagicCore = vCore;
		material.fMaterialRandom = fRandom;
		fx.SetMaterialFromDesc(material, depthMode);
	}
}
