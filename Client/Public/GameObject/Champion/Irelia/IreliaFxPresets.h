#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
	class CFxStaticMeshRenderer;
}

//Irelia Fx Preset Function
//각 함수들이 FxBillboardComponent / FxMeshComponent 값 채우기
//InGameScene의 Spawn에 위임
namespace IreliaFx
{
	//Q
	EntityID SpawnQTrail(CWorld& world, EntityID owner, f32_t fLifetime);
	EntityID SpawnQMark(CWorld& world, EntityID target, f32_t fLifeTime);

	//W
	struct IreliaWHoldFxIds
	{
		EntityID spinFxID = NULL_ENTITY;
		EntityID shieldFxID = NULL_ENTITY;
		EntityID glowFxID = NULL_ENTITY;
		EntityID blockFxID = NULL_ENTITY;
	};

	IreliaWHoldFxIds SpawnWSpinLayers(CWorld& world, EntityID owner, f32_t fLifetime);
	EntityID SpawnWSpin(CWorld& world, EntityID owner, f32_t fLifetime);
	// W stage2 (마우스 방향 본체 앞 2장 겹침)
	void     SpawnWStage2Slash(CWorld& world, EntityID owner, const Vec3& vForward);

	//   W2 (토글 종료) Release Layers — swipe_blades + mis_glow 2장 합성
	//   blade_erode 는 SpawnWStage2Slash 가 담당 — 여기선 swipe + glow 만.
	//   원작 LoL — base AlphaBlend (swipe_blades) + Additive luminance (mis_glow).
	//   PlaneRenderer DepthWriteMask=ZERO 라 깊이 충돌 없음.
	//   vAttachOffset: owner 기준 world-space offset (W2 forward swipe 위치).
	void SpawnWReleaseLayers(CWorld& world, EntityID owner, f32_t fLifetime, f32_t fSize,
		const Vec4& vBladesColor, const Vec4& vGlowColor,
		const Vec3& vAttachOffset = { 0.f, 1.f, 0.f });

	//E
	//SpawnPlaced의 thin wrapper 기존 IreliaBladeSystem의 호출을 유지
	EntityID SpawnEBlade(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		const Vec3& vGround, EntityID owner,
		f32_t fScale, const Vec3& vRotation,
		f32_t fWorldYawSpinSpeed = 0.f);

	EntityID SpawnEBeam(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		const Vec3& vMid, f32_t fYaw, f32_t fLength,
		f32_t fGirth, f32_t fBaseScale, f32_t fAxisScale);

	// 빔 경로상 적 머리 위 스턴 마크 (mark 와 동일하지만 의미 명시)
	EntityID SpawnStunMark(CWorld& world, EntityID target, f32_t fDuration);

	struct IreliaEPlacedFxIds
	{
		EntityID groundGlowFxID = NULL_ENTITY;
		EntityID groundCoreFxID = NULL_ENTITY;
	};

	IreliaEPlacedFxIds SpawnEPlacedLayers(CWorld& world, const Vec3& vBladePos, f32_t fLifetime);

	void SpawnECloseLayers(CWorld& world, const Vec3& vStart, const Vec3& vEnd, f32_t fLifetime);

	// ── R ──
	EntityID SpawnRPulse(CWorld& world, const Vec3& vOrigin, const Vec3& vForward,
		f32_t fSpeed, f32_t fLifetime,
		f32_t fWidth, f32_t fHeight,
		f32_t fYOffset, f32_t fFwdOffset, f32_t fYawOffset);

	void SpawnRHitLayers(CWorld& world, const Vec3& vHitPos, const Vec3& vForward, f32_t fLifetime);

	// R 명중 시 E 본체 칼날 mesh를 명중점 기준 벽으로 배치한다.
	// bTriangle=true: 왼쪽 양변 끝점 -> 전방 꼭짓점 -> 오른쪽 양변 끝점의 직선 위에 배치한다.
	// bTriangle=false: 기존 균등 원호 배치를 유지한다.
	// iCount는 중앙 꼭짓점 칼날이 정확히 하나가 되도록 홀수를 사용한다.
	// fSideShrink는 꼭짓점이 아닌 좌우 끝점으로 갈수록 적용할 scale 감소율이다.
	// 이 함수는 SpawnPlaced를 호출하지 않으므로 E 공전 칼날과 Irelia.E.Place cue를 생성하지 않는다.
	void SpawnRBladeFan(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		const Vec3& vHitPos, const Vec3& vForward,
		int32_t iCount, f32_t fSpreadRad, f32_t fPlaceDist,
		f32_t fLifetime, f32_t fScale, const Vec3& vRotation,
		bool bTriangle = false, f32_t fTipBoost = 0.f, f32_t fSideShrink = 0.f);
}
