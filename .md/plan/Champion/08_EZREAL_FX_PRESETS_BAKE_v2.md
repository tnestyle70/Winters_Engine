# Phase B-11d-FX v2 — Ezreal FxPresets 본격 박제 (Codex 보정)

**작성일**: 2026-04-30
**v1 → v2 변경 사유**: v1 의 "1파일 수정" 범위가 현재 FX mesh API 와 충돌 (Codex 리뷰).
**v2 범위**: 5 파일 수정 — `SkillHookContext` 확장 + `Scene_InGame` ctx 주입 + `Ezreal_FxPresets.h/.cpp` 시그니처/구현 + `Ezreal_Skills.cpp` 호출
**목적**: Q/W/E/R/BA 시각 효과 0 → LoL 원작 95% 재현 + visual/gameplay speed 일치
**전제**: ① fbx 16개 변환 완료 ② 텍스처 22개 존재 ③ FxBillboardComponent / FxMeshComponent / CFxSystem / CFxMeshSystem 인프라 박제 완료

---

## 0. v1 → v2 변경 정리 (Codex 검증 반영)

| # | v1 가정 | 코드 실측 | v2 조치 |
|---|---|---|---|
| **C1** | `CFxMeshSystem::Spawn(world, mesh)` | 실제: `Spawn(CWorld&, Engine::CFxStaticMeshRenderer*, const FxMeshComponent&)` ([FxMeshSystem.h:27](Client/Public/GameObject/FX/FxMeshSystem.h:27)) | `pRenderer` 인자 도입 — Hook ctx 로 전달 |
| **C2** | `SkillHookContext` 에 mesh renderer 있음 | 7개 필드만 ([SkillHookContext.h:14-23](Client/Public/GamePlay/SkillHookContext.h:14)) | `Engine::CFxStaticMeshRenderer* pFxMeshRenderer` 추가 |
| **C3** | E Flash `vAttachOffset = {0,1,0}` 적용됨 | `attachTo != NULL_ENTITY` 분기에서만 offset 적용 ([FxSystem.cpp:50-64](Client/Private/GameObject/FX/FxSystem.cpp:50)) | 월드 FX 는 `vWorldPos.y += 1.0f` 직접 박기 |
| **C4** | Q/W/R mesh 발사 방향 자동 | `vRotation` 디폴트 (0,0,0) — 미지정 시 forward 안 봄 | Kalista 패턴: `vRotation = { 0, atan2f(dir.x, dir.z) + XM_PI, 0 }` ([KalistaFxPresets.cpp:33](Client/Private/GameObject/Champion/Kalista/KalistaFxPresets.cpp:33)) |
| **C5** | Visual speed 임의 (BA=18, Q=30, W=24, R=60) | Gameplay 실측: BA(미정), Q=30, W=18, R=`es.fGlobalSpeed`=25 ([Ezreal_Skills.cpp:71,90](Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp:71), [Ezreal_Components.h:9](Client/Public/GameObject/Champion/Ezreal/Ezreal_Components.h:9)) | Visual = Gameplay 일치. Speed 인자로 받아 Skills.cpp 가 명시 전달 |

**v1 의 좋은 점 (그대로 유지)**:
- 자산 매핑 정확 (`particles/` + `particles/fbx/`)
- Visual hook 빈 채로 유지 ✅ — `Scene_InGame.cpp:1041-1048` 가 Visual hook 먼저 dispatch → SkillHook dispatch. Visual 도 박제하면 2중 FX
- Color 시그니처 (Yellow/Gold/Cyan/White)
- Multi-layer 합성 (Base AlphaBlend + Glow Additive)

---

## 1. 자산 매핑 (v1 동일 — 변경 없음)

### Mesh fbx 5개 (`particles/fbx/`)
| 변수 | 파일 | 용도 |
|---|---|---|
| `kFbxQMis` | `ezreal_base_q_mis.fbx` | Q 발사체 |
| `kFbxWMis` | `ezreal_base_w_mis.fbx` | W 구체 |
| `kFbxRMis` | `ezreal_base_r_mis.fbx` | R 화살 |
| `kFbxRMisTail` | `ezreal_base_r_mis_tail.fbx` | R 꼬리 |
| `kFbxRBow` | `ezreal_r_cas_bow_mesh.fbx` | R 시전 활 |

### Texture 17개 (`particles/`) — v1 표 그대로

### Color 시그니처 4개
- `kColorYellow{ 1.0, 0.85, 0.3, 1.0 }` — BA/Q/E/R
- `kColorGold{ 1.0, 0.7, 0.2, 1.0 }` — halo/glow 강조
- `kColorCyan{ 0.4, 0.8, 1.0, 1.0 }` — W Essence Flux
- `kColorWhite{ 1.0, 1.0, 1.0, 1.0 }` — 중립

---

## 2. 변경 범위 (5 파일)

| 파일 | 변경 종류 | 분량 |
|---|---|---|
| `Client/Public/GamePlay/SkillHookContext.h` | 필드 1개 추가 + forward decl | +3줄 |
| `Client/Private/Scene/Scene_InGame.cpp` | hook ctx 빌드 시 1줄 추가 (cast/recovery 양쪽) | +2줄 |
| `Client/Public/GameObject/Champion/Ezreal/Ezreal_FxPresets.h` | 함수 시그니처 변경 (pRenderer + speed 인자) | +5줄 |
| `Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp` | 전면 재박제 — stub → 본격 | 57 → ~280줄 |
| `Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp` | `Fx::Spawn*` 호출 5곳 수정 | ~10줄 변경 |

**미수정 (그대로)**:
- `Ezreal_Skills.cpp` `Visual::OnCastFrame_*_Visual` — 빈 채 유지 (이중 호출 방지)
- `Ezreal_Skills.cpp` `Gameplay::OnCastFrame_*` — 빈 채 유지
- `Ezreal_Registration.cpp` — Hook 등록 그대로

---

## 3. 파일별 수정 — 전문 / diff

### 3.1 `Client/Public/GamePlay/SkillHookContext.h` (전문 — 26줄)

**수정 전 (현재 — L1-23)**:
```cpp
#pragma once

#include "Defines.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "GameContext.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/SkillDef.h"

#include <string>

class CWorld;

struct SkillHookContext
{
	CWorld* pWorld = nullptr;
	EntityID casterEntity = NULL_ENTITY;
	eTeam casterTeam = eTeam::Blue;
	const SkillDef* pDef = nullptr;
	const CastSkillCommand* pCommand = nullptr;
	f32_t fDeltaTime = 0.f;
	std::string* pKeyOut = nullptr;
};
```

**수정 후 (전문)**:
```cpp
#pragma once

#include "Defines.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "GameContext.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/SkillDef.h"

#include <string>

class CWorld;

namespace Engine { class CFxStaticMeshRenderer; }   // ★ v2 — forward decl

struct SkillHookContext
{
	CWorld* pWorld = nullptr;
	EntityID casterEntity = NULL_ENTITY;
	eTeam casterTeam = eTeam::Blue;
	const SkillDef* pDef = nullptr;
	const CastSkillCommand* pCommand = nullptr;
	f32_t fDeltaTime = 0.f;
	std::string* pKeyOut = nullptr;
	Engine::CFxStaticMeshRenderer* pFxMeshRenderer = nullptr;   // ★ v2 — Mesh FX 스폰용
};
```

---

### 3.2 `Client/Private/Scene/Scene_InGame.cpp` (diff — 2줄 추가)

**수정 위치 1**: cast hook dispatch (L1055-1062 근처)

**수정 전**:
```cpp
SkillHookContext ctx{};
ctx.pWorld = &m_World;
ctx.casterEntity = m_PlayerEntity;
ctx.casterTeam = m_PlayerTeam;
ctx.pDef = m_pActiveSkillDef;
ctx.pCommand = &m_ActiveSkillCommandStorage;
castHandled = CSkillHookRegistry::Instance().Dispatch(
    m_pActiveSkillDef->castFrameHookId, ctx);
```

**수정 후**:
```cpp
SkillHookContext ctx{};
ctx.pWorld = &m_World;
ctx.casterEntity = m_PlayerEntity;
ctx.casterTeam = m_PlayerTeam;
ctx.pDef = m_pActiveSkillDef;
ctx.pCommand = &m_ActiveSkillCommandStorage;
ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();   // ★ v2
castHandled = CSkillHookRegistry::Instance().Dispatch(
    m_pActiveSkillDef->castFrameHookId, ctx);
```

**수정 위치 2-N**: 다른 SkillHookContext 빌드 위치들도 동일하게 1줄 추가
- Recovery hook (있으면)
- BA hook (별도면)
- onCastAccepted hook (E TP — 별도 dispatch 위치)
- 모든 `SkillHookContext ctx{}` / `SkillHookContext ctx;` 직후 한 줄 추가

**검증**: `grep -n "SkillHookContext " Client/Private/Scene/Scene_InGame.cpp` 로 모든 빌드 위치 확인 후 각각 `ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();` 추가.

---

### 3.3 `Client/Public/GameObject/Champion/Ezreal/Ezreal_FxPresets.h` (전문)

**수정 전 (L1-22)**:
```cpp
#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Ezreal::Fx
{
	void SpawnBAProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime);
	void SpawnQProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime);
	void SpawnWProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime);
	void SpawnEFlash(CWorld& world, const Vec3& origin, const Vec3& dest,
		f32_t fLifetime);
	void SpawnRBow(CWorld& world, EntityID owner, f32_t fLifetime);
	void SpawnRProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime);
}
```

**수정 후 (전문)**:
```cpp
#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;
namespace Engine { class CFxStaticMeshRenderer; }   // ★ v2

namespace Ezreal::Fx
{
	// BA — billboard only (mesh 없음, pRenderer 미사용)
	void SpawnBAProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime);

	// Q — mesh + billboard. fSpeed 는 gameplay PendingHit 와 일치시킬 것 (현 30.f)
	void SpawnQProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed);

	// W — mesh + billboard. fSpeed 는 gameplay PendingHit 와 일치 (현 18.f)
	void SpawnWProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed);

	// E — billboard only (월드 고정, 양 끝점)
	void SpawnEFlash(CWorld& world, const Vec3& origin, const Vec3& dest,
		f32_t fLifetime);

	// R Bow — owner 부착 mesh
	void SpawnRBow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, f32_t fLifetime);

	// R Projectile — mesh + billboard. fSpeed = es.fGlobalSpeed (현 25.f)
	void SpawnRProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed);
}
```

---

### 3.4 `Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp` (전문 — ~290줄)

```cpp
#include "GameObject/Champion/Ezreal/Ezreal_FxPresets.h"

#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"

#include "ECS/World.h"

#include <DirectXMath.h>   // XM_PI
#include <Windows.h>
#include <cmath>

namespace
{
	// ───── Mesh paths (UTF-8 / char*) — particles/fbx/ ─────
	constexpr const char* kFbxQMis     = "Client/Bin/Resource/Texture/Character/Ezreal/particles/fbx/ezreal_base_q_mis.fbx";
	constexpr const char* kFbxWMis     = "Client/Bin/Resource/Texture/Character/Ezreal/particles/fbx/ezreal_base_w_mis.fbx";
	constexpr const char* kFbxRMis     = "Client/Bin/Resource/Texture/Character/Ezreal/particles/fbx/ezreal_base_r_mis.fbx";
	constexpr const char* kFbxRMisTail = "Client/Bin/Resource/Texture/Character/Ezreal/particles/fbx/ezreal_base_r_mis_tail.fbx";
	constexpr const char* kFbxRBow     = "Client/Bin/Resource/Texture/Character/Ezreal/particles/fbx/ezreal_r_cas_bow_mesh.fbx";

	// ───── Billboard textures (wchar_t*) — particles/ 루트 ─────
	// BA — Basic Attack
	constexpr const wchar_t* kTexBaGlow   = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_glow.png";
	constexpr const wchar_t* kTexBaMis    = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_mis.png";
	constexpr const wchar_t* kTexBaWhisps = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_whisps.png";
	// Q — Mystic Shot
	constexpr const wchar_t* kTexQTrail    = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_q_mis_trail.png";
	constexpr const wchar_t* kTexQErode    = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_q_erode.png";
	constexpr const wchar_t* kTexQHitSpark = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_q_hit_spark.png";
	// W — Essence Flux
	constexpr const wchar_t* kTexWOrb       = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_w_orb.png";
	constexpr const wchar_t* kTexWRingGlow  = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_w_ringglow.png";
	constexpr const wchar_t* kTexWSpinTex   = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_w_spintex.png";
	constexpr const wchar_t* kTexWMeshLines = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_w_meshlines.png";
	// E — Arcane Shift (z_* + common)
	constexpr const wchar_t* kTexZFlareSun    = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_z_flare-sun.png";
	constexpr const wchar_t* kTexZGlow        = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_z_glow.png";
	constexpr const wchar_t* kTexZRing        = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_z_ring.png";
	constexpr const wchar_t* kTexZBeaconTrail = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_z_color-beacontrail.png";
	constexpr const wchar_t* kTexCrystalFlash = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/common_crystal-flash.png";
	// R — Trueshot Barrage
	constexpr const wchar_t* kTexRMisGlow  = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_r_mis_glow.png";
	constexpr const wchar_t* kTexRTrail    = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_r_trail.png";
	constexpr const wchar_t* kTexRBowTex   = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_r_bow_tex.png";
	constexpr const wchar_t* kTexRBowArrow = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_r_bow_arrow.png";
	constexpr const wchar_t* kTexRTailTex  = L"Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_r_tail_tex.png";

	// ───── Color signatures (LoL Ezreal = 노란-금색, W = essence flux 시안) ─────
	constexpr Vec4 kColorYellow{ 1.f,  0.85f, 0.3f, 1.f };
	constexpr Vec4 kColorGold  { 1.f,  0.7f,  0.2f, 1.f };
	constexpr Vec4 kColorCyan  { 0.4f, 0.8f,  1.f,  1.f };
	constexpr Vec4 kColorWhite { 1.f,  1.f,   1.f,  1.f };

	// ★ v2 — Kalista 패턴: FBX forward 가 -Z 면 +XM_PI 보정 필요
	//   첫 빌드 후 시각이 90/180/270 어긋나면 ±XM_PI/2 또는 0 으로 조정
	inline f32_t YawFromDir(const Vec3& dir)
	{
		return std::atan2f(dir.x, dir.z) + DirectX::XM_PI;
	}
}

namespace Ezreal::Fx
{
	// ───────── BA — Basic Attack (billboard only, mesh 없음) ─────────
	void SpawnBAProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime)
	{
		(void)owner;
		Vec3 vel{ dir.x * 18.f, 0.f, dir.z * 18.f };

		// Layer 1: ba_mis (core energy)
		FxBillboardComponent core{};
		core.vWorldPos   = origin;
		core.vVelocity   = vel;
		core.texturePath = kTexBaMis;
		core.fWidth      = 0.6f;
		core.fHeight     = 0.6f;
		core.fLifetime   = fLifetime;
		core.vColor      = kColorYellow;
		core.blendMode   = eBlendPreset::Additive;
		core.bBillboard  = true;
		CFxSystem::Spawn(world, core);

		// Layer 2: ba_glow (halo)
		FxBillboardComponent glow = core;
		glow.texturePath = kTexBaGlow;
		glow.fWidth      = 1.0f;
		glow.fHeight     = 1.0f;
		glow.vColor      = kColorGold;
		CFxSystem::Spawn(world, glow);

		// Layer 3: ba_whisps (trail)
		FxBillboardComponent trail = core;
		trail.texturePath = kTexBaWhisps;
		trail.fWidth      = 0.4f;
		trail.fHeight     = 0.8f;
		trail.fFadeOut    = 0.2f;
		CFxSystem::Spawn(world, trail);
	}

	// ───────── Q — Mystic Shot (yellow energy bolt) ─────────
	void SpawnQProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed)
	{
		(void)owner;
		Vec3 vel{ dir.x * fSpeed, 0.f, dir.z * fSpeed };

		// Mesh: q_mis.fbx — pRenderer 없으면 mesh skip (빌보드는 그대로)
		if (pRenderer)
		{
			FxMeshComponent mesh{};
			mesh.vWorldPos            = origin;
			mesh.vScale               = { 0.4f, 0.4f, 0.4f };
			mesh.vVelocity            = vel;
			mesh.vRotation            = { 0.f, YawFromDir(dir), 0.f };   // ★ v2 — Kalista 패턴
			mesh.modelPath            = kFbxQMis;
			mesh.texturePath          = kTexQErode;
			mesh.vColor               = kColorYellow;
			mesh.blendMode            = eBlendPreset::Additive;
			mesh.fWorldYawSpinSpeed   = 12.f;
			mesh.fLifetime            = fLifetime;
			mesh.fFadeOut             = 0.15f;
			mesh.bDepthWrite          = false;
			mesh.bBlockableByWindWall = true;
			CFxMeshSystem::Spawn(world, pRenderer, mesh);
		}

		// Trail billboard
		FxBillboardComponent trail{};
		trail.vWorldPos   = origin;
		trail.vVelocity   = vel;
		trail.texturePath = kTexQTrail;
		trail.fWidth      = 0.5f;
		trail.fHeight     = 1.5f;
		trail.fLifetime   = fLifetime;
		trail.fFadeOut    = 0.2f;
		trail.vColor      = kColorYellow;
		trail.blendMode   = eBlendPreset::Additive;
		trail.bBillboard  = true;
		CFxSystem::Spawn(world, trail);

		// Head spark (bright tip)
		FxBillboardComponent head{};
		head.vWorldPos   = origin;
		head.vVelocity   = vel;
		head.texturePath = kTexQHitSpark;
		head.fWidth      = 0.7f;
		head.fHeight     = 0.7f;
		head.fLifetime   = fLifetime;
		head.vColor      = kColorGold;
		head.blendMode   = eBlendPreset::Additive;
		head.bBillboard  = true;
		CFxSystem::Spawn(world, head);
	}

	// ───────── W — Essence Flux (cyan magic orb) ─────────
	void SpawnWProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed)
	{
		(void)owner;
		Vec3 vel{ dir.x * fSpeed, 0.f, dir.z * fSpeed };

		if (pRenderer)
		{
			FxMeshComponent mesh{};
			mesh.vWorldPos            = origin;
			mesh.vScale               = { 0.5f, 0.5f, 0.5f };
			mesh.vVelocity            = vel;
			mesh.vRotation            = { 0.f, YawFromDir(dir), 0.f };   // ★ v2
			mesh.modelPath            = kFbxWMis;
			mesh.texturePath          = kTexWMeshLines;
			mesh.vColor               = kColorCyan;
			mesh.blendMode            = eBlendPreset::Additive;
			mesh.fWorldYawSpinSpeed   = 8.f;
			mesh.fLifetime            = fLifetime;
			mesh.fFadeOut             = 0.2f;
			mesh.bDepthWrite          = false;
			mesh.bBlockableByWindWall = true;
			CFxMeshSystem::Spawn(world, pRenderer, mesh);
		}

		// Orb core
		FxBillboardComponent orb{};
		orb.vWorldPos   = origin;
		orb.vVelocity   = vel;
		orb.texturePath = kTexWOrb;
		orb.fWidth      = 0.8f;
		orb.fHeight     = 0.8f;
		orb.fLifetime   = fLifetime;
		orb.vColor      = kColorCyan;
		orb.blendMode   = eBlendPreset::Additive;
		orb.bBillboard  = true;
		CFxSystem::Spawn(world, orb);

		// Ring glow halo
		FxBillboardComponent ring{};
		ring.vWorldPos   = origin;
		ring.vVelocity   = vel;
		ring.texturePath = kTexWRingGlow;
		ring.fWidth      = 1.4f;
		ring.fHeight     = 1.4f;
		ring.fLifetime   = fLifetime;
		ring.vColor      = { 0.3f, 0.7f, 1.f, 0.7f };
		ring.blendMode   = eBlendPreset::Additive;
		ring.bBillboard  = true;
		CFxSystem::Spawn(world, ring);

		// Spin texture
		FxBillboardComponent spin{};
		spin.vWorldPos   = origin;
		spin.vVelocity   = vel;
		spin.texturePath = kTexWSpinTex;
		spin.fWidth      = 1.0f;
		spin.fHeight     = 1.0f;
		spin.fLifetime   = fLifetime;
		spin.vColor      = kColorCyan;
		spin.blendMode   = eBlendPreset::Additive;
		spin.bBillboard  = true;
		CFxSystem::Spawn(world, spin);
	}

	// ───────── E — Arcane Shift (월드 FX, vAttachOffset 무시 — vWorldPos 직박) ─────────
	void SpawnEFlash(CWorld& world, const Vec3& origin, const Vec3& dest,
		f32_t fLifetime)
	{
		// === Origin (출발 잔상) ===
		{
			FxBillboardComponent flash{};
			flash.vWorldPos   = { origin.x, origin.y + 1.0f, origin.z };   // ★ v2 — offset 직박
			flash.texturePath = kTexCrystalFlash;
			flash.fWidth      = 2.0f;
			flash.fHeight     = 2.0f;
			flash.fLifetime   = fLifetime;
			flash.fFadeOut    = 0.2f;
			flash.vColor      = kColorYellow;
			flash.blendMode   = eBlendPreset::Additive;
			flash.bBillboard  = true;
			CFxSystem::Spawn(world, flash);

			FxBillboardComponent ring{};
			ring.vWorldPos   = { origin.x, origin.y + 0.5f, origin.z };   // ★ v2
			ring.texturePath = kTexZRing;
			ring.fWidth      = 1.5f;
			ring.fHeight     = 1.5f;
			ring.fLifetime   = fLifetime;
			ring.fFadeOut    = 0.3f;
			ring.vColor      = kColorGold;
			ring.blendMode   = eBlendPreset::Additive;
			ring.bBillboard  = false;     // ground 평면
			CFxSystem::Spawn(world, ring);
		}

		// === Dest (도착 플래시) ===
		{
			FxBillboardComponent flash{};
			flash.vWorldPos   = { dest.x, dest.y + 1.0f, dest.z };   // ★ v2
			flash.texturePath = kTexZFlareSun;
			flash.fWidth      = 2.5f;
			flash.fHeight     = 2.5f;
			flash.fLifetime   = fLifetime;
			flash.fFadeOut    = 0.25f;
			flash.vColor      = kColorYellow;
			flash.blendMode   = eBlendPreset::Additive;
			flash.bBillboard  = true;
			CFxSystem::Spawn(world, flash);

			FxBillboardComponent glow{};
			glow.vWorldPos   = { dest.x, dest.y + 1.0f, dest.z };   // ★ v2
			glow.texturePath = kTexZGlow;
			glow.fWidth      = 1.8f;
			glow.fHeight     = 1.8f;
			glow.fLifetime   = fLifetime * 1.2f;
			glow.fFadeOut    = 0.4f;
			glow.vColor      = kColorGold;
			glow.blendMode   = eBlendPreset::Additive;
			glow.bBillboard  = true;
			CFxSystem::Spawn(world, glow);

			FxBillboardComponent trail{};
			trail.vWorldPos   = { dest.x, dest.y + 0.5f, dest.z };   // ★ v2
			trail.texturePath = kTexZBeaconTrail;
			trail.fWidth      = 0.6f;
			trail.fHeight     = 2.5f;
			trail.fLifetime   = fLifetime;
			trail.fFadeOut    = 0.3f;
			trail.vColor      = kColorYellow;
			trail.blendMode   = eBlendPreset::Additive;
			trail.bBillboard  = true;
			CFxSystem::Spawn(world, trail);
		}
	}

	// ───────── R Bow — owner 부착 (vAttachOffset 작동) ─────────
	void SpawnRBow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, f32_t fLifetime)
	{
		if (!pRenderer)
			return;

		FxMeshComponent bow{};
		bow.attachTo      = owner;
		bow.vAttachOffset = { 0.f, 1.5f, 0.5f };
		bow.vScale        = { 0.6f, 0.6f, 0.6f };
		bow.modelPath     = kFbxRBow;
		bow.texturePath   = kTexRBowTex;
		bow.vColor        = kColorYellow;
		bow.blendMode     = eBlendPreset::Additive;
		bow.fLifetime     = fLifetime;
		bow.fFadeIn       = 0.05f;
		bow.fFadeOut      = 0.15f;
		bow.bDepthWrite   = false;
		CFxMeshSystem::Spawn(world, pRenderer, bow);
	}

	// ───────── R Projectile — Trueshot Barrage ─────────
	void SpawnRProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed)
	{
		(void)owner;
		Vec3 vel{ dir.x * fSpeed, 0.f, dir.z * fSpeed };

		if (pRenderer)
		{
			FxMeshComponent mis{};
			mis.vWorldPos            = origin;
			mis.vScale               = { 1.0f, 1.0f, 1.0f };
			mis.vVelocity            = vel;
			mis.vRotation            = { 0.f, YawFromDir(dir), 0.f };   // ★ v2
			mis.modelPath            = kFbxRMis;
			mis.texturePath          = kTexRBowArrow;
			mis.vColor               = kColorYellow;
			mis.blendMode            = eBlendPreset::Additive;
			mis.fLifetime            = fLifetime;
			mis.fFadeOut             = 0.3f;
			mis.bDepthWrite          = false;
			mis.bBlockableByWindWall = true;
			CFxMeshSystem::Spawn(world, pRenderer, mis);

			FxMeshComponent tail{};
			tail.vWorldPos   = origin;
			tail.vScale      = { 1.2f, 1.0f, 1.2f };
			tail.vVelocity   = vel;
			tail.vRotation   = { 0.f, YawFromDir(dir), 0.f };   // ★ v2
			tail.modelPath   = kFbxRMisTail;
			tail.texturePath = kTexRTailTex;
			tail.vColor      = kColorGold;
			tail.blendMode   = eBlendPreset::Additive;
			tail.fLifetime   = fLifetime;
			tail.fFadeOut    = 0.4f;
			tail.bDepthWrite = false;
			CFxMeshSystem::Spawn(world, pRenderer, tail);
		}

		// Glow halo
		FxBillboardComponent glow{};
		glow.vWorldPos   = origin;
		glow.vVelocity   = vel;
		glow.texturePath = kTexRMisGlow;
		glow.fWidth      = 2.5f;
		glow.fHeight     = 2.5f;
		glow.fLifetime   = fLifetime;
		glow.fFadeOut    = 0.3f;
		glow.vColor      = kColorGold;
		glow.blendMode   = eBlendPreset::Additive;
		glow.bBillboard  = true;
		CFxSystem::Spawn(world, glow);

		// Long trail
		FxBillboardComponent trail2{};
		trail2.vWorldPos   = origin;
		trail2.vVelocity   = vel;
		trail2.texturePath = kTexRTrail;
		trail2.fWidth      = 0.8f;
		trail2.fHeight     = 4.0f;
		trail2.fLifetime   = fLifetime;
		trail2.fFadeOut    = 0.4f;
		trail2.vColor      = kColorYellow;
		trail2.blendMode   = eBlendPreset::Additive;
		trail2.bBillboard  = true;
		CFxSystem::Spawn(world, trail2);
	}
}
```

---

### 3.5 `Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp` (diff)

**수정 대상**: `Fx::Spawn*` 호출 5곳 — `ctx.pFxMeshRenderer` 와 speed 인자 추가.

#### 수정 1: `OnCastFrame_BA` (L42-44)
**전**:
```cpp
Fx::SpawnBAProjectile(*ctx.pWorld, ctx.casterEntity,
    GetMuzzlePos(*ctx.pWorld, ctx.casterEntity),
    ctx.pCommand->direction, 0.4f);
```
**후 (변경 없음 — BA 는 mesh 미사용)**:
```cpp
Fx::SpawnBAProjectile(*ctx.pWorld, ctx.casterEntity,
    GetMuzzlePos(*ctx.pWorld, ctx.casterEntity),
    ctx.pCommand->direction, 0.4f);
```

#### 수정 2: `OnCastFrame_Q` (L56)
**전**:
```cpp
Fx::SpawnQProjectile(*ctx.pWorld, ctx.casterEntity, origin, dir, 1.0f);
```
**후**:
```cpp
Fx::SpawnQProjectile(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity,
    origin, dir, 1.0f, /*fSpeed=*/30.f);   // ★ v2 — gameplay speed 30
```

#### 수정 3: `OnCastFrame_W` (L71)
**전**:
```cpp
Fx::SpawnWProjectile(*ctx.pWorld, ctx.casterEntity, origin, dir, 1.5f);
```
**후**:
```cpp
Fx::SpawnWProjectile(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity,
    origin, dir, 1.5f, /*fSpeed=*/18.f);   // ★ v2 — gameplay PendingHit 첫 인자 18
```

#### 수정 4: `OnCastFrame_R` (L90-91)
**전**:
```cpp
Fx::SpawnRBow(*ctx.pWorld, ctx.casterEntity, 0.4f);
Fx::SpawnRProjectile(*ctx.pWorld, ctx.casterEntity, origin, dir, es.fGlobalLifetime);
```
**후**:
```cpp
Fx::SpawnRBow(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, 0.4f);                       // ★ v2
Fx::SpawnRProjectile(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity,
    origin, dir, es.fGlobalLifetime, es.fGlobalSpeed);   // ★ v2 — gameplay 25 일치
```

#### 수정 5: `OnCastAccepted_E` (L126) — 변경 없음
```cpp
Fx::SpawnEFlash(*ctx.pWorld, origin, dest, 0.4f);   // 그대로 (E 는 mesh 미사용)
```

---

## 4. 빌드/검증 절차

### 4.1 빌드
```
1. devenv.exe (Visual Studio) 종료 — vc143.pdb lock 회피
2. Engine.vcxproj 단독 빌드 (PostBuild EngineSDK 동기화)
3. Client.vcxproj 빌드 — /p:MultiProcessorCompilation=false /maxcpucount:1
```

### 4.2 시각 검증
| 입력 | 기대 |
|---|---|
| 좌클릭 BA | 노란 구체 + halo + trail 발사 |
| Q | 노란 발사체 메쉬 (방향 일치 + 회전) + trail + 머리 |
| W | 시안 구체 메쉬 (방향 일치) + orb + ring + spin |
| E | 출발: 플래시+링 / 도착: 플래시+글로우+빔 (모두 y=+0.5~1.0 위로) |
| R | 활 가시 (0.4s) → 노란 화살 (방향 일치) + 꼬리 + 글로우 + 트레일 |

### 4.3 yaw 보정 검증 (★ v2 신규)
첫 빌드 후 **Q/W/R 메쉬가 실제 발사 방향과 다르게 보이면** `YawFromDir` 의 `+ XM_PI` 를 조정:
- 90° 어긋남 → `+ XM_PI/2` 또는 `- XM_PI/2`
- 180° 어긋남 (현재 상태) → `+ XM_PI` 유지 (Kalista 패턴)
- 0° 어긋남 → `+ XM_PI` 제거
이는 fbx 의 forward 축이 +Z 인지 -Z 인지에 따른 것. 본 박제는 Kalista 와 동일 보정 가정.

### 4.4 visual/gameplay sync 검증 (★ v2 신규)
- Q 발사체와 PendingHit 데미지 hitbox 가 **같은 위치에서 같은 시점** 도달하는지
- W 도 동일 — 18 m/s
- R 은 25 m/s × 6s = 150m 사거리, 화살 메쉬가 PendingHit 와 동기 이동

### 4.5 디버그 출력
모든 `Fx::Spawn*` 가 잘 호출되는지 OutputDebugStringA 로 1회 (DIAG) 추가하면 좋음. v1 의 `LogStub` 를 한 줄 DIAG 로 남겨두는 것도 가능.

---

## 5. 잠재 위험 / 롤백 시나리오

### 5.1 위험 1 — Scene_InGame ctx 빌드 위치 누락
`SkillHookContext ctx{}` 가 Scene_InGame.cpp 외에 **다른 파일** 에도 있을 수 있음. 만약 그곳에서 `pFxMeshRenderer = nullptr` 인 채로 dispatch 되면 mesh skip → 빌보드만 보임 (롤백 가능).
**조치**: `grep -rn "SkillHookContext " Client/` 로 전 위치 확인.

### 5.2 위험 2 — yaw 180° 어긋남
`+ XM_PI` 가 이즈리얼 fbx 에 안 맞을 가능성. 첫 빌드 후 Q 발사체가 발사 반대 방향으로 보이면 `YawFromDir` 만 조정 (1줄 변경).

### 5.3 위험 3 — pRenderer 가 nullptr
`if (pRenderer)` guard 가 모든 mesh spawn 앞에 있으므로 crash 안 남. 단 mesh 가 안 보임 → Scene_InGame ctx 주입 누락이 원인.

### 5.4 롤백
v1 의 stub 으로 되돌리려면 5 파일 전부 revert. 단 `SkillHookContext.h` 의 `pFxMeshRenderer` 필드는 유지해도 무해 (다른 챔프가 사용 가능).

---

## 6. 향후 (v1 동일 — 변경 없음)

- **FX2** — fbx → wmesh 사전 변환
- **FX3** — 추가 fbx 활용 (`q_bow_left/right`, `q_cas`, `w_cas`, `r_cas_noodles/pulse`, `death_sphere`, `pearl`)
- **FX4** — Hook 2 분리 마이그레이션
- **FX5** — EffectTuner ImGui 연동

---

## 7. v1 → v2 변경 한 줄

**v1 의 "1파일 수정" 가정이 `CFxMeshSystem::Spawn` API (pRenderer 필요) + `SkillHookContext` 필드 부재 + 월드 FX vAttachOffset 무시 + 메쉬 yaw 미반영 + visual/gameplay speed 불일치 5건과 충돌. v2 는 5 파일 수정으로 확장: ① SkillHookContext + pFxMeshRenderer ② Scene_InGame ctx 주입 ③ Ezreal_FxPresets.h 시그니처 ④ Ezreal_FxPresets.cpp 본격 박제 (vRotation Kalista 패턴 + 월드 FX y 직박 + speed 인자) ⑤ Ezreal_Skills.cpp 호출 5곳. Visual hook 빈 채 유지 (이중 호출 방지) 는 v1 동일.**
