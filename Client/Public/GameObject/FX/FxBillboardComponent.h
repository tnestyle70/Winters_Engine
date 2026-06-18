#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "FX/FxLifeCycle.h"
#include "Renderer/BlendTypes.h"
#include <memory>
#include <string>
#include <utility>

//월드 공간 알파 쿼드 이펙트  
//POD - CWorld Component Store에 값 복사로 보관 
struct FxBillboardComponent
{
	//위치/추종/속도 
	Vec3 vWorldPos = { 0.f, 0.f, 0.f };
	EntityID attachTo = NULL_ENTITY;
	Vec3 vAttachOffset = { 0.f, 3.f, 0.f };
	FxAnchorDesc anchor{};
	FxLifecycleDesc lifecycle{};
	bool_t bAnchorResolvedLastFrame = false;
	Vec3 vVelocity = { 0.f, 0.f, 0.f }; //투사체용

	//텍스쳐 + 크기
	const wchar_t* texturePath = nullptr;
	std::shared_ptr<const wstring_t> texturePathOwner = {};
	FxAssetHandle hAsset{};
	eFxRenderType renderType = eFxRenderType::Billboard;
	u32_t iEmitterIndex = 0;
	f32_t fWidth = 1.f;
	f32_t fHeight = 1.f;
	f32_t fYaw = 0.f;
	f32_t fStartRadius = 0.f;
	f32_t fEndRadius = 0.f;
	f32_t fThickness = 0.2f;
	f32_t fGrowDuration = 0.f;

	//색상/알파/페이드
	Vec4 vColor = { 1.f, 1.f, 1.f, 1.f };
	f32_t fFadeIn = 0.f;
	f32_t fFadeOut = 0.f;

	//아틀라스
	u32_t iAtlasCols = 1;
	u32_t iAtlasRows = 1;
	u32_t iAtlasFrameCount = 1;
	f32_t fAtlasFps = 0.f;
	bool bAtlasLoop = true;

	//UV Scroll
	f32_t fUvScrollU = 0.f;
	f32_t fUvScrollV = 0.f;

	// Material ABI bridge for sprite-style FX.
	f32_t fAlphaClip = 0.05f;
	f32_t fErodeThreshold = 0.f;

	FxMaterialDesc material{};
	eFxDepthMode depthMode = eFxDepthMode::DepthTestWriteOn;
	bool_t bMaterialReady = false;

	//Blend - eBlendPreset 재사용
	eBlendPreset blendMode = eBlendPreset::AlphaBlend;

	// ── 수명 (기존) ──
	f32_t fLifetime = 3.f;
	f32_t fElapsed = 0.f;
	f32_t fStartDelay = 0.f;
	f32_t fCompletionElapsed = 0.f;
	f32_t fCompletionDuration = 0.f;
	eFxLifecycleState eLifecycleState = eFxLifecycleState::Active;
	// ── 회전/페이싱 (기존 — facingMode 보류) ──
	bool bBillboard = true;
	bool bPendingDelete = false;

	//Yasuo Wind Wall
	bool bBlockableByWindWall = false;

	//Desc 매개변수를 통해서 받은 값으로 FxBillboardComponent의 값을 세팅해준다
	void SetMaterialFromDesc(const FxMaterialDesc& desc, eFxDepthMode mode)
	{
		material = desc;
		depthMode = mode;

		vColor = material.vTint;
		fUvScrollU = material.vUVScroll.x;
		fUvScrollV = material.vUVScroll.y;
		fAlphaClip = material.fAlphaClip;
		fErodeThreshold = material.fErodeThreshold;
		bMaterialReady = true;
	}

	void RefreshMaterialFromLegacyFields()
	{
		FxSetMaterialDrawFields(
			material,
			vColor,
			{ 0.f, 0.f, 1.f, 1.f },
			{ fUvScrollU, fUvScrollV },
			fAlphaClip,
			fErodeThreshold);

		bMaterialReady = true;
	}

	void SetTexturePath(const tchar_t* path)
	{
		if (!path || path[0] == 0)
		{
			texturePathOwner.reset();
			texturePath = nullptr;
			return;
		}

		texturePathOwner = std::make_shared<wstring_t>(path);
		texturePath = texturePathOwner->c_str();
	}

	void SetTexturePath(wstring_t path)
	{
		if (path.empty())
		{
			texturePathOwner.reset();
			texturePath = nullptr;
			return;
		}

		texturePathOwner = std::make_shared<wstring_t>(std::move(path));
		texturePath = texturePathOwner->c_str();
	}
};
