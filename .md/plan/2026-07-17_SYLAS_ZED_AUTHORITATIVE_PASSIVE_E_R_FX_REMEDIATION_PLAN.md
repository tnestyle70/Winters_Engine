Session - 사일러스 패시브 광역 공격/R 투사체와 제드 패시브/E/R을 기존 서버 권위 EffectTrigger·WFX 경로로 전수 복구한다.
좌표: 신규 좌표 후보 · 축 C7 · C8
관련: 2026-07-17_SYLAS_PASSIVE_STACK_BA_ANIMATION_FX_PLAN.md · 2026-07-17_SYLAS_PASSIVE_STACK_BA_ANIMATION_FX_RESULT.md · 2026-07-17_ZED_PASSIVE_E_R_AUTHORITATIVE_FX_PLAN.md · 2026-07-17_ZED_PASSIVE_E_R_AUTHORITATIVE_FX_RESULT.md

## 1. 결정 기록

- 문제·제약: 사일러스는 패시브 스택 소비와 `skinned_mesh_sylas_attack_passive.wanim` stage 2 등록까지만 있고, 적중 시 주변 적에게 가는 서버 광역 피해와 `sylas_base_ba_swipe1.wmesh` 보라색 원형 FX가 없다. R의 `r_cast.wfx`에는 `sylas_base_r_hookmis_head.png`가 있으나 GameSim이 EffectTrigger를 내지 않고, `segment_t=1.0`은 끝점 정적 배치라 발사 이동도 아니다.
- 문제·제약: 제드 자산 WFX 4종은 요청한 wmesh/png를 이미 가리키지만, E 본체 stage 1과 R 표식 stage 1이 서버 cue에서 빠져 원격 관전자에게 보장되지 않는다. 더 치명적인 결함은 R 폭발용 수동 `flatAmount`가 `skill.zed.r`의 0 damage atom으로 다시 계산되어 실제 피해가 0이 되는 경로다.
- 선진·대안·실패: Irelia/Viego처럼 기존 GameSim → `ReplicatedEventComponent::EffectTrigger` → VisualHook → WFX 한 경로를 유지한다. 로컬/서버 훅을 둘 다 등록해 즉시 FX를 보이는 대안은 동일 클라이언트에서 중복 재생되므로 제외한다.
- 메커니즘: 사일러스 패시브 BA impact에서 기본 공격과 별도의 마법 `DamageRequest`를 결정적 EntityID 순서로 반경 2.75 내 적 챔피언에게 넣고, 기존 BA stage 2 cue가 보라색 원형 WFX를 재생한다. Sylas R, Zed E/R 본체는 서버가 stage 1 cue를 발행하고 클라이언트는 authoritative event에서만 해당 FX를 재생한다.
- 대조군: 새 렌더러·별도 FX owner를 만들지 않는다. Irelia의 서버 cue 발행과 Viego의 기존 WFX 재생 방식을 재사용하며, `Shared/GameSim`은 Client/Engine 타입을 포함하지 않는다.
- 평가: 사일러스 패시브 수치는 상세 명세가 없으므로 서버 데이터 기본값 `60 + 0.6 AP` 마법 피해, 반경 2.75로 둔다. 제드 패시브는 현재 데이터의 체력 50% 이하·잃은 체력 10%, R은 현재 데이터의 잃은 체력 30%를 유지한다. 런타임 체감 수치는 Champion Tuner/JSON에서 후속 조정할 수 있다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

기존 코드:

```json
      "key": "skill.sylas.basic_attack",
      "params": {
        "maxStacks": 3.0,
        "stackWindowSec": 5.0
      },
```

아래로 교체:

```json
      "key": "skill.sylas.basic_attack",
      "params": {
        "apRatio": 0.6,
        "baseDamage": 60.0,
        "maxStacks": 3.0,
        "radius": 2.75,
        "stackWindowSec": 5.0
      },
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.h

기존 코드:

```cpp
	bool_t TryConsumePassiveBasicAttack(CWorld& world, EntityID caster);
	bool_t CanHijackUltimate(CWorld& world, const TickContext& tc, EntityID caster, EntityID target);
```

아래로 교체:

```cpp
	bool_t TryConsumePassiveBasicAttack(CWorld& world, EntityID caster);
	void EnqueuePassiveBasicAttackAreaDamage(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID primaryTarget);
	bool_t CanHijackUltimate(CWorld& world, const TickContext& tc, EntityID caster, EntityID target);
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.cpp

include 목록의 component/system 구간에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
```

기존 fallback 상수 아래에 추가:

```cpp
    constexpr f32_t kSylasPassiveRadiusFallback = 2.75f;
    constexpr f32_t kSylasPassiveBaseDamageFallback = 60.f;
    constexpr f32_t kSylasPassiveApRatioFallback = 0.6f;
```

`ResolveSylasSkillEffectParam` overload 아래에 다음 helper를 추가한다. `EffectTrigger`는 Sylas의 기존 cast hook ID를 사용하고, passive 피해는 `skillId=0`으로 수동 공식을 보존한다.

```cpp
    u16_t MakeSylasSkillId(u8_t slot)
    {
        return static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::SYLAS) << 8) |
            static_cast<u32_t>(slot));
    }

    void EmitSylasEffect(
        CWorld& world,
        EntityID source,
        EntityID target,
        u8_t slot,
        u8_t rank,
        u8_t stage,
        const Vec3& position,
        const Vec3& direction,
        u16_t durationMs,
        u64_t startTick)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = source;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(
            eChampion::SYLAS,
            slot == static_cast<u8_t>(eSkillSlot::R)
                ? GameplayHookVariant::R_CastFrame
                : GameplayHookVariant::BA_CastFrame);
        event.skillId = MakeSylasSkillId(slot);
        event.slot = slot;
        event.rank = rank;
        event.flags = static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (static_cast<u16_t>(rank & 0x0fu) << 8) |
            static_cast<u16_t>(slot));
        event.position = position;
        event.direction = direction;
        event.durationMs = durationMs;
        event.startTick = startTick;
        EnqueueReplicatedEvent(world, event);
    }

    eTeam ResolveTeam(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).team;
        if (entity != NULL_ENTITY && world.HasComponent<MinionComponent>(entity))
            return world.GetComponent<MinionComponent>(entity).team;
        if (entity != NULL_ENTITY && world.HasComponent<StructureComponent>(entity))
            return world.GetComponent<StructureComponent>(entity).team;
        return eTeam::Neutral;
    }
```

`OnR`의 유효성 검사 직후, spellbook 적용 전에 아래를 추가:

```cpp
        const Vec3 casterPos = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 direction = WintersMath::DirectionXZ(casterPos, targetPos, ResolveCommandDirection(ctx));
        EmitSylasEffect(
            world,
            caster,
            target,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            1u,
            targetPos,
            direction,
            500u,
            tc.tickIndex);
```

`TryConsumePassiveBasicAttack` 아래에 추가:

```cpp
    void EnqueuePassiveBasicAttackAreaDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID primaryTarget)
    {
        if (caster == NULL_ENTITY ||
            primaryTarget == NULL_ENTITY ||
            !world.IsAlive(caster) ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<ChampionComponent>(caster))
        {
            return;
        }

        const f32_t radius = std::max(0.f, ResolveSylasSkillEffectParam(
            world, tc, caster, eSkillSlot::BasicAttack,
            eSkillEffectParamId::Radius, kSylasPassiveRadiusFallback));
        const f32_t baseDamage = std::max(0.f, ResolveSylasSkillEffectParam(
            world, tc, caster, eSkillSlot::BasicAttack,
            eSkillEffectParamId::BaseDamage, kSylasPassiveBaseDamageFallback));
        const f32_t apRatio = std::max(0.f, ResolveSylasSkillEffectParam(
            world, tc, caster, eSkillSlot::BasicAttack,
            eSkillEffectParamId::ApRatio, kSylasPassiveApRatioFallback));
        const Vec3 center = world.GetComponent<TransformComponent>(caster).GetPosition();
        const eTeam sourceTeam = ResolveTeam(world, caster);

        for (EntityID target : DeterministicEntityIterator<HealthComponent>::CollectSorted(world))
        {
            if (target == caster ||
                !world.IsAlive(target) ||
                !world.HasComponent<TransformComponent>(target) ||
                !GameplayStateQuery::CanReceiveDamage(world, caster, target))
            {
                continue;
            }
            const HealthComponent& health = world.GetComponent<HealthComponent>(target);
            if (health.bIsDead || health.fCurrent <= 0.f)
                continue;
            const eTeam targetTeam = ResolveTeam(world, target);
            if (targetTeam == sourceTeam && targetTeam != eTeam::Neutral)
                continue;
            const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
            if (WintersMath::DistanceSqXZ(center, targetPos) > radius * radius)
                continue;

            DamageRequest request{};
            request.source = caster;
            request.target = target;
            request.sourceTeam = sourceTeam;
            request.type = eDamageType::Magic;
            request.flatAmount = baseDamage;
            request.apRatioOverride = apRatio;
            request.iSourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
            request.eSourceKind = eDamageSourceKind::Skill;
            request.flags = DamageFlag_None;
            EnqueueDamageRequest(world, request);
        }
    }
```

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

`KindredGameSim.h` 아래에 추가:

```cpp
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
```

기존 코드:

```cpp
            EnqueueDamageRequest(world, request);
            if ((action.uFlags & CombatActionFlags::ZedPassive) != 0u)
```

아래로 교체:

```cpp
            EnqueueDamageRequest(world, request);
            if ((action.uFlags & CombatActionFlags::SylasPassive) != 0u)
            {
                SylasGameSim::EnqueuePassiveBasicAttackAreaDamage(
                    world,
                    tc,
                    source,
                    target);
            }
            if ((action.uFlags & CombatActionFlags::ZedPassive) != 0u)
```

### 2-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp

`OnBACastFrame` 첫 줄에 아래를 추가하여 passive hit WFX는 impact 서버 cue에서만 재생한다.

```cpp
			if (ctx.skillStage >= 2u && !ctx.bAuthoritativeEvent)
				return;
```

기존 코드:

```cpp
		void OnRCastFrame(VisualHookContext& ctx)
		{
			Vec3 vStart = ResolveCasterPosition(ctx);
```

아래로 교체:

```cpp
		void OnRCastFrame(VisualHookContext& ctx)
		{
			if (!ctx.bAuthoritativeEvent)
				return;

			Vec3 vStart = ResolveCasterPosition(ctx);
```

### 2-6. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/passive_ba.wfx

파일 전체를 아래로 교체:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Sylas.PassiveBA.Hit",
  "emitters": [
    {
      "name": "passive_purple_chain_circle",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Sylas/particles/fbx/sylas_base_ba_swipe1.wmesh",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/render/sylas_base_ba_swipe1.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 0.45,
      "fade_in": 0.01,
      "fade_out": 0.20,
      "scale": [0.040, 0.040, 0.040],
      "color": [1.30, 0.18, 2.10, 0.94],
      "attach_offset": [0.0, 0.08, 0.0],
      "world_yaw_spin_speed": 2.4,
      "blockable_by_wind_wall": false
    }
  ]
}
```

### 2-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Zed/ZedGameSim.cpp

`#include <iostream>`을 삭제하고 이번 범위에서 확인된 `[ZedSim]` routine `std::cout` 블록을 삭제한다.

기존 코드:

```cpp
        request.skillId = MakeZedSkillId(slot);
        request.rank = rank;
```

아래로 교체:

```cpp
        request.skillId = slot == static_cast<u8_t>(eSkillSlot::R)
            ? 0u
            : MakeZedSkillId(slot);
        request.rank = rank;
```

`OnE`의 `casterPos` 계산과 원형 타깃 수집 사이에 아래를 추가:

```cpp
        EmitZedEffect(
            world,
            ctx.casterEntity,
            NULL_ENTITY,
            static_cast<u8_t>(eSkillSlot::E),
            ctx.skillRank,
            1u,
            casterPos,
            dir,
            500u,
            ctx.pTickCtx->tickIndex);
```

`OnR`의 `markDurationSec`/`missingHealthDamageRatio` 해석 직후에 아래를 추가:

```cpp
        EmitZedEffect(
            world,
            ctx.casterEntity,
            target,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            1u,
            targetPos,
            dir,
            static_cast<u16_t>(std::clamp(markDurationSec * 1000.f, 0.f, 65535.f)),
            ctx.pTickCtx->tickIndex);
```

### 2-8. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Zed/Zed_Registration.cpp

`DispatchZedVisualHook` 첫 줄에 아래를 추가하여 passive hit와 E/R 본체 FX는 서버 cue 한 경로만 사용한다.

```cpp
        if (!visualCtx.bAuthoritativeEvent &&
            visualCtx.pDef &&
            ((visualCtx.pDef->slot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
                    visualCtx.skillStage >= 2u) ||
                visualCtx.pDef->slot == static_cast<u8_t>(eSkillSlot::E) ||
                visualCtx.pDef->slot == static_cast<u8_t>(eSkillSlot::R)))
        {
            return;
        }
```

기존 코드:

```cpp
                CSkillHookRegistry::Instance().Register(hook, &Zed::OnCastFrame);
                CVisualHookRegistry::Instance().Register(
```

아래로 교체:

```cpp
                CVisualHookRegistry::Instance().Register(
```

### 2-9. 기존 제드 WFX 및 애니메이션 정적 검증

코드 변경 없이 다음 연결을 전수 확인한다.

```text
Passive stage 2 animation -> skinned_mesh_zed_attack_passive.wanim
Zed.PassiveBA.Hit -> common_circletimer.wmesh + zed_e_hitslash.png
Zed.E.Slash -> zed_atkswipe.wmesh, black tint, radius 2.75
Zed.R.Mark -> zed_crossswipe.wmesh, red tint
Zed.R.LethalMarker -> zed_base_r_hit_marker.wmesh, target head offset, show stage 4/hide stage 5/pop stage 2 clear
```

### 2-10. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp

`#include <cmath>` 위에 추가:

```cpp
#include <algorithm>
```

`PlaySylasCueSegment` 아래에 기존 WFX velocity 경로를 쓰는 helper를 추가:

```cpp
	void PlaySylasProjectileCue(VisualHookContext& ctx, const char* pszCueName,
		const Vec3& vStartWorldPos, const Vec3& vEndWorldPos)
	{
		if (!ctx.pWorld)
			return;

		const Vec3 delta{
			vEndWorldPos.x - vStartWorldPos.x,
			vEndWorldPos.y - vStartWorldPos.y,
			vEndWorldPos.z - vStartWorldPos.z
		};
		const f32_t distance = std::sqrt(
			delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
		const f32_t lifetime = std::clamp(distance / 18.f, 0.12f, 0.45f);

		FxCueContext fx{};
		fx.vWorldPos = vStartWorldPos;
		fx.vForward = ResolveForward(ctx);
		fx.bOverrideVelocity = true;
		fx.vVelocity = {
			delta.x / lifetime,
			delta.y / lifetime,
			delta.z / lifetime
		};
		fx.bOverrideLifetime = true;
		fx.fLifetimeOverride = lifetime;
		fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
		CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx);
	}
```

기존 코드:

```cpp
			PlaySylasCueSegment(ctx, "Sylas.R.Cast", vStart, vEnd);
```

아래로 교체:

```cpp
			PlaySylasCueSegment(ctx, "Sylas.R.Cast", vStart, vEnd);
			PlaySylasProjectileCue(ctx, "Sylas.R.HookProjectile", vStart, vEnd);
```

### 2-11. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/r_cast.wfx

`r_hook_head` emitter 전체를 삭제하여 적 끝점에 정적 head가 중복 생성되지 않게 한다.

삭제할 코드:

```json
    {
      "name": "r_hook_head",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/sylas_base_r_hookmis_head.png",
      "lifetime": 0.36,
      "fade_in": 0.01,
      "fade_out": 0.24,
      "segment_t": 1.0,
      "width": 1.55,
      "height": 1.55,
      "color": [1.85, 1.10, 2.25, 0.92],
      "billboard": true,
      "blockable_by_wind_wall": true
    },
```

### 2-12. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/r_hook_projectile.wfx

새 파일:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Sylas.R.HookProjectile",
  "emitters": [
    {
      "name": "r_hook_head_projectile",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/sylas_base_r_hookmis_head.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 0.45,
      "fade_in": 0.01,
      "fade_out": 0.08,
      "width": 1.55,
      "height": 1.55,
      "color": [1.85, 1.10, 2.25, 0.92],
      "billboard": true,
      "blockable_by_wind_wall": true
    }
  ]
}
```

## 3. 검증

예측:

- Sylas 스킬 사용 후 BA를 맞히면 stage 2 패시브 animation이 선택되고, 사일러스 중심 보라색 `sylas_base_ba_swipe1.wmesh`가 1회 재생되며 반경 2.75 내 적 챔피언 각각에 `60 + 0.6 AP` 마법 피해가 기본 공격과 별도로 들어간다.
- Sylas R은 서버가 target·direction을 포함한 stage 1 cue를 발행하여 `sylas_base_r_hookmis_head.png` 머리가 적 방향 segment 끝으로 진행한다.
- Zed HP 50% 이하 대상 BA는 stage 2 passive animation과 circle/hitslash를 재생하고 잃은 체력 10% 물리 추가 피해를 impact tick에 넣는다.
- Zed E 본체와 그림자 원은 모두 서버 cue에서 보이고, 중복 타깃은 1회만 피해를 받는다. Zed R은 red cross를 target에 보이고, 서버 DamagePipeline preview가 lethal일 때만 머리 위 marker를 보여 pop/hide에서 제거한다.
- Zed R 폭발은 `skillId=0`으로 현재 missing-health 계산값을 보존하므로 0 damage atom에 덮이지 않는다.

검증 명령:

```powershell
python -m json.tool Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json > $null
python -m json.tool Data/LoL/FX/Champions/Sylas/passive_ba.wfx > $null
python -m json.tool Data/LoL/FX/Champions/Sylas/r_cast.wfx > $null
python -m json.tool Data/LoL/FX/Champions/Sylas/r_hook_projectile.wfx > $null
python -m json.tool Data/LoL/FX/Champions/Zed/passive_ba_hit.wfx > $null
python -m json.tool Data/LoL/FX/Champions/Zed/e_slash.wfx > $null
python -m json.tool Data/LoL/FX/Champions/Zed/r_mark.wfx > $null
python -m json.tool Data/LoL/FX/Champions/Zed/r_lethal_marker.wfx > $null
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Server/Include/Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo
rg -n "sylas_base_ba_swipe1|sylas_base_r_hookmis_head|common_circletimer|zed_e_hitslash|zed_atkswipe|zed_crossswipe|zed_base_r_hit_marker" Data/LoL/FX/Champions
rg -n "skinned_mesh_sylas_attack_passive|skinned_mesh_zed_attack_passive" Client Data/LoL/ClientPublic
rg -n "EmitSylasEffect|EnqueuePassiveBasicAttackAreaDamage|EmitZedEffect|kZedRLethalMarker|skillId = slot" Shared Client
git diff --check
```

미검증:

- Server/Client Debug x64 빌드는 후속 사용자 요청으로 실행해 통과했다.
- `.wanim` 존재와 stage 2 선택 경로는 정적·컴파일 단계에서 확인했지만 실제 skeleton 재생·FX 크기·보라색 체감은 사용자 인게임 검증 항목으로 남긴다.

확인 필요:

- Sylas 패시브의 최종 밸런스 수치와 WMesh scale 0.040은 기능 복구 후 인게임 캡처를 기준으로 조정한다.
