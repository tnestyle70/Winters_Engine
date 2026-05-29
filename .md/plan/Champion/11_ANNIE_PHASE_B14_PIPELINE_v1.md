# Phase B-14 (Annie) — 단일 텍스처 fire mage + Hook 패턴 미러

**작성일**: 2026-05-04
**선행**: Phase B-13 Jax 박제 완료 (멀티 텍스처 검증)
**Framework**: [`BUILD_INTEGRITY_FRAMEWORK.md`](../../architecture/BUILD_INTEGRITY_FRAMEWORK.md) v1 적용
**목표**:
1. Annie 1체 추가 — 단일 텍스처 + Ezreal/Jax 패턴 미러 (3-Layer Hook)
2. Annie 의 특수성 박제: animation prefix `annie_2012_` + spell3 만 `annie_` (substring 매칭으로 해결)
3. R (Tibbers 소환) — 1차는 단순 AOE 데미지, 본격 소환 entity 는 B-17 (Pet System)

---

## §0. Agent Contract Evidence — 도구 로그 박제

| 도구 | 호출 | 결과 |
|---|---|---|
| `Bash ls` | Character/Annie/{root, animations, particles} | 19 anim / 50+ particle / single texture |
| `Grep` | spell\|attack 패턴 | 6 anim 키 추출 |
| `Read` | 01_MULTI_MATERIAL_CHAMPION_YONE.md L1-50 | submesh 인덱스 vs 이름 패턴 검증 (Annie 는 단일 메시라 무관) |

---

## §1. Preflight Evidence Table

| 항목 | 결과 | 명령/위치 |
|---|---|---|
| **Read 한 파일** | 0 (Jax 박제 결과 + Framework 인용) | — |
| **Grep 패턴** | 2회 — anim/particle 인벤토리 | `ls Character/Annie/animations/` |
| **발견한 기존 인프라** | Jax 의 `Jax_Registration.cpp` 패턴 100% 미러 가능 | (B-13 산출) |
| **현재 API 시그니처** | SkillHookContext / VisualHookContext / GameplayHookContext (B-12 v2 인용) | 변경 없음 |
| **v1 / 중복 파일 존재** | 없음 (Annie 신규) | — |
| **Hook context 필드** | pWorld / casterEntity / casterTeam / pDef / pCommand / pFxMeshRenderer | 변경 없음 |
| **Asset 경로 실존** | `annie.fbx` ✅ / `annie_base_2012_cm.png` ✅ / `annie.wmesh` ❌ (D-0) / 19 anim ✅ / 50+ particle ✅ | `ls Character/Annie/` |
| **빌드 가드 위치** | WINTERS_MIN_SCENE 26+ hit (B-13 강화 후) | — |
| **Annie 애니 키 (실측)** | idle: `annie_2012_idle1` / run: `annie_2012_run` / BA: `annie_2012_attack1` / Q: `annie_2012_spell1` / W: `annie_2012_spell2` / E: `annie_spell3` (★ prefix 다름) / R: `annie_2012_spell4` | `ls Character/Annie/animations/` |

**★ Annie 특수 사항**: 18/19 anim 이 `annie_2012_` prefix, **spell3 만 `annie_spell3`** (no `_2012_`). `PlayAnimationByName` 의 substring 매칭으로 해결 — `animPrefix=""`, animKey 를 fully qualified 로 박제.

---

## §2. Plan Quality Gate Status

- [x] Full code (5 신규 파일 전문)
- [x] No placeholder (Gameplay ns `(void)ctx;` 는 의도 stub)
- [x] Hook context fields verified (B-12 v2 Read 결과 재사용)
- [x] Asset paths Test-Path verified (§1 표)
- [x] vcxproj registration (§5)

---

## §3. 신규 파일 5개

### 3.1 D-0 변환

**파일**: `Tools/convert_all_assets.bat:34` (Jax 다음)

```bat
call :convert_champ "Jax" "jax.fbx"
call :convert_champ "Annie" "annie.fbx"
```

### 3.2 `Client/Public/GameObject/Champion/Annie/Annie_Components.h`

```cpp
#pragma once

#include "Engine_Defines.h"

// Annie — Phase B-14
// Q/W = 단순 fire skill, E = self shield + 패시브 스택, R = 1차 단순 AOE
struct AnnieStateComponent
{
    // Passive — Pyromania (4 스킬 캐스트마다 다음 스킬 stun)
    u8_t   pyromaniaStacks = 0;
    u8_t   pyromaniaThreshold = 4;
    bool   bNextStunReady = false;

    // E — Molten Shield (5s 방어막 + AS 부스트)
    bool   bShieldActive = false;
    f32_t  fShieldTimer  = 0.f;
    f32_t  fShieldDurationSec = 5.f;
    f32_t  fShieldAmount = 80.f;

    // R — Tibbers (1차 단순 AOE, B-17 본격 소환)
    bool   bTibbersActive = false;
    f32_t  fTibbersTimer  = 0.f;
    f32_t  fTibbersDurationSec = 6.f;
    Vec3   vTibbersPos = { 0.f, 0.f, 0.f };
};
```

### 3.3 `Client/Public/GameObject/Champion/Annie/Annie_FxPresets.h`

```cpp
#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Annie::Fx
{
    void SpawnBAFireFlash(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnQFireball(CWorld& world, EntityID owner, EntityID target, f32_t fLifetime);
    void SpawnWConeFire(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnEMoltenShield(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnRTibbersSummon(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fLifetime);
    void SpawnPyromaniaCharge(CWorld& world, EntityID owner, f32_t fLifetime);
}
```

### 3.4 `Client/Private/GameObject/Champion/Annie/Annie_FxPresets.cpp`

```cpp
#include "GameObject/Champion/Annie/Annie_FxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    constexpr const wchar_t* kPathBaFlashTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_ash.png";
    constexpr const wchar_t* kPathQFireballTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_flames.png";
    constexpr const wchar_t* kPathQFireballTrailTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_trail.png";
    constexpr const wchar_t* kPathWConeTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_w_grounddecalfinal.png";
    constexpr const wchar_t* kPathEShieldTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_circle.png";
    constexpr const wchar_t* kPathETex2 =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_buf_glow2.png";
    constexpr const wchar_t* kPathRSummonTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_brazier_flame_temp_01.png";
    constexpr const wchar_t* kPathPyroChargeTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_glow.png";
}

namespace Annie::Fx
{
    void SpawnBAFireFlash(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathBaFlashTex;
        fx.fWidth = 1.2f; fx.fHeight = 1.2f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.3f, 0.6f, 0.2f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.45f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnQFireball(CWorld& world, EntityID owner, EntityID target, f32_t fLifetime)
    {
        // 1차: caster 손 부근 fire glow + target hit spark.
        // 본격 projectile 은 B-15 (Ashe) 의 PendingHitSystem 합류 후 적용.
        if (owner == NULL_ENTITY) return;

        // Owner side — fireball charge
        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.2f, 0.f };
            fx.texturePath = kPathQFireballTex;
            fx.fWidth = 1.0f; fx.fHeight = 1.0f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime * 0.4f;
            fx.vColor = { 1.4f, 0.7f, 0.2f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.2f;
            CFxSystem::Spawn(world, fx);
        }

        // Target side — fireball impact
        if (target != NULL_ENTITY)
        {
            FxBillboardComponent fx{};
            fx.attachTo = target;
            fx.vAttachOffset = { 0.f, 1.0f, 0.f };
            fx.texturePath = kPathQFireballTrailTex;
            fx.fWidth = 1.6f; fx.fHeight = 1.6f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime;
            fx.vColor = { 1.4f, 0.5f, 0.15f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.5f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnWConeFire(CWorld& world, EntityID owner, const Vec3&, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 0.05f, 0.f };
        fx.texturePath = kPathWConeTex;
        fx.fWidth = 4.0f; fx.fHeight = 4.0f;
        fx.bBillboard = false;          // 지면 평면
        fx.fYaw = 0.f;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.4f, 0.5f, 0.15f, 0.9f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnEMoltenShield(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;

        // Shield ring
        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.0f, 0.f };
            fx.texturePath = kPathEShieldTex;
            fx.fWidth = 2.2f; fx.fHeight = 2.2f;
            fx.bBillboard = true;
            fx.fLifetime = fDuration;
            fx.vColor = { 1.2f, 0.5f, 0.2f, 0.85f };
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.fFadeOut = fDuration * 0.3f;
            CFxSystem::Spawn(world, fx);
        }

        // Inner glow
        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.1f, 0.f };
            fx.texturePath = kPathETex2;
            fx.fWidth = 1.5f; fx.fHeight = 1.5f;
            fx.bBillboard = true;
            fx.fLifetime = fDuration;
            fx.vColor = { 1.3f, 0.7f, 0.3f, 0.7f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fDuration * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnRTibbersSummon(CWorld& world, EntityID owner, const Vec3& /*groundPos*/, f32_t fLifetime)
    {
        // 1차: caster 위치 큰 화염 폭발. B-17 에서 Pet entity 박제 시 attach 변경.
        if (owner == NULL_ENTITY) return;

        // Ground decal (지면 폭발)
        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 0.05f, 0.f };
            fx.texturePath = kPathRSummonTex;
            fx.fWidth = 5.0f; fx.fHeight = 5.0f;
            fx.bBillboard = false;
            fx.fLifetime = fLifetime;
            fx.vColor = { 1.4f, 0.6f, 0.2f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.6f;
            CFxSystem::Spawn(world, fx);
        }

        // Vertical column
        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.5f, 0.f };
            fx.texturePath = kPathRSummonTex;
            fx.fWidth = 2.0f; fx.fHeight = 4.0f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime * 0.7f;
            fx.vColor = { 1.4f, 0.5f, 0.15f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnPyromaniaCharge(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 2.4f, 0.f };
        fx.texturePath = kPathPyroChargeTex;
        fx.fWidth = 0.8f; fx.fHeight = 0.8f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.4f, 0.4f, 0.1f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }
}
```

### 3.5 `Client/Public/GameObject/Champion/Annie/Annie_Skills.h`

```cpp
#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

namespace Annie
{
    void OnCastFrame_BA(SkillHookContext& ctx);
    void OnCastFrame_Q(SkillHookContext& ctx);
    void OnCastFrame_W(SkillHookContext& ctx);
    void OnCastFrame_E(SkillHookContext& ctx);
    void OnCastFrame_R(SkillHookContext& ctx);

    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx);
        void OnCastFrame_Q(GameplayHookContext& ctx);
        void OnCastFrame_W(GameplayHookContext& ctx);
        void OnCastFrame_E(GameplayHookContext& ctx);
        void OnCastFrame_R(GameplayHookContext& ctx);
    }

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx);
        void OnCastFrame_Q_Visual(VisualHookContext& ctx);
        void OnCastFrame_W_Visual(VisualHookContext& ctx);
        void OnCastFrame_E_Visual(VisualHookContext& ctx);
        void OnCastFrame_R_Visual(VisualHookContext& ctx);
    }
}

void Annie_KeepAlive();
```

### 3.6 `Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp`

```cpp
#include "GameObject/Champion/Annie/Annie_Skills.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Annie/Annie_FxPresets.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace Annie
{
    namespace
    {
        u32_t ApplyAOEDamage(CWorld& world, EntityID caster, eTeam casterTeam,
                             const Vec3& origin, f32_t fRadius, f32_t fDamage)
        {
            u32_t hits = 0;
            std::vector<EntityID> targets;
            targets.reserve(8);
            world.ForEach<ChampionComponent, TransformComponent>(
                [&](EntityID e, ChampionComponent& cc, TransformComponent& tf)
                {
                    if (e == caster) return;
                    if (cc.team == casterTeam) return;
                    const Vec3 v = tf.GetPosition();
                    const f32_t dx = v.x - origin.x;
                    const f32_t dz = v.z - origin.z;
                    if (dx * dx + dz * dz <= fRadius * fRadius)
                        targets.push_back(e);
                });
            for (EntityID e : targets)
            {
                ApplyDamage(world, caster, casterTeam, e, fDamage);
                ++hits;
            }
            return hits;
        }

        // Pyromania 패시브 — 4 스킬 캐스트마다 다음 스킬 stun
        // (1차는 stun 효과 미적용, 카운터만 박제. B-17 BuffSystem 합류 시 정식 적용)
        void TickPyromaniaStack(CWorld& world, EntityID caster)
        {
            if (!world.HasComponent<AnnieStateComponent>(caster)) return;
            auto& as = world.GetComponent<AnnieStateComponent>(caster);
            as.pyromaniaStacks = static_cast<u8_t>(as.pyromaniaStacks + 1);
            if (as.pyromaniaStacks >= as.pyromaniaThreshold)
            {
                as.pyromaniaStacks = 0;
                as.bNextStunReady = true;
                Fx::SpawnPyromaniaCharge(world, caster, 1.0f);

                char dbg[64];
                sprintf_s(dbg, "[Annie Pyromania] charge ready (next stun)\n");
                OutputDebugStringA(dbg);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    //  Skill (client local prediction)
    // ─────────────────────────────────────────────────────────────

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY) return;
        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, 50.f);

        char dbg[128];
        sprintf_s(dbg, "[Annie BA] target=%u dmg=50.0\n", static_cast<u32_t>(target));
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY) return;

        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, 80.f);
        TickPyromaniaStack(*ctx.pWorld, ctx.casterEntity);

        char dbg[128];
        sprintf_s(dbg, "[Annie Q] target=%u dmg=80.0 (Disintegrate)\n",
            static_cast<u32_t>(target));
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        const Vec3 vOrigin = ctx.pWorld
            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

        // W = cone 4m radius (1차 단순 AOE)
        const u32_t hits = ApplyAOEDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            vOrigin, 4.0f, 70.f);
        TickPyromaniaStack(*ctx.pWorld, ctx.casterEntity);

        char dbg[128];
        sprintf_s(dbg, "[Annie W] hits=%u dmg=70.0 (Incinerate)\n", hits);
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<AnnieStateComponent>(ctx.casterEntity)) return;

        auto& as = ctx.pWorld->GetComponent<AnnieStateComponent>(ctx.casterEntity);
        as.bShieldActive = true;
        as.fShieldTimer = as.fShieldDurationSec;
        TickPyromaniaStack(*ctx.pWorld, ctx.casterEntity);

        OutputDebugStringA("[Annie E] Molten Shield activated (5s)\n");
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<AnnieStateComponent>(ctx.casterEntity)) return;

        auto& as = ctx.pWorld->GetComponent<AnnieStateComponent>(ctx.casterEntity);
        as.bTibbersActive = true;
        as.fTibbersTimer = as.fTibbersDurationSec;
        as.vTibbersPos = ctx.pCommand->groundPos;

        // 1차: 즉시 AOE 폭발 (B-17 에서 Tibbers entity 본격 소환)
        const u32_t hits = ApplyAOEDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->groundPos, 3.0f, 150.f);

        char dbg[128];
        sprintf_s(dbg, "[Annie R] hits=%u dmg=150.0 (Tibbers stub) pos=(%.1f,%.1f,%.1f)\n",
            hits, ctx.pCommand->groundPos.x, ctx.pCommand->groundPos.y, ctx.pCommand->groundPos.z);
        OutputDebugStringA(dbg);
    }

    // ─────────────────────────────────────────────────────────────
    //  Gameplay (shared sim — stub, 04a v2 D-1 진입 시 본격)
    // ─────────────────────────────────────────────────────────────
    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx) { (void)ctx; }
        void OnCastFrame_Q(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_W(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_E(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_R(GameplayHookContext& ctx)  { (void)ctx; }
    }

    // ─────────────────────────────────────────────────────────────
    //  Visual (client FX/Sound)
    // ─────────────────────────────────────────────────────────────
    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            const EntityID target = ctx.pCommand->targetEntityId;
            if (target == NULL_ENTITY) return;
            Fx::SpawnBAFireFlash(*ctx.pWorld, target, 0.4f);
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            const EntityID target = ctx.pCommand->targetEntityId;
            Fx::SpawnQFireball(*ctx.pWorld, ctx.casterEntity, target, 0.5f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnWConeFire(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->direction, 0.6f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnEMoltenShield(*ctx.pWorld, ctx.casterEntity, 5.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnRTibbersSummon(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->groundPos, 1.5f);
        }
    }
}
```

### 3.7 `Client/Public/GameObject/Champion/Annie/Annie_Registration.h`

```cpp
#pragma once
```

### 3.8 `Client/Private/GameObject/Champion/Annie/Annie_Registration.cpp`

```cpp
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Annie/Annie_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kAnn_BA_Cast = MakeHookId(eChampion::ANNIE, HookVariant::BA_CastFrame);
    constexpr u32_t kAnn_Q_Cast  = MakeHookId(eChampion::ANNIE, HookVariant::Q_CastFrame);
    constexpr u32_t kAnn_W_Cast  = MakeHookId(eChampion::ANNIE, HookVariant::W_CastFrame);
    constexpr u32_t kAnn_E_Cast  = MakeHookId(eChampion::ANNIE, HookVariant::E_CastFrame);
    constexpr u32_t kAnn_R_Cast  = MakeHookId(eChampion::ANNIE, HookVariant::R_CastFrame);

    struct AnnieAutoRegister
    {
        AnnieAutoRegister()
        {
            // Annie animations:
            //   - 18/19 = "annie_2012_*" (idle1, run, attack1/2, spell1/2/4, dance, joke, ...)
            //   - 1/19  = "annie_spell3" (no _2012_)
            // 해결: animPrefix="" + animKey 를 fully qualified 로 박제.
            // PlayAnimationByName 의 substring 매칭이 "annie_2012_attack1" 안에서 "attack1" 을 찾음.

            ChampionDef cd{};
            cd.id = eChampion::ANNIE;
            cd.animPrefix    = "";
            cd.idleAnimKey   = "annie_2012_idle1";
            cd.runAnimKey    = "annie_2012_run";
            cd.basicAttackKey = "annie_2012_attack1";
            cd.basicAttackRange = 6.25f;   // 원거리 (마법사)
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Annie/annie.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            const wchar_t* annieBaseTexture =
                L"Client/Bin/Resource/Texture/Character/Annie/annie_base_2012_cm.png";
            cd.defaultTexturePath = annieBaseTexture;
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = annieBaseTexture;
            cd.spawnPosition = { 36.f, 1.f, 0.f };   // Jax (33) 옆
            cd.spawnScale = 0.01f;
            cd.displayName = "Annie";
            CChampionRegistry::Instance().Add(eChampion::ANNIE, cd);

            // BA — annie_2012_attack1
            // 부등식: 0.8 × 1.0 = 0.8 ≥ 14/24 = 0.583 ✅
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f; s.rangeMax = 6.25f; s.manaCost = 0.f;
                s.animKey = "annie_2012_attack1";
                s.lockDurationSec = 0.8f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f; s.recoveryFrame = 14.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 0, s);
            }

            // Q — Disintegrate (annie_2012_spell1)
            // 부등식: 0.5 × 1.0 = 0.5 ≥ 10/24 = 0.417 ✅
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 1;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 4.f; s.rangeMax = 6.25f; s.manaCost = 60.f;
                s.animKey = "annie_2012_spell1";
                s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 5.f; s.recoveryFrame = 10.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 1, s);
            }

            // W — Incinerate (annie_2012_spell2)
            // 부등식: 0.6 × 1.0 = 0.6 ≥ 12/24 = 0.5 ✅
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 8.f; s.rangeMax = 6.0f; s.manaCost = 80.f;
                s.animKey = "annie_2012_spell2";
                s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 5.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 2, s);
            }

            // E — Molten Shield (annie_spell3 — ★ prefix 다름, fully qualified)
            // 부등식: 0.4 × 1.0 = 0.4 ≥ 8/24 = 0.333 ✅
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 12.f; s.rangeMax = 0.f; s.manaCost = 30.f;
                s.animKey = "annie_spell3";       // ★ 2012 없음
                s.lockDurationSec = 0.4f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 1.f; s.recoveryFrame = 8.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 3, s);
            }

            // R — Summon Tibbers (annie_2012_spell4)
            // 부등식: 1.2 × 1.0 = 1.2 ≥ 24/24 = 1.0 ✅
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 4;
                s.targetMode = eTargetMode::GroundTarget;
                s.cooldownSec = 100.f; s.rangeMax = 6.0f; s.manaCost = 100.f;
                s.animKey = "annie_2012_spell4";
                s.lockDurationSec = 1.2f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 12.f; s.recoveryFrame = 24.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kAnn_BA_Cast, &Annie::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kAnn_Q_Cast,  &Annie::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kAnn_W_Cast,  &Annie::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kAnn_E_Cast,  &Annie::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kAnn_R_Cast,  &Annie::OnCastFrame_R);

            CGameplayHookRegistry::Instance().Register(kAnn_BA_Cast, &Annie::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kAnn_Q_Cast,  &Annie::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kAnn_W_Cast,  &Annie::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kAnn_E_Cast,  &Annie::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kAnn_R_Cast,  &Annie::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kAnn_BA_Cast, &Annie::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_Q_Cast,  &Annie::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_W_Cast,  &Annie::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_E_Cast,  &Annie::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_R_Cast,  &Annie::Visual::OnCastFrame_R_Visual);

            OutputDebugStringA("[Annie] Registration complete\n");
        }
    };

    static AnnieAutoRegister s_register;
}

void Annie_KeepAlive()
{
    (void)&s_register;
}
```

---

## §4. Scene_InGame.cpp 수정 (Anchor + Before/After hunk)

### 4.1 Include 추가 (Jax 다음)

**Anchor**: L78 — `#include "GameObject/Champion/Jax/Jax_Skills.h"`

**Before**:
```cpp
//Jax — Phase B-13
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Jax/Jax_Skills.h"
```

**After**:
```cpp
//Jax — Phase B-13
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Jax/Jax_Skills.h"

//Annie — Phase B-14
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Annie/Annie_Skills.h"
```

### 4.2 `Annie_KeepAlive()` 호출 (Jax 다음)

**Anchor**: `Jax_KeepAlive();`

**After**:
```cpp
    extern void Jax_KeepAlive();
    Jax_KeepAlive();

    extern void Annie_KeepAlive();
    Annie_KeepAlive();   // Phase B-14
```

### 4.3 BindPlayerToECSChampion — ANNIE 추가

**Anchor**: `selectedChampion == eChampion::JAX`

**After**:
```cpp
    if (selectedChampion == eChampion::RIVEN
        || selectedChampion == eChampion::EZREAL
        || selectedChampion == eChampion::FIORA
        || selectedChampion == eChampion::JAX
        || selectedChampion == eChampion::ANNIE)
    {
        BindPlayerToECSChampion(m_PlayerEntity);
    }
```

### 4.4 `CreateECSChampion` — AnnieStateComponent 분기

**Anchor**: `else if (id == eChampion::JAX)`

**After**:
```cpp
    else if (id == eChampion::JAX)
        m_World.AddComponent<JaxStateComponent>(e);
    else if (id == eChampion::ANNIE)
        m_World.AddComponent<AnnieStateComponent>(e);
```

### 4.5 `CreateECSEntities` — Annie 스폰 (B-13 의 jaxEntity 옆)

**Before** (B-13 박제 후):
```cpp
    // Jax — Phase B-13 (MIN_SCENE 에서도 active)
    jaxEntity = CreateECSChampion(eChampion::JAX, eTeam::Blue);
    m_JaxEntity = jaxEntity;
```

**After**:
```cpp
    // Jax — Phase B-13 (MIN_SCENE 에서도 active)
    jaxEntity = CreateECSChampion(eChampion::JAX, eTeam::Blue);
    m_JaxEntity = jaxEntity;

    // Annie — Phase B-14 (MIN_SCENE 가드 안 — Jax/Sylas 만 active 유지)
    EntityID annieEntity = NULL_ENTITY;
#if !WINTERS_MIN_SCENE
    annieEntity = CreateECSChampion(eChampion::ANNIE, eTeam::Blue);
    m_AnnieEntity = annieEntity;
    addSkillStateIfAlive(annieEntity);
#endif
```

> **MIN_SCENE 정책**: 본 phase 에서 Annie 는 가드 안 (사용자 의도: Sylas+Jax 만 active 유지). Annie 테스트 시 `#define WINTERS_MIN_SCENE 0` 토글.

### 4.6 m_PlayerEntity 분기 — ANNIE 추가

**Anchor**: `if (champ == eChampion::FIORA) m_PlayerEntity = fioraEntity;`

**After**:
```cpp
    else if (champ == eChampion::FIORA) m_PlayerEntity = fioraEntity;
    else if (champ == eChampion::ANNIE) m_PlayerEntity = annieEntity;
    else
#endif
    if (champ == eChampion::JAX) m_PlayerEntity = jaxEntity;
```

### 4.7 Scene_InGame.h — `m_AnnieEntity` 멤버

**Anchor**: `EntityID m_JaxEntity = NULL_ENTITY;`

**After**:
```cpp
    EntityID m_JaxEntity = NULL_ENTITY;
    EntityID m_AnnieEntity = NULL_ENTITY;   // Phase B-14
```

---

## §5. Implementation Gate

### 5.1 Client.vcxproj 등록

**ClCompile (Jax 다음)**:
```xml
    <ClCompile Include="..\Private\GameObject\Champion\Jax\Jax_Skills.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Annie\Annie_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Annie\Annie_FxPresets.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Annie\Annie_Skills.cpp" />
```

**ClInclude (Jax 다음)**:
```xml
    <ClInclude Include="..\Public\GameObject\Champion\Jax\Jax_Skills.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Annie\Annie_Components.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Annie\Annie_FxPresets.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Annie\Annie_Registration.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Annie\Annie_Skills.h" />
```

### 5.2 최소 수정 / dirty work 보존

- B-13 의 v1 잔존 cleanup (Fiora ChampionTable/SkillTable 제거) 가 끝났다면 본 phase 는 추가 cleanup 없음
- 끝나지 않았다면 본 phase 에 동시 처리 (cleanup 항목 1번 묶기)

---

## §6. Verification Gate

### 6.1 사전 체크리스트
- [ ] `devenv.exe` 종료
- [ ] `taskkill /F /IM WintersServer.exe /IM WintersGame.exe`
- [ ] `Tools\convert_all_assets.bat champions` → `OK=12`
- [ ] `annie.wmesh / .wskel / anims/*.wanim` 19개

### 6.2 G1: Client build

```cmd
MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### 6.3 G4: 8초 smoke

Output 창에 `[Annie] Registration complete`.

### 6.4 G5: Feature smoke

| 단계 | 액션 | 기대 |
|---|---|---|
| 1 | ChampSelect | "Annie" 버튼 |
| 2 | Annie 픽 → InGame (`#define WINTERS_MIN_SCENE 0` 토글 후) | (36,1,0) 스폰 |
| 3 | annie_2012_idle1 anim 재생 | 정적 idle |
| 4 | annie_2012_run anim 전환 | 이동 |
| 5 | Sylas + A | `[Annie BA] dmg=50.0` + fire flash |
| 6 | Q | `[Annie Q] dmg=80.0` (Disintegrate) — single target |
| 7 | W | `[Annie W] hits=N dmg=70.0` (Incinerate) — cone |
| 8 | E | `Molten Shield activated` + ring FX 5s |
| 9 | R (지면 클릭) | `[Annie R] hits=N dmg=150.0 (Tibbers stub)` + 화염 폭발 |
| 10 | Q→W→E→Q (4 캐스트) | 4번째에서 `[Annie Pyromania] charge ready` (stun 효과는 stub) |

### 6.5 회귀 grep

```bash
grep -c "kAnn_" Client/Private/GameObject/Champion/Annie/Annie_Registration.cpp   # ≥ 25
grep "annie_spell3" Client/Private/GameObject/Champion/Annie/Annie_Registration.cpp # 1 (E animKey)
```

---

## §7. Learning Update

### 7.1 박제 후 갱신 후보

**CLAUDE.md Gotcha 후보**:
```markdown
- **Annie 같은 prefix 혼재 챔프 (B-14, 2026-05-04)**:
  대부분 anim 이 `annie_2012_*` 인데 spell3 만 `annie_spell3` (2012 없음). animPrefix=""
  + animKey 를 fully qualified 로 박제하면 PlayAnimationByName 의 substring 매칭이 정상.
  Animation 키가 prefix 일부만 변형된 경우 매번 substring 매칭 검증 (T-4 Gotcha 연관).
```

**Memory 후보**:
```markdown
파일: feedback_annie_pyromania_passive.md
내용: Annie Pyromania 패시브는 "다음 스킬 stun" — BuffSystem (B-17) 합류 후 정식.
1차 박제는 카운터 (`pyromaniaStacks`) + `bNextStunReady` 플래그만. CC 적용은 N/A.
유사 패시브: Yasuo Q3 Tornado, Lucian Light Slinger 등.
```

---

## §8. 다음 단계

| Phase | 챔프/작업 | 패턴 |
|---|---|---|
| **B-15** | Ashe (12_ASHE...) | Annie 단순 패턴 + projectile 인프라 (PendingHitSystem 확장) |
| **B-16** | Yone (13_YONE...) | 메시 분리 인프라 + 엘든링 확장 설계 |
| **B-17** | BuffSystem + Annie Pyromania CC + Tibbers Pet entity | Annie/Riven Q passive 정식 |

---

## §9. 즉시 진입 명령

```
"Phase B-14 Annie 진행. 11_ANNIE_PHASE_B14_PIPELINE_v1.md §3 D-0 변환부터.
1) convert_all_assets.bat Annie 1줄 추가 → 변환 → 19 anims 검증
2) Annie 5개 신규 파일 박제 (Components/FxPresets h+cpp/Skills h+cpp/Registration cpp)
3) Scene_InGame.cpp 7 영역 수정 (anchor 박제)
4) Client.vcxproj 등록 → G1 → G3 → G4 → G5 검증."
```
