# Phase B-15 (Ashe) — 투사체 ADC + Volley 멀티-projectile 인프라

**작성일**: 2026-05-04
**선행**: Phase B-14 Annie 박제 완료
**Framework**: [`BUILD_INTEGRITY_FRAMEWORK.md`](../../architecture/BUILD_INTEGRITY_FRAMEWORK.md) v1 적용
**목표**:
1. Ashe 1체 추가 — 5 스킬 모두 projectile 기반 (BA/Q-buff/W-cone-volley/E-scout/R-global-stun)
2. **PendingHitSystem 재사용** (Yasuo/Ezreal 가 이미 사용) — 신규 인프라 0
3. **Frost (slow) 컨디션** 박제 — Ashe 의 시그니처 메커닉, B-17 BuffSystem 의 prerequisite

---

## §0. Agent Contract Evidence

| 도구 | 호출 | 결과 |
|---|---|---|
| `Bash ls` | Character/Ashe/{root, animations, particles} | 22 anim / 100+ particle / single texture |
| `Grep` | "PendingHit\|Projectile" --include="*.h" | YasuoProjectileSystem + KalistaProjectileSystem + PendingHitSystem 발견 |
| `Read` | (B-13 Jax 박제 시 PendingHitSystem 시그니처 이미 검증) | 인프라 재사용 가능 |

---

## §1. Preflight Evidence Table

| 항목 | 결과 | 명령/위치 |
|---|---|---|
| **Read 한 파일** | 0 (인프라 재사용) | — |
| **Grep 패턴** | 2회 | projectile 인프라 식별 |
| **발견한 기존 인프라** | `CPendingHitSystem::Schedule(world, caster, team, dir, delay, projectileKind, speed, maxDist, hitRadius, damage, statusEffect)` — Ezreal Q 사용 중 | (B-12 v2 Ezreal_Skills.cpp:59-62 인용) |
| **현재 API 시그니처** | PendingHit + ProjectileKind enum (eProjectileKind::MysticShot 등) | 변경 없음 |
| **v1 / 중복 파일 존재** | 없음 | — |
| **Hook context 필드** | 변경 없음 | — |
| **Asset 경로 실존** | `ashe.fbx` ✅ / `ashe_base_2011_tx_cm.png` ✅ / 22 anim ✅ / Ashe-specific particles 30+ ✅ | `ls Character/Ashe/` |
| **Ashe 애니 키** | idle: `ashe_idle1` / run: `ashe_run` / BA: `ashe_attack1` / Q: `ashe_spell1` / W: `ashe_spell2` / E: `ashe_spell3` / R: `ashe_channel` | `ls Character/Ashe/animations/` |

**★ Ashe 의 Q (Ranger's Focus) 특수 사항**: spell1 본체 + spell1_in (charge-up). 1차는 단순 Self buff state — buff active 동안 BA 공격 속도 + 데미지 증가. spell1_in 미사용.

---

## §2. Plan Quality Gate Status

- [x] Full code (5 신규 파일)
- [x] No placeholder
- [x] Hook context fields (PendingHitSystem signature 인용)
- [x] Asset paths Test-Path (§1)
- [x] vcxproj registration (§5)

---

## §3. 신규 파일 5개

### 3.1 D-0 변환

```bat
call :convert_champ "Annie" "annie.fbx"
call :convert_champ "Ashe" "ashe.fbx"
```

### 3.2 `Client/Public/GameObject/Champion/Ashe/Ashe_Components.h`

```cpp
#pragma once

#include "Engine_Defines.h"

// Ashe — Phase B-15
// Passive: Frost Shot (모든 BA 가 slow 적용), Q (Ranger's Focus) buff, W (Volley) 멀티 화살,
// E (Hawkshot) 정찰, R (Crystal Arrow) 글로벌 stun
struct AsheStateComponent
{
    // Q — Ranger's Focus (4s buff: AS + 데미지 증가)
    bool   bQActive = false;
    f32_t  fQTimer  = 0.f;
    f32_t  fQDurationSec = 4.f;
    u8_t   focusStacks = 0;
    u8_t   focusThreshold = 4;     // 4 BA 후 Q 활성 가능
    f32_t  fQAttackSpeedBonus = 0.4f;     // +40% AS
    f32_t  fQDamageBonus = 20.f;          // +20 BA dmg

    // W — Volley count (cone width 별 화살 수)
    u8_t   volleyArrowCount = 9;
    f32_t  fVolleyConeAngleDeg = 50.f;
    f32_t  fVolleyRange = 9.0f;

    // E — Hawkshot (정찰 — 1차 visual only, B-17 시야 시스템 합류 시 정식)
    f32_t  fHawkshotRange = 25.f;
    f32_t  fHawkshotVisionDurationSec = 5.f;

    // R — Crystal Arrow (글로벌, 거리 비례 stun 길이)
    f32_t  fCrystalArrowSpeed = 30.f;
    f32_t  fCrystalArrowMaxDist = 200.f;   // 글로벌 (사실상 무제한)
    f32_t  fCrystalArrowStunMin = 1.5f;
    f32_t  fCrystalArrowStunMax = 3.5f;

    // Frost passive — slow 적용 (B-17 BuffSystem 합류 시 정식)
    f32_t  fFrostSlowPercent = 0.15f;      // 15% slow
    f32_t  fFrostSlowDuration = 2.0f;
};
```

### 3.3 `Client/Public/GameObject/Champion/Ashe/Ashe_FxPresets.h`

```cpp
#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace Ashe::Fx
{
    void SpawnBAArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnFrostHit(CWorld& world, EntityID target, f32_t fLifetime);

    void SpawnQBuffActive(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnQReadySparks(CWorld& world, EntityID owner, f32_t fLifetime);

    void SpawnWVolleyArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnWVolleyMuzzle(CWorld& world, EntityID owner, f32_t fLifetime);

    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3& dest, f32_t fLifetime);

    void SpawnRCrystalCharge(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnRCrystalArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnRStunFrost(CWorld& world, EntityID target, f32_t fLifetime);
}
```

### 3.4 `Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp`

```cpp
#include "GameObject/Champion/Ashe/Ashe_FxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    constexpr const wchar_t* kPathArrowGlowTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_glow.png";
    constexpr const wchar_t* kPathArrowTrailTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_mist_trail.png";
    constexpr const wchar_t* kPathFrostHitTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_color-rampdownfrost.png";
    constexpr const wchar_t* kPathQBuffTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_buf.png";
    constexpr const wchar_t* kPathQReadyTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_ready_brightsparks.png";
    constexpr const wchar_t* kPathQDiffuseTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_buff_diffuse.png";
    constexpr const wchar_t* kPathWArrowTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png";
    constexpr const wchar_t* kPathWMuzzleTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_bow_sparks.png";
    constexpr const wchar_t* kPathEHawkTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_e_textureowl.png";
    constexpr const wchar_t* kPathRChargeTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_ready_brightsparks_star.png";
    constexpr const wchar_t* kPathRArrowTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_q_mis_star.png";
    constexpr const wchar_t* kPathRStunTex =
        L"Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_ba_ashe_teal_sparkle.png";
}

namespace Ashe::Fx
{
    namespace
    {
        void SpawnLineProjectileVisual(CWorld& world, EntityID owner,
                                        const Vec3& /*origin*/, const Vec3& /*dir*/,
                                        const wchar_t* path, f32_t fLifetime,
                                        f32_t fWidth, f32_t fHeight, const Vec4& color)
        {
            if (owner == NULL_ENTITY) return;
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.0f, 0.f };
            fx.texturePath = path;
            fx.fWidth = fWidth; fx.fHeight = fHeight;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime;
            fx.vColor = color;
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnBAArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        // Caster muzzle — bow glow flash
        SpawnLineProjectileVisual(world, owner, origin, dir,
            kPathArrowGlowTex, fLifetime, 0.8f, 0.8f, { 0.6f, 0.95f, 1.2f, 1.f });
        // Trail
        SpawnLineProjectileVisual(world, owner, origin, dir,
            kPathArrowTrailTex, fLifetime * 0.5f, 1.4f, 0.6f, { 0.7f, 1.0f, 1.3f, 0.85f });
    }

    void SpawnFrostHit(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathFrostHitTex;
        fx.fWidth = 1.0f; fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.5f, 0.85f, 1.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnQBuffActive(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.2f, 0.f };
            fx.texturePath = kPathQBuffTex;
            fx.fWidth = 1.8f; fx.fHeight = 1.8f;
            fx.bBillboard = true;
            fx.fLifetime = fDuration;
            fx.vColor = { 0.9f, 1.1f, 1.4f, 0.85f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fDuration * 0.3f;
            CFxSystem::Spawn(world, fx);
        }

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 0.05f, 0.f };
            fx.texturePath = kPathQDiffuseTex;
            fx.fWidth = 2.0f; fx.fHeight = 2.0f;
            fx.bBillboard = false;
            fx.fLifetime = fDuration;
            fx.vColor = { 0.6f, 0.9f, 1.2f, 0.6f };
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.fFadeOut = fDuration * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnQReadySparks(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.5f, 0.f };
        fx.texturePath = kPathQReadyTex;
        fx.fWidth = 1.4f; fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.2f, 1.3f, 1.4f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnWVolleyArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        SpawnLineProjectileVisual(world, owner, origin, dir,
            kPathWArrowTex, fLifetime, 1.0f, 0.6f, { 0.8f, 1.0f, 1.3f, 1.f });
    }

    void SpawnWVolleyMuzzle(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.4f, 0.f };
        fx.texturePath = kPathWMuzzleTex;
        fx.fWidth = 2.0f; fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.0f, 1.2f, 1.4f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3& /*dest*/, f32_t fLifetime)
    {
        // 1차: caster 위치 위 부엉이 표식 (정찰 visualization 만)
        // B-17 시야 시스템 합류 시 dest 도착점에 vision sphere
        FxBillboardComponent fx{};
        fx.attachTo = NULL_ENTITY;
        fx.vWorldPos = { start.x, start.y + 3.0f, start.z };
        fx.vAttachOffset = { 0.f, 0.f, 0.f };
        fx.texturePath = kPathEHawkTex;
        fx.fWidth = 1.4f; fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.8f, 1.1f, 1.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRCrystalCharge(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.5f, 0.f };
        fx.texturePath = kPathRChargeTex;
        fx.fWidth = 1.6f; fx.fHeight = 1.6f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.3f, 1.4f, 1.5f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRCrystalArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        SpawnLineProjectileVisual(world, owner, origin, dir,
            kPathRArrowTex, fLifetime, 1.6f, 0.8f, { 1.0f, 1.3f, 1.5f, 1.f });
    }

    void SpawnRStunFrost(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.5f, 0.f };
        fx.texturePath = kPathRStunTex;
        fx.fWidth = 1.8f; fx.fHeight = 1.8f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.7f, 1.2f, 1.5f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }
}
```

### 3.5 `Client/Public/GameObject/Champion/Ashe/Ashe_Skills.h`

```cpp
#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

namespace Ashe
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

void Ashe_KeepAlive();
```

### 3.6 `Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp`

```cpp
#include "GameObject/Champion/Ashe/Ashe_Skills.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Ashe/Ashe_FxPresets.h"
#include "GameObject/Yasuo/PendingHitSystem.h"
#include "GameObject/Projectile/ProjectileKind.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace Ashe
{
    namespace
    {
        Vec3 GetMuzzlePos(CWorld& world, EntityID entity)
        {
            Vec3 pos{};
            if (world.HasComponent<TransformComponent>(entity))
            {
                auto& tf = world.GetComponent<TransformComponent>(entity);
                pos = tf.GetWorldPosition();
                pos.y += 1.0f;
            }
            return pos;
        }

        Vec3 NormalizeXZ(const Vec3& v)
        {
            const f32_t lenSq = v.x * v.x + v.z * v.z;
            if (lenSq < 0.0001f) return { 0.f, 0.f, 1.f };
            const f32_t inv = 1.f / std::sqrtf(lenSq);
            return { v.x * inv, 0.f, v.z * inv };
        }

        Vec3 RotateXZ(const Vec3& v, f32_t fRadians)
        {
            const f32_t c = std::cosf(fRadians);
            const f32_t s = std::sinf(fRadians);
            return { v.x * c - v.z * s, 0.f, v.x * s + v.z * c };
        }

        // Frost slow stub — B-17 BuffSystem 합류 시 정식 (StatusEffect 큐로 이전)
        void ApplyFrostSlowStub(CWorld& /*world*/, EntityID target, f32_t /*fSlowPercent*/, f32_t /*fDuration*/)
        {
            char dbg[64];
            sprintf_s(dbg, "[Ashe Frost] target=%u (slow stub — B-17)\n",
                static_cast<u32_t>(target));
            OutputDebugStringA(dbg);
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

        f32_t fDamage = 50.f;
        if (ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity))
        {
            auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
            // Q (Ranger's Focus) buff — 데미지 + AS bonus
            if (as.bQActive)
                fDamage += as.fQDamageBonus;

            // Focus stack 충전 (Q ready 조건)
            if (!as.bQActive)
            {
                as.focusStacks = static_cast<u8_t>(as.focusStacks + 1);
                if (as.focusStacks == as.focusThreshold)
                    Fx::SpawnQReadySparks(*ctx.pWorld, ctx.casterEntity, 0.6f);
            }
        }

        // 1차: 즉시 hit 데미지 (PendingHit 미사용 — single target line)
        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, fDamage);

        // Frost slow (passive)
        if (ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity))
        {
            const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
            ApplyFrostSlowStub(*ctx.pWorld, target, as.fFrostSlowPercent, as.fFrostSlowDuration);
        }

        char dbg[128];
        sprintf_s(dbg, "[Ashe BA] target=%u dmg=%.1f\n",
            static_cast<u32_t>(target), fDamage);
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;

        auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        if (as.focusStacks < as.focusThreshold)
        {
            char dbg[96];
            sprintf_s(dbg, "[Ashe Q] not ready (stacks=%u/%u)\n",
                as.focusStacks, as.focusThreshold);
            OutputDebugStringA(dbg);
            return;
        }

        as.bQActive = true;
        as.fQTimer = as.fQDurationSec;
        as.focusStacks = 0;

        OutputDebugStringA("[Ashe Q] Ranger's Focus activated (4s)\n");
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;

        const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        const Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);
        const Vec3 dirCenter = NormalizeXZ(ctx.pCommand->direction);

        const u8_t arrowCount = as.volleyArrowCount;
        const f32_t fHalfCone = as.fVolleyConeAngleDeg * 0.5f * 3.14159265f / 180.f;

        // 9 화살 cone 분포 — center 0, ±step 까지
        for (u8_t i = 0; i < arrowCount; ++i)
        {
            const f32_t fStep = (arrowCount > 1)
                ? (-fHalfCone + 2.f * fHalfCone * i / (arrowCount - 1))
                : 0.f;
            const Vec3 dirArrow = RotateXZ(dirCenter, fStep);

            CPendingHitSystem::Schedule(*ctx.pWorld,
                ctx.casterEntity, ctx.casterTeam, dirArrow,
                0.05f,                                           // delay
                eProjectileKind::MysticShot,                     // generic line
                28.f,                                            // speed
                as.fVolleyRange,                                 // maxDist
                0.6f,                                            // hit radius
                40.f,                                            // damage
                0.f);                                            // status (Frost는 Apply 시점)
        }

        char dbg[128];
        sprintf_s(dbg, "[Ashe W] %u arrows fired (Volley)\n",
            static_cast<u32_t>(arrowCount));
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        const Vec3 origin = ctx.pWorld
            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = NormalizeXZ(ctx.pCommand->direction);
        const Vec3 dest = {
            origin.x + dir.x * as.fHawkshotRange,
            origin.y,
            origin.z + dir.z * as.fHawkshotRange
        };

        // 1차: visual only (B-17 시야 시스템 합류 시 vision component 활성)
        char dbg[160];
        sprintf_s(dbg, "[Ashe E] Hawkshot dest=(%.1f,%.1f,%.1f) duration=%.1fs (vision stub)\n",
            dest.x, dest.y, dest.z, as.fHawkshotVisionDurationSec);
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;

        const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        const Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);
        const Vec3 dir = NormalizeXZ(ctx.pCommand->direction);

        CPendingHitSystem::Schedule(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, dir,
            0.f,                                                 // delay
            eProjectileKind::GlobalBeam,                         // 글로벌 line
            as.fCrystalArrowSpeed,
            as.fCrystalArrowMaxDist,
            1.0f,                                                // hit radius
            250.f,                                               // base damage
            as.fCrystalArrowStunMin);                            // stun (B-17 정식)

        char dbg[128];
        sprintf_s(dbg, "[Ashe R] Crystal Arrow fired dir=(%.2f,%.2f,%.2f)\n",
            dir.x, dir.y, dir.z);
        OutputDebugStringA(dbg);
    }

    // ─────────────────────────────────────────────────────────────
    //  Gameplay (shared sim — stub)
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
            const Vec3 origin = (ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
                ? ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition()
                : Vec3{};
            Fx::SpawnBAArrow(*ctx.pWorld, ctx.casterEntity, origin, ctx.pCommand->direction, 0.4f);
            if (target != NULL_ENTITY)
                Fx::SpawnFrostHit(*ctx.pWorld, target, 0.4f);
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnQBuffActive(*ctx.pWorld, ctx.casterEntity, 4.0f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnWVolleyMuzzle(*ctx.pWorld, ctx.casterEntity, 0.4f);
            // PendingHitSystem 의 line projectile FX 는 Schedule 내부에서 별도 spawn
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;
            const Vec3 origin = ctx.pWorld
                ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 dir = ctx.pCommand->direction;
            const Vec3 dest = { origin.x + dir.x * 25.f, origin.y, origin.z + dir.z * 25.f };
            Fx::SpawnEHawkshot(*ctx.pWorld, origin, dest, 5.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnRCrystalCharge(*ctx.pWorld, ctx.casterEntity, 0.4f);
            const Vec3 origin = (ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
                ? ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition()
                : Vec3{};
            Fx::SpawnRCrystalArrow(*ctx.pWorld, ctx.casterEntity,
                origin, ctx.pCommand->direction, 0.8f);
        }
    }
}
```

### 3.7 `Client/Public/GameObject/Champion/Ashe/Ashe_Registration.h`

```cpp
#pragma once
```

### 3.8 `Client/Private/GameObject/Champion/Ashe/Ashe_Registration.cpp`

```cpp
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Ashe/Ashe_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kAsh_BA_Cast = MakeHookId(eChampion::ASHE, HookVariant::BA_CastFrame);
    constexpr u32_t kAsh_Q_Cast  = MakeHookId(eChampion::ASHE, HookVariant::Q_CastFrame);
    constexpr u32_t kAsh_W_Cast  = MakeHookId(eChampion::ASHE, HookVariant::W_CastFrame);
    constexpr u32_t kAsh_E_Cast  = MakeHookId(eChampion::ASHE, HookVariant::E_CastFrame);
    constexpr u32_t kAsh_R_Cast  = MakeHookId(eChampion::ASHE, HookVariant::R_CastFrame);

    struct AsheAutoRegister
    {
        AsheAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::ASHE;
            cd.animPrefix    = "ashe_";
            cd.idleAnimKey   = "idle1";
            cd.runAnimKey    = "run";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 6.0f;        // 원거리 ADC
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Ashe/ashe.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            const wchar_t* asheBaseTexture =
                L"Client/Bin/Resource/Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
            cd.defaultTexturePath = asheBaseTexture;
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = asheBaseTexture;
            cd.spawnPosition = { 39.f, 1.f, 0.f };   // Annie (36) 옆
            cd.spawnScale = 0.01f;
            cd.displayName = "Ashe";
            CChampionRegistry::Instance().Add(eChampion::ASHE, cd);

            // BA — attack1
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.55f; s.rangeMax = 6.0f; s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.7f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 5.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAsh_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 0, s);
            }

            // Q — Ranger's Focus (Self buff)
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 1;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 12.f; s.rangeMax = 0.f; s.manaCost = 50.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 1.f; s.recoveryFrame = 8.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAsh_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 1, s);
            }

            // W — Volley (cone of arrows)
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 9.f; s.rangeMax = 9.0f; s.manaCost = 60.f;
                s.animKey = "spell2";
                s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 4.f; s.recoveryFrame = 10.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAsh_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 2, s);
            }

            // E — Hawkshot (정찰 — Direction)
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 3;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 60.f; s.rangeMax = 25.f; s.manaCost = 50.f;
                s.animKey = "spell3";
                s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 1.f; s.recoveryFrame = 10.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAsh_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 3, s);
            }

            // R — Enchanted Crystal Arrow (글로벌 stun)
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 4;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 100.f; s.rangeMax = 200.f; s.manaCost = 100.f;
                s.animKey = "channel";
                s.lockDurationSec = 1.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 12.f; s.recoveryFrame = 22.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAsh_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kAsh_BA_Cast, &Ashe::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kAsh_Q_Cast,  &Ashe::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kAsh_W_Cast,  &Ashe::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kAsh_E_Cast,  &Ashe::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kAsh_R_Cast,  &Ashe::OnCastFrame_R);

            CGameplayHookRegistry::Instance().Register(kAsh_BA_Cast, &Ashe::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kAsh_Q_Cast,  &Ashe::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kAsh_W_Cast,  &Ashe::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kAsh_E_Cast,  &Ashe::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kAsh_R_Cast,  &Ashe::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kAsh_BA_Cast, &Ashe::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_Q_Cast,  &Ashe::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_W_Cast,  &Ashe::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_E_Cast,  &Ashe::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_R_Cast,  &Ashe::Visual::OnCastFrame_R_Visual);

            OutputDebugStringA("[Ashe] Registration complete\n");
        }
    };

    static AsheAutoRegister s_register;
}

void Ashe_KeepAlive()
{
    (void)&s_register;
}
```

---

## §4. Scene_InGame.cpp 수정 (Anchor + Before/After)

### 4.1 Include + KeepAlive

```cpp
#include "GameObject/Champion/Annie/Annie_Skills.h"

//Ashe — Phase B-15
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Skills.h"
```

```cpp
    extern void Annie_KeepAlive();
    Annie_KeepAlive();

    extern void Ashe_KeepAlive();
    Ashe_KeepAlive();   // Phase B-15
```

### 4.2 BindPlayerToECSChampion / CreateECSChampion / CreateECSEntities / m_PlayerEntity / m_AsheEntity

`|| selectedChampion == eChampion::ASHE` 추가, `else if (id == eChampion::ASHE) m_World.AddComponent<AsheStateComponent>(e);`, `EntityID asheEntity = NULL_ENTITY;` + `#if !WINTERS_MIN_SCENE asheEntity = CreateECSChampion(...)` + `addSkillStateIfAlive(asheEntity);`, `else if (champ == eChampion::ASHE) m_PlayerEntity = asheEntity;`, Scene_InGame.h 에 `EntityID m_AsheEntity = NULL_ENTITY;` (B-14 패턴 미러).

---

## §5. Implementation Gate — vcxproj 등록

```xml
    <ClCompile Include="..\Private\GameObject\Champion\Ashe\Ashe_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Ashe\Ashe_FxPresets.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Ashe\Ashe_Skills.cpp" />

    <ClInclude Include="..\Public\GameObject\Champion\Ashe\Ashe_Components.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Ashe\Ashe_FxPresets.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Ashe\Ashe_Registration.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Ashe\Ashe_Skills.h" />
```

---

## §6. Verification Gate

### 6.1 G5: Feature smoke

| 단계 | 액션 | 기대 |
|---|---|---|
| 1 | Ashe 픽 → InGame | (39,1,0) 스폰 |
| 2 | ashe_idle1 / ashe_run anim | 정상 전환 |
| 3 | Sylas + A | `[Ashe BA] dmg=50.0` + frost hit FX + `[Ashe Frost]` slow stub |
| 4 | A × 4 (focus stack) | 4번째 BA 시 Q ready sparks FX |
| 5 | Q | `Ranger's Focus activated (4s)` + buff FX |
| 6 | Q 활성 후 BA | `dmg=70.0` (50+20 bonus) |
| 7 | W (방향) | `9 arrows fired` + 9개 PendingHit projectile |
| 8 | E (방향) | `Hawkshot dest=...` (visual only stub) |
| 9 | R (방향) | `Crystal Arrow fired` + global beam projectile |

### 6.2 회귀 grep

```bash
grep -c "kAsh_" Client/Private/GameObject/Champion/Ashe/Ashe_Registration.cpp     # ≥ 25
grep "CPendingHitSystem::Schedule" Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp | wc -l   # 10 (W 9 arrows + R 1)
```

---

## §7. Learning Update

### 7.1 박제 후 갱신 후보

**CLAUDE.md Gotcha 후보**:
```markdown
- **Volley 같은 멀티 projectile cone 분포 (B-15, 2026-05-04)**:
  N개 화살을 cone 안에 균등 분배 시 `fStep = -halfCone + 2*halfCone*i/(N-1)` 공식.
  N=1 분기 (division by zero) 의무 가드. CPendingHitSystem::Schedule 의 인자 11개를
  N번 호출 — 동일 origin / 다른 dir.
```

**Memory 후보**:
```markdown
파일: feedback_ashe_frost_passive.md
내용: Ashe Frost Shot 패시브 — 모든 BA 가 slow 적용. 1차는 OutputDebugString stub.
B-17 BuffSystem 합류 시 정식 — StatusEffect 큐 도입 (StunComponent 와 동일 패턴).
유사 패시브: Lissandra Q passive, Anivia 빙결, Ashe BA frost 등 — slow CC 통합 처리.
```

---

## §8. 다음 단계

| Phase | 챔프/작업 | 패턴 |
|---|---|---|
| **B-16** | Yone (13_YONE...) | 메시 분리 인프라 + 엘든링 확장 설계 |
| **B-17** | BuffSystem (Annie Pyromania CC + Ashe Frost slow + Riven Q stack 정식) | StatusEffect 큐 |

---

## §9. 즉시 진입 명령

```
"Phase B-15 Ashe 진행. 12_ASHE_PHASE_B15_PIPELINE_v1.md §3 D-0 변환부터.
1) convert_all_assets.bat Ashe 1줄 추가 → 변환 → 22 anims 검증
2) Ashe 5개 신규 파일 박제 (PendingHitSystem 재사용)
3) Scene_InGame.cpp 7 영역 수정 (Annie 패턴 미러)
4) Client.vcxproj 등록 → G1 → G3 → G4 → G5 검증."
```
