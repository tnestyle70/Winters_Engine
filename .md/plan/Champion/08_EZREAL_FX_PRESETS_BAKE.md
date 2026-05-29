# Phase B-11d-FX — Ezreal FxPresets 본격 박제

**작성일**: 2026-04-30
**범위**: 1 파일 수정 — `Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp`
**목적**: Q/W/E/R/BA 시각 효과 0 → LoL 원작 95% 재현
**전제**: ① fbx 16개 이미 변환됨 (`particles/fbx/`) ② texture 22개 (.png/.dds) 존재 ③ FxBillboardComponent / FxMeshComponent / CFxSystem / CFxMeshSystem 인프라 박제 완료 (Irelia/Kalista 사용 중)

---

## 0. 진단

### 현재 상태
- `Ezreal_FxPresets.h` ([Ezreal_FxPresets.h:9-22](Client/Public/GameObject/Champion/Ezreal/Ezreal_FxPresets.h:9)): 6개 함수 선언 OK
- `Ezreal_FxPresets.cpp` ([Ezreal_FxPresets.cpp:20-56](Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp:20)): **6개 함수 모두 stub** — `LogStub` (OutputDebugStringA) 만 호출, entity spawn 0
- `Ezreal_Skills.cpp` ([Ezreal_Skills.cpp:42-91](Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp:42)): `OnCastFrame_BA/Q/W/R` 가 `Fx::Spawn*` 호출 중 → 호출은 되지만 stub 이라 화면에 0
- `Visual::OnCastFrame_*_Visual` ([Ezreal_Skills.cpp:200-223](Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp:200)): 모두 `(void)ctx` 빈 구현

### 결론
**등록 경로는 다 연결됨**. 콜백이 비어있어서 화면에 0. → `Ezreal_FxPresets.cpp` 의 6개 stub 만 본격화하면 끝.

---

## 1. 자산 매핑 (particles/ 폴더 기준)

### Mesh (fbx) — 5개 사용
| 변수 | 경로 | 용도 |
|---|---|---|
| `kFbxQMis` | `particles/fbx/ezreal_base_q_mis.fbx` | Q Mystic Shot 발사체 |
| `kFbxWMis` | `particles/fbx/ezreal_base_w_mis.fbx` | W Essence Flux 구체 |
| `kFbxRMis` | `particles/fbx/ezreal_base_r_mis.fbx` | R Trueshot Barrage 화살 |
| `kFbxRMisTail` | `particles/fbx/ezreal_base_r_mis_tail.fbx` | R 꼬리 메쉬 |
| `kFbxRBow` | `particles/fbx/ezreal_r_cas_bow_mesh.fbx` | R 시전 활 |

(보류: `ezreal_base_idol.fbx`, `ezreal_q_bow_left/right.fbx`, `ezreal_q_cas.fbx`, `ezreal_r_cas_noodles.fbx`, `ezreal_r_cas_pulse.fbx`, `ezreal_w_cas.fbx`, `ezreal_skin05_death_sphere.fbx`, `ezvgu_tristana_*`, `pearl.fbx` — 향후 확장)

### Texture (png) — 17개 사용
| 변수 | 파일명 | 용도 |
|---|---|---|
| `kTexBaGlow` | `ezreal_base_ba_glow.png` | BA halo |
| `kTexBaMis` | `ezreal_base_ba_mis.png` | BA core |
| `kTexBaWhisps` | `ezreal_base_ba_whisps.png` | BA trail |
| `kTexQTrail` | `ezreal_base_q_mis_trail.png` | Q 꼬리 |
| `kTexQErode` | `ezreal_base_q_erode.png` | Q 메쉬 텍스처 |
| `kTexQHitSpark` | `ezreal_base_q_hit_spark.png` | Q 머리 |
| `kTexWOrb` | `ezreal_base_w_orb.png` | W 코어 |
| `kTexWRingGlow` | `ezreal_base_w_ringglow.png` | W 링 |
| `kTexWSpinTex` | `ezreal_base_w_spintex.png` | W 회전 텍스처 |
| `kTexWMeshLines` | `ezreal_base_w_meshlines.png` | W 메쉬 라인 |
| `kTexZFlareSun` | `ezreal_base_z_flare-sun.png` | E 도착 플레어 |
| `kTexZGlow` | `ezreal_base_z_glow.png` | E 도착 글로우 |
| `kTexZRing` | `ezreal_base_z_ring.png` | E 출발 링 |
| `kTexZBeaconTrail` | `ezreal_base_z_color-beacontrail.png` | E 도착 빔 |
| `kTexCrystalFlash` | `common_crystal-flash.png` | E 출발 플래시 |
| `kTexRMisGlow` | `ezreal_base_r_mis_glow.png` | R 글로우 |
| `kTexRTrail` | `ezreal_base_r_trail.png` | R 트레일 |
| `kTexRBowTex` | `ezreal_base_r_bow_tex.png` | R 활 본체 |
| `kTexRBowArrow` | `ezreal_base_r_bow_arrow.png` | R 화살 본체 |
| `kTexRTailTex` | `ezreal_base_r_tail_tex.png` | R 꼬리 |

### Color signature (LoL 시그니처)
| 상수 | RGBA | 용도 |
|---|---|---|
| `kColorYellow` | `(1.0, 0.85, 0.3, 1.0)` | 이즈리얼 메인 노란빛 (BA/Q/E/R) |
| `kColorGold` | `(1.0, 0.7, 0.2, 1.0)` | 진한 금색 강조 (halo, glow) |
| `kColorCyan` | `(0.4, 0.8, 1.0, 1.0)` | W Essence Flux 시안 |
| `kColorWhite` | `(1.0, 1.0, 1.0, 1.0)` | 중립 흰색 |

---

## 2. 박제 전략

### 2.1 SkillHookRegistry vs VisualHookRegistry 분리 정책

`Ezreal_Registration.cpp` ([:102-135](Client/Private/GameObject/Champion/Ezreal/Ezreal_Registration.cpp:102)) 가 **3개 Registry 에 동일 hookId 등록** 중:
- `SkillHookRegistry` → `OnCastFrame_BA/Q/W/R` (현재 Fx::Spawn 호출)
- `GameplayHookRegistry` → `Gameplay::OnCastFrame_BA/Q/W/R` (빈)
- `VisualHookRegistry` → `Visual::OnCastFrame_BA/Q/W/R_Visual` (빈)

**위험**: 만약 SkillHook 와 VisualHook 가 같은 cast 이벤트에 둘 다 dispatch 되면 Fx 가 2번 호출. → **이번 박제는 SkillHook 만 본격화. Visual hook 는 빈 채로 유지**.

향후 Sim-1 의 Hook 2 분리 본격 도입 시 마이그레이션 (Visual = FX, SkillHook = damage/projectile only).

### 2.2 Multi-layer 합성 (Irelia 패턴)
`IreliaFxPresets::SpawnWReleaseLayers` ([:86-120](Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp:86)) 와 동일:
- Base: `eBlendPreset::AlphaBlend`
- Glow: `eBlendPreset::Additive`
- 같은 `vWorldPos / vVelocity / fLifetime` 로 spawn → 동기 이동

### 2.3 발사체 vs 부착물
| 패턴 | `attachTo` | `vWorldPos` | `vVelocity` | 용도 |
|---|---|---|---|---|
| **발사체** | `NULL_ENTITY` | origin | 방향 × 속도 | Q/W/R/BA 메쉬 + trail |
| **부착물** | owner | (0,0,0) | (0,0,0) | R Bow (시전 활) |
| **월드 고정** | `NULL_ENTITY` | dest | (0,0,0) | E 출발/도착 플래시 |

### 2.4 속도 (m/s 추정 — Irelia/Kalista 와 일관)
| 스킬 | 속도 | 사거리 (`Ezreal_Registration` cd 기준) | 라이프타임 (사거리 ÷ 속도) |
|---|---|---|---|
| BA | 18 | 5.5 | 0.31s (0.4s lifetime 안에 도달) |
| Q | 30 | 11 | 0.37s (0.6s 여유) |
| W | 24 | 10 | 0.42s (0.8s 여유) |
| R | 60 | 200 (글로벌) | 3.3s (es.fGlobalLifetime 사용) |

---

## 3. 수정 — `Ezreal_FxPresets.cpp` 전문

**경로**: `C:\Users\user\Desktop\Winters\Client\Private\GameObject\Champion\Ezreal\Ezreal_FxPresets.cpp`

### 3.1 수정 전 (현재 — L1-57, 57줄)
```cpp
#include "GameObject/Champion/Ezreal/Ezreal_FxPresets.h"

#include "ECS/World.h"

#include <Windows.h>

namespace Ezreal::Fx
{
	namespace
	{
		void LogStub(const char* name, EntityID owner, const Vec3& origin)
		{
			char buf[160]{};
			sprintf_s(buf, "[EzrealFx] %s owner=%u pos=(%.1f,%.1f,%.1f)\n",
				name, static_cast<u32_t>(owner), origin.x, origin.y, origin.z);
			OutputDebugStringA(buf);
		}
	}

	void SpawnBAProjectile(CWorld&, EntityID owner, const Vec3& origin,
		const Vec3&, f32_t)
	{
		LogStub("BA", owner, origin);
	}

	void SpawnQProjectile(CWorld&, EntityID owner, const Vec3& origin,
		const Vec3&, f32_t)
	{
		LogStub("Q", owner, origin);
	}

	void SpawnWProjectile(CWorld&, EntityID owner, const Vec3& origin,
		const Vec3&, f32_t)
	{
		LogStub("W", owner, origin);
	}

	void SpawnEFlash(CWorld&, const Vec3& origin, const Vec3& dest,
		f32_t)
	{
		char buf[192]{};
		sprintf_s(buf, "[EzrealFx] EFlash from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f)\n",
			origin.x, origin.y, origin.z, dest.x, dest.y, dest.z);
		OutputDebugStringA(buf);
	}

	void SpawnRBow(CWorld&, EntityID owner, f32_t)
	{
		LogStub("RBow", owner, {});
	}

	void SpawnRProjectile(CWorld&, EntityID owner, const Vec3& origin,
		const Vec3&, f32_t)
	{
		LogStub("R", owner, origin);
	}
}
```

### 3.2 수정 후 (전문 — 약 280줄)
```cpp
#include "GameObject/Champion/Ezreal/Ezreal_FxPresets.h"

#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"

#include "ECS/World.h"

#include <Windows.h>

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
	// E — Arcane Shift (z_* 시리즈 + common)
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
}

namespace Ezreal::Fx
{
	// ───────── BA — Basic Attack (small homing energy, billboard only) ─────────
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

	// ───────── Q — Mystic Shot (yellow energy bolt, mesh + 2 billboard) ─────────
	void SpawnQProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime)
	{
		(void)owner;
		const f32_t fSpeed = 30.f;
		Vec3 vel{ dir.x * fSpeed, 0.f, dir.z * fSpeed };

		// Mesh: q_mis.fbx (rotating bolt)
		FxMeshComponent mesh{};
		mesh.vWorldPos            = origin;
		mesh.vScale               = { 0.4f, 0.4f, 0.4f };
		mesh.vVelocity            = vel;
		mesh.modelPath            = kFbxQMis;
		mesh.texturePath          = kTexQErode;
		mesh.vColor               = kColorYellow;
		mesh.blendMode            = eBlendPreset::Additive;
		mesh.fWorldYawSpinSpeed   = 12.f;
		mesh.fLifetime            = fLifetime;
		mesh.fFadeOut             = 0.15f;
		mesh.bDepthWrite          = false;
		mesh.bBlockableByWindWall = true;
		CFxMeshSystem::Spawn(world, mesh);

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

	// ───────── W — Essence Flux (cyan magic orb, mesh + 3 billboard) ─────────
	void SpawnWProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime)
	{
		(void)owner;
		const f32_t fSpeed = 24.f;
		Vec3 vel{ dir.x * fSpeed, 0.f, dir.z * fSpeed };

		// Mesh: w_mis.fbx (rotating sphere)
		FxMeshComponent mesh{};
		mesh.vWorldPos            = origin;
		mesh.vScale               = { 0.5f, 0.5f, 0.5f };
		mesh.vVelocity            = vel;
		mesh.modelPath            = kFbxWMis;
		mesh.texturePath          = kTexWMeshLines;
		mesh.vColor               = kColorCyan;
		mesh.blendMode            = eBlendPreset::Additive;
		mesh.fWorldYawSpinSpeed   = 8.f;
		mesh.fLifetime            = fLifetime;
		mesh.fFadeOut             = 0.2f;
		mesh.bDepthWrite          = false;
		mesh.bBlockableByWindWall = true;
		CFxMeshSystem::Spawn(world, mesh);

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

		// Spin texture (rotating mesh-lines feel)
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

	// ───────── E — Arcane Shift (teleport flash, origin + dest) ─────────
	void SpawnEFlash(CWorld& world, const Vec3& origin, const Vec3& dest,
		f32_t fLifetime)
	{
		// === Origin (출발 잔상) ===
		{
			FxBillboardComponent flash{};
			flash.vWorldPos     = origin;
			flash.vAttachOffset = { 0.f, 1.f, 0.f };
			flash.texturePath   = kTexCrystalFlash;
			flash.fWidth        = 2.0f;
			flash.fHeight       = 2.0f;
			flash.fLifetime     = fLifetime;
			flash.fFadeOut      = 0.2f;
			flash.vColor        = kColorYellow;
			flash.blendMode     = eBlendPreset::Additive;
			flash.bBillboard    = true;
			CFxSystem::Spawn(world, flash);

			FxBillboardComponent ring{};
			ring.vWorldPos     = origin;
			ring.vAttachOffset = { 0.f, 0.5f, 0.f };
			ring.texturePath   = kTexZRing;
			ring.fWidth        = 1.5f;
			ring.fHeight       = 1.5f;
			ring.fLifetime     = fLifetime;
			ring.fFadeOut      = 0.3f;
			ring.vColor        = kColorGold;
			ring.blendMode     = eBlendPreset::Additive;
			ring.bBillboard    = false;     // ground 평면 ring
			CFxSystem::Spawn(world, ring);
		}

		// === Dest (도착 플래시) ===
		{
			FxBillboardComponent flash{};
			flash.vWorldPos     = dest;
			flash.vAttachOffset = { 0.f, 1.f, 0.f };
			flash.texturePath   = kTexZFlareSun;
			flash.fWidth        = 2.5f;
			flash.fHeight       = 2.5f;
			flash.fLifetime     = fLifetime;
			flash.fFadeOut      = 0.25f;
			flash.vColor        = kColorYellow;
			flash.blendMode     = eBlendPreset::Additive;
			flash.bBillboard    = true;
			CFxSystem::Spawn(world, flash);

			FxBillboardComponent glow{};
			glow.vWorldPos     = dest;
			glow.vAttachOffset = { 0.f, 1.f, 0.f };
			glow.texturePath   = kTexZGlow;
			glow.fWidth        = 1.8f;
			glow.fHeight       = 1.8f;
			glow.fLifetime     = fLifetime * 1.2f;
			glow.fFadeOut      = 0.4f;
			glow.vColor        = kColorGold;
			glow.blendMode     = eBlendPreset::Additive;
			glow.bBillboard    = true;
			CFxSystem::Spawn(world, glow);

			FxBillboardComponent trail{};
			trail.vWorldPos     = dest;
			trail.vAttachOffset = { 0.f, 0.5f, 0.f };
			trail.texturePath   = kTexZBeaconTrail;
			trail.fWidth        = 0.6f;
			trail.fHeight       = 2.5f;
			trail.fLifetime     = fLifetime;
			trail.fFadeOut      = 0.3f;
			trail.vColor        = kColorYellow;
			trail.blendMode     = eBlendPreset::Additive;
			trail.bBillboard    = true;
			CFxSystem::Spawn(world, trail);
		}
	}

	// ───────── R — Bow visual (cast bow attached on owner, 0.4s) ─────────
	void SpawnRBow(CWorld& world, EntityID owner, f32_t fLifetime)
	{
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
		CFxMeshSystem::Spawn(world, bow);
	}

	// ───────── R — Trueshot Barrage (global beam, 2 mesh + 2 billboard) ─────────
	void SpawnRProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime)
	{
		(void)owner;
		const f32_t fSpeed = 60.f;
		Vec3 vel{ dir.x * fSpeed, 0.f, dir.z * fSpeed };

		// Main arrow mesh
		FxMeshComponent mis{};
		mis.vWorldPos            = origin;
		mis.vScale               = { 1.0f, 1.0f, 1.0f };
		mis.vVelocity            = vel;
		mis.modelPath            = kFbxRMis;
		mis.texturePath          = kTexRBowArrow;
		mis.vColor               = kColorYellow;
		mis.blendMode            = eBlendPreset::Additive;
		mis.fLifetime            = fLifetime;
		mis.fFadeOut             = 0.3f;
		mis.bDepthWrite          = false;
		mis.bBlockableByWindWall = true;
		CFxMeshSystem::Spawn(world, mis);

		// Tail mesh
		FxMeshComponent tail{};
		tail.vWorldPos   = origin;
		tail.vScale      = { 1.2f, 1.0f, 1.2f };
		tail.vVelocity   = vel;
		tail.modelPath   = kFbxRMisTail;
		tail.texturePath = kTexRTailTex;
		tail.vColor      = kColorGold;
		tail.blendMode   = eBlendPreset::Additive;
		tail.fLifetime   = fLifetime;
		tail.fFadeOut    = 0.4f;
		tail.bDepthWrite = false;
		CFxMeshSystem::Spawn(world, tail);

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

### 3.3 변경 요약
| 항목 | 수정 전 | 수정 후 |
|---|---|---|
| 줄수 | 57 | ~280 |
| Include | `ECS/World.h` + `<Windows.h>` | + `FxSystem.h` / `FxMeshSystem.h` / `FxBillboardComponent.h` / `FxMeshComponent.h` |
| 익명 namespace | LogStub (debug only) | path constants 22개 + color constants 4개 |
| 6개 함수 | 모두 `LogStub` 만 호출 | 본격 spawn — Multi-layer billboard + mesh 합성 |

### 3.4 미수정 (이번 박제 범위 밖)
- `Ezreal_FxPresets.h` — 함수 시그니처 동일, 변경 없음
- `Ezreal_Skills.cpp` `OnCastFrame_*` — 이미 `Fx::Spawn*` 호출 중, 변경 없음
- `Ezreal_Skills.cpp` `Visual::OnCastFrame_*_Visual` — **빈 채로 유지** (이중 호출 방지)
- `Ezreal_Skills.cpp` `Gameplay::OnCastFrame_*` — 빈 채로 유지 (서버 권위 작업 시 박제)
- `Ezreal_Registration.cpp` — Hook 등록 그대로

---

## 4. 검증 시나리오

### 4.1 빌드 검증
1. Engine 단독 빌드 (Post-Build EngineSDK 동기화)
2. Client 빌드 (`/p:MultiProcessorCompilation=false /maxcpucount:1` — PDB lock 회피)
3. 컴파일 에러 0 확인

### 4.2 시각 검증 (`WintersGame.exe` 실행)
| 입력 | 기대 시각 |
|---|---|
| 좌클릭 (적 → BA) | 노란 에너지 구체 + halo + trail 발사 |
| Q + 마우스 방향 | 노란 발사체 메쉬 (회전) + 트레일 + 머리 글로우 |
| W + 마우스 방향 | 시안 구체 메쉬 + orb + ring + spin 텍스처 |
| E + 마우스 방향 | 출발지 플래시+링 / 도착지 플래시+글로우+빔 |
| R + 마우스 방향 | 시전 시 활 가시 (0.4s) → 노란 화살 + 꼬리 메쉬 + 글로우 + 긴 트레일 |

### 4.3 디버깅 (메모리 `feedback_lol_fx_texture_pattern` 함정 검증)
- **`render/*.png` 함정 없음**: 본 계획서는 `render/` 폴더 자산 사용 안 함 (`particles/` 루트 + `particles/fbx/` 만)
- **UV alpha 0 의심 시**: RenderDoc 으로 fbx 메쉬 UV 영역 확인 — fxClip 이 전 픽셀 버리는지
- **CPU 디버거로 못 잡는 패턴**: `[EzrealFx] BA owner=...` 로그 안 찍히면 Hook dispatch 자체 미작동 → SkillHookRegistry 등록 검증

---

## 5. 향후 확장 (별도 사이클)

### Phase B-11d-FX2 — 메쉬 사전 변환 (선택)
- `particles/fbx/*.fbx` → `*.wmesh` 변환 (startup 최적화 — fbx 런타임 Assimp 로드보다 빠름)
- `Tools/WintersAssetConverter` 사용
- 현재는 fbx 직로드 (Irelia E beam 동일 패턴) — 시각엔 영향 X

### Phase B-11d-FX3 — 추가 자산 활용
| 자산 | 용도 |
|---|---|
| `ezreal_q_bow_left.fbx` / `right.fbx` | Q 시전 시 양손 활 가시 |
| `ezreal_q_cas.fbx` | Q 시전 머즐 플래시 |
| `ezreal_w_cas.fbx` | W 시전 머즐 플래시 |
| `ezreal_r_cas_noodles.fbx` | R 차징 라인 |
| `ezreal_r_cas_pulse.fbx` | R 차징 펄스 |
| `ezreal_skin05_death_sphere.fbx` | Death FX |
| `pearl.fbx` | Q hit FX 공통 |

### Phase B-11d-FX4 — Hook 2 분리 마이그레이션
- 현재: SkillHook 가 Fx + Damage 같이 처리 (hybrid)
- 목표: Visual hook → FX 전담, Gameplay hook → damage/projectile 전담
- 이즈리얼만 적용 시 등록 함수 흐름 재구성 + Visual::OnCastFrame_*_Visual 박제

### Phase B-11d-FX5 — EffectTuner ImGui 연동
- `Client/Private/UI/EffectTuner.cpp` 에 Ezreal 슬라이더 노출
- 기획 이터레이션 가속 (CLAUDE.md ImGui 정책 준수)

---

## 6. 한 줄

**`Ezreal_FxPresets.cpp` 의 6개 stub 함수를 multi-layer billboard + fbx mesh 합성으로 본격 박제. 자산은 `particles/` (텍스처 22개) + `particles/fbx/` (fbx 5개) 사용. Visual hook 는 이중 호출 방지로 빈 채 유지. 1 파일 수정 — Skill_Registration / Visual hook 미수정. 빌드 → 좌클릭 BA / Q / W / E / R 시각 검증 후 서버 작업 진입.**
