# Phase B-10 (제드) — 가렌 패턴 검증된 풀 사이클 미러 + 제드 specific

**작성일**: 2026-04-28
**전제**:
- Phase B-8 가렌 풀 동작 완료 (6 레이어 검증, 7건 gotcha 박제)
- Phase B-9 wmesh/wskel/wanim 자체 포맷 검증 (가렌 + 5 챔프 일괄 통과)
- `.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` 재사용 절차서
**참조**:
- `.md/plan/Champion/03_GAREN_PHASE_B8_PIPELINE.md` v2 — 6 레이어 패턴 (본 계획서가 미러)
- `.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` — §5 신규 챔프 30분 워크플로
- `.md/guide/LOL_CHAMPION_EXTRACT_GUIDE.md` — 추출은 이미 완료 (FBX 존재)

---

## 0. 자원 확인 ✅

```
Client/Bin/Resource/Texture/Character/Zed/
├── zed.fbx              ✓
├── zed.skn / zed.skl    ✓
├── zed_textured.glb     ✓
├── animations/zed_*.anm ✓ (idle1/2/3/4, attack1/2, channel, crit, death, run, spell1, spell4 등)
└── (텍스처 PNG)         ※ 변환 시 머티리얼 경로 확인 필요
```

→ 추출 단계 skip 가능. 바로 wmesh 변환 진입.

---

## 0.5 v4 박제된 P0/P1 미리 적용

가렌 사이클에서 학습된 함정 7건 — 제드도 동일 회피:

| # | 항목 | 제드 적용 |
|---|------|----------|
| **G-1** | wmesh bone index = wskel DFS 순서 | `convert_all_assets.bat` 의 `:convert_champ` 가 `skel → mesh --skel → anim --skel` 순서 강제 — 추가 작업 X |
| **G-2** | wanim tick 단위 | 변환 도구가 자동 |
| **G-3** | POD byte offset 매칭 | 기존 인프라 그대로 사용 — 추가 작업 X |
| **G-4** | CBinaryWriter payload only | Writer 내부 처리 |
| **G-5** | bone_count >= 256 거부 | 제드 본 ~80 추정 — 안전 |
| **G-6** | basename 출력 | `zed.fbx → zed.wmesh` 자동 |
| **G-7** | ★ Skinned IL byte offset | POD 변경 X — 안전 |

**제드 specific 신규 우려 사항** (이전 챔프와 차이):
- **R = Death Mark** — 그림자 분신 시스템. 다중 인스턴스 가능 (W 그림자 + R 그림자). 1차는 단일 시각 효과로 stub, 향후 메쉬 분리 사이클 (B-12) 에서 본격
- **anim 키 prefix `zed_`** — 가렌 `garen_2013_*` / `garen_base_*` 혼재와 달리 `zed_*` 단일. ChampionTable `animPrefix=""` + SkillTable 풀키 방식 동일

---

## 1. 6 레이어 매핑 (가렌과 동일)

| Layer | 책임 | 신규/수정 |
|---|---|---|
| 1. 자원 (RAII) | `ModelRenderer m_Zed` + `CTransform m_ZedTransform` Scene 멤버 + Init/LoadMeshTexture/Update/Render | Scene_InGame.h/.cpp |
| 2. 상태 (ECS) | `CreateChampionEntity` + `SkillStateComponent` | Scene_InGame.cpp::CreateECSEntities |
| 3. 정의 (Table) | `ChampionTable` 1 행 + `SkillTable` 5 행 | 2 파일 |
| 4. 로직 (System) | Scene_InGame castFrame hook + `ApplyZedHit` | Scene_InGame.cpp |
| 5. 연출 (FxPreset) | `ZedFxPresets.h/.cpp` (Q razor + W shadow + E spin + R death mark) | 신규 2 파일 + vcxproj |
| 6. 통합 (Scene) | OnEnter Init + player 분기 + BanPick 버튼 + Sync/Update/Render 3 곳 | Scene_InGame + Scene_BanPick |

---

## 2. 계단식 검증 (가렌과 동일 마일스톤)

```
M0 (자원/enum)            — eChampion::ZED 추가 + ChampionTable.cpp 1행
M1 (변환)                 — convert_all_assets.bat 에 :convert_champ "Zed" 1줄 + 실행
M2 (info 검증)            — wskel/wmesh/wanim spot check (stride=76, bone match, skel_hash match)
M3 (Scene 6 곳)           — h 멤버 2 + cpp Init/player 분기/CreateECSEntities/Sync/Update/Render
M4 (BanPick 버튼)         — Scene_BanPick.cpp 1 블록
   ↓ F5 #1: 제드 모델 + idle/run (스킬 X)
M5 (SkillTable 5행)       — BA/Q/W/E/R, castFrame=1.f W 정정 (가렌 학습)
M6 (ZedFxPresets)         — 신규 2 파일 + vcxproj 등록
M7 (castFrame hook + ApplyZedHit) — slot==0/1/2/3/4 분기
   ↓ F5 #2: 제드 풀 동작
```

---

## 3. M0 — eChampion::ZED 추가 (Layer 2, 0.5분)

[GameContext.h:4-17](Engine/Include/GameContext.h:4) 에 enum 항목 추가:

**before**:
```cpp
enum class eChampion : uint8_t
{
    IRELIA, YASUO, KALISTA, SYLAS, VIEGO,
    ANNIE, ASHE, FIORA, GAREN, RIVEN,
    END
};
```

**after** (제드 + 후속 챔프 미리 추가):
```cpp
enum class eChampion : uint8_t
{
    IRELIA, YASUO, KALISTA, SYLAS, VIEGO,
    ANNIE, ASHE, FIORA, GAREN, RIVEN,
    ZED, EZREAL, YONE, JAX, MASTERYI, KINDRED,   // ★ 신규 6 (B-10 일괄)
    END
};
```

→ 제드만 쓰지만 후속 챔프 enum 도 같이 박제. END 위치는 마지막 유지.

---

## 4. M0.5 — ChampionTable.cpp 1 행 (Layer 3, 0.5분)

[ChampionTable.cpp:9](Client/Private/GameObject/ChampionTable.cpp:9) 가렌 행 다음에 추가:

```cpp
static const ChampionDef s_ChampionTable[] =
{
    { eChampion::IRELIA,  "irelia_",  "idle1", "run", "attack_01" },
    { eChampion::YASUO,   "yasuo_",   "idle1", "run", "attack1"   },
    { eChampion::KALISTA, "kalista_", "idle1", "run", "attack1"   },
    { eChampion::GAREN,   "",         "garen_2013_idle1", "garen_2013_run", "garen_2013_attack_01", 1.5f },
    { eChampion::ZED,     "",         "zed_idle1", "zed_run", "zed_attack1", 1.5f },   // ★ 신규
};
```

`animPrefix=""` + 풀키 방식 (가렌과 동일). attack 키는 `zed_attack1` (FBX 로그 검증 후 정정 필요 — `_01` 접미사 가능성).

---

## 5. M1 — 변환 (1분)

[convert_all_assets.bat:27](Tools/convert_all_assets.bat:27) 가렌 다음에:
```bat
call :convert_champ "Zed"  "zed.fbx"
```

실행:
```bat
cd Tools
convert_all_assets.bat champions
```

→ `OK=7 FAIL=0` 기대. 제드만 새로 변환되고 기존 6 챔프는 idempotent.

---

## 6. M2 — 산출물 검증 (1분)

```bat
WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Zed\zed.wskel
WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Zed\zed.wmesh
WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Zed\anims\<one>.wanim
```

**합격**: `wmesh.stride == 76`, `wmesh.bones == wskel.bones`, `wanim.skel_hash == wskel.hash`. (`.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` §2)

---

## 7. M3 — Scene_InGame 6 곳 (Layer 1+2+6, 5분)

가렌 패턴 그대로 미러 — `m_Garen` → `m_Zed`, `eChampion::GAREN` → `eChampion::ZED`. 6 곳:

| # | 위치 | 추가 |
|---|------|------|
| 1 | [Scene_InGame.h:154](Client/Public/Scene/Scene_InGame.h:154) | `ModelRenderer m_Zed; CTransform m_ZedTransform;` |
| 2 | [Scene_InGame.h:279](Client/Public/Scene/Scene_InGame.h:279) | `EntityID m_ZedEntity = NULL_ENTITY;` |
| 3 | [Scene_InGame.h:273](Client/Public/Scene/Scene_InGame.h:273) (private) | `void ApplyZedHit(EntityID target, f32_t fDamage);` |
| 4 | [Scene_InGame.cpp:191](Client/Private/Scene/Scene_InGame.cpp:191) Garen 다음 | Init + LoadMeshTexture(0~N) + SetPosition(21,1,0) + SetScale(0.01) |
| 5 | [Scene_InGame.cpp:224](Client/Private/Scene/Scene_InGame.cpp:224) GAREN 분기 다음 | `else if (champ == eChampion::ZED) { renderer/transform/idle/run = m_Zed/...; }` |
| 6 | [Scene_InGame.cpp:245](Client/Private/Scene/Scene_InGame.cpp:245) | `m_Zed.PlayAnimationByName("zed_idle1");` |
| 7 | [Scene_InGame.cpp:531](Client/Private/Scene/Scene_InGame.cpp:531) | `m_ZedEntity = CreateChampionEntity(m_Zed, m_ZedTransform, eChampion::ZED, eTeam::Blue);` |
| 8 | [Scene_InGame.cpp:557](Client/Private/Scene/Scene_InGame.cpp:557) | `m_World.AddComponent<SkillStateComponent>(m_ZedEntity);` |
| 9 | [Scene_InGame.cpp:585](Client/Private/Scene/Scene_InGame.cpp:585) | `else if (champ == eChampion::ZED) m_PlayerEntity = m_ZedEntity;` |
| 10 | [Scene_InGame.cpp:636](Client/Private/Scene/Scene_InGame.cpp:636) Sync push | `push(m_ZedEntity, m_ZedTransform);` |
| 11 | [Scene_InGame.cpp:1171](Client/Private/Scene/Scene_InGame.cpp:1171) Update | `m_Zed.Update(dt);` |
| 12 | [Scene_InGame.cpp:1370-1372](Client/Private/Scene/Scene_InGame.cpp:1370) Render | `m_Zed.UpdateCamera/UpdateTransform/Render` 3 줄 |

### 7.1 Init 코드 (가렌 패턴)
```cpp
//Phase B-10 Zed
m_Zed.Init("Client/Bin/Resource/Texture/Character/Zed/zed.fbx",
    L"Shaders/Mesh3D.hlsl");
// ★ 가렌 학습 — Mesh 0 가 더미 (Icosphere) 일 가능성. FBX 로그 확인 후 슬롯 결정
m_Zed.LoadMeshTexture(0, L"Client/Bin/Resource/Texture/Character/Zed/zed_base_tx_cm.png");
// 다중 머티리얼이면 슬롯 1, 2 추가
m_ZedTransform.SetPosition(21.f, 1.f, 0.f);   // 가렌(18) 옆
m_ZedTransform.SetScale(0.01f);
```

**FBX 로드 시 `[CModel DIAG]` 로그로 정확한 머티리얼/메시 슬롯 매트릭스 확정 후 LoadMeshTexture 슬롯 정정**.

---

## 8. M4 — BanPick Zed 버튼 (Layer 6, 1분)

[Scene_BanPick.cpp:91](Client/Private/Scene/Scene_BanPick.cpp:91) Garen 버튼 다음에 미러:

```cpp
ImGui::SameLine();
if (ImGui::Button("Zed", ImVec2(150.f, 60.f)))
{
    CGameInstance::Get()->Get_GameContext().SelectedChampion = eChampion::ZED;
    auto pLoadingMatch = CScene_MatchLoading::Create(
        []() -> std::unique_ptr<IScene> {
            return std::unique_ptr<IScene>(new CScene_InGame());
        }, 3.f);
    CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::MatchLoading, std::move(pLoadingMatch));
    ImGui::End();
    return;   // ★ self-destruct 방지 (B-7 Gotcha)
}
```

---

## ⏸ F5 #1 검증 (M0~M4 후, 5분)

**기대**:
1. BanPick 에 `Zed` 버튼
2. 클릭 → MatchLoading → InGame 진입
3. 제드 모델 표시 (위치 21, 1, 0)
4. `[CModel] .wmesh+.wskel fast-path: ...zed.wmesh` 로그
5. `[CModel] wskel loaded: bones=N hash=0x...`
6. `[CModel] Loaded N wanim files`
7. idle 무한 재생 + 우클릭 이동 → run
8. 카메라 follow

**금지**:
- `falling back to Assimp`
- 모델 안 보임 (byte offset 사고 재발 가능성 → POD 변경 안 했으니 위험 0, 안전)

---

## 9. M5 — SkillTable 제드 5 행 (Layer 3, 3분)

LoL 원작 제드 스킬:
- BA: 평타
- **Q (Razor Shuriken)**: 직선 표창. Direction
- **W (Living Shadow)**: 그림자 소환. GroundTarget
- **E (Shadow Slash)**: AOE 회전 베기. Self
- **R (Death Mark)**: 단일 표식 + 그림자 추적. UnitTarget

**castFrame/recoveryFrame 24 FPS 추정** — F5 검증 후 정정.

[SkillTable.cpp:215](Client/Private/GameObject/SkillTable.cpp:215) `};` 직전에 가렌 다음으로 추가:

```cpp
{ eChampion::GAREN, 4, /* ... */ },

// ── Zed ──────────────────────────────────────────────
// BA
{ eChampion::ZED, 0, eTargetMode::UnitTarget,
  0.5f, 1.5f, 0.f,
  "zed_attack1", nullptr, nullptr,
  1.0f, true, eRotateMode::TowardsTarget,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
  6.f, 14.f, 0.f, 0.f,
  1.0f, 1.f },

// Q — Razor Shuriken
{ eChampion::ZED, 1, eTargetMode::Direction,
  6.f, 9.f, 0.f,    // 에너지 무시 (mana=0)
  "zed_spell1", nullptr, nullptr,
  0.7f, true, eRotateMode::TowardsCursor,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
  4.f, 10.f, 0.f, 0.f,
  1.0f, 1.f },

// W — Living Shadow (그림자 소환)
// ★ 가렌 학습 — castFrame=1.f (0 이면 hook miss)
{ eChampion::ZED, 2, eTargetMode::GroundTarget,
  18.f, 6.5f, 0.f,
  "zed_spell2", nullptr, nullptr,
  0.5f, true, eRotateMode::TowardsCursor,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
  1.f, 8.f, 0.f, 0.f,   // ★ castFrame=1.f
  1.0f, 1.f },

// E — Shadow Slash (AOE)
{ eChampion::ZED, 3, eTargetMode::Self,
  4.f, 2.5f, 0.f,
  "zed_spell3", nullptr, nullptr,
  0.6f, true, eRotateMode::None,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
  6.f, 14.f, 0.f, 0.f,
  1.0f, 1.f },

// R — Death Mark
{ eChampion::ZED, 4, eTargetMode::UnitTarget,
  120.f, 6.25f, 0.f,
  "zed_spell4", nullptr, nullptr,
  1.5f, true, eRotateMode::TowardsTarget,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
  18.f, 30.f, 0.f, 0.f,
  1.0f, 1.f },
};
```

**부등식 검증** (가렌 학습 박제):
- BA: `1.0 × 1.0 = 1.0s ≥ 14/24 = 0.583s` ✅
- Q:  `0.7 × 1.0 ≥ 10/24 = 0.417s` ✅
- W:  `0.5 × 1.0 ≥ 8/24 = 0.333s` ✅
- E:  `0.6 × 1.0 ≥ 14/24 = 0.583s` ✅
- R:  `1.5 × 1.0 ≥ 30/24 = 1.25s` ✅ (마진 0.25s)

**애니 키 정정 우선순위**: F5 시 `[ModelRenderer] Playing: zed_attack1` 로그 안 뜨면 FBX 내 실제 anim 이름 (`zed_attack_01`?) 확인 후 ChampionTable + SkillTable 둘 다 정정.

---

## 10. M6 — ZedFxPresets (Layer 5, 5분)

신규 2 파일. 1차는 가렌과 동일하게 **빌보드 stub** (FX 에셋 추출 후 본격).

### 10.1 `Client/Public/GameObject/Champion/Zed/ZedFxPresets.h`

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

namespace ZedFx
{
    void SpawnQRazor(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWShadow(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fDuration);
    void SpawnESlash(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnRMark(CWorld& world, EntityID target, f32_t fLifetime);
}
```

### 10.2 `Client/Private/GameObject/Champion/Zed/ZedFxPresets.cpp`

`KalistaFxPresets.cpp` 패턴 + 가렌 색상 정책 (Additive blend + vColor RGB ≤ 1.0). 제드 = **녹색/검정 그림자**.

```cpp
#include "GameObject/Champion/Zed/ZedFxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    // 1차 stub — 가렌 패턴 (가렌 base PNG 임시 차용, 제드 FX 추출 후 교체)
    constexpr const wchar_t* kPathQRazorTex =
        L"Client/Bin/Resource/Texture/FX/Garen/particles/garen_aura_self.png";
    constexpr const wchar_t* kPathWShadowTex =
        L"Client/Bin/Resource/Texture/FX/Garen/particles/garen_ball01.png";
    constexpr const wchar_t* kPathESlashTex =
        L"Client/Bin/Resource/Texture/FX/Garen/particles/garen_aura_self_02.png";
    constexpr const wchar_t* kPathRMarkTex =
        L"Client/Bin/Resource/Texture/FX/Kalista/kalista_base_e_trail.png";
}

void ZedFx::SpawnQRazor(CWorld& world, EntityID owner, const Vec3& /*dir*/, f32_t fLifetime)
{
    if (owner == NULL_ENTITY) return;
    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.5f, 0.f };
    fx.texturePath = kPathQRazorTex;
    fx.fWidth = 1.2f;
    fx.fHeight = 1.2f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 0.5f, 0.9f, 0.4f, 1.0f };  // 녹색 (제드 표창)
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.4f;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnWShadow(CWorld& /*world*/, EntityID /*owner*/, const Vec3& /*groundPos*/, f32_t /*fDuration*/)
{
    // Phase B-12 메쉬 분리 사이클 진입 시 제드 그림자 = m_Zed 의 두 번째 인스턴스로 본격 구현
    // 1차는 stub
}

void ZedFx::SpawnESlash(CWorld& world, EntityID owner, f32_t fLifetime)
{
    if (owner == NULL_ENTITY) return;
    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathESlashTex;
    fx.fWidth = 2.5f;
    fx.fHeight = 2.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 0.4f, 0.8f, 0.5f, 0.7f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fLifetime * 0.3f;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnRMark(CWorld& world, EntityID target, f32_t fLifetime)
{
    if (target == NULL_ENTITY) return;
    FxBillboardComponent fx{};
    fx.attachTo = target;
    fx.vAttachOffset = { 0.f, 2.5f, 0.f };
    fx.texturePath = kPathRMarkTex;
    fx.fWidth = 1.5f;
    fx.fHeight = 1.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 0.9f, 0.2f, 0.3f, 1.0f };  // 적색 (Death Mark)
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.5f;
    CFxSystem::Spawn(world, fx);
}
```

### 10.3 vcxproj 등록

`Client/Include/Client.vcxproj` + `.filters` 에 두 파일 등록 (가렌 패턴 동일).

---

## 11. M7 — Scene_InGame castFrame hook + ApplyZedHit (Layer 6, 3분)

[Scene_InGame.cpp:854](Client/Private/Scene/Scene_InGame.cpp:854) GAREN 분기 다음 (Kalista Q 분기 → GAREN → ZED 순서):

```cpp
else if (champCur == eChampion::ZED && m_pActiveSkillDef && m_pPlayerTransform)
{
    const i32_t slot = m_pActiveSkillDef->slot;
    if (slot == 0)        // BA
    {
        const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
        if (target != NULL_ENTITY)
            ApplyZedHit(target, 55.f);
    }
    else if (slot == 1)   // Q — Razor
    {
        ZedFx::SpawnQRazor(m_World, m_PlayerEntity,
            m_ActiveSkillCommandStorage.direction, 0.6f);
        // 1차는 시각만, 표창 투사체는 B-12 사이클
    }
    else if (slot == 2)   // W — Shadow (1차 stub)
    {
        ZedFx::SpawnWShadow(m_World, m_PlayerEntity,
            m_ActiveSkillCommandStorage.groundPos, 4.f);
    }
    else if (slot == 3)   // E — Slash AOE
    {
        ZedFx::SpawnESlash(m_World, m_PlayerEntity, 0.5f);
        // AOE hit 은 1차 보류 (B-12 영역 판정)
    }
    else if (slot == 4)   // R — Death Mark
    {
        const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
        if (target != NULL_ENTITY)
        {
            ZedFx::SpawnRMark(m_World, target, 1.5f);
            ApplyZedHit(target, 240.f);
        }
    }
}
```

### 11.1 ApplyZedHit 정의

[Scene_InGame.cpp:2594](Client/Private/Scene/Scene_InGame.cpp:2594) `ApplyGarenHit` 다음에 미러:

```cpp
void CScene_InGame::ApplyZedHit(EntityID target, f32_t fDamage)
{
    if (target == NULL_ENTITY) return;
    if (target == m_PlayerEntity) return;
    if (!m_World.HasComponent<ChampionComponent>(target)) return;

    auto& champion = m_World.GetComponent<ChampionComponent>(target);
    if (champion.team == m_PlayerTeam) return;

    champion.hp = (champion.hp > fDamage) ? (champion.hp - fDamage) : 0.f;

    f32_t hpCur = champion.hp, hpMax = champion.maxHp;
    if (m_World.HasComponent<HealthComponent>(target))
    {
        auto& hp = m_World.GetComponent<HealthComponent>(target);
        hp.fCurrent = champion.hp;
        hp.fMaximum = champion.maxHp;
        hp.bIsDead = (hp.fCurrent <= 0.f);
        hpCur = hp.fCurrent; hpMax = hp.fMaximum;
    }

    char buf[128];
    sprintf_s(buf, "[ZedHit] target=%u dmg=%.1f hp=%.1f/%.1f\n",
        static_cast<u32_t>(target), fDamage, hpCur, hpMax);
    OutputDebugStringA(buf);
}
```

### 11.2 include 추가

[Scene_InGame.cpp:58](Client/Private/Scene/Scene_InGame.cpp:58) GarenFxPresets 다음:
```cpp
#include "GameObject/Champion/Zed/ZedFxPresets.h"
```

---

## ⏸ F5 #2 검증 (M5+M6+M7 후, 5분)

| 항목 | 합격 |
|---|---|
| BA | `[ModelRenderer] Playing: zed_attack1` + `[ZedHit] dmg=55` |
| Q | `zed_spell1` + 녹색 빌보드 (사일러스 호버 시) |
| W | `zed_spell2` (그림자 stub — 시각 효과 없음 정상) |
| E | `zed_spell3` + AOE 빌보드 |
| R | 사일러스 호버 + R 키 → `zed_spell4` + 적색 마크 + `[ZedHit] dmg=240` |
| 6 챔프 회귀 0 | Irelia/Yasuo/Sylas/Viego/Kalista/Garen 시각 동일 |

---

## 12. 다음 사이클 — 4 챔프 일괄 (피오라/리븐/이즈리얼/요네)

제드 통과 시 챔프당 30분 워크플로 (`.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` §5):

| 챔프 | enum | basename | 특이사항 |
|---|---|---|---|
| Fiora | FIORA (있음) | `fiora.fbx` | E = Bladework (다단 베기) |
| Riven | RIVEN (있음) | `riven.fbx` | Q = 3단 평타 (Conditional 패턴 야스오 참조) |
| Ezreal | EZREAL (M0 추가) | `ezreal.fbx` | 원거리 — 4 스킬 모두 투사체 |
| Yone | YONE (M0 추가) | `yone.fbx` | **R = 영혼 해방 — 메쉬 분리 필요. 1차는 stub, B-12 본격** |

각 챔프당:
1. wmesh 변환 (1분)
2. ChampionTable + SkillTable 행 (3분)
3. Scene_InGame 6 곳 (5분)
4. BanPick 버튼 (1분)
5. FxPresets stub 신규 (5분)
6. castFrame hook + ApplyXxxHit (3분)
7. F5 검증 (5분)
**소계: ~25분 / 챔프 × 4 = ~1.5h**

---

## 13. 후속 — Phase B-12 메쉬 분리 + 엘든링 보스 대비

본 사이클 외부. 별도 계획서 작성 예정 (`.md/plan/Champion/06_MESH_SEPARATION_PIPELINE.md`).

**핵심 작업**:
- `.wmesh` 의 서브메시 단위 selective render (mesh mask flag)
- 다중 ECS Entity 한 ModelRenderer 공유 (instancing 또는 별도 인스턴스)
- 본 마스킹 (서브메시별 본 가시성)
- 요네 R 영혼 해방: 본체 idle + 영혼 = 같은 모델 별도 인스턴스 + 다른 anim
- 엘든링 대비: 보스 부위 파괴 (팔/머리 분리), 무기 교체, 변신 (메쉬 swap)

→ B-10 4 챔프 + 요네 통과 후 진입.

---

## 14. 사이클 종료 후 갱신할 파일

1. **CLAUDE.md** L11-18 — `현재 진행`/`다음 세션` 갱신 (B-10 진행 → B-12 메쉬 분리 예고)
2. **CLAUDE.md** Phase B-10 Gotchas — 발견될 가능성:
   - 제드 anim 키 prefix 미스매치 (`zed_attack1` vs `zed_attack_01`)
   - 다중 머티리얼 슬롯 매핑 사고 (가렌 Icosphere 패턴 재발)
3. **MEMORY.md** + 신규 `project_phase_b10_zed_*.md`
4. **본 계획서** — 학습 결과 부록

---

## 15. 예상 소요 (제드 1체)

| 단계 | 시간 |
|---|---|
| M0 (enum + ChampionTable) | 1분 |
| M1 (변환 bat + 실행) | 2분 |
| M2 (info 검증) | 1분 |
| M3 (Scene 6 곳) | 5분 |
| M4 (BanPick) | 1분 |
| F5 #1 | 5분 |
| M5 (SkillTable) | 3분 |
| M6 (FxPresets + vcxproj) | 5분 |
| M7 (hook + ApplyZedHit) | 3분 |
| F5 #2 | 5분 |
| 회귀 챔프 검증 | 5분 |
| 메모/CLAUDE 박제 | 5분 |
| **합계** | **~40분** |

---

## 한 줄

**가렌 패턴 미러 + 제드 specific (R Death Mark) + 박제된 7 gotcha 회피. M0 → F5 #1 → M5~M7 → F5 #2. 40분 목표. 제드 통과 = 후속 4 챔프 일괄 30분/챔프.**
