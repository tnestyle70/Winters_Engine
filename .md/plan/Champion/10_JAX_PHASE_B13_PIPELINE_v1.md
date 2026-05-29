# Phase B-13 (Jax) — 멀티 메시 챔프 + WINTERS_MIN_SCENE 강화 (Sylas + Jax 만 active)

**작성일**: 2026-05-04
**선행**: Phase B-12 v2 Fiora 박제 완료 (Client.vcxproj 빌드 ✅, 8초 smoke ✅)
**Framework**: 본 계획서는 [`BUILD_INTEGRITY_FRAMEWORK.md`](../../architecture/BUILD_INTEGRITY_FRAMEWORK.md) v1 의 8 섹션 템플릿 첫 적용본
**목표**:
1. Jax 1체 추가 — Ezreal 패턴 100% 미러 (3-Layer Hook). 멀티 텍스처 (body/fish/weapon 3개) 처리.
2. **WINTERS_MIN_SCENE 강화** — Sylas (적, Red) + Jax (Player, Blue) 만 active. Fiora/Zed/Riven/Ezreal/Irelia/Yasuo/Kalista/Garen/Viego 모두 가드 안으로.
3. **v1 잔존 cleanup** — Fiora 의 ChampionTable.cpp/SkillTable.cpp 정적 entry + `FioraFxPresets.h/cpp` (no underscore) 제거.

---

## §0. Agent Contract Evidence — 도구 로그 박제

### 0.1 본 계획서 작성 시점 도구 호출 (실측)

| 도구 | 호출 수 | 핵심 명령 |
|---|---|---|
| `Bash ls` | 8회 | `Character/Jax/animations/`, `Character/Jax/`, `Champion/Fiora/`, `Champion/`, `Champion/Fiora/Private/` |
| `Grep` | 5회 | `eChampion::FIORA`, `WINTERS_MIN_SCENE`, `fioraEntity\|FIORA\|m_Zed\|CreateECSChampion` (Scene_InGame), `Fiora_KeepAlive\|FioraStateComponent` (3 파일) |
| `Read` | 2회 | `Fiora_Components.h`, `Fiora_Registration.cpp:1-25` (Fiora 박제 검증) |
| `Bash find` | 1회 | `_MODULE.md` (Compass 미적용 확인) |

### 0.2 검증된 사실

- **Fiora 박제 상태**: v1 (ChampionTable + SkillTable) + v2 (Fiora_Registration.cpp) **혼합** — `CChampionRegistry::Instance().Add(FIORA, ...)` 가 두 path 에서 호출됨. 현재 동작은 정상 (둘째 Add 가 overwrite 또는 reject) — 단 cleanup 필요.
- **Zed 가 MIN_SCENE 미가드**: `Scene_InGame.cpp` L309-316 / L353-354 / L802 / L920 / L1836 / L2045 / L2109 / L2208 — 모두 `#if !WINTERS_MIN_SCENE` 외부. v2 계획서 L7.2-L7.9 가 MIN_SCENE 가드 강화 항목이었으나 미적용.
- **Compass System 미적용**: `_MODULE.md` 카운트 0. Encyclopedia (grep+Read) 모델 사용 중.
- **Jax 자원 인벤토리**: fbx 1, skl/skn 1쌍, 텍스처 3 (`body/fish/weapon`), animations 49 .anm, particles 200+. wmesh/wskel 미생성 (D-0 변환 필요).

---

## §1. Preflight Evidence Table

| 항목 | 결과 | 명령/위치 |
|---|---|---|
| **Read 한 파일** | 2 (Fiora 박제 검증), 18+ (B-12 v2 세션 누적) | `Fiora_Registration.cpp:1-25` |
| **Grep 패턴** | 5회 | `Grep "WINTERS_MIN_SCENE\|fioraEntity\|FIORA\|m_Zed\|CreateECSChampion" Scene_InGame.cpp` |
| **발견한 기존 인프라** | Ezreal 패턴 (Hook 3-layer) 검증 완료, Fiora 가 동일 패턴 + v1 잔존 동거 | Fiora h/cpp 7 파일 박제 + ChampionTable/SkillTable 잔존 entry |
| **현재 API 시그니처** | `void OnCastFrame_BA(SkillHookContext& ctx)` / `void OnCastFrame_BA(GameplayHookContext& ctx)` / `void OnCastFrame_BA_Visual(VisualHookContext& ctx)` | `Client/Public/GamePlay/SkillHookContext.h:19-29` (B-12 v2 Read) |
| **v1 / 중복 파일 존재** | `FioraFxPresets.h/cpp` (no underscore) + `Fiora_FxPresets.h/cpp` (underscore) 동거 / ChampionTable.cpp:18 FIORA / SkillTable.cpp:305-345 5 entries | `ls Client/Public/GameObject/Champion/Fiora/` |
| **Hook context 필드** | SkillHookContext: pWorld, casterEntity, casterTeam, pDef, pCommand, fDeltaTime, pKeyOut, pFxMeshRenderer | (B-12 v2 Read 인용) |
| **Asset 경로 실존** | `jax.fbx` ✅ / `jax_base_body_tx_cm.png` ✅ / `jax_base_fish_tx_cm.png` ✅ / `jax_base_weapon_tx_cm.png` ✅ / `jax.wmesh` ❌ (D-0 필요) / `animations/*.anm` 49개 ✅ | `Bash ls "Character/Jax/"` |
| **빌드 가드 위치** | `WINTERS_MIN_SCENE` 22+ hit (현재 Zed/Fiora/Riven/Ezreal 외부) | `Grep -n "WINTERS_MIN_SCENE" Scene_InGame.cpp` |
| **Jax 애니 키 (실측 — animPrefix 없음)** | idle: `idle1_v04` / run: `jax_run2` / BA: `attack_1` / Q: `spell1` / W: `spell2_v03` / E: `spell3_attack1` / R: `spell4_idle` | `ls Character/Jax/animations/` 49 .anm |

---

## §2. Plan Quality Gate Status

- [x] **Full code or diff hunk**: 신규 7 파일 전문 박제 (§3) + Scene_InGame.cpp 7 영역 anchor + before/after hunk (§4)
- [x] **No placeholder**: Gameplay namespace 의 `(void)ctx;` 는 의도 stub (이유: 04a v2 D-1 server transport 진입 prerequisite — Ezreal 와 동일 박제). 기타 placeholder 0
- [x] **Hook context fields verified**: SkillHookContext / VisualHookContext / GameplayHookContext 모든 필드 실측 인용
- [x] **Asset paths Test-Path verified**: §1 표 8행 실측
- [x] **vcxproj/filters registration**: §5 박제

---

## §3. 신규 파일 7개 — 전문 박제

### 3.1 자원 변환 (D-0)

**파일**: `Tools/convert_all_assets.bat:33` (Fiora 다음)

```bat
call :convert_champ "Riven" "riven.fbx"
call :convert_champ "Ezreal" "ezreal.fbx"
call :convert_champ "Fiora" "fiora.fbx"
call :convert_champ "Jax" "jax.fbx"
```

실행:
```cmd
.\Tools\convert_all_assets.bat champions
```

검증: `Client/Bin/Resource/Texture/Character/Jax/jax.wmesh` + `jax.wskel` + `anims/*.wanim` 49개.

### 3.2 `Client/Public/GameObject/Champion/Jax/Jax_Components.h`

```cpp
#pragma once

#include "Engine_Defines.h"

// Jax — Phase B-13
// W (Empower) 다음 BA 강화, E (Counter Strike) 패리/스턴, R (Grandmaster's Might) 버프 + 3타 AOE
struct JaxStateComponent
{
    // W — Empower (다음 BA 마법 데미지 추가)
    bool   bEmpowerActive = false;
    f32_t  fEmpowerTimer  = 0.f;
    f32_t  fEmpowerWindowSec  = 10.f;
    f32_t  fEmpowerDamageBonus = 40.f;

    // E — Counter Strike (B-14 정식 parry, 1차는 단순 AOE slash)
    bool   bCounterActive = false;
    f32_t  fCounterTimer  = 0.f;
    f32_t  fCounterWindowSec = 2.0f;

    // R — Grandmaster's Might (passive buff + 3타 AOE)
    bool   bUltActive = false;
    f32_t  fUltTimer  = 0.f;
    f32_t  fUltDurationSec = 8.f;
    u8_t   ultAttackCounter = 0;     // BA 카운터 (3 도달 시 AOE)
    f32_t  fUltAOEDamage = 100.f;
    f32_t  fUltAOERadius = 2.0f;
};
```

### 3.3 `Client/Public/GameObject/Champion/Jax/Jax_FxPresets.h`

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

namespace Jax::Fx
{
    // BA — 무기 hit flash (target 위치)
    void SpawnBAHitFlash(CWorld& world, EntityID target, f32_t fLifetime);

    // Q — Leap Strike: 도약 트레일 (caster, 짧은 fade)
    void SpawnQLeapTrail(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);

    // W — Empower: 무기 글로우 활성 (caster, fDuration 지속)
    void SpawnWEmpowerGlow(CWorld& world, EntityID owner, f32_t fDuration);

    // E — Counter Strike: 카운터 슬래시 AOE 표식 (caster 발 밑)
    void SpawnECounterSlash(CWorld& world, EntityID owner, f32_t fLifetime);

    // R — Grandmaster's Might: 버프 활성 오라 (caster, 8s)
    void SpawnRBuffAura(CWorld& world, EntityID owner, f32_t fDuration);

    // R 3타 AOE — 3타 BA 시 caster 주변 AOE 폭발
    void SpawnRThirdAttackAOE(CWorld& world, EntityID owner, f32_t fLifetime);
}
```

### 3.4 `Client/Private/GameObject/Champion/Jax/Jax_FxPresets.cpp`

```cpp
#include "GameObject/Champion/Jax/Jax_FxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    constexpr const wchar_t* kPathBaHitFlashTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_hit_flash.png";
    constexpr const wchar_t* kPathQTrailTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_trail01.png";
    constexpr const wchar_t* kPathWGlowTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_sparks.png";
    constexpr const wchar_t* kPathESlashTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_flame_01.png";
    constexpr const wchar_t* kPathRAuraTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_gradient_rgb.png";
    constexpr const wchar_t* kPathRAOETex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_flame_01.png";
}

namespace Jax::Fx
{
    void SpawnBAHitFlash(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathBaHitFlashTex;
        fx.fWidth = 1.4f; fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.2f, 1.0f, 0.6f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.45f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnQLeapTrail(CWorld& world, EntityID owner, const Vec3&, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathQTrailTex;
        fx.fWidth = 2.0f; fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.85f, 0.95f, 1.1f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnWEmpowerGlow(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.4f, 0.f };
        fx.texturePath = kPathWGlowTex;
        fx.fWidth = 1.6f; fx.fHeight = 1.6f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.1f, 0.7f, 1.3f, 0.9f };  // 보라끼 (W = magic)
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fDuration * 0.3f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnECounterSlash(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 0.05f, 0.f };
        fx.texturePath = kPathESlashTex;
        fx.fWidth = 3.0f; fx.fHeight = 3.0f;
        fx.bBillboard = false;
        fx.fYaw = 0.f;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.3f, 0.5f, 0.2f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRBuffAura(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.2f, 0.f };
        fx.texturePath = kPathRAuraTex;
        fx.fWidth = 2.4f; fx.fHeight = 2.4f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.2f, 0.6f, 0.9f, 0.85f };  // 보라/마젠타
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fDuration * 0.25f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRThirdAttackAOE(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 0.05f, 0.f };
        fx.texturePath = kPathRAOETex;
        fx.fWidth = 4.0f; fx.fHeight = 4.0f;
        fx.bBillboard = false;
        fx.fYaw = 0.f;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.4f, 0.4f, 0.7f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }
}
```

### 3.5 `Client/Public/GameObject/Champion/Jax/Jax_Skills.h`

```cpp
#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

namespace Jax
{
    // ── Skill (client cast 로직 — local prediction) ──
    void OnCastFrame_BA(SkillHookContext& ctx);
    void OnCastFrame_Q(SkillHookContext& ctx);
    void OnCastFrame_W(SkillHookContext& ctx);
    void OnCastFrame_E(SkillHookContext& ctx);
    void OnCastFrame_R(SkillHookContext& ctx);

    // ── Gameplay (shared sim — server + client 권위 시뮬, 04a v2 D-1 진입 시 본격) ──
    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx);
        void OnCastFrame_Q(GameplayHookContext& ctx);
        void OnCastFrame_W(GameplayHookContext& ctx);
        void OnCastFrame_E(GameplayHookContext& ctx);
        void OnCastFrame_R(GameplayHookContext& ctx);
    }

    // ── Visual (client 전용 FX/Sound) ──
    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx);
        void OnCastFrame_Q_Visual(VisualHookContext& ctx);
        void OnCastFrame_W_Visual(VisualHookContext& ctx);
        void OnCastFrame_E_Visual(VisualHookContext& ctx);
        void OnCastFrame_R_Visual(VisualHookContext& ctx);
    }
}

void Jax_KeepAlive();
```

### 3.6 `Client/Private/GameObject/Champion/Jax/Jax_Skills.cpp`

```cpp
#include "GameObject/Champion/Jax/Jax_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Jax/Jax_FxPresets.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace Jax
{
    namespace
    {
        EntityID FindNearestEnemyInRadius(CWorld& world, EntityID caster, eTeam casterTeam,
                                          const Vec3& origin, f32_t fRadius)
        {
            EntityID best = NULL_ENTITY;
            f32_t fBestDistSq = FLT_MAX;
            world.ForEach<ChampionComponent, TransformComponent>(
                [&](EntityID e, ChampionComponent& cc, TransformComponent& tf)
                {
                    if (e == caster) return;
                    if (cc.team == casterTeam) return;
                    const Vec3 v = tf.GetPosition();
                    const f32_t dx = v.x - origin.x;
                    const f32_t dz = v.z - origin.z;
                    const f32_t fDistSq = dx * dx + dz * dz;
                    if (fDistSq > fRadius * fRadius) return;
                    if (fDistSq < fBestDistSq)
                    {
                        fBestDistSq = fDistSq;
                        best = e;
                    }
                });
            return best;
        }

        u32_t ApplyAOEDamageInRadius(CWorld& world, EntityID caster, eTeam casterTeam,
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

        // W (Empower) — 다음 BA 데미지 강화
        if (ctx.pWorld->HasComponent<JaxStateComponent>(ctx.casterEntity))
        {
            auto& js = ctx.pWorld->GetComponent<JaxStateComponent>(ctx.casterEntity);
            if (js.bEmpowerActive)
            {
                fDamage += js.fEmpowerDamageBonus;
                js.bEmpowerActive = false;
                js.fEmpowerTimer = 0.f;
            }

            // R (Grandmaster's Might) — 3타 마다 AOE
            if (js.bUltActive)
            {
                js.ultAttackCounter = static_cast<u8_t>(js.ultAttackCounter + 1);
                if (js.ultAttackCounter >= 3)
                {
                    js.ultAttackCounter = 0;
                    if (ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
                    {
                        const Vec3 vOrigin = ctx.pWorld
                            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
                        const u32_t hits = ApplyAOEDamageInRadius(*ctx.pWorld,
                            ctx.casterEntity, ctx.casterTeam,
                            vOrigin, js.fUltAOERadius, js.fUltAOEDamage);

                        char dbg[128];
                        sprintf_s(dbg, "[Jax R AOE] hits=%u dmg=%.1f\n",
                            hits, js.fUltAOEDamage);
                        OutputDebugStringA(dbg);
                    }
                }
            }
        }

        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, fDamage);

        char dbg[128];
        sprintf_s(dbg, "[Jax BA] target=%u dmg=%.1f\n",
            static_cast<u32_t>(target), fDamage);
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        auto& tf = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 vOrigin = tf.GetPosition();

        // Leap Strike — UnitTarget (적이 있으면 도약)
        const EntityID target = ctx.pCommand->targetEntityId;
        Vec3 vDest = vOrigin;
        if (target != NULL_ENTITY
            && ctx.pWorld->HasComponent<TransformComponent>(target))
        {
            const Vec3 vTarget = ctx.pWorld
                ->GetComponent<TransformComponent>(target).GetPosition();
            const f32_t dx = vTarget.x - vOrigin.x;
            const f32_t dz = vTarget.z - vOrigin.z;
            const f32_t fLen = std::sqrtf(dx * dx + dz * dz);
            if (fLen > 0.001f)
            {
                // target 1.0m 직전까지 도약
                const f32_t fGap = 1.0f;
                const f32_t fMove = (fLen > fGap) ? (fLen - fGap) : 0.f;
                vDest = {
                    vOrigin.x + (dx / fLen) * fMove,
                    vOrigin.y,
                    vOrigin.z + (dz / fLen) * fMove
                };
                tf.SetPosition(vDest);
                tf.m_bLocalDirty = true;
                tf.m_bWorldDirty = true;
            }

            // 도약 후 hit
            ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, 70.f);
        }

        char dbg[160];
        sprintf_s(dbg, "[Jax Q] origin=(%.1f,%.1f,%.1f) dest=(%.1f,%.1f,%.1f) target=%u\n",
            vOrigin.x, vOrigin.y, vOrigin.z, vDest.x, vDest.y, vDest.z,
            static_cast<u32_t>(target));
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<JaxStateComponent>(ctx.casterEntity)) return;

        auto& js = ctx.pWorld->GetComponent<JaxStateComponent>(ctx.casterEntity);
        js.bEmpowerActive = true;
        js.fEmpowerTimer  = js.fEmpowerWindowSec;

        OutputDebugStringA("[Jax W] Empower armed (next BA enhanced)\n");
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<JaxStateComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        auto& js = ctx.pWorld->GetComponent<JaxStateComponent>(ctx.casterEntity);
        const Vec3 vOrigin = ctx.pWorld
            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

        // 1차: 단순 AOE slash (B-14 에서 actual parry/stun detection)
        const u32_t hits = ApplyAOEDamageInRadius(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam,
            vOrigin, 1.8f, 60.f);

        js.bCounterActive = true;
        js.fCounterTimer  = js.fCounterWindowSec;

        char dbg[128];
        sprintf_s(dbg, "[Jax E] hits=%u counter armed\n", hits);
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<JaxStateComponent>(ctx.casterEntity)) return;

        auto& js = ctx.pWorld->GetComponent<JaxStateComponent>(ctx.casterEntity);
        js.bUltActive       = true;
        js.fUltTimer        = js.fUltDurationSec;
        js.ultAttackCounter = 0;

        OutputDebugStringA("[Jax R] Grandmaster's Might activated (8s)\n");
    }

    // ─────────────────────────────────────────────────────────────
    //  Gameplay (shared sim — 우선 stub, 04a v2 D-1 진입 시 본격)
    //  Ezreal 와 동일 패턴: Skill ns 의 ApplyDamage / TransformComponent 변경 / state
    //  변경을 본 namespace 로 이관 → server/client 양측에서 권위 시뮬.
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
            Fx::SpawnBAHitFlash(*ctx.pWorld, target, 0.4f);

            // R 3타 시 AOE FX
            if (ctx.pWorld->HasComponent<JaxStateComponent>(ctx.casterEntity))
            {
                const auto& js = ctx.pWorld
                    ->GetComponent<JaxStateComponent>(ctx.casterEntity);
                if (js.bUltActive && js.ultAttackCounter == 0)
                    Fx::SpawnRThirdAttackAOE(*ctx.pWorld, ctx.casterEntity, 0.5f);
            }
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnQLeapTrail(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->direction, 0.4f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnWEmpowerGlow(*ctx.pWorld, ctx.casterEntity, 10.0f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnECounterSlash(*ctx.pWorld, ctx.casterEntity, 0.5f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnRBuffAura(*ctx.pWorld, ctx.casterEntity, 8.0f);
        }
    }
}
```

### 3.7 `Client/Public/GameObject/Champion/Jax/Jax_Registration.h`

```cpp
#pragma once
```

### 3.8 `Client/Private/GameObject/Champion/Jax/Jax_Registration.cpp`

```cpp
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Jax/Jax_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kJax_BA_Cast = MakeHookId(eChampion::JAX, HookVariant::BA_CastFrame);
    constexpr u32_t kJax_Q_Cast  = MakeHookId(eChampion::JAX, HookVariant::Q_CastFrame);
    constexpr u32_t kJax_W_Cast  = MakeHookId(eChampion::JAX, HookVariant::W_CastFrame);
    constexpr u32_t kJax_E_Cast  = MakeHookId(eChampion::JAX, HookVariant::E_CastFrame);
    constexpr u32_t kJax_R_Cast  = MakeHookId(eChampion::JAX, HookVariant::R_CastFrame);

    struct JaxAutoRegister
    {
        JaxAutoRegister()
        {
            // ── ChampionDef ──
            // Jax animations 가 prefix 없는 형태 (idle1_v04, jax_run2, attack_1, spell1, ...)
            // animPrefix="" + 각 키 fully qualified
            ChampionDef cd{};
            cd.id = eChampion::JAX;
            cd.animPrefix    = "";
            cd.idleAnimKey   = "idle1_v04";
            cd.runAnimKey    = "jax_run2";
            cd.basicAttackKey = "attack_1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Jax/jax.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";

            // ── 멀티 메시 텍스처 (3 텍스처) ──
            // body / fish / weapon — 메시 슬롯 매핑은 첫 실행 시 Output 로그
            // ([CModel] meshes=N) 로 검증 후 조정.
            // 1차 가정: slot 0~2 body, slot 3 fish, slot 4 weapon, 5~7 body fallback.
            // defaultTexturePath 가 모든 슬롯 1차 로드 → texturePath[i] override.
            const wchar_t* jaxBodyTexture =
                L"Client/Bin/Resource/Texture/Character/Jax/jax_base_body_tx_cm.png";
            const wchar_t* jaxFishTexture =
                L"Client/Bin/Resource/Texture/Character/Jax/jax_base_fish_tx_cm.png";
            const wchar_t* jaxWeaponTexture =
                L"Client/Bin/Resource/Texture/Character/Jax/jax_base_weapon_tx_cm.png";

            cd.defaultTexturePath = jaxBodyTexture;
            cd.texturePath[0] = jaxBodyTexture;
            cd.texturePath[1] = jaxBodyTexture;
            cd.texturePath[2] = jaxBodyTexture;
            cd.texturePath[3] = jaxFishTexture;
            cd.texturePath[4] = jaxWeaponTexture;
            cd.texturePath[5] = jaxBodyTexture;
            cd.texturePath[6] = jaxBodyTexture;
            cd.texturePath[7] = jaxBodyTexture;

            cd.spawnPosition = { 33.f, 1.f, 0.f };   // Fiora (30) 옆
            cd.spawnScale = 0.01f;
            cd.displayName = "Jax";
            CChampionRegistry::Instance().Add(eChampion::JAX, cd);

            // ── BA — attack_1 ──
            // 부등식: 1.0 × 1.0 = 1.0 ≥ 14/24 = 0.583 ✅
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f; s.rangeMax = 1.5f; s.manaCost = 0.f;
                s.animKey = "attack_1";
                s.lockDurationSec = 1.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f; s.recoveryFrame = 14.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 0, s);
            }

            // ── Q — Leap Strike (spell1) ──
            // 부등식: 0.6 × 1.0 = 0.6 ≥ 12/24 = 0.5 ✅
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 1;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 5.f; s.rangeMax = 7.0f; s.manaCost = 0.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 1, s);
            }

            // ── W — Empower (spell2_v03) ──
            // 부등식: 0.5 × 1.0 = 0.5 ≥ 8/24 = 0.333 ✅
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 2;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 7.f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell2_v03";
                s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 1.f; s.recoveryFrame = 8.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 2, s);
            }

            // ── E — Counter Strike (spell3_attack1, 1차 단순 AOE slash) ──
            // 부등식: 0.7 × 1.0 = 0.7 ≥ 14/24 = 0.583 ✅
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 14.f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell3_attack1";
                s.lockDurationSec = 0.7f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 6.f; s.recoveryFrame = 14.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 3, s);
            }

            // ── R — Grandmaster's Might (spell4_idle, 짧은 활성 모션) ──
            // 부등식: 0.6 × 1.0 = 0.6 ≥ 12/24 = 0.5 ✅
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 4;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 80.f; s.rangeMax = 0.f; s.manaCost = 100.f;
                s.animKey = "spell4_idle";
                s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 4.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 4, s);
            }

            // ── Hook 등록 (3 layer) ──
            CSkillHookRegistry::Instance().Register(kJax_BA_Cast, &Jax::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kJax_Q_Cast,  &Jax::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kJax_W_Cast,  &Jax::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kJax_E_Cast,  &Jax::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kJax_R_Cast,  &Jax::OnCastFrame_R);

            CGameplayHookRegistry::Instance().Register(kJax_BA_Cast, &Jax::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kJax_Q_Cast,  &Jax::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kJax_W_Cast,  &Jax::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kJax_E_Cast,  &Jax::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kJax_R_Cast,  &Jax::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kJax_BA_Cast, &Jax::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kJax_Q_Cast,  &Jax::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kJax_W_Cast,  &Jax::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kJax_E_Cast,  &Jax::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kJax_R_Cast,  &Jax::Visual::OnCastFrame_R_Visual);

            OutputDebugStringA("[Jax] Registration complete\n");
        }
    };

    static JaxAutoRegister s_register;
}

void Jax_KeepAlive()
{
    (void)&s_register;
}
```

---

## §4. Scene_InGame.cpp 수정 (Anchor + Before/After hunk)

### 4.1 Include 추가

**Anchor**: L73 — `#include "GameObject/Champion/Riven/RivenFxPresets.h"`

**Before** (L73-L78):
```cpp
#include "GameObject/Champion/Riven/RivenFxPresets.h"

//Fiora — Phase B-12 v2
#include "GameObject/Champion/Fiora/Fiora_Components.h"
#include "GameObject/Champion/Fiora/Fiora_Skills.h"
```

**After**:
```cpp
#include "GameObject/Champion/Riven/RivenFxPresets.h"

//Fiora — Phase B-12 v2
#include "GameObject/Champion/Fiora/Fiora_Components.h"
#include "GameObject/Champion/Fiora/Fiora_Skills.h"

//Jax — Phase B-13
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Jax/Jax_Skills.h"
```

### 4.2 `Jax_KeepAlive()` 호출

**Anchor**: L222 — `Fiora_KeepAlive();`

**Before** (L221-L222):
```cpp
    extern void Fiora_KeepAlive();
    Fiora_KeepAlive();
```

**After**:
```cpp
    extern void Fiora_KeepAlive();
    Fiora_KeepAlive();

    extern void Jax_KeepAlive();
    Jax_KeepAlive();   // Phase B-13
```

### 4.3 Zed Init MIN_SCENE 가드 (B-12 v2 미적용분)

**Anchor**: L309 — `m_Zed.Init("...zed.fbx", ...);`

**Before** (L309-L316):
```cpp
    m_Zed.Init("Client/Bin/Resource/Texture/Character/Zed/zed.fbx",
        L"Shaders/Mesh3D.hlsl");
    m_Zed.LoadMeshTexture(0, L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png");
    m_Zed.LoadMeshTexture(1, L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png");
    m_ZedTransform.SetPosition(21.f, 1.f, 0.f);
    m_ZedTransform.SetScale(0.01f);
```

**After**:
```cpp
#if !WINTERS_MIN_SCENE
    m_Zed.Init("Client/Bin/Resource/Texture/Character/Zed/zed.fbx",
        L"Shaders/Mesh3D.hlsl");
    m_Zed.LoadMeshTexture(0, L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png");
    m_Zed.LoadMeshTexture(1, L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png");
    m_ZedTransform.SetPosition(21.f, 1.f, 0.f);
    m_ZedTransform.SetScale(0.01f);
#endif
```

### 4.4 Player 분기 Zed 가드

**Anchor**: L353 — `m_pPlayerRenderer = &m_Zed;`

**Before** (L351-L357):
```cpp
        else if (champ == eChampion::ZED)
        {
            m_pPlayerRenderer = &m_Zed;
            m_pPlayerTransform = &m_ZedTransform;
            m_pPlayerIdleAnim = "zed_idle1";
            m_pPlayerRunAnim = "zed_run";
        }
```

**After**:
```cpp
#if !WINTERS_MIN_SCENE
        else if (champ == eChampion::ZED)
        {
            m_pPlayerRenderer = &m_Zed;
            m_pPlayerTransform = &m_ZedTransform;
            m_pPlayerIdleAnim = "zed_idle1";
            m_pPlayerRunAnim = "zed_run";
        }
#endif
```

### 4.5 BindPlayerToECSChampion — JAX 추가

**Anchor**: L385 — `if (selectedChampion == eChampion::RIVEN || ... eChampion::FIORA)`

**Before** (L384-L390):
```cpp
    const eChampion selectedChampion = CGameInstance::Get()->Get_GameContext().SelectedChampion;
    if (selectedChampion == eChampion::RIVEN
        || selectedChampion == eChampion::EZREAL
        || selectedChampion == eChampion::FIORA)
    {
        BindPlayerToECSChampion(m_PlayerEntity);
    }
```

**After**:
```cpp
    const eChampion selectedChampion = CGameInstance::Get()->Get_GameContext().SelectedChampion;
    if (selectedChampion == eChampion::RIVEN
        || selectedChampion == eChampion::EZREAL
        || selectedChampion == eChampion::FIORA
        || selectedChampion == eChampion::JAX)
    {
        BindPlayerToECSChampion(m_PlayerEntity);
    }
```

### 4.6 `CreateECSChampion` — JaxStateComponent 분기 추가

**Anchor**: L726 — `else if (id == eChampion::FIORA)`

**Before** (L723-L727):
```cpp
    if (id == eChampion::RIVEN)
        m_World.AddComponent<RivenStateComponent>(e);
    else if (id == eChampion::EZREAL)
        m_World.AddComponent<EzrealStateComponent>(e);
    else if (id == eChampion::FIORA)
        m_World.AddComponent<FioraStateComponent>(e);
```

**After**:
```cpp
    if (id == eChampion::RIVEN)
        m_World.AddComponent<RivenStateComponent>(e);
    else if (id == eChampion::EZREAL)
        m_World.AddComponent<EzrealStateComponent>(e);
    else if (id == eChampion::FIORA)
        m_World.AddComponent<FioraStateComponent>(e);
    else if (id == eChampion::JAX)
        m_World.AddComponent<JaxStateComponent>(e);
```

### 4.7 `CreateECSEntities` — Jax 스폰 + Fiora/Riven/Ezreal/Zed MIN_SCENE 가드 강화

**Anchor**: L800 — `#if !WINTERS_MIN_SCENE` (Irelia 등 wrap 시작)

**Before** (L796-L876, 핵심):
```cpp
void CScene_InGame::CreateECSEntities()
{
#if !WINTERS_MIN_SCENE
    m_IreliaEntity = CreateChampionEntity(m_Irelia, m_IreliaTransform, eChampion::IRELIA, eTeam::Blue);
    m_KalistaEntity = ...
    m_GarenEntity = ...
#endif
    m_ZedEntity = CreateChampionEntity(m_Zed, m_ZedTransform, eChampion::ZED, eTeam::Blue);

    EntityID rivenEntity = CreateECSChampion(eChampion::RIVEN, eTeam::Blue);
    EntityID ezrealEntity = CreateECSChampion(eChampion::EZREAL, eTeam::Blue);
    EntityID fioraEntity = CreateECSChampion(eChampion::FIORA, eTeam::Blue);
    m_FioraEntity = fioraEntity;

    m_SylasEntity = CreateChampionEntity_FromBlueprint(L"Sylas", m_Sylas, m_SylasTransform);
    ...

#if !WINTERS_MIN_SCENE
    if (m_YasuoEntity != NULL_ENTITY) ...
    addSkillStateIfAlive(m_YasuoEntity);
    addSkillStateIfAlive(m_IreliaEntity);
    addSkillStateIfAlive(m_KalistaEntity);
    addSkillStateIfAlive(m_GarenEntity);
#endif
    addSkillStateIfAlive(m_ZedEntity);

#if !WINTERS_MIN_SCENE
    m_ViegoEntity = ...
#endif

    using namespace Engine;
    eChampion champ = ...;
#if !WINTERS_MIN_SCENE
    if (champ == eChampion::IRELIA) m_PlayerEntity = m_IreliaEntity;
    else if (champ == eChampion::YASUO)  m_PlayerEntity = m_YasuoEntity;
    else if (champ == eChampion::KALISTA) m_PlayerEntity = m_KalistaEntity;
    else if (champ == eChampion::GAREN) m_PlayerEntity = m_GarenEntity;
    else
#endif
    if (champ == eChampion::ZED) m_PlayerEntity = m_ZedEntity;
    if (champ == eChampion::RIVEN) m_PlayerEntity = rivenEntity;
    if (champ == eChampion::EZREAL) m_PlayerEntity = ezrealEntity;
    if (champ == eChampion::FIORA) m_PlayerEntity = fioraEntity;
```

**After** (전체 함수 박제 — 핵심 영역):
```cpp
void CScene_InGame::CreateECSEntities()
{
    EntityID jaxEntity = NULL_ENTITY;
#if !WINTERS_MIN_SCENE
    EntityID rivenEntity  = NULL_ENTITY;
    EntityID ezrealEntity = NULL_ENTITY;
    EntityID fioraEntity  = NULL_ENTITY;
#endif

#if !WINTERS_MIN_SCENE
    m_IreliaEntity = CreateChampionEntity(m_Irelia, m_IreliaTransform, eChampion::IRELIA, eTeam::Blue);
    m_KalistaEntity = CreateChampionEntity(m_Kalista, m_KalistaTransform, eChampion::KALISTA, eTeam::Blue);
    m_YasuoEntity = CreateChampionEntity(m_Yasuo, m_YasuoTransform, eChampion::YASUO, eTeam::Blue);
    m_GarenEntity = CreateChampionEntity(m_Garen, m_GarenTransform, eChampion::GAREN, eTeam::Blue);
    m_ZedEntity = CreateChampionEntity(m_Zed, m_ZedTransform, eChampion::ZED, eTeam::Blue);

    rivenEntity  = CreateECSChampion(eChampion::RIVEN, eTeam::Blue);
    ezrealEntity = CreateECSChampion(eChampion::EZREAL, eTeam::Blue);
    fioraEntity  = CreateECSChampion(eChampion::FIORA, eTeam::Blue);
    m_FioraEntity = fioraEntity;
#endif

    // Jax — Phase B-13 (MIN_SCENE 에서도 active, 사용자 요구사항)
    jaxEntity = CreateECSChampion(eChampion::JAX, eTeam::Blue);
    m_JaxEntity = jaxEntity;

    // Sylas — 적 (MIN_SCENE 에서도 active)
    m_SylasEntity = CreateChampionEntity_FromBlueprint(L"Sylas", m_Sylas, m_SylasTransform);
    if (m_SylasEntity == NULL_ENTITY)
    {
        OutputDebugStringA("[Scene_InGame] Sylas Blueprint Clone failed, fallback to direct CreateChampionEntity\n");
        m_SylasEntity = CreateChampionEntity(m_Sylas, m_SylasTransform, eChampion::SYLAS, eTeam::Red);
    }
    {
        char dbg[160]{};
        sprintf_s(dbg, "[Sylas] entity=%u tf=%d champ=%d team=%d pos=(%.1f,%.1f,%.1f)\n",
            static_cast<u32_t>(m_SylasEntity),
            m_World.HasComponent<TransformComponent>(m_SylasEntity) ? 1 : 0,
            m_World.HasComponent<ChampionComponent>(m_SylasEntity) ? 1 : 0,
            m_World.HasComponent<ChampionComponent>(m_SylasEntity)
                ? static_cast<i32_t>(m_World.GetComponent<ChampionComponent>(m_SylasEntity).team) : -1,
            m_vSylasTestPos.x, m_vSylasTestPos.y, m_vSylasTestPos.z);
        ::OutputDebugStringA(dbg);
    }

    auto addSkillStateIfAlive = [&](EntityID entity)
    {
        if (entity != NULL_ENTITY)
            m_World.AddComponent<SkillStateComponent>(entity);
    };

    addSkillStateIfAlive(jaxEntity);   // Phase B-13 (MIN_SCENE active)

#if !WINTERS_MIN_SCENE
    if (m_YasuoEntity != NULL_ENTITY)
        m_World.AddComponent<YasuoStateComponent>(m_YasuoEntity);
    addSkillStateIfAlive(m_YasuoEntity);
    addSkillStateIfAlive(m_IreliaEntity);
    addSkillStateIfAlive(m_KalistaEntity);
    addSkillStateIfAlive(m_GarenEntity);
    addSkillStateIfAlive(m_ZedEntity);

    m_ViegoEntity = CreateChampionEntity(m_Viego, m_ViegoTransform, eChampion::END, eTeam::Red);
#endif

    using namespace Engine;
    eChampion champ = CGameInstance::Get()->Get_GameContext().SelectedChampion;
#if !WINTERS_MIN_SCENE
    if (champ == eChampion::IRELIA) m_PlayerEntity = m_IreliaEntity;
    else if (champ == eChampion::YASUO)  m_PlayerEntity = m_YasuoEntity;
    else if (champ == eChampion::KALISTA) m_PlayerEntity = m_KalistaEntity;
    else if (champ == eChampion::GAREN) m_PlayerEntity = m_GarenEntity;
    else if (champ == eChampion::ZED) m_PlayerEntity = m_ZedEntity;
    else if (champ == eChampion::RIVEN) m_PlayerEntity = rivenEntity;
    else if (champ == eChampion::EZREAL) m_PlayerEntity = ezrealEntity;
    else if (champ == eChampion::FIORA) m_PlayerEntity = fioraEntity;
    else
#endif
    if (champ == eChampion::JAX) m_PlayerEntity = jaxEntity;

    if (m_PlayerEntity != NULL_ENTITY)
        m_World.AddComponent<LocalPlayerTag>(m_PlayerEntity);
```

### 4.8 `SyncECSTransformsFromLegacy` — Zed 가드

**Anchor**: L920 — `push(m_ZedEntity, m_ZedTransform);`

**Before**:
```cpp
    push(m_ZedEntity, m_ZedTransform);
```

**After**:
```cpp
    // Zed: MIN_SCENE 에서 entity 미생성 — push skip (B-13)
```

(L920 한 줄 삭제. push 함수는 NULL_ENTITY 체크를 이미 하지만, 명시적 cleanup.)

### 4.9 OnUpdate — Zed Update 가드

**Anchor**: L1836 — `m_Zed.Update(dt);`

**Before** (L1826-L1840 영역):
```cpp
#if !WINTERS_MIN_SCENE
    m_Irelia.Update(dt);
    m_Yasuo.Update(dt);
    ...
#endif
#if !WINTERS_MIN_SCENE
    m_Viego.Update(dt);
    m_Kalista.Update(dt);
    m_Garen.Update(dt);
#endif
    m_Zed.Update(dt);
```

**After**:
```cpp
#if !WINTERS_MIN_SCENE
    m_Irelia.Update(dt);
    m_Yasuo.Update(dt);
    ...
#endif
#if !WINTERS_MIN_SCENE
    m_Viego.Update(dt);
    m_Kalista.Update(dt);
    m_Garen.Update(dt);
    m_Zed.Update(dt);
#endif
```

### 4.10 OnRender — Zed Render 가드

**Anchor**: L2045 / L2109 — `renderNormalPassRenderer(m_Zed, m_ZedTransform.GetWorldMatrix());` / `renderMainPassRenderer(m_Zed, ...)`

**Before** (L2040-L2050 / L2103-L2115 영역):
```cpp
#if !WINTERS_MIN_SCENE
    renderNormalPassRenderer(m_Irelia, m_IreliaTransform.GetWorldMatrix());
    ...
    renderNormalPassRenderer(m_Garen, m_GarenTransform.GetWorldMatrix());
#endif
    renderNormalPassRenderer(m_Zed, m_ZedTransform.GetWorldMatrix());

// (and similar for renderMainPassRenderer)
```

**After**:
```cpp
#if !WINTERS_MIN_SCENE
    renderNormalPassRenderer(m_Irelia, m_IreliaTransform.GetWorldMatrix());
    ...
    renderNormalPassRenderer(m_Garen, m_GarenTransform.GetWorldMatrix());
    renderNormalPassRenderer(m_Zed, m_ZedTransform.GetWorldMatrix());
#endif

// (and similar for renderMainPassRenderer)
```

### 4.11 OnExit — Zed Shutdown 가드

**Anchor**: L2208 — `m_Zed.Shutdown();`

**Before**:
```cpp
#if !WINTERS_MIN_SCENE
    m_Garen.Shutdown();
#endif
    m_Zed.Shutdown();
```

**After**:
```cpp
#if !WINTERS_MIN_SCENE
    m_Garen.Shutdown();
    m_Zed.Shutdown();
#endif
```

### 4.12 Scene_InGame.h — `m_JaxEntity` 멤버 추가

**Anchor**: `EntityID m_FioraEntity = NULL_ENTITY;` 직후

**Before**:
```cpp
    EntityID m_FioraEntity = NULL_ENTITY;
```

**After**:
```cpp
    EntityID m_FioraEntity = NULL_ENTITY;
    EntityID m_JaxEntity = NULL_ENTITY;   // Phase B-13
```

---

## §5. Implementation Gate

### 5.1 Client.vcxproj 등록

**Anchor**: ClCompile Fiora 항목 직후 (L130 부근)

**Before**:
```xml
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_FxPresets.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_Skills.cpp" />
```

**After**:
```xml
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_FxPresets.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_Skills.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Jax\Jax_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Jax\Jax_FxPresets.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Jax\Jax_Skills.cpp" />
```

**Anchor**: ClInclude Fiora 항목 직후 (L218 부근)

**Before**:
```xml
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Components.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_FxPresets.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Registration.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Skills.h" />
```

**After**:
```xml
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Components.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_FxPresets.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Registration.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Skills.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Jax\Jax_Components.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Jax\Jax_FxPresets.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Jax\Jax_Registration.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Jax\Jax_Skills.h" />
```

### 5.2 v1 잔존 cleanup (Fiora)

**Phase B-13 합류 시 동시 진행** — Fiora 가 ChampionTable.cpp + Fiora_Registration.cpp 두 곳에서 등록되는 중복 제거.

**ChampionTable.cpp L18-L26 — FIORA entry 제거**:

**Before**:
```cpp
    {eChampion::FIORA, "fiora_", "idle1", "run", "attack1", 1.5f,
     "Client/Bin/Resource/Texture/Character/Fiora/fiora.fbx",
     L"Shaders/Mesh3D.hlsl",
     L"Client/Bin/Resource/Texture/Character/Fiora/fiora_base_tx_cm.png",
     {},
     { 30.f, 1.f, 0.f },
     0.01f },
```

**After** (5줄 삭제):
```cpp
    // Fiora: Fiora_Registration.cpp 가 자체 등록 (Phase B-12 v2)
```

**SkillTable.cpp L305-L345 — FIORA 5 SkillDef 제거**:

5개 entry 전부 삭제. (Fiora_Registration.cpp 의 `CSkillRegistry::Instance().Add(FIORA, slot, def)` 가 권위)

**파일 삭제 후보**:
```
Client/Public/GameObject/Champion/Fiora/FioraFxPresets.h    ← Fiora_FxPresets.h 와 중복
Client/Private/GameObject/Champion/Fiora/FioraFxPresets.cpp ← 동일
```

vcxproj 에서 `FioraFxPresets.cpp` ClCompile / `FioraFxPresets.h` ClInclude 항목 제거 → 디스크 파일 삭제.

### 5.3 최소 수정 / 기존 dirty work 보존

- Phase B-12 v2 의 미적용 항목 (Zed MIN_SCENE 가드) 을 본 phase 에서 일괄 처리 — Fiora 박제 시 미완료 부분 보강
- Server `GameRoom.cpp` 의 EZREAL 하드코딩 일반화는 04a v2 D-1 phase 로 분리 (본 phase 미진입)
- ImGui 튜닝 패널 (JaxStateComponent 슬라이더) — B-15 cycle 에 따로 spec

---

## §6. Verification Gate

### 6.1 사전 체크리스트

- [ ] `devenv.exe` 종료 (vc143.pdb lock 회피)
- [ ] `taskkill /F /IM WintersServer.exe /IM WintersGame.exe` (LNK1104 회피)
- [ ] `Tools\convert_all_assets.bat champions` 실행 → `OK=11` (Fiora 10 + Jax 1 추가)
- [ ] `jax.wmesh / .wskel / anims/*.wanim` 49개 확인
- [ ] Engine 단독 빌드 → EngineSDK/inc 동기화

### 6.2 G1: Client build (compile)

```cmd
cd C:\Users\tnest\Desktop\Winters_restored\Winters
MSBuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal
.\UpdateLib.bat
MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal
```

**통과 조건**: error 0. Fiora 와 동일 — Jax 7 신규 파일 + Scene_InGame 11 영역 수정 컴파일 통과.

### 6.3 G2: Full solution build

```cmd
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal
```

**통과 조건**: error 0. LNK1104 발생 시 → G3 진단.

### 6.4 G3: Lock 분리 (F2 vs F3 진단)

```cmd
taskkill /F /IM WintersServer.exe
taskkill /F /IM WintersGame.exe
```

이후 G2 재시도. 여전히 LNK1104 → F2 (link error). 이번엔 통과 → F3 (실행 중 lock) 였음.

### 6.5 G4: 8초 실행 smoke

```cmd
cd Client\Bin\Debug
start /B WintersGame.exe
timeout /T 8 /NOBREAK
taskkill /F /IM WintersGame.exe
```

**통과 조건**:
- exe 8초 동안 살아있음 (1초 내 종료 X = F5 회피)
- Output 창에 `[Jax] Registration complete` 한 줄

### 6.6 G5: Feature-specific smoke

| 단계 | 액션 | 기대 |
|---|---|---|
| 1 | 메인 메뉴 → ChampSelect | "Jax" 버튼 노출 |
| 2 | Jax 픽 → InGame | (33,1,0) 위치 스폰. **첫 실행 시 Output 창 `[CModel] meshes=N material=...` 로그** 확인 — body/fish/weapon 슬롯 매핑 검증 후 텍스처 미스매치 시 §3.8 의 `texturePath[0..7]` 인덱스 조정 |
| 3 | idle1_v04 anim 재생 (서있음 모션) | 정적 idle |
| 4 | 우클릭 이동 → jax_run2 anim | run 전환, 도달 시 idle |
| 5 | Sylas + A | `[Jax BA] dmg=50.0` + hit flash FX |
| 6 | Q (Sylas 타겟) | spell1 anim, 도약 + 70dmg + 칼날 트레일 |
| 7 | W | spell2_v03 anim, 10s glow, `Empower armed` |
| 8 | W 후 BA | dmg=90 (50+40) — Empower bonus 검증 |
| 9 | E | spell3_attack1 anim, AOE slash, hit 수 출력 |
| 10 | R | spell4_idle anim (짧음), 8s buff aura, `Grandmaster's Might activated` |
| 11 | R 후 BA × 3 | 3타째 AOE 폭발 + r_third_attack_aoe FX, `[Jax R AOE]` |

### 6.7 G6: Server/network 경로 (Phase 04a v2 D-1 진입 후)

본 phase 미적용. Jax::Gameplay namespace 가 `(void)ctx;` stub 인 상태 유지. 04a v2 D-1 진입 시 Skill ns 의 ApplyDamage 등 동일 로직 이관 + Server `GameRoom.cpp` 일반화.

### 6.8 회귀 grep

```bash
# Hook 박제 카운트
grep -c "kJax_" Client/Private/GameObject/Champion/Jax/Jax_Registration.cpp        # ≥ 25
grep -c "Register" Client/Private/GameObject/Champion/Jax/Jax_Registration.cpp     # ≥ 15

# WINTERS_MIN_SCENE 강화 (Zed 가드 추가)
grep -c "WINTERS_MIN_SCENE" Client/Private/Scene/Scene_InGame.cpp                  # ≥ 26 (B-12 22 + Zed 4)

# Fiora 잔존 cleanup
grep "FIORA" Client/Private/GameObject/ChampionTable.cpp                           # 0 (cleanup 후)
grep "FIORA" Client/Private/GameObject/SkillTable.cpp                              # 0 (cleanup 후)
ls Client/Public/GameObject/Champion/Fiora/FioraFxPresets.h                        # 미존재 (삭제됨)
```

---

## §7. Learning Update

### 7.1 박제 후 갱신 4 항목 후보

**CLAUDE.md Gotcha 후보**:

```markdown
- **멀티 메시 챔프 텍스처 슬롯 매핑은 첫 실행 후 검증 필수 (B-13, 2026-05-04)**:
  Jax/Viego/Sylas 등 N≥3 텍스처 챔프는 ChampionDef::texturePath[8] 의 어느 인덱스가
  body/weapon/accessory 메시에 매핑되는지 컴파일 시점에 알 수 없음. 첫 실행 시
  Output 창 `[CModel] meshes=N material=...` 로그 검증 후 인덱스 조정. 잘못 매핑되면
  무기에 fish 텍스처 같은 시각 결함 발생 — 빌드 통과하므로 G5 smoke 에서만 발견.
```

**Skill Gotcha 후보** (`winters-skills/code/SKILL.md` §E):

```markdown
### 8. v1 → v2 패턴 정정 시 v1 잔존 cleanup 의무 (2026-05-04, B-13)
Phase B-12 v2 가 Ezreal 패턴 채택했으나 사용자 박제 시 v1 의 ChampionTable.cpp + SkillTable.cpp
정적 entry 도 살림 → 두 등록 path 동거. 작동은 했으나 다음 챔프 (Jax) 박제 시 cleanup 까지
끌고 가야 의도 단일성 회복. 교훈: v1 → v2 정정 계획서는 "삭제할 v1 라인" 명시 의무.
```

**Memory 후보** (`memory/feedback_*.md`):

```markdown
파일: feedback_build_integrity_framework.md
내용: 빌드 통과 ≠ 기능 완성 분리 사례. Fiora compile ✅ + 8s smoke ✅ + Skill ns 데미지 적용 ✅
+ Gameplay ns stub (network bypass 미해결) → 04a v2 D-1 prerequisite. F1-F7 실패 분류 도입.
```

**단일 재발 방지 규칙** (본 framework `BUILD_INTEGRITY_FRAMEWORK.md` §5.1 갱신):

- F1 ~ F7 분류는 본 phase 적용 후 검증
- 새 실패 모드 발견 시 framework §5.1 표 추가
- 본 phase 통과 후 §6.2 self-update 트리거 평가

### 7.2 Framework 적용 측정

| 게이트 | 적용 | 상태 |
|---|---|---|
| §0 Agent Contract Evidence | ✅ | 8 Bash + 5 Grep + 2 Read 박제 |
| §1 Preflight Evidence Table | ✅ | 9행 표 박제 |
| §2 Plan Quality Gate | ✅ | 5/5 체크 통과 |
| §3 Full code 박제 | ✅ | 7 신규 파일 전문 |
| §4 Anchor + before/after hunk | ✅ | Scene_InGame 11 영역 |
| §5 Implementation Gate | ✅ | vcxproj + cleanup |
| §6 Verification Gate | ✅ | G1~G6 명시 |
| §7 Learning Update | ✅ | 4 후보 박제 |

**준수율 8/8** — Framework v1 의 첫 적용 사례.

---

## §8. 다음 단계 (Phase B-14 ~ B-15)

| Phase | 챔프/작업 | 패턴 | 예상 |
|---|---|---|---|
| **B-14** | Ashe | Ezreal/Jax 패턴 미러. Q (Volley 멀티 화살), W (cone), E (Hawkshot 시야), R (Crystal Arrow stun projectile) | 4h |
| **B-15** | FioraState 본격 (W parry incoming attack 차단 + R 4 약점) | Yasuo PendingHitSystem 차용 | 6h |
| **B-16** | JaxState 본격 (E parry detection + R 3타 AOE 정식) | B-15 패턴 미러 | 4h |

**Ashe 변환**: `call :convert_champ "Ashe" "ashe.fbx"`

---

## §9. 즉시 진입 명령

```
"Phase B-13 Jax 진행. 10_JAX_PHASE_B13_PIPELINE_v1.md §3 D-0 변환부터.
1) convert_all_assets.bat Jax 1줄 추가 → 변환 → fiora.wmesh 49 anims 검증
2) Jax 7개 신규 파일 박제 (Components/FxPresets h+cpp/Skills h+cpp/Registration h+cpp)
3) Scene_InGame.cpp 11 영역 수정 (§4 anchor 박제 + Zed MIN_SCENE 가드 강화)
4) v1 잔존 cleanup (Fiora ChampionTable/SkillTable entry + FioraFxPresets 중복 삭제)
5) Client.vcxproj 등록 → G1 Client build → G3 lock 분리 → G4 8초 smoke →
   G5 Jax feature smoke (BanPick → 픽 → BA/Q/W/E/R + Empower bonus + R 3타 AOE 검증)."
```
