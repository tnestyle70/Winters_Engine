# Phase B-11d v3.1 (이즈리얼) — 150 챔프 스케일 아키텍처 도입 (★ 운영형 보정 최종)

**작성일**: 2026-04-29
**v3.1 갱신**: 2026-04-29 — Codex 검토 9건 반영 (블로커 4 + 설계 5).
**전제 문서**: `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md` §5/§10

---

## 0. v3 → v3.1 변경 요약

| # | 분류 | v3 (틀림) | v3.1 (정정) |
|---|------|----------|------------|
| **B-1** | 블로커 | `s.range` / `s.rotateMode` (필드 없음) | **실제 필드 = `rangeMax` / `rotate`** ([SkillDef.h:54,65](Client/Public/GameObject/SkillDef.h:54)) |
| **B-2** | 블로커 | `SkillHookRegistry.h` 안에 Context, include 부족 | **`SkillHookContext.h` 분리** + 명시 includes (`WintersTypes.h` / `ECS/Entity.h` / `GameContext.h` / `SkillDef.h` / `GameplayComponents.h`) |
| **B-3** | 블로커 | hook 안에서 `Scene_InGame*` cast → `ApplyEzrealHit` | **`ApplyDamage(world, source, target, amount)` 공통 함수** + `DamageRequestComponent` POD. Scene 의존 0. 첫 시민 Ezreal 부터 안티패턴 박지 않음 |
| **B-4** | 블로커 | `eYasuoProjectileKind::Wind` 재사용 | **`eProjectileKind` 일반화 enum** (`Generic/MysticShot/EssenceFlux/Wind/Tornado/EQRing/GlobalBeam`) + **`CGenericPendingHitSystem`** 신설. 야스오 전용 enum/system 은 alias 로 호환 |
| **D-1** | 설계 | `castFrameHookId` / `postCastHookId` 의미 모호 | **4 분할**: `keySwapHookId` (애니 키 결정) → `onCastAcceptedHookId` (입력 승인 직후 — 위치/스택) → `castFrameHookId` (애니 프레임) → `recoveryHookId` (복귀 프레임) |
| **D-2** | 설계 | DispatchHook 후 legacy fallback 무조건 진입 → 이중 발동 위험 | **`Dispatch` 가 bool 반환** — `if (!handled) legacy ...` 가드 |
| **D-3** | 설계 | BanPick = 수동 버튼 N개 (Riot 스케일 X) | **`ChampionRegistry::ForEach` 순회 → BanPick UI 자동 생성**. 신규 챔프 = Registration 만 → BanPick 자동 |
| **D-4** | 설계 | `eChampion : uint8_t` 명시값 X — enum 순서 변경이 네트워크/DB 프로토콜 파괴 | **명시값 박제** (`IRELIA = 1` ~ `EZREAL = 11` ~ `RESERVED_END = 255`) — 추가는 빈 ID 채움, 삭제는 절대 X |
| **D-5** | 설계 | `Client/Public/Champions/<Name>/_Components.h` — 서버 시뮬 못 씀 | **권장 위치 = `Shared/Champions/<Name>/_Components.h`** (서버도 include). 1차는 `Client/Public/Champions/` 진입 + Phase 4 직전 일괄 이전 (계획서 §16 명시) |

---

## 1. 신규 인프라 — v3.1 확정

| # | 인프라 | 위치 | 책임 |
|---|--------|------|------|
| I-1 | `CChampionRegistry` | `Client/Public/Gameplay/ChampionRegistry.h/.cpp` | self-register `unordered_map<eChampion, ChampionDef>` + **`ForEach` 순회 API (BanPick UI 자동)** |
| I-2 | `CSkillRegistry` | `Client/Public/Gameplay/SkillRegistry.h/.cpp` | self-register `(champ,slot) → SkillDef` |
| I-3 | `CSkillHookRegistry` | `Client/Public/Gameplay/SkillHookRegistry.h/.cpp` | `unordered_map<u32, HookFn>` + **`bool Dispatch(...)` 반환** |
| I-4 | `SkillHookContext` POD | `Client/Public/Gameplay/SkillHookContext.h` (★ 별도) | 명시 include + `pSceneOpaque` 제거, `pKeyOut` 만 유지 |
| I-5 | `SkillDef` 4 hook 필드 | `Client/Public/GameObject/SkillDef.h` (확장) | `keySwapHookId / onCastAcceptedHookId / castFrameHookId / recoveryHookId` (★ 4분할) |
| I-6 | `eProjectileKind` 일반화 | `Client/Public/GameObject/Combat/ProjectileKind.h` | 신규 enum + `eYasuoProjectileKind` alias 호환 |
| I-7 | `CGenericPendingHitSystem` | `Client/Public/GameObject/Combat/GenericPendingHitSystem.h/.cpp` | 챔프 무관 — `Schedule(..., eProjectileKind, ...)`. `CPendingHitSystem` 은 기존 호환 wrapper |
| I-8 | `DamageRequestComponent` + `ApplyDamage` | `Client/Public/Gameplay/Damage.h/.cpp` | POD 컴포넌트 + 공통 free function. **Scene 의존 제거** — 모든 챔프 hook 이 사용 |
| I-9 | `Champions/Ezreal/` 폴더 | `Client/Public/Champions/Ezreal/` + `Private/...` | self-register Registration + Skills hook + Components + FxPresets (★ Components 만 Phase 4 직전 `Shared/` 이전) |

---

## 2. 핵심 설계 v3.1

### 2.1 Hook 4 단계 분류 (D-1)

```
캐스트 입력
  │
  ├─ [keySwapHook]          ── ApplyLocalPrediction 안 anim key 결정 전
  │                              (Yasuo Q stack→spell1a/b/c, Ezreal E yaw→spell3_-90/90/180)
  │                              ctx.pKeyOut 에 결과 string set
  │
  ├─ [onCastAcceptedHook]   ── 입력 승인 직후 (애니 시작과 동시)
  │                              위치 변경 (Ezreal E 텔포 SetPosition)
  │                              스택 갱신 (Riven Q qStackCount++)
  │                              상태 전이 (Riven R bUlted=true)
  │
  ├─ [castFrameHook]        ── 애니 castFrame 도달 (실 발동)
  │                              투사체 스폰 (CGenericPendingHitSystem::Schedule)
  │                              FX 스폰 (Champion::Fx::Spawn*)
  │                              ApplyDamage (단일 hit 인 BA 등)
  │
  └─ [recoveryHook]         ── recoveryFrame 도달 (복귀)
                                 칼리스타 BA dash 같은 후속 모션
                                 endTransition 진입 직전 cleanup
```

### 2.2 Dispatch bool 반환 (D-2)

```cpp
class CSkillHookRegistry
{
    // ★ bool 반환 — handled 면 legacy fallback skip
    bool Dispatch(uint32_t hookId, SkillHookContext& ctx) const;
};
```

```cpp
// Scene_InGame.cpp 사용 패턴:
bool handled = false;
if (m_pActiveSkillDef->castFrameHookId != 0)
    handled = CSkillHookRegistry::Instance().Dispatch(m_pActiveSkillDef->castFrameHookId, ctx);

if (!handled)
{
    // legacy if-elif (Garen/Zed/Riven 등)
}
```

### 2.3 Damage 공통 경로 (B-3)

```cpp
// Client/Public/Gameplay/Damage.h
struct DamageRequestComponent
{
    EntityID source = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    f32_t amount = 0.f;
    eTeam sourceTeam = eTeam::Neutral;
    uint32_t flags = 0;   // physical/magic/true 향후 확장
};

// 즉시 적용 (간단한 BA hit)
void ApplyDamage(CWorld& world, EntityID source, eTeam srcTeam,
    EntityID target, f32_t amount);

// 큐잉 (PendingHit 같은 지연 적용)
void EnqueueDamage(CWorld& world, const DamageRequestComponent& req);
```

→ Scene_InGame::ApplyEzrealHit / ApplyRivenHit / ApplyGarenHit / ApplyZedHit / ApplyYasuoHit **모두 ApplyDamage 한 줄로 흡수 가능** (B-11d-bis 마이그레이션). 본 사이클은 Ezreal 만 ApplyDamage 사용.

### 2.4 Generic Projectile (B-4)

```cpp
// Client/Public/GameObject/Combat/ProjectileKind.h
enum class eProjectileKind : uint16_t
{
    Generic = 0,
    // Yasuo (alias 호환)
    Wind, Tornado, EQRing,
    // Ezreal
    MysticShot, EssenceFlux, GlobalBeam,
    // 향후 챔프 추가
    PROJECTILE_END
};

// 호환 alias — 야스오 코드 무수정
using eYasuoProjectileKind_Compat = eProjectileKind;
```

`CGenericPendingHitSystem::Schedule` — `CPendingHitSystem::Schedule` 1:1 시그니처 + enum 만 일반화. 기존 야스오 `CPendingHitSystem` 은 Schedule 안에서 GenericPendingHitSystem 으로 forwarding.

### 2.5 BanPick 자동 UI (D-3)

```cpp
// Scene_BanPick.cpp v3.1
ImGui::Begin("Champion Select");
CChampionRegistry::Instance().ForEach(
    [&](eChampion champ, const ChampionDef& def)
    {
        if (ImGui::Button(def.displayName, ImVec2(150.f, 60.f)))
        {
            CGameInstance::Get()->Get_GameContext().SelectedChampion = champ;
            // ... 기존 매치로딩 흐름 ...
        }
        ImGui::SameLine();
    });
ImGui::End();
```

→ ChampionDef 에 `displayName` 필드 추가 (UI 전용). Registration 시 채움. **신규 챔프 = Registration.cpp 1 번만 만지면 BanPick 자동**.

### 2.6 eChampion 명시값 (D-4)

```cpp
// Engine/Include/GameContext.h v3.1 — 명시 값 박제
enum class eChampion : uint8_t
{
    NONE     = 0,    // sentinel
    IRELIA   = 1,
    YASUO    = 2,
    KALISTA  = 3,
    SYLAS    = 4,
    VIEGO    = 5,
    ANNIE    = 6,
    ASHE     = 7,
    FIORA    = 8,
    GAREN    = 9,
    RIVEN    = 10,
    ZED      = 11,
    EZREAL   = 12,   // ← Phase B-11d
    YONE     = 13,
    JAX      = 14,
    MASTERYI = 15,
    KINDRED  = 16,
    // ... 신규 챔프는 빈 ID 채움 (절대 삭제 X — 네트워크/DB 호환)
    END      = 255   // sentinel only — 직렬화 X
};
```

★ **수정 규칙** (CLAUDE.md gotcha 박제):
- 추가 = 다음 빈 ID 사용
- 삭제 = enum 값 유지 + 코드 path 만 disable (DB/세이브 호환)
- 재사용 = 절대 금지

---

## 3. 6 레이어 매핑 (v3.1)

| Layer | 책임 | 파일 | 변경 |
|-------|------|------|------|
| 1. 자원 | ECS owned `unique_ptr<ModelRenderer>` | `m_ChampionRenderers` | 변경 0 (Riven 박제) |
| 2. 상태 | `EzrealStateComponent` | `Client/Public/Champions/Ezreal/Ezreal_Components.h` (Phase 4 직전 `Shared/` 이전) | D-5 |
| 3. 정의 | self-register | `Client/Private/Champions/Ezreal/Ezreal_Registration.cpp` | hookId 4 분할 |
| 4. 로직 | hook 함수 + ApplyDamage + GenericPendingHit | `Client/Private/Champions/Ezreal/Ezreal_Skills.cpp` | Scene 의존 0 |
| 5. 연출 | namespace `Ezreal::Fx` | `Client/Public/Champions/Ezreal/Ezreal_FxPresets.h/cpp` | v2 코드 그대로 |
| 6. 통합 | DispatchHook bool + BanPick 자동 | `Scene_InGame` + `Scene_BanPick` | D-2, D-3 |

---

## 4. 계단식 검증 마일스톤 (★ Codex 권장 순서 반영)

```
Stage 1  Asset 변환 (winters binary)
Stage 2  Registry 3종 + SkillDef 4 hookId + Dispatch bool 반환    [★ 인프라]
Stage 3  SkillRegistry lookup 통합 (DispatchSkillInput)             [★ 즉시 검증]
Stage 4  ChampionRegistry 기반 BanPick 자동 UI                       [★ 운영형]
Stage 5  eChampion 명시값 박제                                       [★ 프로토콜]
Stage 6  GenericPendingHit + eProjectileKind + DamageRequest + ApplyDamage [★ 공통화]
Stage 7  CreateECSChampion → ChampionRegistry 우선 lookup
Stage 8  Champions/Ezreal/ 폴더 (self-register + hook 함수 + components + FX)
Stage 9  Scene_InGame DispatchHook 4 곳 (keySwap / onCastAccepted / castFrame / recovery)
   ↓ F5 #1: 모델 + idle/run + camera + 회귀 9 챔프 0 + Riven anim 2배속 0
Stage 10 Ezreal_Skills hook 본격 (BA/Q/W/E/R 5 hook + E 텔포 + E angle swap)
   ↓ F5 #2: 풀 동작
Stage 11 회귀 검증 + CLAUDE/MEMORY/Architecture 박제
```

---

## 5. Stage 1 — Asset 변환

`Tools/convert_all_assets.bat` Riven 다음:
```bat
call :convert_champ "Ezreal" "ezreal.fbx"
```

검증:
```bat
dir Client\Bin\Resource\Texture\Character\Ezreal\anims\*.wanim
WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Ezreal\ezreal.wskel
WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Ezreal\ezreal.wmesh
```

| 검증 | 합격 |
|------|------|
| `wmesh.stride` == 76 | ✅ |
| bone_count match | ✅ |
| skel_hash match | ✅ |

---

## 6. Stage 2 — Registry 인프라 + SkillDef 확장

### 6.1 `Client/Public/Gameplay/SkillHookContext.h` (★ 분리, B-2)

```cpp
#pragma once

// ★ 명시 의존성 (B-2 정정)
#include "WintersTypes.h"                   // f32_t, u32_t
#include "ECS/Entity.h"                     // EntityID, NULL_ENTITY
#include "GameContext.h"                    // eChampion
#include "ECS/Components/GameplayComponents.h"   // eTeam
#include "GameObject/SkillDef.h"            // SkillDef, CastSkillCommand

#include <string>

class CWorld;

// ─────────────────────────────────────────────────────────────
//  SkillHookContext — 모든 hook 함수의 공통 인자 (POD-ish)
//  ★ pSceneOpaque 제거 (B-3 — Scene 의존 안티패턴 회피)
//  ★ pKeyOut 추가 (D-1 — keySwap hook 결과 채널)
// ─────────────────────────────────────────────────────────────
struct SkillHookContext
{
    CWorld* pWorld = nullptr;
    EntityID casterEntity = NULL_ENTITY;
    eTeam casterTeam = eTeam::Blue;
    const SkillDef* pDef = nullptr;
    const CastSkillCommand* pCommand = nullptr;
    f32_t fDeltaTime = 0.f;

    // keySwap hook 전용 — 다른 hook 에서는 nullptr
    std::string* pKeyOut = nullptr;
};
```

### 6.2 `Client/Public/Gameplay/SkillHookRegistry.h` (★ Dispatch bool, D-2)

```cpp
#pragma once

#include "Gameplay/SkillHookContext.h"
#include "GameContext.h"

#include <cstdint>
#include <functional>
#include <unordered_map>

class CSkillHookRegistry
{
public:
    using HookFn = std::function<void(SkillHookContext&)>;

    static CSkillHookRegistry& Instance();

    void Register(uint32_t hookId, HookFn fn);

    // ★ bool 반환 — true 면 fallback skip (D-2)
    bool Dispatch(uint32_t hookId, SkillHookContext& ctx) const;

    bool Has(uint32_t hookId) const;
    size_t Count() const { return m_Map.size(); }

private:
    CSkillHookRegistry() = default;
    ~CSkillHookRegistry() = default;
    CSkillHookRegistry(const CSkillHookRegistry&) = delete;
    CSkillHookRegistry& operator=(const CSkillHookRegistry&) = delete;

    std::unordered_map<uint32_t, HookFn> m_Map;
};

// hookId 빌드 — 16 bit champ + 16 bit variant
constexpr uint32_t MakeHookId(eChampion champ, uint16_t variant)
{
    return (static_cast<uint32_t>(champ) << 16) | variant;
}

namespace HookVariant
{
    // ── 4 단계 분류 (D-1) ──
    constexpr uint16_t BA_KeySwap        = 0x0011;
    constexpr uint16_t Q_KeySwap         = 0x0012;
    constexpr uint16_t W_KeySwap         = 0x0013;
    constexpr uint16_t E_KeySwap         = 0x0014;
    constexpr uint16_t R_KeySwap         = 0x0015;

    constexpr uint16_t BA_OnCastAccepted = 0x0021;   // 입력 승인 직후
    constexpr uint16_t Q_OnCastAccepted  = 0x0022;
    constexpr uint16_t W_OnCastAccepted  = 0x0023;
    constexpr uint16_t E_OnCastAccepted  = 0x0024;   // Ezreal E 텔포 (즉시 위치 변경)
    constexpr uint16_t R_OnCastAccepted  = 0x0025;   // Riven R bUlted=true

    constexpr uint16_t BA_CastFrame      = 0x0031;   // 평타 hit
    constexpr uint16_t Q_CastFrame       = 0x0032;   // 투사체 스폰
    constexpr uint16_t W_CastFrame       = 0x0033;
    constexpr uint16_t E_CastFrame       = 0x0034;
    constexpr uint16_t R_CastFrame       = 0x0035;

    constexpr uint16_t BA_Recovery       = 0x0041;   // 칼리스타 BA dash 같은 후속
    constexpr uint16_t Q_Recovery        = 0x0042;
    constexpr uint16_t W_Recovery        = 0x0043;
    constexpr uint16_t E_Recovery        = 0x0044;
    constexpr uint16_t R_Recovery        = 0x0045;
}
```

### 6.3 `Client/Private/Gameplay/SkillHookRegistry.cpp`

```cpp
#include "Gameplay/SkillHookRegistry.h"

CSkillHookRegistry& CSkillHookRegistry::Instance()
{
    static CSkillHookRegistry s_inst;
    return s_inst;
}

void CSkillHookRegistry::Register(uint32_t hookId, HookFn fn)
{
    auto [it, inserted] = m_Map.try_emplace(hookId, std::move(fn));
    if (!inserted)
    {
        char buf[160];
        sprintf_s(buf, "[SkillHookRegistry] DUPLICATE hookId=0x%08X — keeping first\n", hookId);
        OutputDebugStringA(buf);
    }
}

bool CSkillHookRegistry::Dispatch(uint32_t hookId, SkillHookContext& ctx) const
{
    if (hookId == 0) return false;
    auto it = m_Map.find(hookId);
    if (it == m_Map.end()) return false;
    it->second(ctx);
    return true;
}

bool CSkillHookRegistry::Has(uint32_t hookId) const
{
    return m_Map.find(hookId) != m_Map.end();
}
```

### 6.4 `Client/Public/Gameplay/ChampionRegistry.h` (★ ForEach API, D-3)

```cpp
#pragma once

#include "GameObject/ChampionDef.h"
#include "GameContext.h"

#include <unordered_map>
#include <functional>

class CChampionRegistry
{
public:
    using IterFn = std::function<void(eChampion, const ChampionDef&)>;

    static CChampionRegistry& Instance();

    void Add(eChampion id, const ChampionDef& def);
    const ChampionDef* Find(eChampion id) const;

    // ★ BanPick UI 자동 생성용 (D-3)
    void ForEach(const IterFn& fn) const;

    size_t Count() const { return m_Map.size(); }

private:
    CChampionRegistry() = default;
    ~CChampionRegistry() = default;
    CChampionRegistry(const CChampionRegistry&) = delete;
    CChampionRegistry& operator=(const CChampionRegistry&) = delete;

    std::unordered_map<eChampion, ChampionDef> m_Map;
};
```

### 6.5 `Client/Private/Gameplay/ChampionRegistry.cpp`

```cpp
#include "Gameplay/ChampionRegistry.h"

CChampionRegistry& CChampionRegistry::Instance()
{
    static CChampionRegistry s_inst;
    return s_inst;
}

void CChampionRegistry::Add(eChampion id, const ChampionDef& def)
{
    auto [it, inserted] = m_Map.try_emplace(id, def);
    if (!inserted)
    {
        char buf[160];
        sprintf_s(buf, "[ChampionRegistry] DUPLICATE id=%u\n", static_cast<u32_t>(id));
        OutputDebugStringA(buf);
    }
}

const ChampionDef* CChampionRegistry::Find(eChampion id) const
{
    auto it = m_Map.find(id);
    return (it != m_Map.end()) ? &it->second : nullptr;
}

void CChampionRegistry::ForEach(const IterFn& fn) const
{
    for (const auto& [id, def] : m_Map) fn(id, def);
}
```

### 6.6 `Client/Public/Gameplay/SkillRegistry.h/.cpp`

(v3 와 동일 — Add / Find / Count. 변경 0)

### 6.7 `Client/Public/GameObject/SkillDef.h` 확장 (★ 4 hookId, D-1)

기존 SkillDef 구조체 끝에 추가:

```cpp
struct SkillDef
{
    // ... 기존 필드 다 그대로 (rangeMax, rotate 등 — B-1 검증) ...

    // ★ Phase B-11d v3.1 — 4 단계 hookId (POD 유지, FlatBuffers 호환)
    //   기본 0 = no-op → 기존 챔프 (Garen/Zed/Riven) 영향 0
    uint32_t keySwapHookId        = 0;
    uint32_t onCastAcceptedHookId = 0;
    uint32_t castFrameHookId      = 0;
    uint32_t recoveryHookId       = 0;
};
```

### 6.8 `Client/Public/GameObject/ChampionDef.h` 확장 (★ displayName, D-3)

```cpp
struct ChampionDef
{
    // ... 기존 필드 (animPrefix / idleAnimKey / fbxPath / texturePath 등) ...

    // ★ Phase B-11d v3.1 — BanPick 자동 UI 용 (D-3)
    const char* displayName = nullptr;   // "Ezreal" — nullptr fallback = enum 이름
};
```

### 6.9 vcxproj 등록

```xml
<ClInclude Include="..\Public\Gameplay\SkillHookContext.h" />
<ClInclude Include="..\Public\Gameplay\SkillHookRegistry.h" />
<ClInclude Include="..\Public\Gameplay\ChampionRegistry.h" />
<ClInclude Include="..\Public\Gameplay\SkillRegistry.h" />
<ClCompile Include="..\Private\Gameplay\SkillHookRegistry.cpp" />
<ClCompile Include="..\Private\Gameplay\ChampionRegistry.cpp" />
<ClCompile Include="..\Private\Gameplay\SkillRegistry.cpp" />
```

`.vcxproj.filters`: 새 필터 `02. Gameplay`.

---

## 7. Stage 3 — SkillRegistry lookup 통합

`DispatchSkillInput` 안 `FindSkillDef` 호출 (★ 즉시 검증):

```cpp
// before:
const SkillDef* def = FindSkillDef(champ, slot);

// after:
const SkillDef* def = CSkillRegistry::Instance().Find(champ, slot);
if (!def) def = FindSkillDef(champ, slot);   // legacy fallback (Riven 등)
```

**검증** (Stage 3 단독): 빌드 통과 + Riven Q 정상 동작 (legacy fallback hit). Ezreal 미진입.

---

## 8. Stage 4 — BanPick 자동 UI (D-3)

`Client/Private/Scene/Scene_BanPick.cpp` 의 수동 9 버튼을 **Registry 순회**로 교체:

```cpp
// before: 9 개 버튼 수동 나열 (Irelia/Yasuo/Kalista/Garen/Zed/Riven/...)

// after:
ImGui::Begin("Champion Select");
{
    int count = 0;
    CChampionRegistry::Instance().ForEach(
        [&](eChampion champ, const ChampionDef& def)
        {
            const char* label = def.displayName ? def.displayName : "(unnamed)";
            if (ImGui::Button(label, ImVec2(150.f, 60.f)))
            {
                CGameInstance::Get()->Get_GameContext().SelectedChampion = champ;
                auto pLoadingMatch = CScene_MatchLoading::Create(
                    []() -> std::unique_ptr<IScene> {
                        return std::unique_ptr<IScene>(new CScene_InGame());
                    }, 3.f);
                CGameInstance::Get()->Change_Scene(
                    (uint32_t)eSceneID::MatchLoading, std::move(pLoadingMatch));
                ImGui::End();
                return;
            }
            // 4 개씩 줄바꿈 (5번째마다 SameLine 끊기)
            if ((++count % 4) != 0) ImGui::SameLine();
        });
}
ImGui::End();
```

**검증** (Stage 4 단독): Ezreal Registration 안 됐으니 9 챔프 (Riven 까지) 만 보여야 함. **자동 생성 검증**. 신규 챔프 = Registration → BanPick 자동 (★ 운영형 1차 검증).

> **현 9 챔프 ChampionDef 가 ChampionRegistry 에 안 들어있음** — 기존 Garen/Zed 등은 `s_ChampionTable[]` 에만 있고 Registry 비어있음. 본 사이클에서 마이그레이션 옵션 2 가지:
>
> **옵션 A (권장)**: `CGameApp::OnInit` 에서 `s_ChampionTable[]` 순회하며 `CChampionRegistry::Add` 자동 등록 (1회). 기존 챔프 9 명 + Ezreal Registration → 총 10 명 BanPick. Phase B-11d-bis 에서 챔프별 Registration.cpp 로 이전.
>
> **옵션 B**: BanPick 만 Registry + legacy 둘 다 순회 (Set 으로 중복 제거). 더 복잡.
>
> → **A 채택**: `ChampionTable.cpp` 끝에 `RegisterAllLegacy()` 추가, `CGameApp::OnInit` 에서 1회 호출.

```cpp
// ChampionTable.cpp 끝에 추가:
void RegisterAllLegacy()
{
    for (const auto& cd : s_ChampionTable)
    {
        CChampionRegistry::Instance().Add(cd.id, cd);
    }
}

// CGameApp::OnInit 에서:
RegisterAllLegacy();
```

→ 기존 9 챔프 (`displayName = nullptr` → fallback 으로 enum 이름) 자동 표시.

> **차후 (B-11d-bis)**: 9 챔프 각각 `Champions/<Name>/<Name>_Registration.cpp` 추출 → `s_ChampionTable[]` + `RegisterAllLegacy` 폐기. **본 사이클 책임 X**.

---

## 9. Stage 5 — eChampion 명시값 박제 (D-4)

`Engine/Include/GameContext.h` 갱신:

```cpp
enum class eChampion : uint8_t
{
    NONE     = 0,
    IRELIA   = 1,
    YASUO    = 2,
    KALISTA  = 3,
    SYLAS    = 4,
    VIEGO    = 5,
    ANNIE    = 6,
    ASHE     = 7,
    FIORA    = 8,
    GAREN    = 9,
    RIVEN    = 10,
    ZED      = 11,
    EZREAL   = 12,
    YONE     = 13,
    JAX      = 14,
    MASTERYI = 15,
    KINDRED  = 16,
    END      = 255   // sentinel only
};
```

★ **수정 규칙 (CLAUDE.md gotcha 박제)**:
- 추가 = 다음 빈 ID 사용 (16 다음 = 17)
- 삭제 = 절대 X — code path 만 disable
- 재사용 = 절대 X — DB 호환 깨짐

**검증**: 기존 enum 순서 변경 없음 (IRELIA→YASUO→... 순). 명시값만 추가. switch / `static_cast<u32_t>` 사용처 모두 호환.

---

## 10. Stage 6 — GenericPendingHit + DamageRequest (B-3, B-4)

### 10.1 `Client/Public/GameObject/Combat/ProjectileKind.h`

```cpp
#pragma once
#include <cstdint>

enum class eProjectileKind : uint16_t
{
    Generic       = 0,
    // Yasuo (alias 호환)
    Wind          = 1,
    Tornado       = 2,
    EQRing        = 3,
    // Ezreal
    MysticShot    = 10,
    EssenceFlux   = 11,
    GlobalBeam    = 12,
    PROJECTILE_END
};
```

기존 `eYasuoProjectileKind.h` 옆에 alias:
```cpp
// Client/Public/GameObject/Champion/Yasuo/YasuoProjectileSystem.h
using eYasuoProjectileKind = eProjectileKind;   // 호환 — 기존 코드 무수정
```

### 10.2 `CGenericPendingHitSystem` — `CPendingHitSystem` 그대로 rename

★ 1차 단순 안: `CPendingHitSystem` 클래스 이름 유지 + Schedule 시그니처의 `eYasuoProjectileKind` → `eProjectileKind` 로 변경 (alias 덕분에 기존 호출 무수정). **새 클래스 신설 X — 기존 클래스 일반화만**.

위치 이동: `Client/Public/GameObject/Champion/Yasuo/PendingHitSystem.h` → `Client/Public/GameObject/Combat/PendingHitSystem.h` (Yasuo 종속 제거). `#include` 사용처 (Scene_InGame.cpp 등) 경로 갱신.

### 10.3 `Client/Public/Gameplay/Damage.h/.cpp`

```cpp
// Damage.h
#pragma once
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"   // eTeam

class CWorld;

struct DamageRequestComponent
{
    EntityID source = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    f32_t amount = 0.f;
    eTeam sourceTeam = eTeam::Neutral;
    uint32_t flags = 0;
};

// 즉시 적용 — 단일 hit (BA, R 처형)
void ApplyDamage(CWorld& world, EntityID source, eTeam srcTeam,
    EntityID target, f32_t amount);

// 큐잉 — 향후 ProcessDamageQueue 시스템에서 일괄 처리
void EnqueueDamage(CWorld& world, const DamageRequestComponent& req);
```

```cpp
// Damage.cpp
#include "Gameplay/Damage.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/HealthComponent.h"

void ApplyDamage(CWorld& world, EntityID source, eTeam srcTeam,
    EntityID target, f32_t amount)
{
    if (target == NULL_ENTITY || target == source) return;
    if (!world.HasComponent<ChampionComponent>(target)) return;

    auto& champion = world.GetComponent<ChampionComponent>(target);
    if (champion.team == srcTeam) return;   // 같은 팀 friendly fire 방지

    champion.hp = (champion.hp > amount) ? (champion.hp - amount) : 0.f;

    if (world.HasComponent<HealthComponent>(target))
    {
        auto& hp = world.GetComponent<HealthComponent>(target);
        hp.fCurrent = champion.hp;
        hp.fMaximum = champion.maxHp;
        hp.bIsDead = (hp.fCurrent <= 0.f);
    }

    char buf[128];
    sprintf_s(buf, "[Damage] src=%u→tgt=%u amount=%.1f hp=%.1f/%.1f\n",
        static_cast<u32_t>(source), static_cast<u32_t>(target), amount,
        champion.hp, champion.maxHp);
    OutputDebugStringA(buf);
}

void EnqueueDamage(CWorld& world, const DamageRequestComponent& req)
{
    EntityID e = world.CreateEntity();
    world.AddComponent<DamageRequestComponent>(e, req);
    // 별도 ProcessDamageQueue 시스템이 다음 프레임에 ApplyDamage + Destroy
}
```

→ Ezreal hook 은 **`ApplyDamage(world, caster, casterTeam, target, dmg)` 한 줄** 호출. Scene 의존 0. Riven/Garen 마이그레이션 시 `ApplyRivenHit`/`ApplyGarenHit` 도 동일 호출로 흡수 가능.

---

## 11. Stage 7 — CreateECSChampion ChampionRegistry 우선

(Riven 박제 그대로 + Registry lookup 추가):

```cpp
EntityID CScene_InGame::CreateECSChampion(eChampion id, eTeam team)
{
    // ★ v3.1: Registry 우선
    const ChampionDef* cd = CChampionRegistry::Instance().Find(id);
    if (!cd) cd = FindChampionDef(id);   // legacy fallback

    if (!cd || !cd->fbxPath) { /* FAIL log + return NULL_ENTITY */ }

    // 나머지 (ModelRenderer alloc + components + map 저장) Riven 박제 그대로
    // ...
}
```

---

## 12. Stage 8 — Champions/Ezreal/ 폴더

### 12.1 `Client/Public/Champions/Ezreal/Ezreal_Components.h`

```cpp
#pragma once
#include "Engine_Defines.h"

// ★ D-5 : Phase 4 직전 Shared/Champions/Ezreal/ 로 이전
//   서버 시뮬레이션도 이 컴포넌트를 봐야 함 (네트워크 직렬화 대상)
struct EzrealStateComponent
{
    f32_t fTeleportDistance = 4.75f;
    f32_t fGlobalLifetime = 6.0f;
    f32_t fGlobalSpeed = 25.f;
    uint8_t baCounter = 0;
};
```

### 12.2 `Client/Public/Champions/Ezreal/Ezreal_Skills.h`

```cpp
#pragma once
#include "Gameplay/SkillHookContext.h"

namespace Ezreal
{
    void OnCastFrame_BA(SkillHookContext& ctx);
    void OnCastFrame_Q(SkillHookContext& ctx);
    void OnCastFrame_W(SkillHookContext& ctx);
    void OnCastFrame_R(SkillHookContext& ctx);

    void OnCastAccepted_E(SkillHookContext& ctx);   // ★ 텔포 (D-1 4 단계)
    void OnKeySwap_E(SkillHookContext& ctx);        // ★ angle anim
}

// static init dead-strip 방지 — Scene_InGame::OnEnter 에서 호출
void Ezreal_KeepAlive();
```

### 12.3 `Client/Public/Champions/Ezreal/Ezreal_FxPresets.h`

```cpp
#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Ezreal::Fx
{
    void SpawnBAProjectile(CWorld&, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnQProjectile(CWorld&, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnWProjectile(CWorld&, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnEFlash(CWorld&, const Vec3& origin, const Vec3& dest, f32_t fLifetime);
    void SpawnRBow(CWorld&, EntityID owner, f32_t fLifetime);
    void SpawnRProjectile(CWorld&, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
}
```

cpp 본문은 v3 의 EzrealFxPresets.cpp 그대로 (`namespace Ezreal::Fx { ... }` 감쌈). 변경 0.

### 12.4 `Client/Private/Champions/Ezreal/Ezreal_Registration.cpp`

★ 실제 SkillDef 필드명 (`rangeMax`, `rotate`) 사용 (B-1 정정).

```cpp
#include "Gameplay/ChampionRegistry.h"
#include "Gameplay/SkillRegistry.h"
#include "Gameplay/SkillHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "Champions/Ezreal/Ezreal_Skills.h"

namespace
{
    // ── hookId 상수 (4 단계 분류, D-1) ──
    constexpr uint32_t kEz_BA_Cast      = MakeHookId(eChampion::EZREAL, HookVariant::BA_CastFrame);
    constexpr uint32_t kEz_Q_Cast       = MakeHookId(eChampion::EZREAL, HookVariant::Q_CastFrame);
    constexpr uint32_t kEz_W_Cast       = MakeHookId(eChampion::EZREAL, HookVariant::W_CastFrame);
    constexpr uint32_t kEz_E_OnAccept   = MakeHookId(eChampion::EZREAL, HookVariant::E_OnCastAccepted);
    constexpr uint32_t kEz_E_KeySwap    = MakeHookId(eChampion::EZREAL, HookVariant::E_KeySwap);
    constexpr uint32_t kEz_R_Cast       = MakeHookId(eChampion::EZREAL, HookVariant::R_CastFrame);

    struct EzrealAutoRegister
    {
        EzrealAutoRegister()
        {
            // ── 1. ChampionDef ──
            ChampionDef cd{};
            cd.id = eChampion::EZREAL;
            cd.animPrefix = "ezreal_";
            cd.idleAnimKey = "idle";
            cd.runAnimKey = "run";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 5.5f;
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Ezreal/ezreal.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.texturePath[0] = L"Client/Bin/Resource/Texture/Character/Ezreal/ezreal_base_tx_cm.png";
            cd.spawnPosition = { 27.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Ezreal";   // ★ BanPick UI (D-3)
            CChampionRegistry::Instance().Add(eChampion::EZREAL, cd);

            // ── 2. SkillDef[5] ── (★ 실제 필드명 rangeMax/rotate, B-1)
            // BA
            {
                SkillDef s{};
                s.champ = eChampion::EZREAL; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.5f;
                s.rangeMax = 5.5f;          // ★ rangeMax (not range)
                s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.65f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;   // ★ rotate (not rotateMode)
                s.castFrame = 6.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kEz_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::EZREAL, 0, s);
            }
            // Q — Mystic Shot
            {
                SkillDef s{};
                s.champ = eChampion::EZREAL; s.slot = 1;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 6.f; s.rangeMax = 11.f;
                s.animKey = "spell1"; s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 4.f; s.recoveryFrame = 10.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kEz_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::EZREAL, 1, s);
            }
            // W — Essence Flux
            {
                SkillDef s{};
                s.champ = eChampion::EZREAL; s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 8.f; s.rangeMax = 10.f;
                s.animKey = "spell2"; s.lockDurationSec = 0.5f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 4.f; s.recoveryFrame = 10.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kEz_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::EZREAL, 2, s);
            }
            // E — Arcane Shift
            {
                SkillDef s{};
                s.champ = eChampion::EZREAL; s.slot = 3;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 16.f; s.rangeMax = 4.75f;
                s.animKey = "spell3_generic"; s.lockDurationSec = 0.6f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 1.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.onCastAcceptedHookId = kEz_E_OnAccept;   // ★ 텔포 = OnCastAccepted (D-1)
                s.keySwapHookId        = kEz_E_KeySwap;    // ★ angle 기반 anim
                CSkillRegistry::Instance().Add(eChampion::EZREAL, 3, s);
            }
            // R — Trueshot Barrage
            {
                SkillDef s{};
                s.champ = eChampion::EZREAL; s.slot = 4;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 120.f; s.rangeMax = 200.f;   // 글로벌
                s.animKey = "spell4"; s.lockDurationSec = 1.0f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 16.f; s.recoveryFrame = 24.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kEz_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::EZREAL, 4, s);
            }

            // ── 3. Hook 함수 등록 ──
            CSkillHookRegistry::Instance().Register(kEz_BA_Cast,    &Ezreal::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kEz_Q_Cast,     &Ezreal::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kEz_W_Cast,     &Ezreal::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kEz_E_OnAccept, &Ezreal::OnCastAccepted_E);
            CSkillHookRegistry::Instance().Register(kEz_E_KeySwap,  &Ezreal::OnKeySwap_E);
            CSkillHookRegistry::Instance().Register(kEz_R_Cast,     &Ezreal::OnCastFrame_R);

            OutputDebugStringA("[Ezreal] Registration complete\n");
        }
    };
    static EzrealAutoRegister s_register;
}

void Ezreal_KeepAlive()
{
    // dead-strip 방지 — Scene_InGame::OnEnter 가 호출
    (void)&s_register;
}
```

### 12.5 `Client/Private/Champions/Ezreal/Ezreal_Skills.cpp` (★ Scene 의존 0, B-3)

```cpp
#include "Champions/Ezreal/Ezreal_Skills.h"
#include "Champions/Ezreal/Ezreal_Components.h"
#include "Champions/Ezreal/Ezreal_FxPresets.h"
#include "Gameplay/Damage.h"                          // ★ ApplyDamage (B-3 — Scene 의존 X)
#include "GameObject/SkillDef.h"
#include "GameObject/Combat/PendingHitSystem.h"       // ★ Generic enum (B-4)
#include "GameObject/Combat/ProjectileKind.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include <cmath>
#include <string>

namespace Ezreal
{
    static Vec3 GetMuzzlePos(CWorld& w, EntityID e)
    {
        Vec3 p{};
        if (w.HasComponent<TransformComponent>(e))
        {
            auto& tf = w.GetComponent<TransformComponent>(e);
            p = tf.GetWorldPosition();
            p.y += 1.0f;
        }
        return p;
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY) return;

        Fx::SpawnBAProjectile(*ctx.pWorld, ctx.casterEntity,
            GetMuzzlePos(*ctx.pWorld, ctx.casterEntity),
            ctx.pCommand->direction, 0.4f);

        // ★ ApplyDamage (B-3 — Scene 의존 0)
        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, 55.f);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);
        Vec3 dir = ctx.pCommand->direction;
        Fx::SpawnQProjectile(*ctx.pWorld, ctx.casterEntity, origin, dir, 1.0f);

        // ★ Generic enum (B-4)
        CPendingHitSystem::Schedule(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, dir,
            0.f, eProjectileKind::MysticShot,
            30.f, 30.f, 0.6f, 70.f, 0.f);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);
        Vec3 dir = ctx.pCommand->direction;
        Fx::SpawnWProjectile(*ctx.pWorld, ctx.casterEntity, origin, dir, 1.5f);
        CPendingHitSystem::Schedule(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, dir,
            0.f, eProjectileKind::EssenceFlux,
            18.f, 27.f, 0.8f, 65.f, 0.f);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld->HasComponent<EzrealStateComponent>(ctx.casterEntity)) return;
        auto& es = ctx.pWorld->GetComponent<EzrealStateComponent>(ctx.casterEntity);
        Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);
        Vec3 dir = ctx.pCommand->direction;

        Fx::SpawnRBow(*ctx.pWorld, ctx.casterEntity, 0.4f);
        Fx::SpawnRProjectile(*ctx.pWorld, ctx.casterEntity, origin, dir, es.fGlobalLifetime);
        CPendingHitSystem::Schedule(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, dir,
            0.f, eProjectileKind::GlobalBeam,
            es.fGlobalSpeed, es.fGlobalSpeed * es.fGlobalLifetime, 1.2f,
            250.f, 0.f);
    }

    // ★ E 텔포 — OnCastAccepted (D-1) — 입력 승인 직후 위치 변경
    void OnCastAccepted_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<EzrealStateComponent>(ctx.casterEntity)) return;

        auto& tf = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity);
        auto& es = ctx.pWorld->GetComponent<EzrealStateComponent>(ctx.casterEntity);

        Vec3 origin = tf.GetWorldPosition();
        Vec3 dir = ctx.pCommand->direction;
        dir.y = 0.f;
        Vec3 dest = { origin.x + dir.x * es.fTeleportDistance,
                      origin.y,
                      origin.z + dir.z * es.fTeleportDistance };

        tf.SetPosition(dest);
        tf.m_bLocalDirty = true;
        tf.m_bWorldDirty = true;

        Fx::SpawnEFlash(*ctx.pWorld, origin, dest, 0.4f);

        char dbg[160]{};
        sprintf_s(dbg, "[Ezreal E TP] from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f)\n",
            origin.x, origin.y, origin.z, dest.x, dest.y, dest.z);
        ::OutputDebugStringA(dbg);
    }

    // ★ E anim key swap — angle 기반 (D-1 keySwap 단계)
    void OnKeySwap_E(SkillHookContext& ctx)
    {
        if (!ctx.pKeyOut || !ctx.pCommand) return;
        const Vec3& dir = ctx.pCommand->direction;
        f32_t yawDeg = std::atan2f(dir.x, dir.z) * 57.2958f;

        if      (yawDeg < -135.f || yawDeg > 135.f) *ctx.pKeyOut = "spell3_180";
        else if (yawDeg < -45.f)                    *ctx.pKeyOut = "spell3_-90";
        else if (yawDeg <  45.f)                    *ctx.pKeyOut = "spell3_generic";
        else                                        *ctx.pKeyOut = "spell3_90";
    }
}
```

### 12.6 vcxproj

```xml
<ClInclude Include="..\Public\Champions\Ezreal\Ezreal_Components.h" />
<ClInclude Include="..\Public\Champions\Ezreal\Ezreal_Skills.h" />
<ClInclude Include="..\Public\Champions\Ezreal\Ezreal_FxPresets.h" />
<ClCompile Include="..\Private\Champions\Ezreal\Ezreal_Registration.cpp" />
<ClCompile Include="..\Private\Champions\Ezreal\Ezreal_Skills.cpp" />
<ClCompile Include="..\Private\Champions\Ezreal\Ezreal_FxPresets.cpp" />
```

`.vcxproj.filters`: `03. Champions\Ezreal`.

---

## 13. Stage 9 — Scene_InGame DispatchHook 4 곳 (★ bool 가드, D-2)

### 13.1 ApplyLocalPrediction — keySwap (★ 첫 hook)

`Scene_InGame.cpp` ApplyLocalPrediction 안 `key` 결정 직후, 야스오 Q swap 분기 **다음**:

```cpp
// 야스오 / Riven 기존 swap 분기 그대로 ...

// ★ Phase B-11d v3.1 — DispatchHook keySwap (bool 반환 가드)
if (def.keySwapHookId != 0)
{
    SkillHookContext ctx{};
    ctx.pWorld = &m_World;
    ctx.casterEntity = m_PlayerEntity;
    ctx.casterTeam = m_PlayerTeam;
    ctx.pDef = &def;
    ctx.pCommand = &cmd;
    ctx.pKeyOut = &key;
    CSkillHookRegistry::Instance().Dispatch(def.keySwapHookId, ctx);
    // bool 반환 받지 않음 — keySwap 은 fallback 없음 (chain 가능)
}
```

### 13.2 ApplyLocalPrediction — onCastAccepted (★ 입력 승인 직후, E 텔포)

`Scene_InGame.cpp` ApplyLocalPrediction 안 `key` 결정 + animator play **이후**, post-cast 야스오/Riven 분기 **앞**:

```cpp
// 야스오/Riven post-cast 기존 분기 ...

// ★ DispatchHook onCastAccepted (bool 가드, D-2)
bool acceptedHandled = false;
if (def.onCastAcceptedHookId != 0)
{
    SkillHookContext ctx{};
    ctx.pWorld = &m_World;
    ctx.casterEntity = m_PlayerEntity;
    ctx.casterTeam = m_PlayerTeam;
    ctx.pDef = &def;
    ctx.pCommand = &cmd;
    acceptedHandled = CSkillHookRegistry::Instance().Dispatch(def.onCastAcceptedHookId, ctx);
}

if (!acceptedHandled)
{
    // legacy fallback (없으면 비움) — Riven Q post-cast 같은 야스오/Riven 기존 분기
    // 본 사이클에서 신규 추가 X — Riven 마이그레이션 시 acceptedHandled 위로 옮김
}
```

### 13.3 OnUpdate — castFrame (★ 본격 hook)

`Scene_InGame.cpp:937` 영역, 기존 챔프별 if-elif **앞**:

```cpp
if (bCastHit)
{
    m_bCastFrameFired = true;

    // ★ DispatchHook castFrame (bool 가드, D-2)
    bool castHandled = false;
    if (m_pActiveSkillDef && m_pActiveSkillDef->castFrameHookId != 0)
    {
        SkillHookContext ctx{};
        ctx.pWorld = &m_World;
        ctx.casterEntity = m_PlayerEntity;
        ctx.casterTeam = m_PlayerTeam;
        ctx.pDef = m_pActiveSkillDef;
        ctx.pCommand = &m_ActiveSkillCommandStorage;
        castHandled = CSkillHookRegistry::Instance().Dispatch(
            m_pActiveSkillDef->castFrameHookId, ctx);
    }

    if (!castHandled)
    {
        // legacy fallback — 기존 champCur if-elif (Garen/Zed/Riven) 그대로
        using namespace Engine;
        const eChampion champCur = CGameInstance::Get()->Get_GameContext().SelectedChampion;
        if (champCur == eChampion::KALISTA && /* ... */) { /* 기존 */ }
        // ... (Ezreal 분기는 추가 X — DispatchHook 가 처리)
    }
}
```

### 13.4 OnUpdate — recovery (★ 신규)

`bRecoveryHit` 영역에 동일 패턴 (Stage 9 본격은 1차 보류 — Ezreal 은 recovery hook 미사용. Kalista BA dash 마이그레이션 시 활용):

```cpp
if (bRecoveryHit)
{
    m_bRecoveryFrameFired = true;

    // ★ DispatchHook recovery (B-11d-bis 칼리스타 마이그레이션 시 활용)
    bool recoveryHandled = false;
    if (m_pActiveSkillDef && m_pActiveSkillDef->recoveryHookId != 0)
    {
        SkillHookContext ctx{};
        ctx.pWorld = &m_World;
        ctx.casterEntity = m_PlayerEntity;
        ctx.casterTeam = m_PlayerTeam;
        ctx.pDef = m_pActiveSkillDef;
        ctx.pCommand = &m_ActiveSkillCommandStorage;
        recoveryHandled = CSkillHookRegistry::Instance().Dispatch(
            m_pActiveSkillDef->recoveryHookId, ctx);
    }

    if (!recoveryHandled)
    {
        // 기존 칼리스타 dash 분기 그대로
    }
}
```

### 13.5 CreateECSEntities — Ezreal 1줄 + KeepAlive 호출

`Scene_InGame::OnEnter` 시작부:
```cpp
extern void Ezreal_KeepAlive();
Ezreal_KeepAlive();   // ★ static init dead-strip 방지
```

`CreateECSEntities`:
```cpp
EntityID rivenEntity  = CreateECSChampion(eChampion::RIVEN, eTeam::Blue);
EntityID ezrealEntity = CreateECSChampion(eChampion::EZREAL, eTeam::Blue);

// Player 분기:
else if (champ == eChampion::EZREAL) m_PlayerEntity = ezrealEntity;
```

---

## ⏸ F5 #1 검증 (Stage 1~9 후, BanPick 자동 + 모델 표시)

| 항목 | 합격 |
|------|------|
| 빌드 통과 | Gameplay/ + Champions/Ezreal/ 모든 신규 파일 |
| `[Ezreal] Registration complete` | static init 정상 |
| BanPick UI 자동 — Irelia/Yasuo/Kalista/Garen/Zed/Riven/Ezreal 표시 | ChampionRegistry.ForEach 동작 (★ D-3 검증) |
| Ezreal 클릭 → InGame → 모델 (27,1,0) + idle/run | CreateECSChampion + Riven 박제 인프라 |
| Riven 클릭 → 정상 동작 | legacy fallback (RegisterAllLegacy + Registry hit) |
| 회귀 8 챔프 | 동일 |

**실패 패턴**:
| 증상 | 원인 |
|------|------|
| BanPick 비어있음 | RegisterAllLegacy 미호출 또는 ChampionRegistry::ForEach 람다 시그니처 mismatch |
| Ezreal 모델 안 뜸 | static init dead-strip — `Ezreal_KeepAlive()` 호출 누락 |
| `[Ezreal] Registration` 보임 + `[CreateECSChampion] FAIL` | ChampionDef.fbxPath 누락 |

---

## ⏸ F5 #2 검증 (Stage 10 — Ezreal 풀 동작)

| 항목 | 합격 |
|------|------|
| BA | `[Damage] src=N→tgt=M amount=55.0` (★ ApplyDamage 로그) |
| Q | `[PendingHit ... MysticShot] dmg=70` |
| W | `[PendingHit ... EssenceFlux] dmg=65` |
| E | `[Ezreal E TP from ... to ...]` + 즉시 텔포 + 카메라 자동 + spell3_generic |
| E (좌측) | spell3_-90 anim |
| E (우측) | spell3_90 anim |
| E (뒤) | spell3_180 anim |
| R | `[PendingHit ... GlobalBeam] dmg=250` |
| 회귀 9 챔프 | 시각·동작 0 변화 — 야스오/Riven Q 정상 (legacy fallback) |
| **Scene 의존 0 검증** | grep `pSceneOpaque` 결과 0 (Ezreal_Skills.cpp 안) |
| **bool 가드 동작** | `castHandled=true` 시 fallback skip 로그 |

---

## 14. 사이클 종료 후 갱신할 파일

1. **CLAUDE.md** — 진행/다음:
   - 완료: B-11d v3.1 — Registry 3종 + hookId 4분할 + ChampionRegistry BanPick + GenericPendingHit + DamageRequest + Champions/Ezreal/
   - 다음: B-11d-bis (Riven → Champions/Riven/ 마이그레이션 + 9 v1 챔프 일괄 + Apply*Hit → ApplyDamage 흡수 + ChampionTable.cpp 폐기)
2. **CLAUDE.md** Phase B-11d Gotchas:
   - **G1**: SkillDef 필드명 — `rangeMax` (not `range`), `rotate` (not `rotateMode`). 신규 챔프 Registration 작성 시 1순위 검증
   - **G2**: static init dead-strip — Registration TU 가 unused 일 때 컴파일러/링커 제거 가능. `_KeepAlive()` 함수 export + Scene::OnEnter 호출 강제
   - **G3**: eChampion 명시값 — 추가는 빈 ID, 삭제/재사용 절대 X. 네트워크/DB 호환 박제
   - **G4**: hookId 4분할 의미:
     - keySwap: anim key 결정 (yaw/stack 기반) — `pKeyOut` 채널
     - onCastAccepted: 입력 승인 직후 (위치/스택/상태 전이)
     - castFrame: 애니 castFrame 도달 (FX/투사체/hit)
     - recovery: recoveryFrame 도달 (후속 모션)
   - **G5**: Dispatch bool — 신규 챔프가 hook 채우면 legacy fallback skip. 마이그레이션 중 이중 발동 방지
3. **`.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md`** §10 결정 ⑦ 추가:
   - "⑦ Hook 단계는 `keySwap / onCastAccepted / castFrame / recovery` 4 분할 박제. 단일 `castFrame` 으로 합치지 말 것 — Riven Q stack 갱신과 Ezreal E 텔포가 같은 hook 에 섞이면 의미 혼재"
4. **MEMORY.md** + 신규 메모 `project_phase_b11d_v31_ezreal_scale.md`
5. **Phase 4 직전 TODO 박제**: `Client/Public/Champions/<Name>/_Components.h` → `Shared/Champions/<Name>/_Components.h` 이전 (D-5)

---

## 15. 예상 소요 (v3.1)

| 단계 | 시간 |
|---|---|
| Stage 1 변환 | 4분 |
| Stage 2 Registry 3종 + SkillDef 4 hookId + ChampionDef.displayName + Dispatch bool + vcxproj | 30분 |
| Stage 3 SkillRegistry lookup | 3분 |
| Stage 4 BanPick 자동 UI + RegisterAllLegacy | 10분 |
| Stage 5 eChampion 명시값 | 3분 |
| Stage 6 GenericPendingHit + ProjectileKind + Damage.h/cpp + 야스오 알리아스 + 경로 이동 | 25분 |
| Stage 7 CreateECSChampion Registry 우선 | 3분 |
| Stage 8 Champions/Ezreal/ 폴더 6 파일 + vcxproj | 35분 |
| Stage 9 Scene_InGame DispatchHook 4 곳 + Ezreal_KeepAlive | 15분 |
| F5 #1 검증 | 15분 |
| Stage 10 Ezreal_Skills 본격 (Stage 8 안에서 거의 끝, 디버그) + F5 #2 | 20분 |
| 회귀 검증 | 10분 |
| CLAUDE / MEMORY / Architecture 박제 | 12분 |
| **합계** | **~185분 (3시간 5분)** |

(v3 134분 +51분 — Codex 보정 9건 정식 도입. 51번째 챔프부터는 폴더 1개 = **15~20분/챔프** ChampionDef.displayName 만 채우면 BanPick 자동, Damage.h 만 import 하면 Scene 의존 0)

---

## 16. ★ 학습 체크리스트 (v3.1)

| Q | 모범 답안 |
|---|----------|
| Hook 단계 4 분할의 의미? | keySwap (anim key 결정), onCastAccepted (위치/스택/상태 전이), castFrame (FX/투사체/hit), recovery (후속 모션). Riven Q stack=onCastAccepted, Ezreal E 텔포=onCastAccepted, Ezreal Q 투사체=castFrame |
| Dispatch 가 bool 반환하는 이유? | 마이그레이션 중 신규 hook + legacy fallback 이중 발동 방지. handled=true 면 fallback skip |
| ChampionRegistry.ForEach 가 BanPick UI 자동 만드는 효과? | 신규 챔프 = Registration.cpp 1번 → BanPick 자동. Scene_BanPick.cpp 수동 수정 0. 50/100/150 챔프 운영 가능 |
| eChampion 명시값 박제 이유? | enum 순서 변경이 네트워크/DB/세이브 프로토콜 파괴. EZREAL=12 박힌 후 절대 재사용 X. 추가는 빈 ID |
| Scene 의존 hook (pSceneOpaque cast) 가 안티패턴인 이유? | (1) 서버 권위 시뮬에서 Scene 없음 (2) 모든 챔프가 Scene 메서드에 의존 → Scene 폭발 (3) 인터페이스 미정의 → 변경 시 N 챔프 영향. **ApplyDamage(world, src, srcTeam, tgt, amount) 공통 함수가 정답** |
| eYasuoProjectileKind 재사용이 안티패턴인 이유? | 첫 확장 챔프 Ezreal 이 야스오 enum 쓰면 **모든 후속 챔프가 야스오에 의존**. 일반 enum + alias 호환이 정답 |
| Components 위치 = Shared/ vs Client/Public/Champions/ 차이? | Shared = 서버도 include (Phase 4 네트워크 시뮬 권위). Client = 클라 전용 (FX/UI). 1차는 Client/Public/ 진입 + Phase 4 직전 일괄 이전. 첫 시민에서 박지 않으면 마이그레이션 비용 폭발 |
| 51번째 챔프 (예: 럭스) 추가 시? | Champions/Lux/ 폴더 6 파일 + vcxproj 6 줄. 중앙 테이블 / Scene_InGame / Scene_BanPick / Engine 수정 0. Damage.h + ProjectileKind.h import 만. 15~20분 |

---

## 17. 의존 그래프

```
B-11 v2 Riven (★ 순수 ECS Champion + Codex 4 보정) ─┐
                                                     │
                                                     ▼
        B-11d v3.1 Ezreal (★ 본 사이클 — Registry 3종 + hookId 4분할 + GenericPendingHit + Damage 공통)
                                                     │
                ┌────────────────────────────────────┼────────────────────────────────────┐
                ▼                                    ▼                                    ▼
        B-11d-bis (Riven 마이그레이션 +       B-11e (피오라/요네/...           B-11f (Components → Shared/
        Apply*Hit → ApplyDamage 흡수)         15~20분/챔프)                    Champions/, Phase 4 대비)
                │                                    │                                    │
                └─────────────┬──────────────────────┴─────────────────────┬──────────────┘
                              ▼                                            ▼
                    B-11c 9 v1 챔프 일괄 마이그레이션          B-12 메쉬 분리 (요네 R / 엘든링)
                    (ChampionTable.cpp/SkillTable.cpp 폐기)                │
                                              │                            │
                                              └──────────────┬─────────────┘
                                                             ▼
                                            Phase 4 네트워크 (hookId 결정성 → 서버 권위 sim 직접)
```

---

## 한 줄

**v3.1 final = Codex 9 보정 (블로커 4 + 설계 5) 박제 — `rangeMax/rotate` 실제 필드명 / `SkillHookContext.h` 분리 / Scene 의존 → ApplyDamage 공통 / 야스오 enum → eProjectileKind 일반 / hook 4 분할 (keySwap/onCastAccepted/castFrame/recovery) / Dispatch bool / BanPick 자동 UI / eChampion 명시값 / Components Shared 권고. 추천 순서 = Registry → SkillRegistry lookup → BanPick 자동 → GenericProjectile/Damage → Champions/Ezreal/. 185분 (인프라 비용 일회성). 51번째 챔프 = 15~20분. 50명 누적 = 13시간. LoL 운영 패턴 1:1 미러.**
