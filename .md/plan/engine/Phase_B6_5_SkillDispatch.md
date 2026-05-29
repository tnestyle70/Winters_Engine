# Phase B-6.5 — eTargetMode + SkillDef 테이블 + DispatchSkillInput

**상태**: 📋 계획 완성 (2026-04-18) · 구현 대기
**선행**: Phase B-6 완료 (사일러스 호버 + 우클릭 평타/QWER + 맵 튜너 + 액션 락 + ECS 엔티티 등록)
**이후**: Day 3 Command 모델 (`MoveToCommand`/`CastSkillCommand` 직렬화 + Shared/Schemas/Commands.fbs)

---

## Context

Phase B-6 에서 사일러스 호버 + 우클릭/QWER 단일 패턴으로 스킬을 붙였지만, 실제 LoL 은 스킬마다 타겟팅 방식이 다 다름:

- **이렐리아**: 평타/Q = UnitTarget, W = Self, E = GroundTarget, R = Direction
- **야스오**: 평타/E/R = UnitTarget, Q = Conditional(기본 Direction · E시전중 AOE), W = Direction

이 다양성을 **데이터 드리븐 (C++ `SkillDef` 테이블)** 로 수용한다.

- 개별 캐릭터 클래스 ❌ (가상 함수 → 네트워크 직렬화 불가, LoL 본작도 안 씀)
- 지금 Lua ❌ (VM·바인딩 3일 인프라, 25개 스킬엔 과투자, Phase 7 이관 예정)
- **C++ 데이터 테이블 ✅** — POD, 직렬화 공짜, Phase 7 에 Lua 테이블로 이관 호환

실제 대시·벽·투사체 스폰은 Phase 4 서버 시뮬레이션으로 미루고, 이번엔 **입력 해석 + 애니 + Command 준비** 까지.

---

## 4-레이어 아키텍처

```
┌─ 1. 입력 레이어 (클라)          ─ eTargetMode → payload 구성
│                                    호버·커서·방향 계산
├─ 2. 커맨드 레이어 (클라·서버)    ─ CastSkillCommand (직렬화 가능)
├─ 3. 시뮬레이션 (서버 + 클라 예측) ─ SkillDef[].execute(...)
│                                    ← Phase 7 에서 Lua 교체
└─ 4. 연출 (클라 전용)             ─ 애니 + VFX + SFX
```

이번 범위: **레이어 1 + 2 (커맨드 구성까지) + 레이어 4 의 애니 재생**. 레이어 3 (실제 효과) 은 Phase 4.

---

## 파일 변경 목록

| # | 파일 | 종류 |
|---|---|---|
| ① | `Client/Public/Gameplay/SkillDef.h` | **신규** |
| ② | `Client/Private/Gameplay/SkillTable.cpp` | **신규** |
| ③ | `Engine/Public/ECS/Components/GameplayComponents.h` | 수정 (YasuoStateComponent 추가) |
| ④ | `Client/Public/Scene/Scene_InGame.h` | 수정 (include + 3 메서드 선언) |
| ⑤ | `Client/Private/Scene/Scene_InGame.cpp` | 수정 (UpdateCombatInput 교체 + 3 메서드 본문 + Yasuo 상태 타이머 + ImGui) |
| ⑥ | `Client/Include/Client.vcxproj(.filters)` | 수정 (신규 파일 등록) |

---

## ① 신규 — `Client/Public/Gameplay/SkillDef.h`

**전문**:

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "Entity.h"         // EntityID
#include "GameContext.h"    // eChampion

// ─────────────────────────────────────────────────────────────
//  Phase B-6.5  Skill Definition + Cast Command
//
//  eTargetMode       : 스킬의 타겟팅 방식 (입력 해석 분기)
//  eSkillSlot        : 슬롯 식별 (기본공격 + 4 스킬)
//  SkillDef          : 챔프별 스킬 1개의 정의 (POD, 네트워크 직렬화 가능)
//  CastSkillCommand  : 플레이어가 스킬 사용 시 클라 → 서버 전송될 커맨드
//
//  Phase 7에서 g_SkillTable[]을 Lua 로 이관. 구조체는 유지.
// ─────────────────────────────────────────────────────────────

enum class eTargetMode : uint8_t
{
    Self,          // 자기 자신 (이렐리아 W — 방어막)
    UnitTarget,    // 호버된 적 유닛 (평타, 이렐리아 Q, 야스오 E·R)
    GroundTarget,  // 마우스 지면 좌표 (이렐리아 E — 검 생성 지점)
    Direction,     // 플레이어→커서 방향 벡터 (이렐리아 R, 야스오 Q·W)
    Conditional,   // 상태 기반 분기 (야스오 Q: 기본=Direction / E시전중=AOE)
};

enum class eSkillSlot : uint8_t
{
    BasicAttack = 0,   // 우클릭
    Q           = 1,
    W           = 2,
    E           = 3,
    R           = 4,
    SLOT_END    = 5,
};

struct SkillDef
{
    eChampion   champ        = eChampion::CHAMPION_END;
    uint8_t     slot         = 0;           // eSkillSlot 값
    eTargetMode targetMode   = eTargetMode::Self;

    // 판정값 — Phase 4 서버 시뮬레이션에서 사용
    f32_t       cooldownSec  = 0.f;
    f32_t       rangeMax     = 0.f;
    f32_t       manaCost     = 0.f;

    // 연출 — 지금 단계에서 직접 참조
    const char* animKey      = nullptr;     // "attack_01" / "spell1" (prefix 제외)
    const char* vfxKey       = nullptr;     // Phase 2+ 파티클 매핑
    const char* sfxKey       = nullptr;
};

// ─────────────────────────────────────────────────────────────
//  CastSkillCommand — 직렬화 가능 POD
//    Day 3에서 Shared/Schemas/Commands.fbs 로 옮길 때 필드 그대로 매핑
// ─────────────────────────────────────────────────────────────
struct CastSkillCommand
{
    uint8_t   slot                = 0;
    uint8_t   resolvedTargetMode  = 0;      // Conditional 해석 후의 실제 모드
    uint16_t  _pad                = 0;

    // payload — 타겟 모드에 따라 유효 필드가 달라짐
    // union 대신 명시 필드(네트워크 직렬화 / FlatBuffers 친화)
    EntityID  targetEntityId      = NULL_ENTITY;   // UnitTarget / Self
    Vec3      groundPos{ 0.f, 0.f, 0.f };          // GroundTarget
    Vec3      direction{ 0.f, 0.f, 0.f };          // Direction (단위 벡터)
};

// ─────────────────────────────────────────────────────────────
//  테이블 조회
// ─────────────────────────────────────────────────────────────
extern const SkillDef* const g_SkillTable;
extern const uint32_t        g_SkillCount;

// champ + slot 조회. 못 찾으면 nullptr.
const SkillDef* FindSkillDef(eChampion champ, uint8_t slot);
```

---

## ② 신규 — `Client/Private/Gameplay/SkillTable.cpp`

**전문**:

```cpp
#include "Gameplay/SkillDef.h"

// ─────────────────────────────────────────────────────────────
//  g_SkillTable — 데이터 드리븐 스킬 정의
//
//  cooldown / range / mana 는 Phase 4 서버 시뮬레이션 때 사용.
//  지금은 eTargetMode 와 animKey 가 실제로 쓰임.
//
//  야스오 Q 는 Conditional: 기본은 Direction 로 해석, E 시전 중이면 AOE.
//  3타 회오리는 slot 과 독립적으로 Execute 시점에서 스택 카운팅
//  (YasuoStateComponent::qStackCount) → Phase 4 에서 처리.
// ─────────────────────────────────────────────────────────────

static const SkillDef s_SkillTable[] =
{
    // ── Irelia ─────────────────────────────────────────────
    //                     slot  targetMode                     cd    range  mana  anim          vfx   sfx
    { eChampion::IRELIA,   0,    eTargetMode::UnitTarget,       0.6f, 2.5f,  0.f,  "attack_01",  nullptr, nullptr },
    { eChampion::IRELIA,   1,    eTargetMode::UnitTarget,       7.f,  6.f,   25.f, "spell1",     nullptr, nullptr },
    { eChampion::IRELIA,   2,    eTargetMode::Self,             8.f,  0.f,   40.f, "spell2",     nullptr, nullptr },
    { eChampion::IRELIA,   3,    eTargetMode::GroundTarget,     16.f, 9.f,   80.f, "spell3",     nullptr, nullptr },
    { eChampion::IRELIA,   4,    eTargetMode::Direction,        90.f, 12.f,  100.f,"spell4",     nullptr, nullptr },

    // ── Yasuo ──────────────────────────────────────────────
    { eChampion::YASUO,    0,    eTargetMode::UnitTarget,       0.55f,2.5f,  0.f,  "attack1",    nullptr, nullptr },
    { eChampion::YASUO,    1,    eTargetMode::Conditional,      1.3f, 5.f,   0.f,  "spell1",     nullptr, nullptr },
    { eChampion::YASUO,    2,    eTargetMode::Direction,        26.f, 4.f,   0.f,  "spell2",     nullptr, nullptr },
    { eChampion::YASUO,    3,    eTargetMode::UnitTarget,       0.5f, 4.75f, 0.f,  "spell3",     nullptr, nullptr },
    { eChampion::YASUO,    4,    eTargetMode::UnitTarget,       80.f, 14.f,  0.f,  "spell4",     nullptr, nullptr },
};

const SkillDef* const g_SkillTable = s_SkillTable;
const uint32_t        g_SkillCount = sizeof(s_SkillTable) / sizeof(s_SkillTable[0]);

const SkillDef* FindSkillDef(eChampion champ, uint8_t slot)
{
    for (uint32_t i = 0; i < g_SkillCount; ++i)
    {
        if (s_SkillTable[i].champ == champ && s_SkillTable[i].slot == slot)
            return &s_SkillTable[i];
    }
    return nullptr;
}
```

---

## ③ 수정 — `Engine/Public/ECS/Components/GameplayComponents.h`

**Before** (파일 끝):
```cpp
struct ServerIdComponent
{
    uint32_t serverEntityId = 0;
};
```

**After** — 끝에 추가:
```cpp
struct ServerIdComponent
{
    uint32_t serverEntityId = 0;
};

// ─────────────────────────────────────────────────────────────
//  Yasuo 고유 상태 — Q Conditional 분기 + 3타 회오리 카운팅
//    Phase 4 서버 시뮬레이션에서 Q execute 시 참조
// ─────────────────────────────────────────────────────────────
struct YasuoStateComponent
{
    uint8_t qStackCount  = 0;      // 0→1→2→3(회오리 발사) 사이클
    f32_t   qStackTimer  = 0.f;    // 스택 유지 시간(초). 0 이하 → 리셋
    bool    bEActive     = false;  // E(질풍) 시전 중 여부 (Q 가 AOE 로 변경)
    f32_t   eActiveTimer = 0.f;
};
```

---

## ④ 수정 — `Client/Public/Scene/Scene_InGame.h`

### (A) include 추가

**Before** (ECS include 블록 끝):
```cpp
// ── ECS (Phase B-6 마이그레이션) ──
#include "World.h"
#include "TransformComponent.h"
#include "GameplayComponents.h"
```

**After** — 한 줄 추가:
```cpp
// ── ECS (Phase B-6 마이그레이션) ──
#include "World.h"
#include "TransformComponent.h"
#include "GameplayComponents.h"

// ── Gameplay 데이터 (Phase B-6.5) ──
#include "Gameplay/SkillDef.h"
```

### (B) 멤버 함수 선언 추가

**Before** (private: Handler 블록 끝):
```cpp
    void OnImGui_CombatDebug();
    void OnImGui_MapTuner();

    void SaveObjectLayout(const char* path);
    void LoadObjectLayout(const char* path);
```

**After** — `OnImGui_MapTuner();` 와 `void SaveObjectLayout` 사이에 삽입:
```cpp
    void OnImGui_CombatDebug();
    void OnImGui_MapTuner();

    // [Phase B-6.5] 스킬 디스패처
    //   slot: 0=BA, 1=Q, 2=W, 3=E, 4=R
    //   returns: 발사 성공 여부 (호버 조건 불충족·커서 유효성 실패 등으로 무시될 수 있음)
    bool DispatchSkillInput(uint8_t slot);
    void ApplyLocalPrediction(const CastSkillCommand& cmd, const SkillDef& def);
    bool BuildCastCommand(const SkillDef& def, CastSkillCommand& outCmd);

    void SaveObjectLayout(const char* path);
    void LoadObjectLayout(const char* path);
```

---

## ⑤ 수정 — `Client/Private/Scene/Scene_InGame.cpp`

### (A) `CreateECSEntities()` — 야스오 엔티티 생성 직후 YasuoStateComponent 추가

**Before** (현 파일):
```cpp
    m_IreliaEntity  = CreateChampionEntity(m_Irelia,  m_IreliaTransform,  eChampion::IRELIA,  eTeam::Blue);
    m_YasuoEntity   = CreateChampionEntity(m_Yasuo,   m_YasuoTransform,   eChampion::YASUO,   eTeam::Blue);
    m_SylasEntity   = CreateChampionEntity(m_Sylas,   m_SylasTransform,   eChampion::CHAMPION_END, eTeam::Red);
    m_ViegoEntity   = CreateChampionEntity(m_Viego,   m_ViegoTransform,   eChampion::CHAMPION_END, eTeam::Red);
    m_KalistaEntity = CreateChampionEntity(m_Kalista, m_KalistaTransform, eChampion::CHAMPION_END, eTeam::Red);
```

**After**:
```cpp
    m_IreliaEntity  = CreateChampionEntity(m_Irelia,  m_IreliaTransform,  eChampion::IRELIA,  eTeam::Blue);
    m_YasuoEntity   = CreateChampionEntity(m_Yasuo,   m_YasuoTransform,   eChampion::YASUO,   eTeam::Blue);
    m_SylasEntity   = CreateChampionEntity(m_Sylas,   m_SylasTransform,   eChampion::CHAMPION_END, eTeam::Red);
    m_ViegoEntity   = CreateChampionEntity(m_Viego,   m_ViegoTransform,   eChampion::CHAMPION_END, eTeam::Red);
    m_KalistaEntity = CreateChampionEntity(m_Kalista, m_KalistaTransform, eChampion::CHAMPION_END, eTeam::Red);

    // [Phase B-6.5] 야스오 전용 상태 컴포넌트 (Q Conditional + 3타 회오리)
    m_World.AddComponent<YasuoStateComponent>(m_YasuoEntity);
```

### (B) `UpdateCombatInput()` — 완전 교체

**Before**:
```cpp
void CScene_InGame::UpdateCombatInput(bool& outSkipGroundMove)
{
    outSkipGroundMove = false;
    if (!m_pPlayerRenderer) return;

    auto& in = CInput::Get();
    const bool bImGuiMouse = ImGui::GetIO().WantCaptureMouse;
    const bool bImGuiKbd = ImGui::GetIO().WantCaptureKeyboard;

    // QWER — 호버 필수
    if (!bImGuiKbd && m_bSylasHovered)
    {
        if (in.IsKeyPressed('Q')) FirePlayerAction("spell1");
        if (in.IsKeyPressed('W')) FirePlayerAction("spell2");
        if (in.IsKeyPressed('E')) FirePlayerAction("spell3");
        if (in.IsKeyPressed('R')) FirePlayerAction("spell4");
    }

    // 우클릭 평타 — 호버 필수, 이 경우 지면 이동 스킵
    if (!bImGuiMouse && m_bSylasHovered && in.IsRButtonPressed())
    {
        FirePlayerAction("attack_01");
        outSkipGroundMove = true;
    }
}
```

**After**:
```cpp
void CScene_InGame::UpdateCombatInput(bool& outSkipGroundMove)
{
    outSkipGroundMove = false;
    if (!m_pPlayerRenderer) return;

    auto& in = CInput::Get();
    const bool bImGuiMouse = ImGui::GetIO().WantCaptureMouse;
    const bool bImGuiKbd   = ImGui::GetIO().WantCaptureKeyboard;

    // ── Q/W/E/R (에지 트리거) ──
    // 호버 필요 여부는 SkillDef.targetMode 에 따라 DispatchSkillInput 내부에서 판정
    if (!bImGuiKbd)
    {
        if (in.IsKeyPressed('Q')) DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::Q));
        if (in.IsKeyPressed('W')) DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::W));
        if (in.IsKeyPressed('E')) DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::E));
        if (in.IsKeyPressed('R')) DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::R));
    }

    // ── 우클릭 평타 (에지) ──
    // 평타는 타겟 모드가 UnitTarget 이므로 호버 체크는 내부에서. 성공 시 지면 이동 스킵.
    if (!bImGuiMouse && in.IsRButtonPressed())
    {
        const bool fired = DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::BasicAttack));
        if (fired) outSkipGroundMove = true;
    }
}
```

### (C) `DispatchSkillInput` / `BuildCastCommand` / `ApplyLocalPrediction` 신규 구현

**위치**: `FirePlayerAction` 구현 직후에 삽입.

```cpp
// ===================================================================
//  [Phase B-6.5] 스킬 디스패처 — 타겟 모드별 입력 해석
// ===================================================================

bool CScene_InGame::DispatchSkillInput(uint8_t slot)
{
    if (!m_pPlayerRenderer || m_PlayerEntity == NULL_ENTITY)
        return false;

    using namespace Engine;
    const eChampion champ = CGameInstance::Get()->Get_GameContext().SelectedChampion;
    const SkillDef* def = FindSkillDef(champ, slot);
    if (!def) return false;

    CastSkillCommand cmd{};
    cmd.slot = slot;

    if (!BuildCastCommand(*def, cmd))
        return false;

    ApplyLocalPrediction(cmd, *def);
    // Day 3: m_CommandQueue.Push(cmd, m_ClientTick++);
    // Phase 4: SendToServer(cmd);
    return true;
}

// 타겟 모드별 payload 구성.
// 호버·커서 유효성 실패 시 false 반환 → 스킬 무시.
bool CScene_InGame::BuildCastCommand(const SkillDef& def, CastSkillCommand& outCmd)
{
    eTargetMode mode = def.targetMode;

    // Conditional (야스오 Q) — YasuoStateComponent.bEActive 로 분기
    //   지금은 AOE/Direction 둘 다 Direction 으로 해석(연출 동일),
    //   실제 피해 판정은 Phase 4 execute 쪽 분기.
    if (mode == eTargetMode::Conditional)
    {
        if (m_World.HasComponent<YasuoStateComponent>(m_PlayerEntity))
        {
            // Conditional 모드는 현재 두 갈래 모두 Direction 으로 payload 구성
            mode = eTargetMode::Direction;
        }
        else
        {
            mode = eTargetMode::Direction;
        }
    }

    outCmd.resolvedTargetMode = static_cast<uint8_t>(mode);

    switch (mode)
    {
    case eTargetMode::Self:
    {
        outCmd.targetEntityId = m_PlayerEntity;
        return true;
    }
    case eTargetMode::UnitTarget:
    {
        if (!m_bSylasHovered) return false;   // 호버 필수
        outCmd.targetEntityId = m_SylasEntity;
        return true;
    }
    case eTargetMode::GroundTarget:
    {
        if (!m_pCamera) return false;
        Vec3 ground = CInput::Get().GetMouseGroundPos(
            *m_pCamera,
            static_cast<i32_t>(g_iWinSizeX),
            static_cast<i32_t>(g_iWinSizeY));
        outCmd.groundPos = ground;
        return true;
    }
    case eTargetMode::Direction:
    {
        if (!m_pCamera) return false;
        Vec3 cursor = CInput::Get().GetMouseGroundPos(
            *m_pCamera,
            static_cast<i32_t>(g_iWinSizeX),
            static_cast<i32_t>(g_iWinSizeY));
        if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
            return false;
        const Vec3 origin = m_World.GetComponent<TransformComponent>(m_PlayerEntity).m_LocalPosition;
        f32_t dx = cursor.x - origin.x;
        f32_t dz = cursor.z - origin.z;
        f32_t len = sqrtf(dx * dx + dz * dz);
        if (len < 1e-3f) return false;        // 커서가 캐릭터 위
        outCmd.direction = { dx / len, 0.f, dz / len };
        return true;
    }
    default:
        return false;
    }
}

// 커맨드를 클라에 즉시 적용 (애니 재생 + 예측)
// Phase 4: 서버 보정 시 Replay 할 수 있도록 m_CommandQueue 에 저장하게 확장
void CScene_InGame::ApplyLocalPrediction(const CastSkillCommand& cmd, const SkillDef& def)
{
    // 애니메이션 (기존 FirePlayerAction 재활용 — prefix 자동 분기)
    if (def.animKey)
        FirePlayerAction(def.animKey);

    // 야스오 Q 스택·E 상태 갱신 (Phase 4 서버 이관 전의 로컬 근사)
    using namespace Engine;
    if (def.champ == eChampion::YASUO
        && m_World.HasComponent<YasuoStateComponent>(m_PlayerEntity))
    {
        auto& ys = m_World.GetComponent<YasuoStateComponent>(m_PlayerEntity);

        if (def.slot == static_cast<uint8_t>(eSkillSlot::Q))
        {
            ys.qStackCount = (ys.qStackCount + 1) % 4;   // 0→1→2→3→0
            ys.qStackTimer = 6.f;                         // 6 초 유지
        }
        else if (def.slot == static_cast<uint8_t>(eSkillSlot::E))
        {
            ys.bEActive = true;
            ys.eActiveTimer = 0.5f;                       // E 질풍 이동 시간 근사
        }
    }

    // 디버그 로그
    char buf[192];
    const char* modeName = "?";
    switch (static_cast<eTargetMode>(cmd.resolvedTargetMode))
    {
    case eTargetMode::Self:         modeName = "Self";         break;
    case eTargetMode::UnitTarget:   modeName = "UnitTarget";   break;
    case eTargetMode::GroundTarget: modeName = "GroundTarget"; break;
    case eTargetMode::Direction:    modeName = "Direction";    break;
    default: break;
    }
    sprintf_s(buf, "[Cast] slot=%u mode=%s anim=%s target=%u ground=(%.1f,%.1f,%.1f) dir=(%.2f,%.2f)\n",
        cmd.slot, modeName, def.animKey ? def.animKey : "(null)",
        cmd.targetEntityId,
        cmd.groundPos.x, cmd.groundPos.y, cmd.groundPos.z,
        cmd.direction.x, cmd.direction.z);
    OutputDebugStringA(buf);
}
```

### (D) `OnUpdate` 의 야스오 상태 타이머 감소

**Before** (`OnUpdate` 앞부분):
```cpp
    // Last action 타이머 감소
    if (m_fLastActionTimer > 0.f) m_fLastActionTimer -= dt;
```

**After**:
```cpp
    // Last action 타이머 감소
    if (m_fLastActionTimer > 0.f) m_fLastActionTimer -= dt;

    // [B-6.5] 야스오 상태 타이머 감소
    if (m_YasuoEntity != NULL_ENTITY
        && m_World.HasComponent<YasuoStateComponent>(m_YasuoEntity))
    {
        auto& ys = m_World.GetComponent<YasuoStateComponent>(m_YasuoEntity);
        if (ys.qStackTimer > 0.f)
        {
            ys.qStackTimer -= dt;
            if (ys.qStackTimer <= 0.f) ys.qStackCount = 0;   // 스택 리셋
        }
        if (ys.eActiveTimer > 0.f)
        {
            ys.eActiveTimer -= dt;
            if (ys.eActiveTimer <= 0.f) ys.bEActive = false;
        }
    }
```

### (E) `OnImGui_CombatDebug` — 야스오 상태 표시

**Before** (현 `OnImGui_CombatDebug` 내 `ImGui::Separator();` 바로 앞):
```cpp
    ImGui::Text("Player: %s", champName);
    ImGui::Text("Sylas hovered: %s", m_bSylasHovered ? "YES" : "no");
    ImGui::Text("Last action : %s (%.1fs)",
        m_pLastActionLabel, m_fLastActionTimer > 0 ? m_fLastActionTimer : 0.f);

    ImGui::Separator();
```

**After**:
```cpp
    ImGui::Text("Player: %s", champName);
    ImGui::Text("Sylas hovered: %s", m_bSylasHovered ? "YES" : "no");
    ImGui::Text("Last action : %s (%.1fs)",
        m_pLastActionLabel, m_fLastActionTimer > 0 ? m_fLastActionTimer : 0.f);

    // Yasuo 전용 상태 (선택됐을 때만)
    if (champ == eChampion::YASUO
        && m_World.HasComponent<YasuoStateComponent>(m_PlayerEntity))
    {
        auto& ys = m_World.GetComponent<YasuoStateComponent>(m_PlayerEntity);
        ImGui::Text("Yasuo Q stack: %u  (%.1fs)", ys.qStackCount, ys.qStackTimer);
        ImGui::Text("Yasuo E active: %s (%.2fs)",
            ys.bEActive ? "YES" : "no", ys.eActiveTimer);
    }

    ImGui::Separator();
```

---

## ⑥ 수정 — `Client/Include/Client.vcxproj(.filters)`

### `.vcxproj` — ClInclude/ClCompile ItemGroup 에 신규 파일 추가

```xml
    <ClInclude Include="..\Public\Scene\Scene_InGame.h" />
    <ClInclude Include="..\Public\Gameplay\SkillDef.h" />
```

```xml
    <ClCompile Include="..\Private\Scene\Scene_InGame.cpp" />
    <ClCompile Include="..\Private\Gameplay\SkillTable.cpp" />
```

### `.filters` — Gameplay 필터 신규

```xml
    <Filter Include="Gameplay">
      <UniqueIdentifier>{A1B2C3D4-E5F6-4789-A0B1-C2D3E4F5A6B7}</UniqueIdentifier>
    </Filter>
```

```xml
    <ClInclude Include="..\Public\Gameplay\SkillDef.h">
      <Filter>Gameplay</Filter>
    </ClInclude>
    <ClCompile Include="..\Private\Gameplay\SkillTable.cpp">
      <Filter>Gameplay</Filter>
    </ClCompile>
```

UUID 는 임의값. **VS 에서 "기존 항목 추가"** 로 추가하면 자동 생성되므로 그 방식을 권장.

---

## 빌드 순서

```
1. 신규 파일 2개 생성
   - Client/Public/Gameplay/SkillDef.h
   - Client/Private/Gameplay/SkillTable.cpp
   (Gameplay 폴더가 없으면 mkdir)

2. Engine 측 GameplayComponents.h 에 YasuoStateComponent 추가
   → Engine 빌드 시 PostBuildEvent 의 UpdateLib.bat 이 EngineSDK/inc 로 자동 배포

3. Client 측 Scene_InGame.h / .cpp 변경 적용

4. Client.vcxproj(.filters) 에 신규 파일 등록
   (VS "기존 항목 추가" 권장)

5. Debug x64 빌드

6. 런타임 검증
```

---

## 검증 체크리스트

- [ ] **Irelia** 선택 후 인게임 진입
  - [ ] 우클릭 호버 → `[Cast] slot=0 mode=UnitTarget anim=attack_01 target=<id>`
  - [ ] Q 호버 → `[Cast] slot=1 mode=UnitTarget anim=spell1 target=<id>`
  - [ ] W → `[Cast] slot=2 mode=Self anim=spell2 target=<플레이어 id>` (호버 무관)
  - [ ] E → `[Cast] slot=3 mode=GroundTarget anim=spell3 ground=(x,_,z)`
  - [ ] R → `[Cast] slot=4 mode=Direction anim=spell4 dir=(x,_,z)` (커서가 플레이어 위면 무시)
  - [ ] 호버 아닐 때 Q·우클릭 → 로그 없음
- [ ] **Yasuo** 재선택 후
  - [ ] 우클릭 호버 → `yasuo_attack1`
  - [ ] Q → Direction 해석, Combat Debug 에 `Yasuo Q stack: 1 (6.0s)` 표시, 3번 누르면 0 리셋
  - [ ] E 호버 → UnitTarget 발동, `Yasuo E active: YES (0.5s)` 잠깐 표시
  - [ ] E 직후 Q → 여전히 Direction payload (연출 동일)
- [ ] ImGui 창 위 클릭/키는 통과 안 됨

---

## Day 3 연결 (이후)

- `m_CommandQueue` 추가 → `ApplyLocalPrediction` 끝에서 push
- `Shared/Schemas/Commands.fbs` 에 CastSkillCommand 필드 매핑
- `MoveToCommand` 도 같은 패턴으로 추가 (`OnUpdate` 의 우클릭 지면 이동 블록 교체)
- Phase 4 네트워크 붙일 때 `SendToServer(cmd)` 한 줄 추가
