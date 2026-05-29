# Phase B-12 v2 (피오라) — Ezreal 패턴 미러: 3-Layer Hook 시스템 박제

**작성일**: 2026-05-04
**v1 폐기 사유**: Riven 패턴 (`SkillTable.cpp` 정적 배열 + Scene_InGame castFrame 분기) 채택 → `castFrameHookId = 0` → GameplayHook/VisualHook/SkillHook 3 dispatch path 모두 skip → **legacy fallback** 으로 흘러감 → 04a v2 server transport 합류 시 재공정 필요. v2 는 Ezreal `Ezreal_Registration.cpp` 자체 모듈 패턴 100% 미러.

**목표**: Fiora 1체를 Ezreal 와 동일한 hook chassis 위에 박제 → 04a v2 D-1 서버 전송 합격 즉시 server-authoritative 시뮬 자동 인계. Sylas (적) 와 단독 매치 가능. Zed/Riven/Ezreal 등 다른 챔프는 `#define WINTERS_MIN_SCENE 1` 으로 빌드 제외.

---

## 0. v1 vs v2 — 패턴 차이

| 항목 | v1 (Riven 패턴, 폐기) | v2 (Ezreal 패턴) |
|---|---|---|
| 등록 위치 | `ChampionTable.cpp` 정적 배열 1줄 | `Fiora_Registration.cpp` 자체 모듈 (static initializer) |
| Skill 등록 | `SkillTable.cpp` 정적 배열 5 SkillDef | `CSkillRegistry::Instance().Add(FIORA, slot, def)` 5번 호출 |
| `castFrameHookId` | `0` (미설정) | `MakeHookId(FIORA, BA_CastFrame)` 등 5종 박제 |
| Cast frame 처리 | Scene_InGame `if (!castHandled)` legacy if-else | `CGameplayHookRegistry / CSkillHookRegistry / CVisualHookRegistry` 3개 dispatch |
| 데미지 적용 위치 | Scene_InGame `ApplyFioraHit` (private 메서드) | `Fiora::OnCastFrame_X` (Skill ns) → `ApplyDamage(...)` 공유 함수 |
| Server 합류 | 04a v2 시 재공정 필요 | 04a v2 D-1 즉시 합류 가능 (Gameplay ns 함수 본체만 추가) |
| Visual FX | Scene_InGame 분기 안에서 호출 | `Fiora::Visual::OnCastFrame_X_Visual` 에서 호출 (분리) |
| 신규 파일 수 | 2 | 7 |

### Hook 시스템 작동 흐름 (Fiora castFrame 도달 시)

```
[Player A 키] → [DispatchSkillInput(0)] → [BuildCastCommand → ApplyLocalPrediction]
   → [castFrame=6 at 0.25s 도달]
   → [Scene_InGame OnUpdate L1175-1234 castHit 블록]
       ├──→ CGameplayHookRegistry::Dispatch(kFio_BA_Cast)
       │       └──→ Fiora::Gameplay::OnCastFrame_BA(ctx)  ← stub (04a v2 D-1 본격)
       ├──→ CVisualHookRegistry::Dispatch(kFio_BA_Cast)
       │       └──→ Fiora::Visual::OnCastFrame_BA_Visual(ctx) → SpawnBAHitSpark
       └──→ CSkillHookRegistry::Dispatch(kFio_BA_Cast)
               └──→ Fiora::OnCastFrame_BA(ctx) → ApplyDamage(...)
```

---

## 1. 자원 확인 (실측)

```
Client/Bin/Resource/Texture/Character/Fiora/
├── fiora.fbx                       ✓
├── fiora.skl / fiora.skn           ✓
├── fiora_base_tx_cm.png            ✓ ★ 메인 텍스처
├── fiora.wmesh / fiora.wskel       ✗ D-0 변환 필요
├── animations/  19 .anm            (idle1, run, attack1, attack2, spell1, spell2, spell2_in, channel, channel_windup, ...)
└── particles/   200+ PNG (필요 11개만)
```

E (Bladework) — LoL 원작 자세 변경 없는 패시브. `attack1` placeholder anim + Visual FX 활성화만.

---

## 2. D-0 자원 변환

### 2.1 `Tools/convert_all_assets.bat` 수정 (L32 다음)

```bat
call :convert_champ "Riven" "riven.fbx"
call :convert_champ "Ezreal" "ezreal.fbx"
call :convert_champ "Fiora" "fiora.fbx"
```

### 2.2 변환 실행

```cmd
.\Tools\convert_all_assets.bat champions
```

검증: `fiora.wmesh / .wskel / anims/*.wanim` 19개.

---

## 3. 신규 파일 7개 — 전문 박제

### 3.1 `Client/Public/GameObject/Champion/Fiora/Fiora_Components.h`

```cpp
#pragma once

#include "Engine_Defines.h"

struct FioraStateComponent
{
    bool   bBladeworkActive = false;
    f32_t  fBladeworkTimer  = 0.f;
    u8_t   bladeworkHitsRemaining = 0;
    f32_t  fBladeworkDamageBonus = 30.f;

    bool   bRiposteActive = false;
    f32_t  fRiposteTimer  = 0.f;
    f32_t  fRiposteWindowSec = 0.75f;

    bool      bRActive = false;
    f32_t     fRTimer  = 0.f;
    EntityID  rTargetEntity = NULL_ENTITY;
    f32_t     fRHealZoneRadius = 6.f;
};
```

### 3.2 `Client/Public/GameObject/Champion/Fiora/Fiora_FxPresets.h`

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

namespace Fiora::Fx
{
    void SpawnBAHitSpark(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWParryActive(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnWBlockFlash(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnRMark(CWorld& world, EntityID target, f32_t fDuration);
    void SpawnRHealZone(CWorld& world, EntityID owner, f32_t fDuration);
}
```

### 3.3 `Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp`

```cpp
#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    constexpr const wchar_t* kPathBaHitSparkTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_hit_spark_yellow.png";
    constexpr const wchar_t* kPathQSlashTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_q_slash.png";
    constexpr const wchar_t* kPathQGlowTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_q_swordglow.png";
    constexpr const wchar_t* kPathWParryTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_w_block_glow.png";
    constexpr const wchar_t* kPathWFlashTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_w_block_flash.png";
    constexpr const wchar_t* kPathEBuffTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_buff_mult_yellow.png";
    constexpr const wchar_t* kPathRMarkTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_crest_glow.png";
    constexpr const wchar_t* kPathRHealTex =
        L"Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_healzone.png";
}

namespace Fiora::Fx
{
    void SpawnBAHitSpark(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathBaHitSparkTex;
        fx.fWidth = 1.4f; fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.1f, 0.95f, 0.45f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.45f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3&, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.0f, 0.f };
            fx.texturePath = kPathQSlashTex;
            fx.fWidth = 2.4f; fx.fHeight = 1.6f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime;
            fx.vColor = { 0.95f, 0.85f, 0.55f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.45f;
            CFxSystem::Spawn(world, fx);
        }
        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.1f, 0.f };
            fx.texturePath = kPathQGlowTex;
            fx.fWidth = 1.4f; fx.fHeight = 1.4f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime * 0.6f;
            fx.vColor = { 1.0f, 0.95f, 0.7f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.3f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnWParryActive(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathWParryTex;
        fx.fWidth = 2.0f; fx.fHeight = 2.0f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.1f, 0.9f, 0.4f, 0.85f };
        fx.blendMode = eBlendPreset::AlphaBlend;
        fx.fFadeOut = fDuration * 0.35f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnWBlockFlash(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.2f, 0.f };
        fx.texturePath = kPathWFlashTex;
        fx.fWidth = 2.6f; fx.fHeight = 2.6f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.4f, 1.2f, 0.6f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.55f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.3f, 0.f };
        fx.texturePath = kPathEBuffTex;
        fx.fWidth = 1.6f; fx.fHeight = 1.6f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.0f, 0.85f, 0.3f, 0.9f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fDuration * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRMark(CWorld& world, EntityID target, f32_t fDuration)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 2.5f, 0.f };
        fx.texturePath = kPathRMarkTex;
        fx.fWidth = 1.0f; fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.2f, 0.9f, 0.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fDuration * 0.3f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRHealZone(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 0.05f, 0.f };
        fx.texturePath = kPathRHealTex;
        fx.fWidth = 6.0f; fx.fHeight = 6.0f;
        fx.bBillboard = false;
        fx.fYaw = 0.f;
        fx.fLifetime = fDuration;
        fx.vColor = { 0.85f, 1.0f, 0.55f, 0.7f };
        fx.blendMode = eBlendPreset::AlphaBlend;
        fx.fFadeOut = fDuration * 0.5f;
        CFxSystem::Spawn(world, fx);
    }
}
```

### 3.4 `Client/Public/GameObject/Champion/Fiora/Fiora_Skills.h`

```cpp
#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

namespace Fiora
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

void Fiora_KeepAlive();
```

### 3.5 `Client/Private/GameObject/Champion/Fiora/Fiora_Skills.cpp`

```cpp
#include "GameObject/Champion/Fiora/Fiora_Skills.h"
#include "GameObject/Champion/Fiora/Fiora_Components.h"
#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace Fiora
{
    namespace
    {
        EntityID FindEnemyInCone(CWorld& world, EntityID caster, eTeam casterTeam,
                                 const Vec3& origin, const Vec3& dir,
                                 f32_t fRange, f32_t fHitRadius)
        {
            const f32_t lenSq = dir.x * dir.x + dir.z * dir.z;
            if (lenSq <= 0.0001f) return NULL_ENTITY;
            const f32_t invLen = 1.f / std::sqrtf(lenSq);
            const Vec3 fwd{ dir.x * invLen, 0.f, dir.z * invLen };

            EntityID best = NULL_ENTITY;
            f32_t fBestDistSq = FLT_MAX;
            world.ForEach<ChampionComponent, TransformComponent>(
                [&](EntityID e, ChampionComponent& cc, TransformComponent& tf)
                {
                    if (e == caster) return;
                    if (cc.team == casterTeam) return;
                    const Vec3 vEnemy = tf.GetPosition();
                    const f32_t dx = vEnemy.x - origin.x;
                    const f32_t dz = vEnemy.z - origin.z;
                    const f32_t fDist = std::sqrtf(dx * dx + dz * dz);
                    if (fDist > fRange + fHitRadius) return;
                    const f32_t fDot = (fDist > 0.001f)
                        ? (fwd.x * dx / fDist + fwd.z * dz / fDist) : 1.f;
                    if (fDot < 0.5f) return;
                    const f32_t fDistSq = dx * dx + dz * dz;
                    if (fDistSq < fBestDistSq)
                    {
                        fBestDistSq = fDistSq;
                        best = e;
                    }
                });
            return best;
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY) return;

        f32_t fDamage = 55.f;
        if (ctx.pWorld->HasComponent<FioraStateComponent>(ctx.casterEntity))
        {
            auto& fs = ctx.pWorld->GetComponent<FioraStateComponent>(ctx.casterEntity);
            if (fs.bBladeworkActive && fs.bladeworkHitsRemaining > 0)
            {
                fDamage += fs.fBladeworkDamageBonus;
                fs.bladeworkHitsRemaining = (fs.bladeworkHitsRemaining > 0)
                    ? static_cast<u8_t>(fs.bladeworkHitsRemaining - 1) : 0;
                if (fs.bladeworkHitsRemaining == 0)
                    fs.bBladeworkActive = false;
            }
        }
        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, fDamage);

        char dbg[128];
        sprintf_s(dbg, "[Fiora BA] target=%u dmg=%.1f\n",
            static_cast<u32_t>(target), fDamage);
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        auto& tf = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 vOrigin = tf.GetPosition();
        const Vec3 vDir = ctx.pCommand->direction;

        const f32_t lenSq = vDir.x * vDir.x + vDir.z * vDir.z;
        if (lenSq > 0.0001f)
        {
            const f32_t invLen = 1.f / std::sqrtf(lenSq);
            const Vec3 fwd{ vDir.x * invLen, 0.f, vDir.z * invLen };
            const Vec3 vDest{
                vOrigin.x + fwd.x * 3.0f, vOrigin.y, vOrigin.z + fwd.z * 3.0f };
            tf.SetPosition(vDest);
            tf.m_bLocalDirty = true;
            tf.m_bWorldDirty = true;
        }

        const EntityID hitTarget = FindEnemyInCone(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, vOrigin, vDir, 4.0f, 1.0f);
        if (hitTarget != NULL_ENTITY)
            ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, hitTarget, 70.f);

        char dbg[160];
        sprintf_s(dbg, "[Fiora Q] origin=(%.1f,%.1f,%.1f) hit=%u\n",
            vOrigin.x, vOrigin.y, vOrigin.z, static_cast<u32_t>(hitTarget));
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<FioraStateComponent>(ctx.casterEntity)) return;
        auto& fs = ctx.pWorld->GetComponent<FioraStateComponent>(ctx.casterEntity);
        fs.bRiposteActive = true;
        fs.fRiposteTimer = fs.fRiposteWindowSec;
        OutputDebugStringA("[Fiora W] Riposte armed\n");
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<FioraStateComponent>(ctx.casterEntity)) return;
        auto& fs = ctx.pWorld->GetComponent<FioraStateComponent>(ctx.casterEntity);
        fs.bBladeworkActive = true;
        fs.fBladeworkTimer = 5.0f;
        fs.bladeworkHitsRemaining = 2;
        OutputDebugStringA("[Fiora E] Bladework activated (next 2 BA enhanced)\n");
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<FioraStateComponent>(ctx.casterEntity)) return;
        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY) return;

        auto& fs = ctx.pWorld->GetComponent<FioraStateComponent>(ctx.casterEntity);
        fs.bRActive = true;
        fs.fRTimer = 8.0f;
        fs.rTargetEntity = target;
        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, 80.f);

        char dbg[128];
        sprintf_s(dbg, "[Fiora R] target=%u marked\n", static_cast<u32_t>(target));
        OutputDebugStringA(dbg);
    }

    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx) { (void)ctx; }
        void OnCastFrame_Q(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_W(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_E(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_R(GameplayHookContext& ctx)  { (void)ctx; }
    }

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            const EntityID target = ctx.pCommand->targetEntityId;
            if (target == NULL_ENTITY) return;
            Fx::SpawnBAHitSpark(*ctx.pWorld, target, 0.4f);
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnQSlash(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->direction, 0.4f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnWParryActive(*ctx.pWorld, ctx.casterEntity, 1.5f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnEBladeworkBuff(*ctx.pWorld, ctx.casterEntity, 5.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            const EntityID target = ctx.pCommand->targetEntityId;
            if (target == NULL_ENTITY) return;
            Fx::SpawnRMark(*ctx.pWorld, target, 8.0f);
            Fx::SpawnRHealZone(*ctx.pWorld, ctx.casterEntity, 4.0f);
        }
    }
}
```

### 3.6 `Client/Public/GameObject/Champion/Fiora/Fiora_Registration.h`

```cpp
#pragma once
```

### 3.7 `Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp`

```cpp
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Fiora/Fiora_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kFio_BA_Cast = MakeHookId(eChampion::FIORA, HookVariant::BA_CastFrame);
    constexpr u32_t kFio_Q_Cast  = MakeHookId(eChampion::FIORA, HookVariant::Q_CastFrame);
    constexpr u32_t kFio_W_Cast  = MakeHookId(eChampion::FIORA, HookVariant::W_CastFrame);
    constexpr u32_t kFio_E_Cast  = MakeHookId(eChampion::FIORA, HookVariant::E_CastFrame);
    constexpr u32_t kFio_R_Cast  = MakeHookId(eChampion::FIORA, HookVariant::R_CastFrame);

    struct FioraAutoRegister
    {
        FioraAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::FIORA;
            cd.animPrefix    = "fiora_";
            cd.idleAnimKey   = "idle1";
            cd.runAnimKey    = "run";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Fiora/fiora.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            const wchar_t* fioraBaseTexture =
                L"Client/Bin/Resource/Texture/Character/Fiora/fiora_base_tx_cm.png";
            cd.defaultTexturePath = fioraBaseTexture;
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = fioraBaseTexture;
            cd.spawnPosition = { 30.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Fiora";
            CChampionRegistry::Instance().Add(eChampion::FIORA, cd);

            // BA
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.6f; s.rangeMax = 1.5f; s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 1.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f; s.recoveryFrame = 14.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kFio_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 0, s);
            }
            // Q
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 1;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 1.5f; s.rangeMax = 4.0f; s.manaCost = 0.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 4.f; s.recoveryFrame = 10.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kFio_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 1, s);
            }
            // W
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 2;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 12.f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell2";
                s.lockDurationSec = 1.5f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 1.f; s.recoveryFrame = 18.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kFio_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 2, s);
            }
            // E
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 13.f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.4f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 1.f; s.recoveryFrame = 8.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kFio_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 3, s);
            }
            // R
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 4;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 80.f; s.rangeMax = 5.0f; s.manaCost = 100.f;
                s.animKey = "channel";
                s.lockDurationSec = 2.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 18.f; s.recoveryFrame = 36.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kFio_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::OnCastFrame_R);

            CGameplayHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Visual::OnCastFrame_R_Visual);

            OutputDebugStringA("[Fiora] Registration complete\n");
        }
    };

    static FioraAutoRegister s_register;
}

void Fiora_KeepAlive()
{
    (void)&s_register;
}
```

---

## 4. Scene_InGame.cpp 수정 — 6곳

### 4.1 Include 추가 (L73 다음)

```cpp
#include "GameObject/Champion/Riven/RivenFxPresets.h"
#include "GameObject/Champion/Fiora/Fiora_Components.h"
#include "GameObject/Champion/Fiora/Fiora_Skills.h"
```

### 4.2 `Fiora_KeepAlive()` 호출 (L218 다음)

```cpp
    extern void Ezreal_KeepAlive();
    Ezreal_KeepAlive();

    Fiora_KeepAlive();
```

### 4.3 `CreateECSChampion` — FioraStateComponent (L709-L713)

```cpp
    if (id == eChampion::RIVEN)
        m_World.AddComponent<RivenStateComponent>(e);
    else if (id == eChampion::EZREAL)
        m_World.AddComponent<EzrealStateComponent>(e);
    else if (id == eChampion::FIORA)
        m_World.AddComponent<FioraStateComponent>(e);
```

### 4.4 `CreateECSEntities` 전체 (전문 박제, §4.4 v2 본문 참조)

### 4.5 `OnEnter` 끝 BindPlayerToECSChampion (L376)

```cpp
    if (selectedChampion == eChampion::RIVEN
        || selectedChampion == eChampion::EZREAL
        || selectedChampion == eChampion::FIORA)
    {
        BindPlayerToECSChampion(m_PlayerEntity);
    }
```

### 4.6 WINTERS_MIN_SCENE 강화 — Zed 4곳 가드

L301-L309 (Init), L343-L349 (Player 분기), L2031-L2033 (OnRender), L2131 (Shutdown), L902 (Sync) 모두 `#if !WINTERS_MIN_SCENE / #endif` wrap.

---

## 5. Client.vcxproj 수정

### 5.1 ClCompile (L128 다음)

```xml
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_Registration.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_FxPresets.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\Fiora_Skills.cpp" />
```

### 5.2 ClInclude (L212 다음)

```xml
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Components.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_FxPresets.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Registration.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\Fiora_Skills.h" />
```

---

## 6. 빌드 + 검증

### 6.1 사전 체크리스트
- [ ] `devenv.exe` 종료
- [ ] `Tools\convert_all_assets.bat champions` → `OK=10`
- [ ] `fiora.wmesh / .wskel / anims/*.wanim` 19개

### 6.2 빌드
```cmd
MSBuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
.\UpdateLib.bat
MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### 6.3 검증 시나리오

| 단계 | 액션 | 기대 |
|---|---|---|
| 1 | 실행 → Output | `[Fiora] Registration complete` |
| 2 | ChampSelect | "Fiora" 버튼 |
| 3 | Fiora 픽 → InGame | (30,1,0) 스폰 |
| 4 | Sylas + A | `[Fiora BA] dmg=55.0` + hit spark |
| 5 | Q | spell1, 3m 돌진, q_slash, hit 70dmg |
| 6 | W | spell2, block_glow 1.5s, `Riposte armed` |
| 7 | E | buff_mult_yellow 5s, `Bladework activated` |
| 8 | E 후 BA | dmg=85 (55 + 30 bonus) |
| 9 | R | channel, r_crest_glow, r_healzone, dmg=80 |

### 6.4 회귀 grep
```bash
grep -c "kFio_" Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp  # ≥ 25
grep -c "WINTERS_MIN_SCENE" Client/Private/Scene/Scene_InGame.cpp  # ≥ 22
grep "CSkillHookRegistry::Instance().Register" Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp | wc -l  # 5
grep "CGameplayHookRegistry::Instance().Register" Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp | wc -l  # 5
grep "CVisualHookRegistry::Instance().Register" Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp | wc -l  # 5
```

---

## 7. 디버깅 매트릭스

| 증상 | 1순위 | 확인 |
|---|---|---|
| `[Fiora] Registration` 미출력 | static dead-strip | `Fiora_KeepAlive()` 호출 (L219) |
| BanPick Fiora 안 보임 | RegisterAllLegacy 충돌 | Output 시점 ChampionTable 후인지 |
| BA 데미지 0 | castFrameHookId 미박제 | Registration cpp 5 SkillDef 모두 hookId 박제 |
| OnCastFrame_BA 호출 OK 데미지 0 | team 동일 | Sylas=Red, Fiora=Blue |
| Q 돌진 안 됨 | dirty flag 누락 | tf.m_bLocalDirty=true |
| FX 흰색 포화 | vColor RGB > 1.5 + Additive | 0.7~1.4 범위 |
| anim 안 바뀜 | animKey FBX 미스매치 | `[ModelRenderer] animation NOT FOUND` (T-4) |
| 인게임 진입 크래시 | wmesh/wskel 부재 | convert 재실행 |
| FioraStateComponent 못 찾음 | CreateECSChampion 분기 누락 | L709-L713 FIORA 분기 |
| Bladework BA dmg=55 그대로 | OnCastFrame_BA fs.bBladeworkActive 분기 미작동 | 디버거 BP |

---

## 8. 04a v2 D-1 합류 시 확장 절차

1. `Fiora_Skills.cpp` 의 `Gameplay::OnCastFrame_X` 본체 채움 (Skill ns 의 ApplyDamage 등 동일 로직 이관)
2. Server `GameRoom.cpp` 의 EZREAL 하드코딩 일반화 (sessionId → BanPick championId)
3. Server `GameLogic.cpp` castFrame tick 처리에서 `CGameplayHookRegistry::Dispatch` 호출 추가
4. `Fiora::OnCastFrame_X` (Skill ns) 는 prediction 표시로 잔존, server reconciliation 보정

**v2 박제 시 1단계만 추가하면 server 동기화 즉시 호환**.

---

## 9. 다음 단계

| Phase | 챔프 | 패턴 | 예상 |
|---|---|---|---|
| **B-13** | Ashe | Ezreal 패턴 미러 (5 hook + AsheStateComponent + Q Volley 멀티 projectile) | 4h |
| **B-14** | Jax | Ezreal 패턴 + multi-mesh (body/fish/weapon 3 텍스처). Q (Leap), W (Empower), E (Counterstrike), R (Grandmaster) | 5h |
| **B-15** | FioraState 본격 — W parry incoming attack 차단 + R 4 약점 | Yasuo PendingHitSystem 차용 | 6h |

**Ashe**: `call :convert_champ "Ashe" "ashe.fbx"`
**Jax**: `call :convert_champ "Jax" "jax.fbx"`

---

## 10. 즉시 진입 명령

```
"Phase B-12 v2 Fiora 진행. 09_FIORA_PHASE_B12_PIPELINE_v2.md §2 D-0 변환부터.
1) convert_all_assets.bat Fiora 1줄 추가 → 변환 실행
2) Fiora 7개 신규 파일 박제 (Components/FxPresets h+cpp/Skills h+cpp/Registration h+cpp)
3) Scene_InGame.cpp 6곳 수정 (include + KeepAlive + CreateECSChampion FIORA 분기 +
   CreateECSEntities + BindPlayer + WINTERS_MIN_SCENE Zed 가드)
4) Client.vcxproj 등록
5) 빌드 → BanPick → Fiora 픽 → §6.3 9단계 검증."
```
