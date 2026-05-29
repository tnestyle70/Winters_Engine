# 10. Gameplay Framework — UE5-Style GameMode / Controller / Character

> **UE5 대응**: `AGameModeBase`, `AGameStateBase`, `APlayerController`, `APlayerState`, `APawn`, `ACharacter`
> **현재 Winters**: `CScene_InGame` 3000줄 모놀리식 — 매치 규칙 / 스폰 / 입력 / 카메라 / 전투 / FX / 네트워크 전부 한 클래스
> **목표**: Scene_InGame 을 GameMode(100) + Controller(100) + Character(200) 로 분해, 역할별 클래스 명확 분리

---

## 1. Architecture Overview

### 1.1 현재 Scene_InGame 의 역할 분석

```
CScene_InGame (~3000줄) 담당:
├─ 매치 규칙/스폰          → WGameMode
├─ 매치 상태 (타이머/점수)    → WGameState
├─ 입력 처리/카메라/HUD      → WPlayerController
├─ 플레이어 킬/데스/골드/레벨  → WPlayerState
├─ 챔피언 이동/애니/스킬      → WChampionCharacter
├─ 전투 프레임 이벤트 추적     → WSkillComponent
├─ 네트워크 스냅샷 적용       → WPlayerController + WGameState
├─ FX/이펙트 시스템          → FX Subsystem (별도)
└─ 디버그 UI (ImGui)        → 각 클래스의 OnImGui()
```

### 1.2 목표 아키텍처

```
WGameMode_LOL                            ← 매치 규칙 (스폰/승리 조건/페이즈)
  ├─ WGameState_LOL                      ← 공유 매치 상태 (리플리케이션 대상)
  ├─ WPlayerController[0..9]             ← 입력 → 커맨드, 카메라, HUD
  │   ├─ WPlayerState                    ← 플레이어별 킬/데스/골드
  │   └─ WChampionCharacter (Possessed)  ← 이동/애니/스킬/스탯
  │       ├─ WSkillComponent             ← SkillDef 기반 스킬 시스템
  │       ├─ WStatComponent              ← HP/Mana/스탯
  │       └─ WMovementComponent          ← NavAgent 기반 이동
  └─ WChampionCharacter[bots]            ← AI Controller 소유

Scene 은 GameMode 생성 + World 초기화만 담당 (100줄 이하)
```

---

## 2. 파일 구조

```
Engine/
├── Public/Gameplay/
│   ├── WGameMode.h          ← 매치 규칙 기반 클래스
│   ├── WGameState.h         ← 매치 공유 상태
│   ├── WPlayerController.h  ← 입력 → 커맨드, 카메라 소유
│   ├── WPlayerState.h       ← 플레이어별 리플리케이션 상태
│   ├── WPawn.h              ← 소유 가능한 엔티티
│   ├── WCharacter.h         ← Pawn + 스켈레탈 메시 + 이동
│   └── WSkillComponent.h    ← SkillDef/SkillHook 통합
├── Private/Gameplay/
│   ├── WGameMode.cpp
│   ├── WGameState.cpp
│   ├── WPlayerController.cpp
│   ├── WPlayerState.cpp
│   ├── WPawn.cpp
│   ├── WCharacter.cpp
│   └── WSkillComponent.cpp

Client/
├── Private/Gameplay/
│   ├── WGameMode_LOL.h      ← LOL 모작 전용 GameMode
│   ├── WGameMode_LOL.cpp
│   ├── WChampionCharacter.h ← WCharacter 확장 (챔피언 전용)
│   └── WChampionCharacter.cpp
```

---

## 3. 코드 전문

### 3.1 `Engine/Public/Gameplay/WGameMode.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include <vector>
#include <memory>
#include <functional>

class CWorld;
class WPlayerController;
class WGameState;
class WPawn;

// ─────────────────────────────────────────────────────────────────
//  WGameMode — 매치 규칙 / 스폰 / 승리 조건
//
//  UE5 AGameModeBase 대응. 서버 권위 (서버에만 존재).
//  클라이언트는 WGameState 를 통해 매치 상태만 참조.
//
//  Scene 이 OnEnter 에서 1회 생성. GameMode 가 World 수명 관리.
//  현재 CScene_InGame::OnEnter / CreateECSEntities / OnUpdate 의
//  스폰/승리 판정 로직이 여기로 이동.
// ─────────────────────────────────────────────────────────────────
class WINTERS_ENGINE WGameMode
{
public:
    virtual ~WGameMode();

    // ── Lifecycle ──
    virtual void InitGame(CWorld* pWorld);
    virtual void StartMatch();
    virtual void EndMatch();
    virtual void Tick(f32_t fDeltaTime);

    // ── Spawn ──
    virtual EntityID SpawnDefaultPawnForController(WPlayerController* pController);
    virtual void HandleStartingNewPlayer(WPlayerController* pController);

    // ── Match Flow ──
    virtual bool ReadyToStartMatch() const;
    virtual bool ReadyToEndMatch() const;
    virtual void HandleMatchIsWaitingToStart();

    // ── Score/Kill ──
    virtual void OnPlayerKilled(EntityID killerEntity, EntityID victimEntity);
    virtual void OnStructureDestroyed(EntityID structureEntity, u8_t team);

    // ── Access ──
    CWorld*           GetWorld()     const { return m_pWorld; }
    WGameState*       GetGameState() const { return m_pGameState.get(); }
    u32_t             GetPlayerCount() const { return static_cast<u32_t>(m_vecControllers.size()); }
    WPlayerController* GetController(u32_t index) const;

    // ── Registration ──
    void RegisterController(std::unique_ptr<WPlayerController> pController);

    // ── ImGui ──
    virtual void OnImGui();

protected:
    WGameMode();

    CWorld*                                          m_pWorld = nullptr;
    std::unique_ptr<WGameState>                      m_pGameState;
    std::vector<std::unique_ptr<WPlayerController>>  m_vecControllers;

    bool m_bMatchStarted = false;
    bool m_bMatchEnded   = false;
};
```

### 3.2 `Engine/Private/Gameplay/WGameMode.cpp`

```cpp
#include "Gameplay/WGameMode.h"
#include "Gameplay/WPlayerController.h"
#include "Gameplay/WGameState.h"
#include "Gameplay/WPawn.h"
#include "ECS/World.h"

WGameMode::WGameMode() = default;
WGameMode::~WGameMode() = default;

void WGameMode::InitGame(CWorld* pWorld)
{
    m_pWorld = pWorld;
    m_pGameState = std::make_unique<WGameState>();
    m_pGameState->Initialize(pWorld);
}

void WGameMode::StartMatch()
{
    if (m_bMatchStarted) return;
    m_bMatchStarted = true;

    if (m_pGameState)
        m_pGameState->SetMatchPhase(eMatchPhase::InProgress);

    for (auto& pCtrl : m_vecControllers)
        HandleStartingNewPlayer(pCtrl.get());
}

void WGameMode::EndMatch()
{
    if (m_bMatchEnded) return;
    m_bMatchEnded = true;

    if (m_pGameState)
        m_pGameState->SetMatchPhase(eMatchPhase::PostMatch);
}

void WGameMode::Tick(f32_t fDeltaTime)
{
    if (!m_bMatchStarted) return;

    if (m_pGameState)
        m_pGameState->Tick(fDeltaTime);

    for (auto& pCtrl : m_vecControllers)
        pCtrl->Tick(fDeltaTime);

    if (ReadyToEndMatch())
        EndMatch();
}

EntityID WGameMode::SpawnDefaultPawnForController(WPlayerController* pController)
{
    // Base: 빈 엔티티 생성. 서브클래스가 챔피언별 스폰 구현.
    if (!m_pWorld) return NULL_ENTITY;
    return m_pWorld->CreateEntity();
}

void WGameMode::HandleStartingNewPlayer(WPlayerController* pController)
{
    if (!pController) return;
    EntityID pawnEntity = SpawnDefaultPawnForController(pController);
    pController->Possess(pawnEntity);
}

bool WGameMode::ReadyToStartMatch() const
{
    return !m_vecControllers.empty();
}

bool WGameMode::ReadyToEndMatch() const
{
    // Base: 끝나지 않음. LOL 서브클래스가 넥서스 파괴 판정.
    return false;
}

void WGameMode::HandleMatchIsWaitingToStart()
{
    if (ReadyToStartMatch())
        StartMatch();
}

void WGameMode::OnPlayerKilled(EntityID killerEntity, EntityID victimEntity)
{
    // 서브클래스에서 킬/데스 카운트 + 골드 보상 처리
}

void WGameMode::OnStructureDestroyed(EntityID structureEntity, u8_t team)
{
    // 서브클래스에서 타워/넥서스 파괴 → 승리 조건 확인
}

WPlayerController* WGameMode::GetController(u32_t index) const
{
    if (index >= m_vecControllers.size()) return nullptr;
    return m_vecControllers[index].get();
}

void WGameMode::RegisterController(std::unique_ptr<WPlayerController> pController)
{
    m_vecControllers.push_back(std::move(pController));
}

void WGameMode::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (m_pGameState) m_pGameState->OnImGui();
#endif
}
```

### 3.3 `Engine/Public/Gameplay/WGameState.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

class CWorld;

enum class eMatchPhase : u8_t
{
    WaitingToStart = 0,
    BanPick,
    Loading,
    InProgress,
    PostMatch,
    END
};

// ─────────────────────────────────────────────────────────────────
//  WGameState — 모든 클라이언트에게 리플리케이션되는 매치 상태
//
//  UE5 AGameStateBase 대응.
//  현재 Scene_InGame 에 흩어진 매치 타이머 / 스코어 / 페이즈 상태 통합.
// ─────────────────────────────────────────────────────────────────
class WINTERS_ENGINE WGameState
{
public:
    WGameState()  = default;
    ~WGameState() = default;

    void Initialize(CWorld* pWorld);
    void Tick(f32_t fDeltaTime);

    // ── Match Phase ──
    eMatchPhase GetMatchPhase()  const { return m_ePhase; }
    void        SetMatchPhase(eMatchPhase phase) { m_ePhase = phase; }

    // ── Match Timer ──
    f32_t GetMatchTimeSec() const { return m_fMatchTimeSec; }

    // ── Team Score ──
    u32_t GetTeamKills(u8_t team) const;
    void  AddTeamKill(u8_t team);
    u32_t GetTeamTowerKills(u8_t team) const;
    void  AddTeamTowerKill(u8_t team);

    // ── ImGui ──
    void OnImGui();

private:
    CWorld*     m_pWorld = nullptr;
    eMatchPhase m_ePhase = eMatchPhase::WaitingToStart;
    f32_t       m_fMatchTimeSec = 0.f;

    // 2팀 (Blue=0, Red=1)
    u32_t m_aTeamKills[2]      = { 0, 0 };
    u32_t m_aTeamTowerKills[2] = { 0, 0 };
};
```

### 3.4 `Engine/Private/Gameplay/WGameState.cpp`

```cpp
#include "Gameplay/WGameState.h"
#include "ECS/World.h"

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

void WGameState::Initialize(CWorld* pWorld)
{
    m_pWorld = pWorld;
}

void WGameState::Tick(f32_t fDeltaTime)
{
    if (m_ePhase == eMatchPhase::InProgress)
        m_fMatchTimeSec += fDeltaTime;
}

u32_t WGameState::GetTeamKills(u8_t team) const
{
    if (team >= 2) return 0;
    return m_aTeamKills[team];
}

void WGameState::AddTeamKill(u8_t team)
{
    if (team < 2) ++m_aTeamKills[team];
}

u32_t WGameState::GetTeamTowerKills(u8_t team) const
{
    if (team >= 2) return 0;
    return m_aTeamTowerKills[team];
}

void WGameState::AddTeamTowerKill(u8_t team)
{
    if (team < 2) ++m_aTeamTowerKills[team];
}

void WGameState::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (ImGui::CollapsingHeader("GameState"))
    {
        const char* phaseNames[] = {
            "WaitingToStart", "BanPick", "Loading", "InProgress", "PostMatch"
        };
        i32_t phaseIdx = static_cast<i32_t>(m_ePhase);
        ImGui::Text("Phase: %s", (phaseIdx < 5) ? phaseNames[phaseIdx] : "Unknown");

        i32_t minutes = static_cast<i32_t>(m_fMatchTimeSec) / 60;
        i32_t seconds = static_cast<i32_t>(m_fMatchTimeSec) % 60;
        ImGui::Text("Match Time: %02d:%02d", minutes, seconds);
        ImGui::Text("Blue Kills: %u  |  Red Kills: %u", m_aTeamKills[0], m_aTeamKills[1]);
        ImGui::Text("Blue Towers: %u |  Red Towers: %u", m_aTeamTowerKills[0], m_aTeamTowerKills[1]);
    }
#endif
}
```

### 3.5 `Engine/Public/Gameplay/WPlayerController.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameContext.h"

class CWorld;
class CDynamicCamera;
class WPawn;
struct CastSkillCommand;
struct SkillDef;

// ─────────────────────────────────────────────────────────────────
//  WPlayerController — 입력 → 커맨드 변환, 카메라 소유, HUD 소유
//
//  UE5 APlayerController 대응.
//  현재 Scene_InGame 의 UpdateTargeting / UpdateCombatInput /
//  DispatchSkillInput / ApplyLocalPrediction / 카메라 로직이 여기로 이동.
//
//  로컬 플레이어 1명 = 1 Controller. 리모트 플레이어도 1 Controller.
//  서버에선 모든 Controller 가 존재 (입력 수신용).
// ─────────────────────────────────────────────────────────────────
class WINTERS_ENGINE WPlayerController
{
public:
    WPlayerController();
    virtual ~WPlayerController();

    // ── Lifecycle ──
    virtual void Initialize(CWorld* pWorld, eChampion selectedChampion, eTeam team);
    virtual void Tick(f32_t fDeltaTime);

    // ── Possession ──
    virtual void Possess(EntityID pawnEntity);
    virtual void UnPossess();
    EntityID     GetPawnEntity() const { return m_PawnEntity; }
    bool         HasPawn()       const { return m_PawnEntity != NULL_ENTITY; }

    // ── Input Processing ──
    virtual void ProcessInput(f32_t fDeltaTime);
    virtual void UpdateTargeting();
    virtual bool DispatchSkillInput(u8_t slot);
    virtual void ProcessMoveCommand(const Vec3& vWorldDest);

    // ── Camera ──
    void SetCamera(CDynamicCamera* pCamera) { m_pCamera = pCamera; }
    CDynamicCamera* GetCamera() const       { return m_pCamera; }

    // ── Identity ──
    eChampion   GetChampion()  const { return m_eChampion; }
    eTeam       GetTeam()      const { return m_eTeam; }
    u32_t       GetNetId()     const { return m_iNetId; }
    void        SetNetId(u32_t id)   { m_iNetId = id; }

    // ── Targeting ──
    EntityID    GetHoveredEntity() const { return m_HoveredEntity; }
    eTeam       GetHoveredTeam()   const { return m_HoveredTeam; }
    bool        IsEnemyOfPlayer(EntityID entity) const;

    // ── Player State ──
    u32_t GetKills()  const { return m_iKills; }
    u32_t GetDeaths() const { return m_iDeaths; }
    u32_t GetGold()   const { return m_iGold; }
    u8_t  GetLevel()  const { return m_iLevel; }
    void  AddKill();
    void  AddDeath();
    void  AddGold(u32_t amount);
    void  SetLevel(u8_t level);

    // ── ImGui ──
    virtual void OnImGui();

    // ── Network ──
    bool IsLocalController() const { return m_bIsLocal; }
    void SetIsLocal(bool b)        { m_bIsLocal = b; }

protected:
    CWorld*          m_pWorld       = nullptr;
    CDynamicCamera*  m_pCamera      = nullptr;
    EntityID         m_PawnEntity   = NULL_ENTITY;

    // Identity
    eChampion m_eChampion = eChampion::END;
    eTeam     m_eTeam     = eTeam::TEAM_END;
    u32_t     m_iNetId    = 0;
    bool      m_bIsLocal  = true;

    // Targeting (from Scene_InGame::UpdateTargeting)
    EntityID m_HoveredEntity = NULL_ENTITY;
    eTeam    m_HoveredTeam   = eTeam::TEAM_END;

    // Player State (from Scene_InGame scattered members, UE5 APlayerState 해당)
    u32_t m_iKills   = 0;
    u32_t m_iDeaths  = 0;
    u32_t m_iGold    = 500;     // 시작 골드
    u8_t  m_iLevel   = 1;

    // Attack mode (A key)
    bool  m_bAttackMode     = false;
    bool  m_bShowAttackRange = false;
};
```

### 3.6 `Engine/Private/Gameplay/WPlayerController.cpp`

```cpp
#include "Gameplay/WPlayerController.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "Core/CInput.h"
#include "DynamicCamera.h"

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

WPlayerController::WPlayerController()  = default;
WPlayerController::~WPlayerController() = default;

void WPlayerController::Initialize(CWorld* pWorld, eChampion selectedChampion, eTeam team)
{
    m_pWorld    = pWorld;
    m_eChampion = selectedChampion;
    m_eTeam     = team;
}

void WPlayerController::Tick(f32_t fDeltaTime)
{
    if (!m_bIsLocal) return;   // 리모트 컨트롤러는 서버 입력 수신만

    ProcessInput(fDeltaTime);
    UpdateTargeting();
}

void WPlayerController::Possess(EntityID pawnEntity)
{
    if (m_PawnEntity != NULL_ENTITY)
        UnPossess();

    m_PawnEntity = pawnEntity;

    // 카메라 팔로우 연결
    // (서브클래스에서 카메라를 Pawn Transform 에 바인딩)
}

void WPlayerController::UnPossess()
{
    m_PawnEntity = NULL_ENTITY;
}

void WPlayerController::ProcessInput(f32_t fDeltaTime)
{
    // ── 스킬 입력 (현재 Scene_InGame::UpdateCombatInput 에서 이관) ──
    //
    // 기존 코드:
    //   if (GetAsyncKeyState('Q') & 0x8000) DispatchSkillInput(1);
    //   if (GetAsyncKeyState('W') & 0x8000) DispatchSkillInput(2);
    //   if (GetAsyncKeyState('E') & 0x8000) DispatchSkillInput(3);
    //   if (GetAsyncKeyState('R') & 0x8000) DispatchSkillInput(4);
    //
    // 이 로직이 그대로 여기로 이동.
    // 단, 쿨다운/마나 검사는 WSkillComponent 에 위임.

    if (GetAsyncKeyState('Q') & 0x8000) DispatchSkillInput(1);
    if (GetAsyncKeyState('W') & 0x8000) DispatchSkillInput(2);
    if (GetAsyncKeyState('E') & 0x8000) DispatchSkillInput(3);
    if (GetAsyncKeyState('R') & 0x8000) DispatchSkillInput(4);

    // ── 이동 (우클릭) ──
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
    {
        // 카메라 → ScreenToWorld ray → 지면 교차점 계산
        // ProcessMoveCommand(worldHitPoint);
    }

    // ── A키 공격 모드 ──
    if (GetAsyncKeyState('A') & 0x8000)
    {
        m_bAttackMode = true;
        m_bShowAttackRange = true;
    }
}

void WPlayerController::UpdateTargeting()
{
    // ── 현재 Scene_InGame::UpdateTargeting 로직 이관 ──
    //
    // 기존 코드 (Scene_InGame.cpp L900~L1000):
    //   마우스 ray vs 모든 챔피언 cylinder → 가장 가까운 엔티티 → m_HoveredEntity
    //   팀 판별 → m_HoveredTeam
    //
    // 변경 없이 멤버만 이동. CWorld 를 순회하며 ChampionComponent + TransformComponent 가진
    // 엔티티 대상으로 ray pick.

    if (!m_pWorld || !m_pCamera) return;

    // TODO: Ray casting 로직 이관 (현재 Scene_InGame::RayVsCylinder 참조)
    // m_HoveredEntity = ...;
    // m_HoveredTeam   = ...;
}

bool WPlayerController::DispatchSkillInput(u8_t slot)
{
    // ── 현재 Scene_InGame::DispatchSkillInput 이관 ──
    //
    // 기존 코드:
    //   1. FindSkillDef(champ, slot) 조회
    //   2. 쿨다운/마나 검사
    //   3. BuildCastCommand → CastSkillCommand 구성
    //   4. ApplyLocalPrediction (애니 재생 + lockDurationSec 설정)
    //   5. 네트워크 전송 (CommandSerializer)
    //
    // 이후: WSkillComponent::TryCastSkill(slot, cmd) 호출로 단순화.
    // WSkillComponent 가 SkillDef 조회 + 쿨다운 + 예측 처리.

    return false;
}

void WPlayerController::ProcessMoveCommand(const Vec3& vWorldDest)
{
    if (!m_pWorld || m_PawnEntity == NULL_ENTITY) return;

    // NavAgent 에 목표 설정 (현재 Scene_InGame 의 이동 코드 이관)
    if (m_pWorld->HasComponent<NavAgentComponent>(m_PawnEntity))
    {
        auto& nav = m_pWorld->GetComponent<NavAgentComponent>(m_PawnEntity);
        nav.vGoal     = vWorldDest;
        nav.bHasGoal  = true;
        nav.bPathDirty = true;
    }
}

bool WPlayerController::IsEnemyOfPlayer(EntityID entity) const
{
    if (!m_pWorld || entity == NULL_ENTITY) return false;
    if (!m_pWorld->HasComponent<ChampionComponent>(entity)) return false;
    const auto& cc = m_pWorld->GetComponent<ChampionComponent>(entity);
    return cc.team != m_eTeam;
}

void WPlayerController::AddKill()   { ++m_iKills; }
void WPlayerController::AddDeath()  { ++m_iDeaths; }
void WPlayerController::AddGold(u32_t amount) { m_iGold += amount; }
void WPlayerController::SetLevel(u8_t level)  { m_iLevel = level; }

void WPlayerController::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (ImGui::CollapsingHeader("PlayerController"))
    {
        ImGui::Text("Champion: %d  Team: %d", static_cast<i32_t>(m_eChampion), static_cast<i32_t>(m_eTeam));
        ImGui::Text("K/D: %u/%u  Gold: %u  Lv: %u", m_iKills, m_iDeaths, m_iGold, m_iLevel);
        ImGui::Text("Pawn: %u  Hovered: %u", m_PawnEntity, m_HoveredEntity);
        ImGui::Checkbox("Attack Mode", &m_bAttackMode);
    }
#endif
}
```

### 3.7 `Engine/Public/Gameplay/WPawn.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

class CWorld;

// ─────────────────────────────────────────────────────────────────
//  WPawn — Controller 에 의해 소유(Possess)되는 최소 단위
//
//  ECS 엔티티 위에 얹는 래퍼. 엔티티 ID 와 World 참조를 보유.
//  UE5 APawn 대응: 소유 가능 + 이동 컴포넌트 인터페이스.
// ─────────────────────────────────────────────────────────────────
class WINTERS_ENGINE WPawn
{
public:
    WPawn();
    virtual ~WPawn();

    virtual void Initialize(CWorld* pWorld, EntityID entity);
    virtual void Tick(f32_t fDeltaTime);

    EntityID GetEntityID() const { return m_Entity; }
    CWorld*  GetWorld()    const { return m_pWorld; }

    // Controller 바인딩
    void SetControllerNetId(u32_t id) { m_iControllerNetId = id; }
    u32_t GetControllerNetId() const  { return m_iControllerNetId; }

    bool IsPossessed() const { return m_iControllerNetId != 0; }

protected:
    CWorld*  m_pWorld  = nullptr;
    EntityID m_Entity  = NULL_ENTITY;
    u32_t    m_iControllerNetId = 0;
};
```

### 3.8 `Engine/Private/Gameplay/WPawn.cpp`

```cpp
#include "Gameplay/WPawn.h"
#include "ECS/World.h"

WPawn::WPawn()  = default;
WPawn::~WPawn() = default;

void WPawn::Initialize(CWorld* pWorld, EntityID entity)
{
    m_pWorld = pWorld;
    m_Entity = entity;
}

void WPawn::Tick(f32_t fDeltaTime)
{
    // Base: 아무것도 안 함. WCharacter 가 이동/애니 업데이트.
}
```

### 3.9 `Engine/Public/Gameplay/WCharacter.h`

```cpp
#pragma once

#include "Gameplay/WPawn.h"
#include "WintersMath.h"
#include <string>
#include <memory>

class ModelRenderer;
class CTransform;
namespace Engine { class CAnimator; }

// ─────────────────────────────────────────────────────────────────
//  WCharacter — Pawn + 스켈레탈 메시 + 이동 컴포넌트
//
//  UE5 ACharacter 대응.
//  현재 Scene_InGame 의 m_pPlayerRenderer / m_pPlayerTransform /
//  m_bMoving / m_fPlayerSpeed / m_vPlayerDest + 이동 보간 로직 통합.
//
//  렌더러(ModelRenderer)와 트랜스폼(CTransform)을 소유.
//  매 Tick 에서 NavAgent 결과 → Transform 동기화 + 애니메이션 업데이트.
// ─────────────────────────────────────────────────────────────────
class WINTERS_ENGINE WCharacter : public WPawn
{
public:
    WCharacter();
    ~WCharacter() override;

    void Initialize(CWorld* pWorld, EntityID entity) override;
    void Tick(f32_t fDeltaTime) override;

    // ── Renderer Binding ──
    void BindRenderer(ModelRenderer* pRenderer, CTransform* pTransform);
    ModelRenderer* GetRenderer()  const { return m_pRenderer; }
    CTransform*    GetTransform() const { return m_pTransform; }

    // ── Animation ──
    void PlayAnimation(const std::string& strAnimKey, bool bLoop = true);
    void SetAnimPlaySpeed(f32_t fSpeed);
    const Engine::CAnimator* GetAnimator() const;
    Engine::CAnimator*       GetAnimator();

    // ── Locomotion ──
    void SetMoveDestination(const Vec3& vDest);
    void StopMovement();
    bool IsMoving()       const { return m_bMoving; }
    f32_t GetMoveSpeed()  const { return m_fMoveSpeed; }
    void  SetMoveSpeed(f32_t fSpeed) { m_fMoveSpeed = fSpeed; }

    // ── Idle/Run Anim Keys ──
    void SetIdleAnimKey(const std::string& key) { m_strIdleAnim = key; }
    void SetRunAnimKey(const std::string& key)   { m_strRunAnim = key; }
    const std::string& GetIdleAnimKey() const    { return m_strIdleAnim; }
    const std::string& GetRunAnimKey()  const    { return m_strRunAnim; }

    // ── Facing ──
    void FaceDirection(const Vec3& vDir);
    void FaceTarget(EntityID target);
    f32_t GetYaw() const { return m_fYaw; }

    // ── State ──
    bool IsActionLocked()    const { return m_fActionLockTimer > 0.f; }
    void SetActionLock(f32_t sec)  { m_fActionLockTimer = sec; }
    void ClearActionLock()         { m_fActionLockTimer = 0.f; }

    // ── ImGui ──
    void OnImGui();

protected:
    // ── Locomotion Update (현재 Scene_InGame OnUpdate 이동 코드 이관) ──
    void UpdateLocomotion(f32_t fDeltaTime);
    void UpdateAnimation(f32_t fDeltaTime);
    void SyncTransformToECS();
    void SyncTransformFromECS();

    ModelRenderer* m_pRenderer   = nullptr;
    CTransform*    m_pTransform  = nullptr;

    // Locomotion (from Scene_InGame)
    Vec3  m_vMoveDestination{ 0.f, 0.f, 0.f };
    bool  m_bMoving    = false;
    f32_t m_fMoveSpeed = 8.f;
    f32_t m_fArriveRadius = 0.15f;

    // Facing
    f32_t m_fYaw = 0.f;

    // Action lock (from Scene_InGame::m_fLastActionTimer)
    f32_t m_fActionLockTimer = 0.f;

    // Animation keys (from Scene_InGame::m_pPlayerIdleAnim / m_pPlayerRunAnim)
    std::string m_strIdleAnim;
    std::string m_strRunAnim;
};
```

### 3.10 `Engine/Private/Gameplay/WCharacter.cpp`

```cpp
#include "Gameplay/WCharacter.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "Renderer/ModelRenderer.h"
#include "Core/CTransform.h"
#include "Resource/Animator.h"
#include <cmath>

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

WCharacter::WCharacter()  = default;
WCharacter::~WCharacter() = default;

void WCharacter::Initialize(CWorld* pWorld, EntityID entity)
{
    WPawn::Initialize(pWorld, entity);
}

void WCharacter::Tick(f32_t fDeltaTime)
{
    // ── Action Lock Timer (기존 Scene_InGame::m_fLastActionTimer) ──
    if (m_fActionLockTimer > 0.f)
    {
        m_fActionLockTimer -= fDeltaTime;
        if (m_fActionLockTimer <= 0.f)
        {
            m_fActionLockTimer = 0.f;
            // 락 해제 → idle/run 복귀
            if (m_pRenderer)
            {
                PlayAnimation(m_bMoving ? m_strRunAnim : m_strIdleAnim, true);
            }
        }
    }

    UpdateLocomotion(fDeltaTime);
    UpdateAnimation(fDeltaTime);
    SyncTransformToECS();
}

void WCharacter::BindRenderer(ModelRenderer* pRenderer, CTransform* pTransform)
{
    m_pRenderer  = pRenderer;
    m_pTransform = pTransform;
}

void WCharacter::PlayAnimation(const std::string& strAnimKey, bool bLoop)
{
    if (!m_pRenderer || strAnimKey.empty()) return;
    m_pRenderer->PlayAnimationByName(strAnimKey, bLoop);
}

void WCharacter::SetAnimPlaySpeed(f32_t fSpeed)
{
    if (auto* pAnim = GetAnimator())
        pAnim->SetPlaySpeed(fSpeed);
}

const Engine::CAnimator* WCharacter::GetAnimator() const
{
    return m_pRenderer ? m_pRenderer->GetAnimator() : nullptr;
}

Engine::CAnimator* WCharacter::GetAnimator()
{
    return m_pRenderer ? m_pRenderer->GetAnimator() : nullptr;
}

void WCharacter::SetMoveDestination(const Vec3& vDest)
{
    m_vMoveDestination = vDest;
    m_bMoving = true;

    // NavAgent 에 목표 설정
    if (m_pWorld && m_Entity != NULL_ENTITY
        && m_pWorld->HasComponent<NavAgentComponent>(m_Entity))
    {
        auto& nav = m_pWorld->GetComponent<NavAgentComponent>(m_Entity);
        nav.vGoal      = vDest;
        nav.bHasGoal   = true;
        nav.bPathDirty = true;
    }
}

void WCharacter::StopMovement()
{
    m_bMoving = false;

    if (m_pWorld && m_Entity != NULL_ENTITY
        && m_pWorld->HasComponent<NavAgentComponent>(m_Entity))
    {
        auto& nav = m_pWorld->GetComponent<NavAgentComponent>(m_Entity);
        nav.bHasGoal = false;
    }
}

void WCharacter::FaceDirection(const Vec3& vDir)
{
    // 모델 yaw +XM_PI 보정 (CLAUDE.md 5.5 규칙)
    m_fYaw = std::atan2(vDir.x, vDir.z) + 3.14159265f;
    if (m_pTransform)
        m_pTransform->SetRotation({ 0.f, m_fYaw, 0.f });
}

void WCharacter::FaceTarget(EntityID target)
{
    if (!m_pWorld || target == NULL_ENTITY) return;
    if (!m_pWorld->HasComponent<TransformComponent>(target)) return;

    const Vec3 selfPos = m_pTransform ? m_pTransform->GetPosition() : Vec3{};
    const Vec3 targetPos = m_pWorld->GetComponent<TransformComponent>(target).m_LocalPosition;

    const f32_t dx = targetPos.x - selfPos.x;
    const f32_t dz = targetPos.z - selfPos.z;
    const f32_t len = std::sqrtf(dx * dx + dz * dz);
    if (len > 0.01f)
        FaceDirection({ dx / len, 0.f, dz / len });
}

void WCharacter::UpdateLocomotion(f32_t fDeltaTime)
{
    // ── 기존 Scene_InGame OnUpdate 이동 보간 코드 이관 ──
    //
    // ECS NavAgent 가 경로를 계산하고, NavigationSystem 이 velocity 를 갱신.
    // 여기서는 Transform 동기화 + 도착 판정만 수행.

    if (!m_bMoving || !m_pTransform) return;

    SyncTransformFromECS();

    const Vec3 pos = m_pTransform->GetPosition();
    const f32_t dx = m_vMoveDestination.x - pos.x;
    const f32_t dz = m_vMoveDestination.z - pos.z;
    const f32_t dist = std::sqrtf(dx * dx + dz * dz);

    if (dist <= m_fArriveRadius)
    {
        StopMovement();
        if (!IsActionLocked())
            PlayAnimation(m_strIdleAnim, true);
    }
    else
    {
        // 이동 방향으로 회전
        FaceDirection({ dx / dist, 0.f, dz / dist });
    }
}

void WCharacter::UpdateAnimation(f32_t fDeltaTime)
{
    if (!m_pRenderer) return;
    m_pRenderer->Update(fDeltaTime);
}

void WCharacter::SyncTransformToECS()
{
    if (!m_pWorld || m_Entity == NULL_ENTITY || !m_pTransform) return;
    if (!m_pWorld->HasComponent<TransformComponent>(m_Entity)) return;

    auto& tf = m_pWorld->GetComponent<TransformComponent>(m_Entity);
    tf.m_LocalPosition = m_pTransform->GetPosition();
    tf.m_LocalRotation = m_pTransform->GetRotation();
    tf.m_LocalScale    = m_pTransform->GetScale();
    tf.m_bLocalDirty   = true;
}

void WCharacter::SyncTransformFromECS()
{
    if (!m_pWorld || m_Entity == NULL_ENTITY || !m_pTransform) return;
    if (!m_pWorld->HasComponent<TransformComponent>(m_Entity)) return;

    const auto& tf = m_pWorld->GetComponent<TransformComponent>(m_Entity);
    m_pTransform->SetPosition(tf.m_LocalPosition.x, tf.m_LocalPosition.y, tf.m_LocalPosition.z);
}

void WCharacter::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (ImGui::CollapsingHeader("WCharacter"))
    {
        ImGui::Text("Entity: %u  Moving: %s", m_Entity, m_bMoving ? "Yes" : "No");
        ImGui::SliderFloat("MoveSpeed", &m_fMoveSpeed, 1.f, 20.f);
        ImGui::SliderFloat("ArriveRadius", &m_fArriveRadius, 0.05f, 1.f);
        ImGui::Text("ActionLock: %.2f s", m_fActionLockTimer);
        ImGui::Text("Yaw: %.2f rad", m_fYaw);
        if (m_pRenderer)
        {
            if (auto* pAnim = m_pRenderer->GetAnimator())
            {
                f32_t speed = pAnim->GetPlaySpeed();
                if (ImGui::SliderFloat("AnimSpeed", &speed, 0.01f, 5.f))
                    pAnim->SetPlaySpeed(speed);
            }
        }
    }
#endif
}
```

### 3.11 `Engine/Public/Gameplay/WSkillComponent.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "GameContext.h"
#include <functional>

struct SkillDef;
struct CastSkillCommand;
class CWorld;
class WCharacter;

// ─────────────────────────────────────────────────────────────────
//  WSkillComponent — SkillDef + SkillHook 통합 컴포넌트
//
//  현재 Scene_InGame 의 m_pActiveSkillDef / m_fActivePrevFrame /
//  castFrame/recoveryFrame 감지 / SkillHookRegistry dispatch 전부 통합.
//
//  WChampionCharacter 가 소유. 매 Tick 에서 프레임 이벤트 감시.
//  SkillDef.lockDurationSec 와 animPlaySpeed 연동 자동화.
// ─────────────────────────────────────────────────────────────────
class WINTERS_ENGINE WSkillComponent
{
public:
    WSkillComponent();
    ~WSkillComponent();

    void Initialize(CWorld* pWorld, EntityID ownerEntity, eChampion champ);
    void Tick(f32_t fDeltaTime, WCharacter* pCharacter);

    // ── Skill Execution ──
    bool TryCastSkill(u8_t slot, const CastSkillCommand& cmd);
    void CancelActiveSkill(const char* reason);

    // ── State Query ──
    bool HasActiveSkill()     const { return m_pActiveSkillDef != nullptr; }
    u8_t GetActiveSlot()      const;
    f32_t GetCooldownRemaining(u8_t slot) const;
    bool IsOnCooldown(u8_t slot) const;

    // ── Frame Event Callbacks ──
    using FrameCallback = std::function<void(const SkillDef&, const CastSkillCommand&)>;
    void SetOnCastFrame(FrameCallback cb)     { m_cbOnCastFrame = std::move(cb); }
    void SetOnRecoveryFrame(FrameCallback cb) { m_cbOnRecoveryFrame = std::move(cb); }

    // ── ImGui ──
    void OnImGui();

private:
    void UpdateFrameEvents(WCharacter* pCharacter);
    void UpdateCooldowns(f32_t fDeltaTime);

    CWorld*   m_pWorld       = nullptr;
    EntityID  m_OwnerEntity  = NULL_ENTITY;
    eChampion m_eChampion    = eChampion::END;

    // Active skill tracking (from Scene_InGame)
    SkillDef         m_ActiveSkillDefStorage{};
    CastSkillCommand m_ActiveCommandStorage{};
    const SkillDef*  m_pActiveSkillDef  = nullptr;
    f32_t            m_fActivePrevFrame = 0.f;
    bool             m_bCastFrameFired     = false;
    bool             m_bRecoveryFrameFired = false;

    // Cooldowns (5 slots: BA + Q/W/E/R)
    static constexpr u32_t kMaxSlots = 5;
    f32_t m_afCooldowns[kMaxSlots]   = {};

    // Frame event callbacks
    FrameCallback m_cbOnCastFrame;
    FrameCallback m_cbOnRecoveryFrame;
};
```

### 3.12 `Engine/Private/Gameplay/WSkillComponent.cpp`

```cpp
#include "Gameplay/WSkillComponent.h"
#include "Gameplay/WCharacter.h"
#include "ECS/World.h"
#include "Resource/Animator.h"

// SkillDef lookup
extern const SkillDef* FindSkillDef(eChampion champ, uint8_t slot);

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

WSkillComponent::WSkillComponent()  = default;
WSkillComponent::~WSkillComponent() = default;

void WSkillComponent::Initialize(CWorld* pWorld, EntityID ownerEntity, eChampion champ)
{
    m_pWorld      = pWorld;
    m_OwnerEntity = ownerEntity;
    m_eChampion   = champ;
    for (u32_t i = 0; i < kMaxSlots; ++i)
        m_afCooldowns[i] = 0.f;
}

void WSkillComponent::Tick(f32_t fDeltaTime, WCharacter* pCharacter)
{
    UpdateCooldowns(fDeltaTime);

    if (m_pActiveSkillDef && pCharacter)
        UpdateFrameEvents(pCharacter);
}

bool WSkillComponent::TryCastSkill(u8_t slot, const CastSkillCommand& cmd)
{
    if (slot >= kMaxSlots) return false;
    if (m_afCooldowns[slot] > 0.f) return false;
    if (m_pActiveSkillDef) return false;   // 이미 스킬 사용 중

    const SkillDef* pDef = FindSkillDef(m_eChampion, slot);
    if (!pDef) return false;

    // 쿨다운 시작
    m_afCooldowns[slot] = pDef->cooldownSec;

    // 활성 스킬 저장 (댕글링 방지: 값 복사, CLAUDE.md 5.3)
    m_ActiveSkillDefStorage = *pDef;
    m_pActiveSkillDef       = &m_ActiveSkillDefStorage;
    m_ActiveCommandStorage  = cmd;
    m_fActivePrevFrame      = 0.f;
    m_bCastFrameFired       = false;
    m_bRecoveryFrameFired   = false;

    return true;
}

void WSkillComponent::CancelActiveSkill(const char* reason)
{
    m_pActiveSkillDef     = nullptr;
    m_bCastFrameFired     = false;
    m_bRecoveryFrameFired = false;
    m_fActivePrevFrame    = 0.f;
}

u8_t WSkillComponent::GetActiveSlot() const
{
    return m_pActiveSkillDef ? m_pActiveSkillDef->slot : 255;
}

f32_t WSkillComponent::GetCooldownRemaining(u8_t slot) const
{
    if (slot >= kMaxSlots) return 0.f;
    return m_afCooldowns[slot];
}

bool WSkillComponent::IsOnCooldown(u8_t slot) const
{
    return GetCooldownRemaining(slot) > 0.f;
}

void WSkillComponent::UpdateFrameEvents(WCharacter* pCharacter)
{
    // ── 기존 Scene_InGame.cpp L1147~L1300 castFrame/recoveryFrame 감지 이관 ──
    //
    // 핵심 로직:
    //   const CAnimator* pAnim = pCharacter->GetAnimator();
    //   const f32_t curF = pAnim->GetCurrentFrame();
    //   HasFramePassed(castFrame, prevFrame) → castFrame 콜백
    //   HasFramePassed(recoveryFrame, prevFrame) → recoveryFrame 콜백
    //
    // castFrame 감지 블록 분리 금지 규칙 준수 (CLAUDE.md 5.6):
    //   단일 블록에서 bCastHit / bRecoveryHit 판정 후, 맨 마지막에 prevFrame 갱신.

    const Engine::CAnimator* pAnim = pCharacter->GetAnimator();
    if (!pAnim || !pAnim->IsPlaying()) return;

    const f32_t curF = pAnim->GetCurrentFrame();
    const SkillDef& d = *m_pActiveSkillDef;

    const bool bCastHit =
        !m_bCastFrameFired
        && d.castFrame > 0.f
        && pAnim->HasFramePassed(d.castFrame, m_fActivePrevFrame);

    const bool bRecoveryHit =
        !m_bRecoveryFrameFired
        && d.recoveryFrame > 0.f
        && pAnim->HasFramePassed(d.recoveryFrame, m_fActivePrevFrame);

    if (bCastHit)
    {
        m_bCastFrameFired = true;
        if (m_cbOnCastFrame)
            m_cbOnCastFrame(d, m_ActiveCommandStorage);
    }

    if (bRecoveryHit)
    {
        m_bRecoveryFrameFired = true;
        if (m_cbOnRecoveryFrame)
            m_cbOnRecoveryFrame(d, m_ActiveCommandStorage);
    }

    // ★ 반드시 모든 판정 후 마지막에 prevFrame 갱신
    m_fActivePrevFrame = curF;

    // 애니 종료 → 스킬 해제
    if (!pAnim->IsPlaying() && m_bRecoveryFrameFired)
        CancelActiveSkill("anim_complete");
}

void WSkillComponent::UpdateCooldowns(f32_t fDeltaTime)
{
    for (u32_t i = 0; i < kMaxSlots; ++i)
    {
        if (m_afCooldowns[i] > 0.f)
        {
            m_afCooldowns[i] -= fDeltaTime;
            if (m_afCooldowns[i] < 0.f)
                m_afCooldowns[i] = 0.f;
        }
    }
}

void WSkillComponent::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (ImGui::CollapsingHeader("WSkillComponent"))
    {
        const char* slotNames[] = { "BA", "Q", "W", "E", "R" };
        for (u32_t i = 0; i < kMaxSlots; ++i)
        {
            ImGui::Text("%s CD: %.1f s", slotNames[i], m_afCooldowns[i]);
        }
        if (m_pActiveSkillDef)
        {
            ImGui::Text("Active: slot=%u  anim=%s", m_pActiveSkillDef->slot,
                m_pActiveSkillDef->animKey ? m_pActiveSkillDef->animKey : "?");
            ImGui::Text("CastFired: %s  RecoveryFired: %s",
                m_bCastFrameFired ? "Y" : "N", m_bRecoveryFrameFired ? "Y" : "N");
        }
        else
        {
            ImGui::Text("Active: (none)");
        }
    }
#endif
}
```

### 3.13 `Client/Private/Gameplay/WGameMode_LOL.h`

```cpp
#pragma once

#include "Gameplay/WGameMode.h"
#include "GameContext.h"
#include <unordered_map>
#include <memory>

class ModelRenderer;
class CTransform;

// ─────────────────────────────────────────────────────────────────
//  WGameMode_LOL — 리그 오브 레전드 모작 전용 GameMode
//
//  현재 Scene_InGame::OnEnter 의 스폰 / 팀 배정 / 승리 조건 이관.
//  10인 매치 (5v5), 넥서스 파괴 시 EndMatch.
// ─────────────────────────────────────────────────────────────────
class WGameMode_LOL : public WGameMode
{
public:
    WGameMode_LOL();
    ~WGameMode_LOL() override;

    void InitGame(CWorld* pWorld) override;
    void StartMatch() override;
    void Tick(f32_t fDeltaTime) override;

    EntityID SpawnDefaultPawnForController(WPlayerController* pController) override;
    bool ReadyToEndMatch() const override;

    void OnPlayerKilled(EntityID killerEntity, EntityID victimEntity) override;
    void OnStructureDestroyed(EntityID structureEntity, u8_t team) override;

    void OnImGui() override;

private:
    EntityID SpawnChampion(eChampion champ, eTeam team, const Vec3& spawnPos);

    // 넥서스 상태
    bool m_bBlueNexusDestroyed = false;
    bool m_bRedNexusDestroyed  = false;

    // 킬 보상
    static constexpr u32_t kKillGold   = 300;
    static constexpr u32_t kAssistGold = 150;
};
```

### 3.14 `Client/Private/Gameplay/WGameMode_LOL.cpp`

```cpp
#include "WGameMode_LOL.h"
#include "Gameplay/WPlayerController.h"
#include "Gameplay/WGameState.h"
#include "Gameplay/WCharacter.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameInstance.h"

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

WGameMode_LOL::WGameMode_LOL()  = default;
WGameMode_LOL::~WGameMode_LOL() = default;

void WGameMode_LOL::InitGame(CWorld* pWorld)
{
    WGameMode::InitGame(pWorld);

    // LOL 전용 GameState 초기화
    if (m_pGameState)
        m_pGameState->SetMatchPhase(eMatchPhase::Loading);
}

void WGameMode_LOL::StartMatch()
{
    WGameMode::StartMatch();

    // 미니언 웨이브 타이머 시작 등 LOL 전용 초기화
}

void WGameMode_LOL::Tick(f32_t fDeltaTime)
{
    WGameMode::Tick(fDeltaTime);

    // LOL 전용 틱: 미니언 스폰 타이머, 정글 리스폰 등
    // 현재 CMinion_Manager / CJungle_Manager / CStructure_Manager 가 처리하는 로직은
    // 당분간 싱글턴으로 유지하고, 점진적으로 GameMode 서브시스템으로 이관.
}

EntityID WGameMode_LOL::SpawnDefaultPawnForController(WPlayerController* pController)
{
    if (!m_pWorld || !pController) return NULL_ENTITY;

    // ── 기존 Scene_InGame::CreateChampionEntity 이관 ──
    //
    // 챔피언 선택에 따라 스폰 위치 결정 + 엔티티 생성
    const eChampion champ = pController->GetChampion();
    const eTeam team      = pController->GetTeam();

    // 팀별 스폰 위치 (현재 하드코딩, 추후 SpawnPoint 시스템)
    Vec3 spawnPos = { 0.f, 1.f, 0.f };
    if (team == eTeam::Blue) spawnPos = { -3.f, 3.f, 0.f };
    else if (team == eTeam::Red) spawnPos = { 3.f, 3.f, 0.f };

    return SpawnChampion(champ, team, spawnPos);
}

EntityID WGameMode_LOL::SpawnChampion(eChampion champ, eTeam team, const Vec3& spawnPos)
{
    // ── Scene_InGame::CreateChampionEntity 핵심 로직 ──
    EntityID e = m_pWorld->CreateEntity();

    TransformComponent tf;
    tf.m_LocalPosition = spawnPos;
    tf.m_LocalScale    = { 0.01f, 0.01f, 0.01f };
    m_pWorld->AddComponent<TransformComponent>(e, tf);

    ChampionComponent cc;
    cc.id        = champ;
    cc.team      = team;
    cc.hp        = 600.f;
    cc.maxHp     = 600.f;
    cc.mana      = 300.f;
    cc.maxMana   = 300.f;
    cc.moveSpeed = 8.f;
    m_pWorld->AddComponent<ChampionComponent>(e, cc);

    HealthComponent hp;
    hp.fCurrent = cc.hp;
    hp.fMaximum = cc.maxHp;
    hp.bIsDead  = false;
    m_pWorld->AddComponent<HealthComponent>(e, hp);

    NavAgentComponent agent;
    agent.fSpeed        = cc.moveSpeed;
    agent.fArriveRadius = 0.15f;
    agent.bHasGoal      = false;
    agent.bPathDirty    = false;
    m_pWorld->AddComponent<NavAgentComponent>(e, agent);

    m_pWorld->AddComponent<VelocityComponent>(e);
    m_pWorld->AddComponent<ServerIdComponent>(e);

    return e;
}

bool WGameMode_LOL::ReadyToEndMatch() const
{
    return m_bBlueNexusDestroyed || m_bRedNexusDestroyed;
}

void WGameMode_LOL::OnPlayerKilled(EntityID killerEntity, EntityID victimEntity)
{
    WGameMode::OnPlayerKilled(killerEntity, victimEntity);

    // 킬러에게 골드 지급
    // 팀 킬 카운트 증가
    if (!m_pWorld) return;

    if (m_pWorld->HasComponent<ChampionComponent>(killerEntity))
    {
        const auto& killerCC = m_pWorld->GetComponent<ChampionComponent>(killerEntity);
        if (m_pGameState)
            m_pGameState->AddTeamKill(static_cast<u8_t>(killerCC.team));
    }

    // 컨트롤러 찾아서 골드/킬 추가
    for (auto& pCtrl : m_vecControllers)
    {
        if (pCtrl->GetPawnEntity() == killerEntity)
            pCtrl->AddKill();
        if (pCtrl->GetPawnEntity() == victimEntity)
            pCtrl->AddDeath();
    }
}

void WGameMode_LOL::OnStructureDestroyed(EntityID structureEntity, u8_t team)
{
    WGameMode::OnStructureDestroyed(structureEntity, team);

    if (m_pGameState)
        m_pGameState->AddTeamTowerKill(team);

    // 넥서스 파괴 판정 (Structure_Manager 에서 넥서스 여부 조회)
    // TODO: 넥서스 = 특정 StructureComponent 태그 확인
    // if (isNexus) { team==0 ? m_bBlueNexusDestroyed : m_bRedNexusDestroyed = true; }
}

void WGameMode_LOL::OnImGui()
{
#ifdef WINTERS_EDITOR
    WGameMode::OnImGui();
    if (ImGui::CollapsingHeader("LOL GameMode"))
    {
        ImGui::Text("Blue Nexus: %s", m_bBlueNexusDestroyed ? "DESTROYED" : "ALIVE");
        ImGui::Text("Red Nexus: %s",  m_bRedNexusDestroyed ? "DESTROYED" : "ALIVE");
    }
#endif
}
```

---

## 4. Scene_InGame 리팩터 결과 — 전후 비교

### 4.1 Before: Scene_InGame 3000줄 (일부 발췌)

```cpp
// Scene_InGame.cpp — 3000줄 모놀리식
void CScene_InGame::OnUpdate(f32_t dt)
{
    // L900~L1000: 타겟팅 → WPlayerController::UpdateTargeting()
    UpdateTargeting();

    // L1000~L1050: 입력 → WPlayerController::ProcessInput()
    bool bSkipGroundMove = false;
    UpdateCombatInput(bSkipGroundMove);

    // L1117~L1140: 액션 락 → WCharacter::Tick() 내부
    if (m_fLastActionTimer > 0.f) m_fLastActionTimer -= dt;

    // L1147~L1300: castFrame/recoveryFrame → WSkillComponent::UpdateFrameEvents()
    if (m_pActiveSkillDef && m_pPlayerRenderer) { ... }

    // L1300~L1500: 이동 보간 → WCharacter::UpdateLocomotion()
    // L1500~L2000: FX/이펙트 → FX Subsystem (별도)
    // L2000~L2500: 네트워크 → WPlayerController + GameState
    // L2500~L3000: 디버그 UI → 각 클래스 OnImGui()
}
```

### 4.2 After: Scene_InGame ~100줄

```cpp
// Scene_InGame_Refactored.cpp — ~100줄
bool CScene_InGame::OnEnter()
{
    // 1. GameMode 생성
    m_pGameMode = std::make_unique<WGameMode_LOL>();
    m_pGameMode->InitGame(&m_World);

    // 2. 로컬 플레이어 Controller 생성
    auto pCtrl = std::make_unique<WPlayerController>();
    eChampion champ = CGameInstance::Get()->Get_GameContext().SelectedChampion;
    pCtrl->Initialize(&m_World, champ, eTeam::Blue);
    pCtrl->SetIsLocal(true);
    pCtrl->SetCamera(m_pCamera.get());
    m_pLocalController = pCtrl.get();
    m_pGameMode->RegisterController(std::move(pCtrl));

    // 3. 매치 시작
    m_pGameMode->HandleMatchIsWaitingToStart();

    return true;
}

void CScene_InGame::OnUpdate(f32_t dt)
{
    m_pGameMode->Tick(dt);
}

void CScene_InGame::OnRender()
{
    // ECS RenderComponent 순회 → Render() (기존과 동일)
}

void CScene_InGame::OnImGui()
{
    m_pGameMode->OnImGui();
    if (m_pLocalController)
        m_pLocalController->OnImGui();
}
```

---

## 5. Irelia Q 스킬 통합 예시

### 5.1 기존 방식 (Scene_InGame.cpp L1180~L1300)

```cpp
// Scene_InGame.cpp 에서 직접 castFrame 감지 + 데미지 + FX 호출
if (bCastHit && d.castFrameHookId == kIreliaQCast)
{
    // 데미지 적용
    CGameplayHookRegistry::Instance().Dispatch(kIreliaQCast, gameCtx);
    // FX 스폰
    CVisualHookRegistry::Instance().Dispatch(kIreliaQCast, visualCtx);
    // 스킬 후크
    CSkillHookRegistry::Instance().Dispatch(kIreliaQCast, skillCtx);
}
```

### 5.2 새 방식 (WSkillComponent 콜백)

```cpp
// WChampionCharacter::InitializeSkills() 에서 1회 등록
m_pSkillComponent->SetOnCastFrame(
    [this](const SkillDef& def, const CastSkillCommand& cmd)
    {
        // GameplayHook dispatch (서버 권위 로직)
        GameplayHookContext gameCtx{};
        gameCtx.pWorld        = GetWorld();
        gameCtx.casterEntity  = GetEntityID();
        gameCtx.pDef          = &def;
        gameCtx.pCommand      = &cmd; // NOTE: cmd 는 CastSkillCommand* 아닌 포인터 변환 필요
        CGameplayHookRegistry::Instance().Dispatch(def.castFrameHookId, gameCtx);

        // VisualHook dispatch (클라이언트 FX)
        VisualHookContext vizCtx{};
        vizCtx.pWorld       = GetWorld();
        vizCtx.casterEntity = GetEntityID();
        vizCtx.pDef         = &def;
        CVisualHookRegistry::Instance().Dispatch(def.castFrameHookId, vizCtx);
    });
```

---

## 6. Verification Checklist

| # | 검증 항목 | 합격 조건 |
|---|----------|----------|
| 1 | WGameMode 생성 + InitGame | CWorld 에 엔티티 0개 상태에서 크래시 없음 |
| 2 | WPlayerController::Possess | Pawn 엔티티 바인딩 후 GetPawnEntity() == 해당 ID |
| 3 | WCharacter::PlayAnimation | ModelRenderer::PlayAnimationByName 정상 호출 |
| 4 | WSkillComponent::TryCastSkill | 쿨다운 / 중복 시전 차단 + ActiveSkillDef 설정 |
| 5 | WSkillComponent castFrame 감지 | HasFramePassed 정확도 + 콜백 1회만 호출 |
| 6 | WGameMode_LOL::SpawnChampion | ECS 엔티티에 Transform/Champion/Health/NavAgent 전부 부착 |
| 7 | Scene_InGame 100줄 이하 | OnEnter + OnUpdate + OnRender + OnImGui 합산 100줄 이내 |
| 8 | LoL 빌드 통과 | 컴파일 에러 0, 링크 에러 0 |
| 9 | 프레임 성능 | 기존 ~9ms 대비 ±1ms 이내 |

---

## 7. Migration Strategy

### Phase 1: 병렬 공존 (1주)
- WGameMode / WPlayerController / WCharacter 엔진에 추가
- Scene_InGame 은 기존 코드 유지, WGameMode 를 멤버로 추가
- OnUpdate 에서 WGameMode::Tick 호출 시작 (기존 코드와 병행)

### Phase 2: 로직 이관 (2주)
- UpdateTargeting → WPlayerController::UpdateTargeting
- UpdateCombatInput → WPlayerController::ProcessInput
- castFrame/recoveryFrame → WSkillComponent::UpdateFrameEvents
- 이동 보간 → WCharacter::UpdateLocomotion
- 각 이관 후 Scene_InGame 에서 해당 코드 제거

### Phase 3: Scene_InGame 축소 (1주)
- CreateECSEntities → WGameMode_LOL::SpawnChampion
- 렌더 호출은 ECS RenderComponent 순회로 통합
- Scene_InGame 100줄 이하 달성
- 기존 멤버 변수 (m_Irelia, m_Yasuo 등) 제거, WChampionCharacter 가 소유

### Phase 4: 네트워크 통합 (2주)
- WPlayerController 가 CommandSerializer 소유
- WGameState 가 SnapshotApplier 소유
- 서버 WGameMode 가 WPlayerController[0..9] 전체 관리
