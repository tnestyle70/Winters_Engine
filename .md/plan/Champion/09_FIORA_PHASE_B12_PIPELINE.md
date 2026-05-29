# Phase B-12 (피오라) — 순수 ECS 챔피언 추가 + WINTERS_MIN_SCENE 강화 + FX 박제

**작성일**: 2026-05-03
**기반**: Riven (B-11) 패턴 100% 미러 + Garen/Zed 의 cast-frame 분기 패턴 참조
**목표**: Fiora 1체를 순수 ECS 경로로 추가 → BanPick 등록 → 인게임 BA / Q / W / E / R + FX 동작 → Sylas (적) 와 단독 매치 가능. Zed/Riven/Ezreal 등 다른 챔프는 `#define WINTERS_MIN_SCENE 1` 으로 빌드 제외.

---

## 0. 자원 확인 (실측)

```
Client/Bin/Resource/Texture/Character/Fiora/
├── fiora.fbx                              ✓ 입력
├── fiora.skl / fiora.skn                  ✓
├── fiora_textured.glb                     ✓ (참고용, 사용 안 함)
├── fiora_base.png                         ✓ 텍스처 변형
├── fiora_base_tx_cm.png                   ✓ ★ 바디 메인 텍스처
├── fiora.wmesh                            ✗ 변환 필요 (D-0)
├── fiora.wskel                            ✗ 변환 필요 (D-0)
├── animations/                            ✓ 19 .anm
│   ├── fiora_idle1.anm  / fiora_idle3.anm / fiora_idlein.anm
│   ├── fiora_run.anm    / fiora_runwalk.anm
│   ├── fiora_attack1.anm  / fiora_attack2.anm  / fiora_crit.anm
│   ├── fiora_spell1.anm           ★ Q (Lunge — 짧은 돌진)
│   ├── fiora_spell2.anm           ★ W (Riposte — 패리)
│   ├── fiora_spell2_in.anm        ★ W in (패리 stage1, 짧은 빌드업)
│   ├── fiora_channel.anm          ★ R (Grand Challenge — 채널 모션)
│   ├── fiora_channel_windup.anm   ★ R 빌드업
│   ├── fiora_death.anm            (사망)
│   └── (dance/joke/laugh/taunt 등 idle 변형)
└── particles/                     ✓ 200+ PNG (필요분만 사용)
    ├── fiora_base_q_slash.png            ★ Q 슬래시
    ├── fiora_base_q_swordglow.png        ★ Q 칼날 광채
    ├── fiora_base_q_dash2.png            ★ Q 돌진 트레일
    ├── fiora_base_w_block_flash.png      ★ W 패리 플래시
    ├── fiora_base_w_block_glow.png       ★ W 활성 글로우
    ├── fiora_base_w_activate_wall.png    ★ W 활성화 벽 모션
    ├── fiora_base_e_buff_mult_yellow.png ★ E 버프 (블레이드워크)
    ├── fiora_base_e_sword_glow.png       ★ E 칼날 글로우
    ├── fiora_base_r_aoe_petals.png       ★ R 그랜드 챌린지 영역
    ├── fiora_base_r_crest_glow.png       ★ R 약점 표식
    └── fiora_base_r_healzone.png         ★ R 처형 시 힐 존
```

**E 애니 결정**: Fiora E (Bladework) 는 LoL 원작도 별도 모션 없는 패시브 버프 (다음 2 평타 강화). 본 계획서도 **자세 변경 없이 self-target FxBillboard 활성화** 만 박제.

---

## 1. 핵심 설계 — Riven 패턴 미러 (순수 ECS)

### 1.1 패턴 분류 (코드베이스 실측)

| 챔프 | 등록 위치 | Spawn 경로 | Apply*Hit 위치 | FX 모듈 | 빌드 격리 |
|---|---|---|---|---|---|
| Irelia | `Scene_InGame::OnEnter` 직접 (`m_Irelia.Init`) | `CreateChampionEntity(m_Irelia,...)` | Scene_InGame 내장 | `IreliaFxPresets` + `IreliaBladeSystem` | `MIN_SCENE` |
| Yasuo | 동일 | 동일 | 동일 | `YasuoFxPresets` + 4 시스템 | `MIN_SCENE` |
| Garen | 동일 | 동일 | `ApplyGarenHit` | `GarenFxPresets` | `MIN_SCENE` |
| Zed | 동일 | 동일 | `ApplyZedHit` | `ZedFxPresets` | (현재 active) |
| Kalista | 동일 | 동일 + Blueprint Clone fallback | 분리 시스템 (Projectile/Rend) | `KalistaFxPresets` | `MIN_SCENE` |
| **Riven** ★ | `ChampionTable.cpp` 데이터 | `CreateECSChampion(eChampion::RIVEN)` (순수 ECS) | `ApplyRivenHit` | `RivenFxPresets` | (현재 active) |
| Ezreal | `Ezreal_Registration.cpp` (자체 모듈, static initializer) | 동일 | Hook 시스템 분리 | `Ezreal_FxPresets` + `Ezreal_Skills` | (현재 active) |
| **Fiora** (신규) | **Riven 패턴 채택** — `ChampionTable.cpp` 1줄 + `SkillTable.cpp` 5줄 | `CreateECSChampion(eChampion::FIORA)` | `ApplyFioraHit` (신규) | `FioraFxPresets.h/.cpp` (신규) | active |

### 1.2 4개 파일 수정 + 2개 파일 신규 작성

**수정**:
1. `Tools/convert_all_assets.bat` — Fiora 변환 1줄
2. `Client/Private/GameObject/ChampionTable.cpp` — Fiora ChampionDef 1줄
3. `Client/Private/GameObject/SkillTable.cpp` — Fiora 5 SkillDef
4. `Client/Private/Scene/Scene_InGame.cpp` — cast 분기 + `WINTERS_MIN_SCENE` 가드 강화 + spawn 등록
5. `Client/Public/Scene/Scene_InGame.h` — `ApplyFioraHit` 헬퍼 + `m_FioraEntity` 멤버
6. `Client/Include/Client.vcxproj` — `FioraFxPresets.cpp/.h` 등록

**신규**:
1. `Client/Public/GameObject/Champion/Fiora/FioraFxPresets.h`
2. `Client/Private/GameObject/Champion/Fiora/FioraFxPresets.cpp`

---

## 2. D-0 자원 변환 (`.wmesh / .wskel / .wanim`)

### 2.1 `Tools/convert_all_assets.bat` 수정

**파일**: `Tools/convert_all_assets.bat:31` 직후 (Riven 다음 줄)

```bat
call :convert_champ "Riven" "riven.fbx"
call :convert_champ "Ezreal" "ezreal.fbx"
call :convert_champ "Fiora" "fiora.fbx"
```

### 2.2 변환 실행

```cmd
cd C:\Users\tnest\Desktop\Winters_restored\Winters
.\Tools\convert_all_assets.bat champions
```

검증: `Client/Bin/Resource/Texture/Character/Fiora/` 에 `fiora.wmesh`, `fiora.wskel` 생성 + `anims/*.wanim` 19개 채워짐.

---

## 3. ChampionTable.cpp — Fiora ChampionDef

**파일**: `Client/Private/GameObject/ChampionTable.cpp` L18 (Riven 다음)

```cpp
static const ChampionDef s_ChampionTable[] =
{
    { eChampion::IRELIA,  "irelia_",  "idle1", "run", "attack_01" },
    { eChampion::YASUO,   "yasuo_",   "idle1", "run", "attack1" },
    { eChampion::KALISTA, "kalista_", "idle1", "run", "attack1" },
    { eChampion::GAREN,   "", "garen_2013_idle1", "garen_2013_run", "garen_2013_attack_01", 1.5f },
    { eChampion::ZED, "", "zed_idle1", "zed_run", "zed_attack1", 1.5f },
    { eChampion::RIVEN, "riven_", "idle1", "run", "attack1", 1.5f,
      "Client/Bin/Resource/Texture/Character/Riven/riven.fbx",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Riven/riven_base_tx_cm.png",
      {},
      { 24.f, 1.f, 0.f },
      0.01f },
    // Fiora — Phase B-12 (2026-05-03)
    { eChampion::FIORA, "fiora_", "idle1", "run", "attack1", 1.5f,
      "Client/Bin/Resource/Texture/Character/Fiora/fiora.fbx",
      L"Shaders/Mesh3D.hlsl",
      L"Client/Bin/Resource/Texture/Character/Fiora/fiora_base_tx_cm.png",
      {},
      { 30.f, 1.f, 0.f },
      0.01f },
};
```

> ChampionTable.cpp L41 의 `GetChampionDisplayName` 에 `case eChampion::FIORA: return "Fiora";` 는 이미 존재 — 추가 수정 불필요.

---

## 4. SkillTable.cpp — Fiora 5 SkillDef

**파일**: `Client/Private/GameObject/SkillTable.cpp` L299 (Riven R 다음)

**부등식 검증** (CLAUDE.md Gotcha): `lockDurationSec × animPlaySpeed ≥ recoveryFrame / FBX_FPS`
- BA: `1.0 × 1.0 = 1.0 ≥ 14/24 = 0.583` ✅
- Q: `0.5 × 1.0 = 0.5 ≥ 10/24 = 0.417` ✅
- W: `1.5 × 1.0 = 1.5 ≥ 18/24 = 0.75` ✅
- E: `0.4 × 1.0 = 0.4 ≥ 8/24 = 0.333` ✅
- R: `2.0 × 1.0 = 2.0 ≥ 36/24 = 1.5` ✅

```cpp
        // ── Fiora ──────────────────────────────────────────────
        // BA — fiora_attack1
        { eChampion::FIORA, 0, eTargetMode::UnitTarget,
          0.6f, 1.5f, 0.f,
          "attack1", nullptr, nullptr,
          1.0f, true, eRotateMode::TowardsTarget,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          6.f, 14.f, 0.f, 0.f,
          1.0f, 1.f },

        // Q — Lunge
        { eChampion::FIORA, 1, eTargetMode::Direction,
          1.5f, 4.0f, 0.f,
          "spell1", nullptr, nullptr,
          0.5f, true, eRotateMode::TowardsCursor,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          4.f, 10.f, 0.f, 0.f,
          1.0f, 1.f },

        // W — Riposte (1차 단순화: stageCount=1)
        { eChampion::FIORA, 2, eTargetMode::Self,
          12.f, 0.f, 0.f,
          "spell2", nullptr, nullptr,
          1.5f, true, eRotateMode::None,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          0.f, 18.f, 0.f, 0.f,
          1.0f, 1.f },

        // E — Bladework (자세 변경 없는 버프)
        { eChampion::FIORA, 3, eTargetMode::Self,
          13.f, 0.f, 0.f,
          "attack1", nullptr, nullptr,
          0.4f, true, eRotateMode::None,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          1.f, 8.f, 0.f, 0.f,
          1.0f, 1.f },

        // R — Grand Challenge
        { eChampion::FIORA, 4, eTargetMode::UnitTarget,
          80.f, 5.0f, 100.f,
          "channel", nullptr, nullptr,
          2.0f, true, eRotateMode::TowardsTarget,
          1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,
          18.f, 36.f, 0.f, 0.f,
          1.0f, 1.f },
```

---

## 5. FioraFxPresets.h / .cpp 신규 작성

### 5.1 헤더

**신규 파일**: `Client/Public/GameObject/Champion/Fiora/FioraFxPresets.h`

```cpp
#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace FioraFx
{
    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWParry(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnWBlockFlash(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnEBuff(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnRMark(CWorld& world, EntityID target, f32_t fDuration);
    void SpawnRHealZone(CWorld& world, EntityID owner, f32_t fDuration);
}
```

### 5.2 구현

**신규 파일**: `Client/Private/GameObject/Champion/Fiora/FioraFxPresets.cpp`

```cpp
#include "GameObject/Champion/Fiora/FioraFxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
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

void FioraFx::SpawnQSlash(CWorld& world, EntityID owner, const Vec3& /*dir*/, f32_t fLifetime)
{
    if (owner == NULL_ENTITY) return;

    {
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathQSlashTex;
        fx.fWidth = 2.4f;
        fx.fHeight = 1.6f;
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
        fx.fWidth = 1.4f;
        fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime * 0.6f;
        fx.vColor = { 1.0f, 0.95f, 0.7f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.3f;
        CFxSystem::Spawn(world, fx);
    }
}

void FioraFx::SpawnWParry(CWorld& world, EntityID owner, f32_t fDuration)
{
    if (owner == NULL_ENTITY) return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathWParryTex;
    fx.fWidth = 2.0f;
    fx.fHeight = 2.0f;
    fx.bBillboard = true;
    fx.fLifetime = fDuration;
    fx.vColor = { 1.1f, 0.9f, 0.4f, 0.85f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fDuration * 0.35f;
    CFxSystem::Spawn(world, fx);
}

void FioraFx::SpawnWBlockFlash(CWorld& world, EntityID owner, f32_t fLifetime)
{
    if (owner == NULL_ENTITY) return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.2f, 0.f };
    fx.texturePath = kPathWFlashTex;
    fx.fWidth = 2.6f;
    fx.fHeight = 2.6f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.4f, 1.2f, 0.6f, 1.f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.55f;
    CFxSystem::Spawn(world, fx);
}

void FioraFx::SpawnEBuff(CWorld& world, EntityID owner, f32_t fDuration)
{
    if (owner == NULL_ENTITY) return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.3f, 0.f };
    fx.texturePath = kPathEBuffTex;
    fx.fWidth = 1.6f;
    fx.fHeight = 1.6f;
    fx.bBillboard = true;
    fx.fLifetime = fDuration;
    fx.vColor = { 1.0f, 0.85f, 0.3f, 0.9f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fDuration * 0.4f;
    CFxSystem::Spawn(world, fx);
}

void FioraFx::SpawnRMark(CWorld& world, EntityID target, f32_t fDuration)
{
    if (target == NULL_ENTITY) return;

    FxBillboardComponent fx{};
    fx.attachTo = target;
    fx.vAttachOffset = { 0.f, 2.5f, 0.f };
    fx.texturePath = kPathRMarkTex;
    fx.fWidth = 1.0f;
    fx.fHeight = 1.4f;
    fx.bBillboard = true;
    fx.fLifetime = fDuration;
    fx.vColor = { 1.2f, 0.9f, 0.3f, 1.f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fDuration * 0.3f;
    CFxSystem::Spawn(world, fx);
}

void FioraFx::SpawnRHealZone(CWorld& world, EntityID owner, f32_t fDuration)
{
    if (owner == NULL_ENTITY) return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 0.05f, 0.f };
    fx.texturePath = kPathRHealTex;
    fx.fWidth = 6.0f;
    fx.fHeight = 6.0f;
    fx.bBillboard = false;
    fx.fYaw = 0.f;
    fx.fLifetime = fDuration;
    fx.vColor = { 0.85f, 1.0f, 0.55f, 0.7f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fDuration * 0.5f;
    CFxSystem::Spawn(world, fx);
}
```

---

## 6. Scene_InGame.h — 헬퍼 함수 + 멤버 추가

**파일**: `Client/Public/Scene/Scene_InGame.h`

### 6.1 멤버 함수 선언 추가 (L294 직후)

```cpp
    void ApplyZedHit(EntityID target, f32_t fDamage);
    void ApplyRivenHit(EntityID target, f32_t fDamage);
    void ApplyFioraHit(EntityID target, f32_t fDamage);
```

### 6.2 EntityID 멤버 (L313 직후)

```cpp
    EntityID m_ZedEntity = NULL_ENTITY;
    EntityID m_FioraEntity = NULL_ENTITY;
```

---

## 7. Scene_InGame.cpp — 핵심 로직 수정

### 7.1 Include 추가 (L73 직후)

```cpp
#include "GameObject/Champion/Riven/RivenFxPresets.h"
#include "GameObject/Champion/Fiora/FioraFxPresets.h"
```

### 7.2 WINTERS_MIN_SCENE 강화 — Zed Init 가드 (L301-L309 wrap)

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

### 7.3 CreateECSEntities — Fiora pure ECS 스폰 (L779-L856)

```cpp
void CScene_InGame::CreateECSEntities()
{
#if !WINTERS_MIN_SCENE
    m_IreliaEntity = CreateChampionEntity(m_Irelia, m_IreliaTransform, eChampion::IRELIA, eTeam::Blue);
    m_KalistaEntity = CreateChampionEntity(m_Kalista, m_KalistaTransform, eChampion::KALISTA, eTeam::Blue);
    m_YasuoEntity = CreateChampionEntity(m_Yasuo, m_YasuoTransform, eChampion::YASUO, eTeam::Blue);
    m_GarenEntity = CreateChampionEntity(m_Garen, m_GarenTransform, eChampion::GAREN, eTeam::Blue);
    m_ZedEntity = CreateChampionEntity(m_Zed, m_ZedTransform, eChampion::ZED, eTeam::Blue);

    EntityID rivenEntity = CreateECSChampion(eChampion::RIVEN, eTeam::Blue);
    EntityID ezrealEntity = CreateECSChampion(eChampion::EZREAL, eTeam::Blue);
#endif

    m_FioraEntity = CreateECSChampion(eChampion::FIORA, eTeam::Blue);
    if (m_FioraEntity != NULL_ENTITY)
        m_World.AddComponent<SkillStateComponent>(m_FioraEntity);

    m_SylasEntity = CreateChampionEntity_FromBlueprint(L"Sylas", m_Sylas, m_SylasTransform);
    if (m_SylasEntity == NULL_ENTITY)
        m_SylasEntity = CreateChampionEntity(m_Sylas, m_SylasTransform, eChampion::SYLAS, eTeam::Red);

    auto addSkillStateIfAlive = [&](EntityID entity)
    {
        if (entity != NULL_ENTITY)
            m_World.AddComponent<SkillStateComponent>(entity);
    };

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
    else
#endif
    if (champ == eChampion::FIORA) m_PlayerEntity = m_FioraEntity;

    if (m_PlayerEntity != NULL_ENTITY)
        m_World.AddComponent<LocalPlayerTag>(m_PlayerEntity);
}
```

### 7.4 OnEnter 끝 BindPlayerToECSChampion 호출 (L376-L380)

```cpp
    const eChampion selectedChampion = CGameInstance::Get()->Get_GameContext().SelectedChampion;
    if (selectedChampion == eChampion::RIVEN
        || selectedChampion == eChampion::EZREAL
        || selectedChampion == eChampion::FIORA)
    {
        BindPlayerToECSChampion(m_PlayerEntity);
    }
```

### 7.5 castFrame Dispatch 분기 — Fiora 전용 (Riven 분기 직후)

```cpp
    //Fiora — Phase B-12
    else if (champCur == eChampion::FIORA && m_pActiveSkillDef && m_pPlayerTransform)
    {
        const i32_t slot = m_pActiveSkillDef->slot;

        if (slot == 0)
        {
            const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
            if (target != NULL_ENTITY)
                ApplyFioraHit(target, 55.f);
        }
        else if (slot == 1)
        {
            FioraFx::SpawnQSlash(m_World, m_PlayerEntity,
                m_ActiveSkillCommandStorage.direction, 0.4f);

            const Vec3 vOrigin = m_pPlayerTransform->GetPosition();
            Vec3 vDir = m_ActiveSkillCommandStorage.direction;
            const f32_t fLen = std::sqrtf(vDir.x * vDir.x + vDir.z * vDir.z);
            if (fLen > 0.01f)
            {
                vDir = { vDir.x / fLen, 0.f, vDir.z / fLen };
                const Vec3 vCheckPoint = {
                    vOrigin.x + vDir.x * 3.0f, vOrigin.y, vOrigin.z + vDir.z * 3.0f };

                EntityID hitTarget = NULL_ENTITY;
                f32_t fBestDistSq = FLT_MAX;
                m_World.ForEach<ChampionComponent, TransformComponent>(
                    [&](EntityID e, ChampionComponent& cc, TransformComponent& tf)
                    {
                        if (e == m_PlayerEntity) return;
                        if (cc.team == m_PlayerTeam) return;
                        const Vec3 vEnemyPos = tf.GetPosition();
                        const f32_t dx = vEnemyPos.x - vCheckPoint.x;
                        const f32_t dz = vEnemyPos.z - vCheckPoint.z;
                        const f32_t fDistSq = dx * dx + dz * dz;
                        if (fDistSq < 1.5f * 1.5f && fDistSq < fBestDistSq)
                        {
                            fBestDistSq = fDistSq;
                            hitTarget = e;
                        }
                    });
                if (hitTarget != NULL_ENTITY)
                    ApplyFioraHit(hitTarget, 70.f);
            }
        }
        else if (slot == 2)
        {
            FioraFx::SpawnWParry(m_World, m_PlayerEntity, 1.5f);
        }
        else if (slot == 3)
        {
            FioraFx::SpawnEBuff(m_World, m_PlayerEntity, 5.0f);
        }
        else if (slot == 4)
        {
            const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
            if (target != NULL_ENTITY)
            {
                FioraFx::SpawnRMark(m_World, target, 4.0f);
                ApplyFioraHit(target, 80.f);
                FioraFx::SpawnRHealZone(m_World, m_PlayerEntity, 4.0f);
            }
        }
    }
```

### 7.6 OnRender — Zed 가드 (L2031-L2033)

```cpp
#if !WINTERS_MIN_SCENE
    m_Zed.UpdateCamera(vp);
    m_Zed.UpdateTransform(m_ZedTransform.GetWorldMatrix());
    m_Zed.Render();
#endif
```

### 7.7 OnExit — Zed Shutdown 가드 (L2131)

```cpp
#if !WINTERS_MIN_SCENE
    m_Zed.Shutdown();
#endif
```

### 7.8 SyncECSTransformsFromLegacy — Zed 가드 (L902)

```cpp
#if !WINTERS_MIN_SCENE
    push(m_IreliaEntity,  m_IreliaTransform);
    push(m_YasuoEntity,   m_YasuoTransform);
    push(m_ViegoEntity,   m_ViegoTransform);
    push(m_KalistaEntity, m_KalistaTransform);
    push(m_GarenEntity,   m_GarenTransform);
    push(m_ZedEntity,     m_ZedTransform);
    push(m_MapEntity,     m_MapTransform);
#endif
    push(m_SylasEntity,   m_SylasTransform);
```

### 7.9 ApplyFioraHit 구현 (L3343 다음)

```cpp
void CScene_InGame::ApplyFioraHit(EntityID target, f32_t fDamage)
{
    if (target == NULL_ENTITY || target == m_PlayerEntity) return;
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
    sprintf_s(buf, "[FioraHit] target=%u dmg=%.1f hp=%.1f/%.1f\n",
        static_cast<u32_t>(target), fDamage, hpCur, hpMax);
    OutputDebugStringA(buf);
}
```

---

## 8. Client.vcxproj — 신규 파일 등록

### 8.1 ClCompile (L128 직후)

```xml
    <ClCompile Include="..\Private\GameObject\Champion\Zed\ZedFxPresets.cpp" />
    <ClCompile Include="..\Private\GameObject\Champion\Fiora\FioraFxPresets.cpp" />
```

### 8.2 ClInclude (L212 직후)

```xml
    <ClInclude Include="..\Public\GameObject\Champion\Zed\ZedFxPresets.h" />
    <ClInclude Include="..\Public\GameObject\Champion\Fiora\FioraFxPresets.h" />
```

---

## 9. 빌드 + 검증 절차

### 9.1 사전 체크리스트
- [ ] `devenv.exe` (Visual Studio) 종료
- [ ] `Tools\convert_all_assets.bat champions` 실행 → `OK=10`
- [ ] `fiora.wmesh / .wskel / anims/*.wanim` 19개 확인

### 9.2 빌드
```cmd
MSBuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
.\UpdateLib.bat
MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

### 9.3 런타임 검증

| 단계 | 액션 | 기대 |
|---|---|---|
| 1 | 메인 메뉴 → ChampSelect | "Fiora" 버튼 노출 |
| 2 | Fiora 선택 → InGame | (30,1,0) 스폰, idle1 anim |
| 3 | 우클릭 이동 | run anim, 도달 시 idle |
| 4 | Sylas 호버 + A | BA, attack1, 55dmg, Sylas HP 600→545 |
| 5 | Q | spell1, q_slash + q_swordglow, 4m 안 hit 70dmg |
| 6 | W | spell2, 1.5s block_glow |
| 7 | E | attack1 짧게, 5s buff_mult_yellow |
| 8 | R (Sylas 타겟) | channel, r_crest_glow 마크, 80dmg, r_healzone 4s |
| 9 | F3 | "Champion::Render" active |

---

## 10. 디버깅 매트릭스

| 증상 | 1순위 의심 | 확인 |
|---|---|---|
| BanPick 에 Fiora 안 보임 | RegisterAllLegacy 미호출 | CGameApp.cpp:21 + ChampionTable 컴파일 |
| missing ChampionDef/fbxPath | fbxPath 오타 | 로그 + 절대 경로 시도 |
| 모델 안 보임 | wmesh stride mismatch | `[CModel] .wmesh+.wskel fast-path` 로그 |
| anim 안 바뀜 | animKey 매칭 실패 (T-4 Gotcha) | Model.cpp:233 "animations=N" 덤프 |
| BA Hit 안 들어감 | castFrame Dispatch 누락 | FIORA 분기 존재 확인 |
| HP 안 깎임 | team 동일 | Sylas team=eTeam::Red 검증 |
| 인게임 진입 크래시 | wmesh/wskel 부재 | convert_all_assets.bat 재실행 |
| FX 안 보임 | PNG 경로 typo | `[CTexture::Create] failed` 검색 |
| 흰색 포화 | vColor RGB > 1.0 + Additive (B-7.4) | vColor 0.7~1.2 범위 |

---

## 11. 다음 단계 (Phase B-13 ~ B-15)

| Phase | 챔프 | 패턴 | 예상 시간 |
|---|---|---|---|
| **B-13** | Ashe | Riven 패턴 미러. Q (Volley), W (cone), E (Hawkshot), R (Crystal Arrow stun projectile) | 4h |
| **B-14** | Jax | Riven 패턴 + multi-mesh (body/fish/weapon — Garen LoadMeshTexture 슬롯별). Q (Leap), W (Empower), E (Counterstrike — Fiora W 유사), R (Grandmaster) | 5h |
| **B-15** | FioraStateComponent + W parry hit detection 본격 | Riven/Yasuo 패턴 미러 — `FioraStateComponent { eBladeworkStacks, fParryWindow, vRMarkPos[4] }` | 6h |

**Ashe 변환**:
```bat
call :convert_champ "Ashe" "ashe.fbx"
```

**Jax 변환**:
```bat
call :convert_champ "Jax" "jax.fbx"
```

---

## 12. 즉시 진입 명령

```
"Phase B-12 Fiora 진행. 09_FIORA_PHASE_B12_PIPELINE.md §2 D-0 변환부터.
1) convert_all_assets.bat Fiora 1줄 추가 → 변환 실행
2) ChampionTable.cpp + SkillTable.cpp 박제
3) FioraFxPresets.h/.cpp 신규 작성
4) Scene_InGame.h/cpp 9곳 수정 + WINTERS_MIN_SCENE 가드 강화
5) vcxproj 등록 → 빌드 → BanPick → Fiora 픽 → §9.3 9 단계 검증."
```
