Session - LeeSin WFX Vertical Slice

1. 반영해야 하는 코드

1-1. `Engine/Private/FX/FxAsset.cpp`

의도: `.wfx` 데이터가 디자이너 친화적인 alias를 받아도 안정적으로 로드되게 하고, 현재 texture 파싱 버그를 먼저 제거한다.

기존 코드:

```cpp
		if (ExtractString(block, "texture", value) ||
			ExtractString(block, "material", value))
		{
			emitter.strTexturePath = WidenAscii(value);
		}
			emitter.strTexturePath = WidenAscii(value);

		if (ExtractString(block, "render_type", value) ||
			ExtractString(block, "renderType", value))
			emitter.renderType = ParseRenderType(value);
		if (ExtractString(block, "blend_mode", value) ||
			ExtractString(block, "blend_mode", value))
			emitter.blendMode = ParseBlendPreset(value);
```

아래로 교체:

```cpp
		if (ExtractString(block, "texture", value) ||
			ExtractString(block, "material", value))
		{
			emitter.strTexturePath = WidenAscii(value);
		}

		if (ExtractString(block, "render_type", value) ||
			ExtractString(block, "renderType", value))
			emitter.renderType = ParseRenderType(value);
		if (ExtractString(block, "blend_mode", value) ||
			ExtractString(block, "blend", value))
			emitter.blendMode = ParseBlendPreset(value);
```

기존 코드:

```cpp
		ExtractInt(block, "atlas_cols", emitter.iAtlasCols);
		ExtractInt(block, "atlas_row", emitter.iAtlasRows);
		ExtractFloat(block, "atlas_fps", emitter.fAtlasFPS);
		ExtractBool(block, "atlas_loop", emitter.bAtlasLoop);
```

아래로 교체:

```cpp
		ExtractInt(block, "atlas_cols", emitter.iAtlasCols);
		if (!ExtractInt(block, "atlas_rows", emitter.iAtlasRows))
			ExtractInt(block, "atlas_row", emitter.iAtlasRows);
		ExtractInt(block, "atlas_frame_count", emitter.iAtlasFrameCount);
		ExtractFloat(block, "atlas_fps", emitter.fAtlasFPS);
		ExtractBool(block, "atlas_loop", emitter.bAtlasLoop);
```

1-2. `Client/Private/GameObject/FX/FxCuePlayer.cpp`

의도: 이름 기반 cue 로드/재생을 실제 구현한다. 첫 세로 파이프라인은 billboard/ground/shockwave 계열만 직접 지원하고, beam 계열은 다음 단계에서 별도 확장한다.

기존 코드:

```cpp
```

아래로 교체:

```cpp
#include "GameObject/FX/FxCuePlayer.h"

#include "ECS/World.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"

#include <Windows.h>
#include <cstdio>

namespace
{
	bool IsCueBillboardType(FxRenderType type)
	{
		return type == FxRenderType::Billboard ||
			type == FxRenderType::GroundDecal ||
			type == FxRenderType::ShockwaveRing;
	}

	void LogMissingCue(const char* pszCueName)
	{
		char szBuffer[192]{};
		sprintf_s(szBuffer, "[FxCuePlayer] Missing cue: %s\n", pszCueName ? pszCueName : "(null)");
		OutputDebugStringA(szBuffer);
	}

	FxBillboardComponent BuildCueBillboard(const FxEmitterDesc& emitter, const FxCueContext& ctx)
	{
		FxBillboardComponent fx{};
		fx.renderType = emitter.renderType;
		fx.blendMode = emitter.blendMode;
		fx.depthMode = emitter.depthMode;
		fx.SetTexturePath(emitter.strTexturePath);
		fx.SetMaterialFromDesc(emitter.material);
		fx.vWorldPos = ctx.vWorldPos;
		fx.vAttachOffset = emitter.vAttachOffset;
		fx.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
		fx.vScale = emitter.vScale;
		fx.vRotation = emitter.vRotation;
		fx.vColor = emitter.vColor;
		fx.fWidth = emitter.fWidth;
		fx.fHeight = emitter.fHeight;
		fx.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
		fx.fAge = 0.f;
		fx.fStartRadius = emitter.fStartRadius;
		fx.fEndRadius = emitter.fEndRadius;
		fx.fThickness = emitter.fThickness;
		fx.fGrowTime = emitter.fGrowTime;
		fx.fFadeInTime = emitter.fFadeInTime;
		fx.fFadeOutTime = emitter.fFadeOutTime;
		fx.iAtlasCols = emitter.iAtlasCols;
		fx.iAtlasRows = emitter.iAtlasRows;
		fx.iAtlasFrameCount = emitter.iAtlasFrameCount;
		fx.fAtlasFPS = emitter.fAtlasFPS;
		fx.bAtlasLoop = emitter.bAtlasLoop;
		fx.bBillboard = emitter.bBillboard;
		fx.bDepthWrite = emitter.bDepthWrite;
		fx.bBlockableByWindWall = emitter.bBlockableByWindWall;
		fx.attachTo = ctx.attachTo;

		const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);
		fx.fYaw = emitter.fYaw;
		if (vForward.x != 0.f || vForward.z != 0.f)
			fx.fYaw += WintersMath::YawFromDirectionXZ(vForward);

		return fx;
	}
}

u32_t CFxCuePlayer::PreloadDirectory(const wchar_t* wszDirectoryPath)
{
	if (!wszDirectoryPath || !wszDirectoryPath[0])
		return 0u;

	return CFxSystem::GetAssetRegistry().LoadDirectory(wszDirectoryPath);
}

FxAssetHandle CFxCuePlayer::FindCue(const char* pszCueName)
{
	if (!pszCueName || !pszCueName[0])
		return {};

	return CFxSystem::GetAssetRegistry().FindByName(pszCueName);
}

EntityID CFxCuePlayer::Play(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
{
	const FxAssetHandle handle = FindCue(pszCueName);
	const FxAsset* pAsset = CFxSystem::GetAssetRegistry().Find(handle);
	if (!pAsset)
	{
		LogMissingCue(pszCueName);
		return NULL_ENTITY;
	}

	EntityID firstEntity = NULL_ENTITY;
	for (const FxEmitterDesc& emitter : pAsset->emitters)
	{
		if (!IsCueBillboardType(emitter.renderType))
			continue;

		EntityID entity = world.CreateEntity();
		world.AddComponent<FxBillboardComponent>(entity, BuildCueBillboard(emitter, ctx));
		if (firstEntity == NULL_ENTITY)
			firstEntity = entity;
	}

	return firstEntity;
}
```

1-3. `Client/Public/GameObject/Champion/LeeSin/LeeSin_Skills.h`

의도: 리신 visual hook 선언을 public header로 정리한다. `Client/Private/GameObject/Champion/LeeSin/LeeSin_Skills.h`는 private 중복 파일이므로 참조가 없으면 삭제한다.

기존 코드:

```cpp
#pragma once
```

아래로 교체:

```cpp
#pragma once

struct VisualHookContext;

namespace LeeSin
{
	namespace Visual
	{
		void OnQCastFrame(VisualHookContext& ctx);
	}
}

void LeeSin_KeepAlive();
```

1-4. 새 파일: `Client/Private/GameObject/Champion/LeeSin/LeeSin_Skills.cpp`

```cpp
#include "GameObject/Champion/LeeSin/LeeSin_Skills.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GamePlay/VisualHookRegistry.h"
#include "WintersMath.h"

namespace
{
	bool TryGetTransform(CWorld& world, EntityID entity, TransformComponent*& outTransform)
	{
		outTransform = nullptr;
		if (entity == NULL_ENTITY)
			return false;

		outTransform = world.GetComponent<TransformComponent>(entity);
		return outTransform != nullptr;
	}

	Vec3 ResolveCasterPosition(VisualHookContext& ctx)
	{
		TransformComponent* pCasterTransform = nullptr;
		if (TryGetTransform(ctx.world, ctx.casterEntity, pCasterTransform))
			return pCasterTransform->position;

		return ctx.command.groundPos;
	}

	Vec3 ResolveForward(VisualHookContext& ctx)
	{
		const Vec3 vCommandDir = WintersMath::NormalizeXZOrZero(ctx.command.direction);
		if (vCommandDir.x != 0.f || vCommandDir.z != 0.f)
			return vCommandDir;

		TransformComponent* pCasterTransform = nullptr;
		TransformComponent* pTargetTransform = nullptr;
		if (TryGetTransform(ctx.world, ctx.casterEntity, pCasterTransform) &&
			TryGetTransform(ctx.world, ctx.command.targetEntityId, pTargetTransform))
		{
			const Vec3 vToTarget{
				pTargetTransform->position.x - pCasterTransform->position.x,
				0.f,
				pTargetTransform->position.z - pCasterTransform->position.z
			};
			const Vec3 vTargetDir = WintersMath::NormalizeXZOrZero(vToTarget);
			if (vTargetDir.x != 0.f || vTargetDir.z != 0.f)
				return vTargetDir;
		}

		if (pCasterTransform)
			return WintersMath::DirectionFromYawXZ(pCasterTransform->rotation.y);

		return { 0.f, 0.f, 1.f };
	}

	void PlayLeeSinCue(VisualHookContext& ctx, const char* pszCueName, bool_t bAttachToCaster)
	{
		FxCueContext fx{};
		fx.vWorldPos = ResolveCasterPosition(ctx);
		fx.vForward = ResolveForward(ctx);
		fx.attachTo = bAttachToCaster ? ctx.casterEntity : NULL_ENTITY;
		CFxCuePlayer::Play(ctx.world, pszCueName, fx);
	}
}

namespace LeeSin
{
	namespace Visual
	{
		void OnQCastFrame(VisualHookContext& ctx)
		{
			if (ctx.command.itemId == 2u)
				PlayLeeSinCue(ctx, "LeeSin.Q2.Dash", true);
			else
				PlayLeeSinCue(ctx, "LeeSin.Q.Cast", false);
		}
	}
}
```

1-5. `Client/Private/GameObject/Champion/LeeSin/LeeSin_Registration.cpp`

기존 코드:

```cpp
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
```

아래로 교체:

```cpp
#include "GameObject/Champion/LeeSin/LeeSin_Skills.h"

#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/SkillDef.h"
```

기존 코드:

```cpp
            RegisterSkill(4, eTargetMode::UnitTarget, "skinned_mesh_spell4a", kLeeSin_R_Cast);

            OutputDebugStringA("[LeeSin] Registration complete\n");
```

아래로 교체:

```cpp
            RegisterSkill(4, eTargetMode::UnitTarget, "skinned_mesh_spell4a", kLeeSin_R_Cast);

            CFxCuePlayer::PreloadDirectory(L"Data/LoL/FX/Champions/LeeSin");
            CVisualHookRegistry::Instance().Register(kLeeSin_Q_Cast, &LeeSin::Visual::OnQCastFrame);

            OutputDebugStringA("[LeeSin] Registration complete\n");
```

1-6. `Client/Private/Network/Client/EventApplier.cpp`

의도: 서버 projectile event에서 리신 Q projectile/hit/mark cue를 한 번만 재생한다. generic projectile/hit billboard와 중복되지 않아야 한다.

기존 코드:

```cpp
#include "GameObject/FX/FxSystem.h"
```

아래로 교체:

```cpp
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxSystem.h"
```

기존 코드:

```cpp
		if (kind == ProjectileKindIds::Wind ||
			kind == ProjectileKindIds::Tornado ||
			kind == ProjectileKindIds::EQRing ||
			kind == ProjectileKindIds::MysticShot)
		{
			return true;
		}
```

아래로 교체:

```cpp
		if (kind == ProjectileKindIds::Wind ||
			kind == ProjectileKindIds::Tornado ||
			kind == ProjectileKindIds::EQRing ||
			kind == ProjectileKindIds::MysticShot ||
			kind == ProjectileKindIds::LeeSinQ)
		{
			return true;
		}
```

기존 코드:

```cpp
	const wchar_t* ResolveProjectileHitTexture(u16_t kind)
	{
		if (kind == ProjectileKindIds::MysticShot)
			return L"Client/Bin/Resource/Texture/Character/Ezreal/Ezreal_Q_hit.png";
		return L"Client/Bin/Resource/Texture/Effect/SilkSpark.png";
	}
```

아래에 추가:

```cpp

	const char* ResolveProjectileSpawnCue(u16_t kind)
	{
		if (kind == ProjectileKindIds::LeeSinQ)
			return "LeeSin.Q.Projectile";
		return nullptr;
	}

	const char* ResolveProjectileHitCue(u16_t kind)
	{
		if (kind == ProjectileKindIds::LeeSinQ)
			return "LeeSin.Q.Hit";
		return nullptr;
	}

	const char* ResolveProjectileAttachedCue(u16_t kind)
	{
		if (kind == ProjectileKindIds::LeeSinQ)
			return "LeeSin.Q.Mark";
		return nullptr;
	}
```

기존 코드:

```cpp
		const Vec3 velocity{ dir.x * speed, dir.y * speed, dir.z * speed };
		const f32_t lifetime = speed > 0.01f ? (range / speed) : 0.65f;
		const bool_t bChampionProjectileVisual = UsesChampionProjectileVisual(ev->kind());

		EntityID entity = NULL_ENTITY;
```

아래에 추가:

```cpp
		if (const char* pszCueName = ResolveProjectileSpawnCue(ev->kind()))
		{
			FxCueContext fx{};
			fx.vWorldPos = pos;
			fx.vForward = dir;
			fx.vVelocity = velocity;
			fx.bOverrideVelocity = true;
			fx.fLifetimeOverride = lifetime;
			fx.bOverrideLifetime = true;
			CFxCuePlayer::Play(world, pszCueName, fx);
		}
```

기존 코드:

```cpp
		const wchar_t* hitTexture = ResolveProjectileHitTexture(ev->kind());
		CFxSystem::SpawnBillboard(world, hitTexture, pos, { 0.f, 0.25f, 0.f }, 0.45f, 0.45f, 0.28f, { 1.f, 0.9f, 0.45f, 1.f });
```

아래로 교체:

```cpp
		bool_t bPlayedWfxCue = false;
		if (const char* pszHitCueName = ResolveProjectileHitCue(ev->kind()))
		{
			FxCueContext fx{};
			fx.vWorldPos = pos;
			fx.vForward = { 0.f, 0.f, 1.f };
			CFxCuePlayer::Play(world, pszHitCueName, fx);
			bPlayedWfxCue = true;
		}

		if (const char* pszAttachedCueName = ResolveProjectileAttachedCue(ev->kind()))
		{
			EntityID attachTo = NULL_ENTITY;
			if (ev->target_net_id() != 0u)
				attachTo = m_link.GetEntity(static_cast<uint32_t>(ev->target_net_id()));

			if (attachTo != NULL_ENTITY)
			{
				FxCueContext fx{};
				fx.vWorldPos = pos;
				fx.attachTo = attachTo;
				CFxCuePlayer::Play(world, pszAttachedCueName, fx);
				bPlayedWfxCue = true;
			}
		}

		if (!bPlayedWfxCue)
		{
			const wchar_t* hitTexture = ResolveProjectileHitTexture(ev->kind());
			CFxSystem::SpawnBillboard(world, hitTexture, pos, { 0.f, 0.25f, 0.f }, 0.45f, 0.45f, 0.28f, { 1.f, 0.9f, 0.45f, 1.f });
		}
```

1-7. `Client/Include/Client.vcxproj`

기존 코드:

```xml
    <ClCompile Include="..\Private\GameObject\Champion\Kindred\Kindred_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\LeeSin\LeeSin_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\MasterYi\MasterYi_Registration.cpp" />
```

아래로 교체:

```xml
    <ClCompile Include="..\Private\GameObject\Champion\Kindred\Kindred_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\LeeSin\LeeSin_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\LeeSin\LeeSin_Skills.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\MasterYi\MasterYi_Registration.cpp" />
```

1-8. `Client/Include/Client.vcxproj.filters`

기존 코드:

```xml
    <ClCompile Include="..\Private\GameObject\Champion\LeeSin\LeeSin_Registration.cpp">
      <Filter>02. GameObject\01. Champions\15. LeeSin</Filter>
    </ClCompile>
```

아래에 추가:

```xml
    <ClCompile Include="..\Private\GameObject\Champion\LeeSin\LeeSin_Skills.cpp">
      <Filter>02. GameObject\01. Champions\15. LeeSin</Filter>
    </ClCompile>
```

1-9. 새 파일: `Data/LoL/FX/Champions/LeeSin/q_cast.wfx`

```json
{
  "name": "LeeSin.Q.Cast",
  "emitters": [
    {
      "name": "q_cast_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/leesin_base_q_hit_mis_01_muzzle.png",
      "lifetime": 0.22,
      "width": 0.9,
      "height": 0.9,
      "color": [1.0, 0.78, 0.28, 0.85],
      "offset": [0.0, 0.8, 0.35],
      "fade_in": 0.02,
      "fade_out": 0.12,
      "billboard": true
    }
  ]
}
```

1-10. 새 파일: `Data/LoL/FX/Champions/LeeSin/q_projectile.wfx`

```json
{
  "name": "LeeSin.Q.Projectile",
  "emitters": [
    {
      "name": "q_projectile_front",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_q_mis_front.png",
      "lifetime": 0.65,
      "width": 0.75,
      "height": 0.75,
      "color": [1.0, 0.82, 0.32, 0.95],
      "offset": [0.0, 0.55, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.16,
      "billboard": true,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_projectile_trail",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_q_mis_trail.png",
      "lifetime": 0.65,
      "width": 1.1,
      "height": 0.42,
      "color": [1.0, 0.64, 0.18, 0.72],
      "offset": [0.0, 0.55, -0.35],
      "fade_in": 0.02,
      "fade_out": 0.2,
      "billboard": true,
      "blockable_by_wind_wall": true
    }
  ]
}
```

1-11. 새 파일: `Data/LoL/FX/Champions/LeeSin/q_hit.wfx`

```json
{
  "name": "LeeSin.Q.Hit",
  "emitters": [
    {
      "name": "q_hit_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/leesin_base_q_hit_mis_01_muzzle.png",
      "lifetime": 0.32,
      "width": 1.25,
      "height": 1.25,
      "color": [1.0, 0.82, 0.36, 0.95],
      "offset": [0.0, 0.75, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.18,
      "billboard": true
    }
  ]
}
```

1-12. 새 파일: `Data/LoL/FX/Champions/LeeSin/q_mark.wfx`

```json
{
  "name": "LeeSin.Q.Mark",
  "emitters": [
    {
      "name": "q_mark",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_q_mark_01.png",
      "lifetime": 3.0,
      "width": 0.85,
      "height": 0.85,
      "color": [1.0, 0.78, 0.24, 0.9],
      "offset": [0.0, 1.45, 0.0],
      "fade_in": 0.08,
      "fade_out": 0.3,
      "billboard": true
    }
  ]
}
```

1-13. 새 파일: `Data/LoL/FX/Champions/LeeSin/q2_dash.wfx`

```json
{
  "name": "LeeSin.Q2.Dash",
  "emitters": [
    {
      "name": "q2_dash_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_q_mis_trail.png",
      "lifetime": 0.28,
      "width": 1.35,
      "height": 0.55,
      "color": [1.0, 0.68, 0.22, 0.78],
      "offset": [0.0, 0.75, -0.35],
      "fade_in": 0.02,
      "fade_out": 0.18,
      "billboard": true
    }
  ]
}
```

2. 검증

2-1. 작업 트리 정리 기준

- 다음 구현에서 실제로 만질 파일은 위 목록으로 한정한다.
- 기존 dirty 파일 중 math 공통화, vcxproj `FxCuePlayer` 경로 수정, `Shaders/Mesh3D.hlsl` whitespace 정리는 이미 빌드 통과한 묶음으로 분리한다.
- `Client/Private/GameObject/Champion/LeeSin/LeeSin_Skills.h`는 private 중복 헤더다. `rg "Private/GameObject/Champion/LeeSin/LeeSin_Skills.h|LeeSin_Skills.h" Client`로 include 주체를 확인한 뒤, public header만 쓰는 구조면 삭제한다.
- `Client/Bin/Resource/Texture/Character/LeeSin` 원본 리소스는 `.gitignore` 대상이라 코드/데이터 계획과 분리해서 관리한다.

2-2. 정적 검증

```powershell
rg "LeeSin.Q." Data/LoL/FX/Champions/LeeSin
rg "LeeSin_Skills" Client/Include/Client.vcxproj Client/Include/Client.vcxproj.filters Client/Private/GameObject/Champion/LeeSin Client/Public/GameObject/Champion/LeeSin
git diff --check
```

2-3. 빌드 검증

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "Server/Include/Server.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "Client/Include/Client.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m
```

2-4. 런타임 검증

- 리신 Q1 cast frame에서 `LeeSin.Q.Cast`가 한 번 재생된다.
- 리신 Q projectile spawn event에서 generic billboard가 중복되지 않고 `LeeSin.Q.Projectile`만 재생된다.
- Q hit event에서 `LeeSin.Q.Hit`가 재생되고, target entity가 있으면 `LeeSin.Q.Mark`가 target에 attach된다.
- Q2 cast frame에서 `LeeSin.Q2.Dash`가 caster에 attach되어 재생된다.
- `.wfx` 파일 수치만 바꿔도 C++ 재빌드 없이 크기/색/수명 변화가 확인된다.

2-5. CONFIRM_NEEDED

- F5 실행 working directory가 repo root가 아니어서 `Data/LoL/FX/Champions/LeeSin` 로드가 실패하면, 다음 반영에서 `Client/Bin/Data/LoL/FX/Champions/LeeSin` copy step 또는 preload fallback path를 별도 계획으로 추가한다.
