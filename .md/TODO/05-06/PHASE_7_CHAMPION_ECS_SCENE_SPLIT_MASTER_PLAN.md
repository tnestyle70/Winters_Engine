# Phase 7 마스터 계획서 - 챔피언 순수 ECS + Scene 분리 + Engine 경계 정리

날짜: 2026-05-06  
최종 갱신: 2026-05-07 - S1 자동 smoke 진입로 확장 + skill/full-map telemetry 통과  
상태: Phase 7A/7B/7C/7D/7E 완료, S1 자동 smoke로 roster/start/snapshot/Q skill hook/full-map init 확인, 직접 시각 smoke 잔여  
범위: 레거시 챔피언 제거 완료, BanPick 기반 순수 ECS 스폰, `Scene_InGame` 축소, Client/Engine/Shared 경계 정리

---

2026-05-07 update:
- S1 hidden fast smoke passed for Server 1 + Client 3.
- Confirmed Yone(slot0), Fiora(slot1), Ezreal(slot2) Join/Pick/StartGame/local binding/Snapshot.
- Fast smoke defaults to local-only roster and skips heavy map/FX preload. Use `--smoke-human-roster`, `--smoke-full-roster`, `--smoke-full-map`, or `--smoke-full-ingame` for heavier coverage.
- Remaining S1 visual gate: direct visible champion/map render smoke (user-owned verification).

2026-05-07 update 2:
- S1 hidden skill smoke passed for Server 1 + Client 3.
- Confirmed Yone(slot0), Fiora(slot1), Ezreal(slot2) Q Dispatch -> castFrame -> hook dispatch.
- Hook result for all three local clients: gameplay=1, visual=1, legacy=0.
- 1-client `--smoke-full-map` telemetry passed: map init ok=1, InGameMap entity created, bootstrap done, snapshot received.
- Remaining S1 visual gate: direct visible champion/map render smoke (user-owned verification).

2026-05-07 update 3:
- 7F F-2/F-3 and 7G are implemented.
- Direct local fallback now logs selected player slot plus Sylas bot slot via `[ECS:RosterFallback]` and per-slot `[ECS:Roster] created` telemetry.
- Direct visual confirmation remains a runtime/user verification gate, not a code blocker.

## 0. 목표

이 계획의 목적은 엘든링 프로젝트로 넘어가기 전에 현재 LoL 프로토타입을 유지보수 가능한 제품 단위로 정리하는 것이다.

핵심 목표:

- 구현된 모든 LoL 챔피언을 BanPick에서 선택 가능하게 만든다.
- 선택된 모든 챔피언은 동일한 순수 ECS 스폰 경로로 InGame에 진입한다.
- `Scene_InGame`은 기능 덩어리가 아니라 오케스트레이터 역할만 맡는다.
- Client 전용 챔피언/스킬/FX 코드가 Engine 책임으로 새지 않게 한다.
- Engine은 WintersLOL과 WintersElden 양쪽에서 재사용 가능한 범용 엔진으로 유지한다.

목표 문장:

```txt
BanPick은 roster 데이터를 선택하고, InGame은 roster 데이터를 소비하고,
ChampionSpawnService는 ECS 엔티티를 생성하고, 챔피언 모듈은 스킬/FX를 소유하며,
Scene_InGame은 시스템 연결 순서만 관리한다.
```

---

## 1. 현재 기준선

### 이미 완료된 작업

```txt
Phase 6C
  - Scene이 직접 들고 있던 레거시 챔피언 renderer 멤버 제거.
  - InGame 스폰을 roster-only 경로로 전환.
  - 로컬 플레이어 바인딩은 sessionId/netId 우선, slotId는 fallback으로만 사용.
  - 레거시 챔피언 asset 정의를 ChampionTable에 채움.

Phase 6D-1
  - CInGameRosterSpawner 추출.
  - roster slot -> ECS champion spawn 정책을 Scene_InGame 밖으로 이동.

Phase 6D-2
  - CInGameNetworkBridge 추출.
  - GameSessionClient callback setup/teardown을 Scene_InGame 밖으로 이동.

Engine Filters
  - Engine.vcxproj.filters의 ClCompile 104개, ClInclude 149개 전부 Filter 매핑 완료.
  - Engine Debug / Debug-DX12 빌드 통과.

Phase 7A-1차
  - CChampionCatalog 추가.
  - BanPick 선택 목록을 하드코딩 배열에서 ChampionCatalog 기반으로 전환.
  - Client Debug|x64 빌드 통과.

Phase 7B-1차
  - CChampionSpawnService 추가.
  - Scene_InGame::CreateECSChampion 선언/정의 제거.
  - roster spawn callback / snapshot 신규 엔티티 callback이 ChampionSpawnService를 사용.
  - Client Debug|x64 빌드 통과.

Phase 7C-1차
  - Scene_InGame::GetPlayerChampionId fallback을 roster identity 기준으로 정리.
  - MatchLoading champion 표시명을 ChampionCatalog 우선 조회로 전환.
  - Scene_InGame 내부 ChampionDef 조회를 catalog -> registry -> legacy table 순서 helper로 통일.

Phase 7D 진행분
  - Fiora duplicate cast-frame dispatch 제거.
  - Yone gameplay/state hook을 shared GameplayHook 경로로 이동.
  - Ezreal gameplay/visual hook split.
  - Kalista BA/Q/E + passive recoveryHook 이관, Kalista_Tuning 단일화.
  - Yasuo tuning, Q keySwap, Q/W/E/R accepted hook 이관.
  - Irelia Q/W/E/R accepted hook, tuning/local blade state 이관.
  - Garen/Zed/Riven cast fallback hook 이관.
  - ChampionModuleBootstrap 추가.
```

### 현재 핵심 파일

```txt
Client/Private/Scene/Scene_InGame.cpp
Client/Public/Scene/Scene_InGame.h
Client/Public/Scene/InGameRosterSpawner.h
Client/Private/Scene/InGameRosterSpawner.cpp
Client/Public/Scene/InGameNetworkBridge.h
Client/Private/Scene/InGameNetworkBridge.cpp
Client/Public/GamePlay/ChampionCatalog.h
Client/Private/GamePlay/ChampionCatalog.cpp
Client/Private/GameObject/ChampionTable.cpp
Engine/Include/GameContext.h
Shared/GameSim/Definitions/ChampionDef.h
```

### 현재 문제 형태

```txt
1. ChampionCatalog facade는 생겼지만, spawn/service/hook까지 완전히 단일 소스로 묶인 상태는 아니다.
2. BanPick, MatchLoading, InGame, Skill dispatch가 서로 다른 소스를 읽으면 다시 drift가 날 수 있다.
3. CreateECSChampion 본체는 CChampionSpawnService로 이동했다.
4. Skill/FX 로직의 7D 잔여는 champion module hook으로 이동했다. `Scene_InGame`은 Render/PlayerControl/CombatInput/Debug bridge로 주요 루프를 위임한다.
5. Engine / Client / Shared 경계가 아직 충분히 엄격하지 않다.
6. Engine/Client/Server/AssetConverter filters verified.
```

---

## 2. 목표 아키텍처

### 런타임 흐름

```txt
MainMenu / Custom Room
  -> BanPick
  -> GameContext.Roster[10]
  -> MatchLoading
  -> Scene_InGame
  -> CInGameRosterSpawner
  -> CChampionSpawnService
  -> ECS entity + components + renderer bridge
  -> Skill/FX hooks
```

### 책임 분리 목표

```txt
Shared
  - deterministic POD / schema
  - eChampion / GameRosterSlot / GameContext
  - 서버와 클라이언트가 함께 쓰는 shared sim component

Engine
  - ECS core
  - RHI / Renderer / Resource / JobSystem
  - 범용 spatial / visibility / navigation primitive
  - 범용 AI framework primitive

Client
  - LoL champion catalog
  - LoL champion spawn policy
  - LoL skill/FX hooks
  - BanPick / MatchLoading / InGame scene
  - UI 및 visual-only component

Server
  - 권위 있는 lobby/game room
  - roster authority
  - server world / snapshot generation
```

### 금지 방향

```txt
Engine -> Client
Shared -> Client
Engine generic systems -> concrete LoL champion classes
Renderer -> gameplay-specific ECS components
Resource -> gameplay-specific ECS components
Scene_InGame -> direct per-champion implementation details
```

---

## 3. Phase 순서

권장 순서:

```txt
S0. 안정화 게이트
7A. ChampionCatalog 단일 read model
7B. ChampionSpawnService 추출
7C. BanPick catalog-driven UI 확정
7D. Skill/FX hook 이관
7E. Scene_InGame bridge 분리
7F. Engine / Client / Shared 경계 정리
7G. Filters 최종 정리
S1. 3-client smoke 후 freeze
```

7A/7B가 통과하기 전에는 7D를 시작하지 않는다. Skill/FX 이관은 안정적인 champion id와 안정적인 ECS spawn 경로에 의존한다.

---

## S0. 안정화 게이트

### 목적

더 큰 코드 추출 전에 현재 흔들리는 기준선을 고정한다.

### 작업

```txt
[x] Engine Debug|x64 빌드
[x] Client Debug|x64 빌드
[x] Server Debug|x64 빌드
[x] AssetConverter Debug|x64 빌드
[ ] 로컬 Client 직접 InGame 진입 1회 (시각 확인 잔여)
[x] Server 1 + Client 3 BanPick hidden smoke
[ ] Yone/Fiora/Ezreal이 각각 자기 자신으로 InGame에 들어가는지 확인 (서버 SpawnLobby id는 확인, 클라 렌더 확인 잔여)
[x] skill dispatch가 local ECS champion id를 사용하는지 확인
[ ] map render 확인
[ ] direct local fallback selected champion + Sylas bot visual confirmation (telemetry added)
```

2026-05-07 S1 telemetry override:
```txt
[x] Server 1 + Client 3 hidden smoke
[x] Yone/Fiora/Ezreal local entity binding by session/net id
[x] local ECS champion skill dispatch (Q) uses the bound ChampionComponent id
[x] Q castFrame hook dispatch: gameplay=1 visual=1 legacy=0
[x] 1-client full-map init telemetry: map init ok=1 + InGameMap entity
[ ] direct visible champion/map render smoke
[x] direct local fallback selected champion + Sylas bot telemetry added (`[ECS:RosterFallback]` + per-slot `[ECS:Roster] created`)
[ ] direct local fallback selected champion + Sylas bot visual confirmation
```

### 중단 조건

```txt
- 어떤 챔피언이든 잘못된 챔피언으로 InGame에 진입한다.
- 어떤 클라이언트든 host slot에 잘못 바인딩된다.
- Server/Client lobby state가 서로 달라진다.
- Client 빌드에 수동 프로젝트 파일 수정이 필요해진다.
```

---

## 7A. ChampionCatalog 단일 read model

### 목적

BanPick, MatchLoading, InGame spawn, skill lookup, debug UI가 동일한 챔피언 목록을 바라보게 한다.

### 현재 이슈

`ChampionTable.cpp`와 챔피언별 self-registration이 동시에 존재한다. UI와 스폰은 하나의 read model을 통해 읽어야 한다.

### 대상 파일

```txt
Client/Public/GamePlay/ChampionCatalog.h
Client/Private/GamePlay/ChampionCatalog.cpp
Client/Public/GameObject/ChampionDef.h
Client/Private/GameObject/ChampionTable.cpp
Client/Private/GameObject/Champion/*/*_Registration.cpp
```

### 현재 반영된 API

```cpp
struct ChampionCatalogEntry
{
    eChampion id = eChampion::END;
    const ChampionDef* pDef = nullptr;
    bool_t bSelectable = false;
    bool_t bPlayable = false;
    bool_t bBotAllowed = false;
    const char* displayName = nullptr;
};

class CChampionCatalog final
{
public:
    static CChampionCatalog& Instance();

    void RebuildFromRegistry();

    const ChampionCatalogEntry* Find(eChampion id) const;
    const std::vector<ChampionCatalogEntry>& GetSelectableChampions() const;
    const std::vector<ChampionCatalogEntry>& GetPlayableChampions() const;
    const std::vector<ChampionCatalogEntry>& GetBotChampions() const;

    bool_t IsSelectable(eChampion id) const;
    bool_t IsPlayable(eChampion id) const;
};
```

### 챔피언 커버리지

초기 최소 roster:

```txt
Ezreal
Fiora
Jax
Annie
Ashe
Yone
Irelia
Yasuo
Kalista
Sylas
Viego
Garen
Zed
Riven
```

후보 placeholder:

```txt
Master Yi
Kindred
```

### 마이그레이션 규칙

```txt
FindChampionDef(eChampion)은 compatibility wrapper로 유지 가능.
신규 코드는 CChampionCatalog를 먼저 조회한다.
BanPick은 별도 hardcoded selectable list를 유지하면 안 된다.
```

### 합격 기준

```txt
[x] Catalog가 현재 구현된 챔피언을 registry 기준으로 반환한다.
[x] 각 entry가 displayName / fbxPath / shaderPath / idle/run/attack key를 가진다.
[x] Selectable list가 END/NONE 및 깨진 asset entry를 제외한다.
[x] BanPick이 catalog에서 목록을 렌더링한다.
[x] InGame spawn이 scene fallback table 없이 catalog/registry data를 사용한다.
```

---

## 7B. ChampionSpawnService 추출

### 목적

`CreateECSChampion`을 `Scene_InGame` 밖으로 이동한다.

### 현재 이슈

`Scene_InGame`이 아직 다음 생성을 직접 담당한다.

```txt
ModelRenderer
TransformComponent
ChampionComponent
RenderComponent
SkillStateComponent
NavAgentComponent
SpatialAgentComponent
VisibilityComponent
champion-specific state components
```

이것은 Scene 책임이 아니다.

### 대상 파일

```txt
Client/Public/GameObject/ChampionSpawnService.h
Client/Private/GameObject/ChampionSpawnService.cpp
Client/Public/GameObject/ChampionSpawnTypes.h
```

### 제안 API

```cpp
struct ChampionSpawnRequest
{
    eChampion champion = eChampion::END;
    eTeam team = eTeam::TEAM_END;
    Vec3 position{};
    bool_t bUseCatalogSpawnPosition = true;
    bool_t bLocalPlayer = false;
    bool_t bBot = false;
    u8_t botDifficulty = 0;
};

struct ChampionSpawnResult
{
    EntityID entity = NULL_ENTITY;
    ModelRenderer* pRenderer = nullptr;
    const ChampionDef* pDef = nullptr;
};

struct ChampionSpawnContext
{
    CWorld& world;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>>& renderers;
};

class CChampionSpawnService final
{
public:
    static ChampionSpawnResult Spawn(
        ChampionSpawnContext& context,
        const ChampionSpawnRequest& request);
};
```

### Scene 연동

변경 전:

```cpp
return CreateECSChampion(champion, team);
```

변경 후:

```cpp
ChampionSpawnRequest req{};
req.champion = champion;
req.team = team;

ChampionSpawnResult result =
    CChampionSpawnService::Spawn(m_ChampionSpawnContext, req);
return result.entity;
```

### Component 규칙

모든 챔피언은 기본적으로 다음 component를 받는다.

```txt
TransformComponent
ChampionComponent
RenderComponent
HealthComponent
ManaComponent
SkillStateComponent
StatComponent
NavAgentComponent
SpatialAgentComponent
VisionSourceComponent 또는 VisibilityComponent
NetAnimationComponent
```

챔피언별 component:

```txt
YasuoStateComponent
RivenStateComponent
FioraStateComponent
JaxStateComponent
AnnieStateComponent
AsheStateComponent
Yone state / soul components
Kalista rend/passive state
Irelia blade state if needed
```

### 합격 기준

```txt
[x] Scene_InGame이 CreateECSChampion을 더 이상 선언하지 않는다.
[x] CInGameRosterSpawner의 createChampion callback이 CChampionSpawnService를 호출한다.
[x] SnapshotApplier OnNewEntity callback도 CChampionSpawnService를 호출한다.
[x] 모든 selectable champion이 같은 service를 통해 스폰된다.
[ ] direct local fallback visual confirmation (selected champion + Sylas bot telemetry added)
[x] 3-client BanPick hidden smoke가 그대로 동작한다.
```

---

## 7C. BanPick Catalog-Driven UI 확정

### 목적

BanPick이 InGame과 같은 catalog를 사용하게 한다.

### 대상 파일

```txt
Client/Public/Scene/Scene_BanPick.h
Client/Private/Scene/Scene_BanPick.cpp
Client/Public/GamePlay/ChampionCatalog.h
Client/Private/GamePlay/ChampionCatalog.cpp
```

### 현재 위험

BanPick이 별도 목록을 유지하거나 button index를 champion enum으로 매핑하면 다시 drift가 발생한다.

### 목표 흐름

```txt
BanPick OnEnter
  -> RegisterAllLegacy / per-champion registration 완료
  -> catalog.GetSelectableChampions()
  -> champion grid/list 렌더링
  -> SendLobbyCommand(PickChampion, slotId, championId)
  -> 서버 lobby state에서 GameContext.Roster[slot].champion 갱신
```

### UI 규칙

```txt
Champion button은 반드시 eChampion id를 직접 들고 있어야 한다.
Button index를 champion id로 취급하면 안 된다.
Display name은 catalog에서 가져온다.
사용 불가 챔피언은 다른 챔피언으로 remap하지 말고 disabled 처리한다.
```

### 합격 기준

```txt
[x] Fiora 선택 시 eChampion::FIORA가 전송된다.
[x] Yone 선택 시 eChampion::YONE이 전송된다.
[x] Irelia 선택 시 eChampion::IRELIA가 전송 가능한 목록에 올라온다.
[x] catalog가 invalid 처리하지 않는 한 선택 챔피언이 Ezreal로 fallback되지 않는다.
[x] Host와 non-host client가 같은 slot별 selected champion을 보는지 hidden smoke 검증한다. (server Join/Pick/SpawnLobby 로그 기준)
```

---

## 7D. Skill / FX Hook 이관

### 목적

`Scene_InGame`에서 챔피언별 skill/FX 구현을 제거한다.

### 현재 이슈

`Scene_InGame` 안에 아직 다음과 같은 챔피언별 분기가 남아 있다.

```txt
직접 skill/FX 구현 잔여:
  Irelia Q/W/E/R accepted hook + tuning/local blade state -> moved
  Garen BA/Q/W/E/R cast fallback -> moved
  Zed BA/Q/W/E/R cast fallback -> moved
  Riven Q accepted + BA/W/E/R cast fallback -> moved

Scene callback/runtime state로 남아 있는 것:
  Yasuo dash/R local state
  Kalista passive dash local state
```

이 구현은 각 챔피언 모듈로 이동해야 한다.

### 목표 디렉토리 패턴

```txt
Client/Public/GameObject/Champion/Irelia/
  Irelia_Components.h
  Irelia_Skills.h
  Irelia_Registration.h
  IreliaFxPresets.h

Client/Private/GameObject/Champion/Irelia/
  Irelia_Skills.cpp
  Irelia_Registration.cpp
```

반복 대상:

```txt
Yasuo
Kalista
Sylas
Viego
Garen
Zed
Riven
Ezreal
Fiora
Jax
Annie
Ashe
Yone
```

### Hook 계약

Gameplay hook:

```cpp
struct GameplayHookContext
{
    CWorld* pWorld = nullptr;
    EntityID casterEntity = NULL_ENTITY;
    eTeam casterTeam = eTeam::TEAM_END;
    eChampion casterChampion = eChampion::END;
    const SkillDef* pDef = nullptr;
    const CastSkillCommand* pCommand = nullptr;
};
```

Visual hook:

```cpp
struct VisualHookContext
{
    CWorld* pWorld = nullptr;
    EntityID casterEntity = NULL_ENTITY;
    const SkillDef* pDef = nullptr;
    const CastSkillCommand* pCommand = nullptr;
    Engine::CFxStaticMeshRenderer* pFxMeshRenderer = nullptr;
};
```

### 마이그레이션 규칙

```txt
Scene_InGame은 hook dispatch만 할 수 있다.
Scene_InGame은 이관 후 챔피언별 hit/FX 로직을 구현하면 안 된다.
```

### 이관 순서

```txt
1. Yone / Fiora / Ezreal
   상태: 완료.

2. Irelia / Yasuo / Kalista
   상태: Yasuo 완료, Kalista 핵심 완료, Irelia tuning + Q/W/E/R 완료.
   잔여: runtime smoke.

3. Garen / Zed / Riven
   상태: 완료. 기존 skill/FX branch를 champion SkillHook module로 이관.

4. Annie / Ashe / Jax
   이유: newer pure ECS champion module에 가까운 구조.

5. Sylas / Viego
   이유: legacy enemy/test role. asset/anim 가용성 먼저 확인.
```

### Fiber 작업 이후 처리 완료된 7D 순서

```txt
7D-6B. Irelia_Tuning 분리 [x]
  - Scene_InGame.h의 blade/beam/wave/W-layer/R-triangle 값을 Irelia_Tuning.h/.cpp로 이동.
  - ChampionTuner getter/setter는 Irelia::GetTuning() 경유.
  - 기능 변경 없이 값 저장소만 이동.

7D-6C. Irelia E stage hook 분리 [x]
  - E 1타 sword1, E 2타 sword2 spawn을 Irelia_Skills.cpp로 이동.
  - 두 검 id / beam delay / beam spawned 상태를 Irelia module 또는 component로 이동.
  - Scene은 필요한 경우 "spawn placed blade" callback 정도만 제공.

7D-6D. Irelia W stage hook 분리 [x]
  - W1 spin spawn / W2 spin delete / W2 slash + release layer spawn을 Irelia_Skills.cpp로 이동.
  - m_IreliaWSpinFxId를 Scene에서 제거.

7D-6E. Irelia R accepted hook 분리 [x]
  - UltWave spawn + R billboard pulse를 Irelia_Skills.cpp로 이동.
  - CUltWaveSystem 파라미터는 Irelia_Tuning에서 읽는다.

7D-7. Garen/Zed/Riven fallback 제거 [x]
  - 제한된 castFrame fallback을 각 champion module hook으로 이동.
  - Scene_InGame castFrame fallback chain에서 champion-specific branch 제거.

7D-8. ChampionModuleBootstrap [x]
  - Scene_InGame OnEnter의 extern KeepAlive 목록을 Client-owned bootstrap 함수로 단일화.
```

### 합격 기준

```txt
[x] Scene_InGame에 챔피언별 FX spawn call이 남지 않는다.
[x] Scene_InGame skill dispatch는 command 생성 + registry hook dispatch만 한다.
[x] 각 champion module이 ChampionDef + SkillDef + hook을 self-register한다.
[x] BanPick 진입 후 local player skill이 동작한다.
[x] Bot champion은 local player skill state branch 없이 존재 가능하다.
```

---

## 7E. Scene_InGame Bridge 분리

### 목적

`Scene_InGame` 크기를 줄이고 협업 가능한 파일 단위로 나눈다.

### 이미 추출된 bridge

```txt
CInGameRosterSpawner
CInGameNetworkBridge
CInGameBootstrapBridge
CInGameRenderBridge
CInGamePlayerControlBridge
CInGamePlayerTransformBridge
CInGameCombatInputBridge
CInGameDebugBridge
CInGameChampionStateBridge
CInGameLifecycleBridge
CInGameSkillDispatchBridge
```

### 7E 완료 bridge

#### 7E-1. InGameRenderBridge

대상 파일:

```txt
Client/Public/Scene/InGameRenderBridge.h
Client/Private/Scene/InGameRenderBridge.cpp
```

책임:

```txt
ECS champion normal pass loop
ECS champion main render loop
network champion locomotion visual state update if render-only
SSAO per-renderer AO binding
visibility check before champion draw
```

포함하지 않을 것:

```txt
Map direct render
Fog of war texture update
UI overlay
```

이유:

```txt
Map은 아직 direct ModelRenderer 소유다.
Map ECS 정책이 결정되기 전까지 champion ECS render bridge와 섞지 않는다.
```

#### 7E-2. InGamePlayerControlBridge

대상 파일:

```txt
Client/Public/Scene/InGamePlayerControlBridge.h
Client/Private/Scene/InGamePlayerControlBridge.cpp
```

책임:

```txt
BindPlayerToECSChampion
GetPlayerChampionId
SyncPlayerEntityTransformFromECS
SyncPlayerEntityTransformToECS
camera follow target update
local movement destination state
```

#### 7E-3. InGameCombatInputBridge

대상 파일:

```txt
Client/Public/Scene/InGameCombatInputBridge.h
Client/Private/Scene/InGameCombatInputBridge.cpp
```

책임:

```txt
UpdateTargeting
UpdateCombatInput
BuildCastCommand
DispatchSkillInput
PreemptAction
basic attack input
right-click move/attack decision
```

주의:

```txt
챔피언별 skill implementation을 여기로 옮기면 안 된다.
이 bridge는 generic input/command flow만 만든다.
```

#### 7E-4. InGameDebugBridge

대상 파일:

```txt
Client/Public/Scene/InGameDebugBridge.h
Client/Private/Scene/InGameDebugBridge.cpp
```

책임:

```txt
CombatDebugPanel
MapTunerPanel
RenderDebug
SkillTimingPanel
ChampionTuner
EffectTuner
MinimapPanel
```

#### 7E-5. InGameChampionStateBridge

대상 파일:

```txt
Client/Public/Scene/InGameChampionStateBridge.h
Client/Private/Scene/InGameChampionStateBridge.cpp
```

책임:

```txt
YasuoStateComponent timer tick
RivenStateComponent q/ult/shield timer tick
JaxStateComponent empower/counter/ult timer tick
AnnieStateComponent shield/tibbers timer tick
AsheStateComponent q timer tick
local player idle/run recovery when champion state expires
```

주의:

```txt
Scene_InGame은 champion-specific state component 이름을 직접 알면 안 된다.
각 champion module 전용 system으로 더 내려보내기 전까지, Scene bridge가 임시 소유한다.
```

#### 7E-6. InGameLifecycleBridge

대상 파일:

```txt
Client/Public/Scene/InGameLifecycleBridge.h
Client/Private/Scene/InGameLifecycleBridge.cpp
```

책임:

```txt
OnEnter의 bootstrap 순서 묶음
OnExit의 shutdown/reset 순서 묶음
resource reset symmetry
manager enable/disable symmetry
```

1차 범위:

```txt
OnExit shutdown bridge 먼저 추출한다.
OnEnter는 resource/map/scheduler/fx/network bootstrap로 더 쪼갠 뒤 이동한다.
```

#### 7E-7. InGameSkillDispatchBridge

대상 파일:

```txt
Client/Public/Scene/InGameSkillDispatchBridge.h
Client/Private/Scene/InGameSkillDispatchBridge.cpp
```

책임:

```txt
DispatchSkillInput
BuildCastCommand
ApplyLocalPrediction
RotatePlayerToward
cast accepted / cast frame / recovery hook context assembly
```

주의:

```txt
Skill implementation 자체는 champion module이 소유한다.
이 bridge는 command 생성, target resolve, registry dispatch wiring만 소유한다.
```

#### 7E-8. InGamePlayerTransformBridge / Legacy Player Adapter 축소

대상 파일:

```txt
Client/Public/Scene/InGamePlayerTransformBridge.h
Client/Private/Scene/InGamePlayerTransformBridge.cpp
```

대상:

```txt
m_pPlayerTransform
m_pPlayerRenderer
GetPlayerPosition / SetPlayerPosition / GetPlayerYaw / SetPlayerYaw
SyncPlayerEntityTransformFromECS / SyncPlayerEntityTransformToECS
```

목표:

```txt
Scene_InGame이 CTransform legacy adapter를 덜 직접 만진다.
B-12 CTransform 제거 전까지 bridge로 격리한다.
```

#### 7E-9. InGameBootstrapBridge

대상 파일:

```txt
Client/Public/Scene/InGameBootstrapBridge.h
Client/Private/Scene/InGameBootstrapBridge.cpp
```

책임:

```txt
OnEnter bootstrap order
network bridge initialization
scheduler/system registration
stage/manager/map/camera/roster/UI/Nav/FOW/FX resource bootstrap
practice bush seed creation
```

### 크기 목표

```txt
현재 Scene_InGame.cpp: 1185줄 (2026-05-07, 7E-1~9 + 7F F-5 완료 후).
7E-5/6 이후 목표: 2100줄 이하. [달성]
7E-7/8 이후 목표: 1500줄 이하. [달성]
7D + 7E 전체 완료 후 목표: 1000줄 이하. [후속 7F/S1 이후 재평가]
최종 목표: 500-800줄 오케스트레이터.
```

### 합격 기준

```txt
[x] Scene_InGame OnEnter가 subsystem 초기화 순서처럼 읽힌다.
[x] Scene_InGame OnUpdate가 bridge update 순서처럼 읽힌다.
[x] Scene_InGame OnRender가 bridge render 순서처럼 읽힌다.
[x] Scene_InGame OnExit가 bridge shutdown 순서처럼 읽힌다.
[x] Scene_InGame이 champion-specific state timer tick을 직접 구현하지 않는다.
[x] Scene_InGame이 skill dispatch command 생성/예측/registry wiring을 직접 구현하지 않는다.
[x] 신규 기능이 별도 파일 소유권으로 작업 가능해져 merge conflict가 줄어든다.
```

---

## 7F. Engine / Client / Shared 경계 정리

### 목적

LoL Client를 충분히 정리해서 Elden이 LoL 전용 의존성을 끌고 가지 않고 Engine을 재사용할 수 있게 한다.

### 경계 규칙

#### Shared가 소유 가능

```txt
GameContext
GameRosterSlot
eChampion
eTeam if server/client both need it
flatbuffer schemas
deterministic stats/sim POD
network packet enums
```

#### Engine이 소유 가능

```txt
ECS World/Entity/ComponentStore
TransformComponent if generic
RenderComponent only if renderer-neutral
SpatialAgentComponent if generic
VisibilityComponent if generic
RHI
Renderer
Resource
JobSystem
generic AI framework
```

#### Client가 반드시 소유

```txt
ChampionCatalog
ChampionSpawnService
LoL champion SkillDef
LoL champion FX presets
BanPick UI
InGame bridges
Client-only render state
Champion-specific state components unless server sim needs them
```

#### Server가 반드시 소유

```txt
GameRoom authority
Lobby command handling
ServerWorld
SnapshotBuilder
Anti-cheat validation
```

### 알려진 경계 위험

```txt
Renderer/Resource가 gameplay-specific ECS component를 참조
Shared가 Engine ECS 구현 세부에 의존
GameInstance.h가 너무 많은 concrete Engine header 노출
AI <-> ECS circular include
Client header가 SDK를 통해 Engine Private 개념을 끌어옴
```

### 2026-05-07 dependency scan result

```txt
Engine -> Client-ish dependency candidates:
  Engine/Private/Manager/UI/UI_Manager.cpp
    hardcoded Client/Bin/Resource/Texture/UI/... paths
  Engine/Public/ECS/Components/GameplayComponents.h
    LoL gameplay component set + GameContext/eChampion + RenderComponent ModelRenderer bridge
  Engine/Public/ECS/SystemScheduler.h
    comment-level Scene_InGame caller coupling

Shared -> Engine implementation dependency candidates:
  Shared/GameSim/Systems/* includes ECS/World.h
    BuffSystem / DamagePipeline / StatSystem / SkillRankSystem / MoveSystem / etc.
    Current shared simulation is source-shared with Engine ECS, not engine-independent yet.

Client tight coupling candidates:
  InGame*Bridge and debug UI panels include Scene/Scene_InGame.h directly.
  This is accepted for 7E bridge extraction, but future step is narrow bridge desc/interfaces.

Decision:
  7F scan is complete. Do not batch-fix all in S1.
  Next safe cleanup slices:
    F-1 UI asset path ownership out of Engine UI_Manager [done 2026-05-07]
    F-2 Shared GameSim world facade or ECS adapter boundary [done 2026-05-07]
    F-3 RenderComponent split away from gameplay component header [done 2026-05-07]
    F-4 SkillDispatch/CombatInput bridge local runtime boundary cleanup [done 2026-05-07]
    F-5 Scene local champion runtime implementation moved behind InGameChampionStateBridge [done 2026-05-07]

2026-05-07 F-1 result:
  Engine/Private/Manager/UI/UI_Manager.cpp no longer hardcodes Client/Bin resource paths.
  UI textures use Resource/... paths and WintersResolveContentPath().
  Engine Debug + Client Debug build passed; 1-client fast smoke still reaches InGame bind/snapshot.

2026-05-07 F-2/F-3 result:
  Shared/GameSim code includes Shared/GameSim/World.h instead of ECS/World.h directly.
  Shared/GameSim/World.h is the temporary one-file adapter to Engine CWorld.
  RenderComponent moved out of GameplayComponents.h into ECS/Components/RenderComponent.h.
  Client render users include RenderComponent.h explicitly; Engine project + SDK mirror include the new header.

2026-05-07 F-4 result:
  InGameSkillDispatchBridge and InGameCombatInputBridge no longer include champion module skill headers.
  Local dash, ultimate dash, damage, skill-start preconditions, passive dash queue, and passive reset are routed through CInGameChampionStateBridge APIs.
  Riven Q animation key selection moved from InGameSkillDispatchBridge into the Riven VisualHook.

2026-05-07 F-5 result:
  Scene_InGame no longer implements Yasuo/Kalista local runtime functions directly.
  InGameChampionStateBridge owns local runtime update, passive dash queue/reset, target dash/R dash, local damage application, and local champion FX/state system ticking.
  Scene_InGame keeps orchestration and hook wiring; direct visible champion/map smoke remains user-owned verification.
```

### 정리 순서

```txt
1. LoL-only champion code를 Engine-facing header 밖으로 이동한다.
2. GameContext는 SDK/Shared-compatible POD 형태로 유지한다.
3. gameplay-specific render visibility mask를 generic render tag 뒤로 숨긴다.
4. Engine -> ECS gameplay component 직접 읽기를 generic render data로 대체한다.
5. AI framework type과 ECS system wrapper를 분리한다.
```

### 합격 기준

```txt
[ ] Engine이 Client/GameObject 경로를 include하지 않고 빌드된다.
[ ] Shared schema/codegen이 Client header를 include하지 않는다.
[ ] Client champion module 변경 시 public SDK가 바뀌지 않는 한 Engine rebuild가 필요 없다.
[ ] Elden이 LoL champion file을 건드리지 않고 자체 catalog/spawn service를 정의할 수 있다.
```

---

## 7G. Filters 최종 정리

### 목적

Solution Explorer가 실제 협업 경계와 일치하게 만든다.

### 현재 상태

Engine/Client/Server/AssetConverter filters verified.

2026-05-07 result:

```txt
MissingInFilters=0
FilterOnly=0
MissingFilter=0
Removed stale .vcxproj.filters.bak.2026-05-06 files
```

### Client filters가 보존해야 할 파일

```txt
Client/Public/Scene/InGameRosterSpawner.h
Client/Private/Scene/InGameRosterSpawner.cpp
Client/Public/Scene/InGameNetworkBridge.h
Client/Private/Scene/InGameNetworkBridge.cpp
Client/Public/GamePlay/ChampionCatalog.h
Client/Private/GamePlay/ChampionCatalog.cpp
Client/Public/Network/Client/GameSessionClient.h
Client/Private/Network/Client/GameSessionClient.cpp
Shared/Schemas/Generated/cpp/Lobby*.h
```

### 권장 Client layout

```txt
00. App
01. Scene
  BanPick
  MatchLoading
  InGame
    Bridges
02. GameObject
  Champion
    Irelia
    Yasuo
    Kalista
    Sylas
    Viego
    Garen
    Zed
    Riven
    Ezreal
    Fiora
    Jax
    Annie
    Ashe
    Yone
  FX
  Projectile
03. GamePlay
  Champion
  Skill
  System
04. Manager
05. UI
06. Network
  Backend
  Client
07. Shared
99. Defines
Shaders
```

### 합격 기준

```txt
[x] Client filters에 Filter 없는 self-closing ClCompile/ClInclude가 없다.
[x] 새 bridge 파일은 01. Scene\InGame 또는 01. Scene\InGame\Bridges에 보인다.
[x] ChampionCatalog는 03. GamePlay\02. Champion에 보인다.
[x] GameSessionClient는 06. Network\Client에 보인다.
[x] generated lobby schema는 07. Shared\Schemas\Generated에 보인다.
[x] Client Debug 빌드가 계속 통과한다.
```

---

## S1. Runtime Smoke Freeze

### 2026-05-07 자동 smoke 결과

```txt
추가된 실행 옵션:
  Server/Bin/Debug/WintersServer.exe --smoke-seconds=130
  Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=0 --smoke-champion=YONE --smoke-start --smoke-start-min-humans=3
  Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=1 --smoke-champion=FIORA
  Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=2 --smoke-champion=EZREAL

확인된 서버 로그:
  JoinSlot slot=0/1/2
  PickChampion YONE(13) / FIORA(8) / EZREAL(12)
  SpawnLobby slot=0 champ=13 netId=1
  SpawnLobby slot=1 champ=8  netId=2
  SpawnLobby slot=2 champ=12 netId=3
  StartGame locked revision=9
  snap broadcast to 3 sids

Client telemetry:
  slot=0 sid=1 net=1 champ=YONE(13) local entity bound
  slot=1 sid=2 net=2 champ=FIORA(8) local entity bound
  slot=2 sid=3 net=3 champ=EZREAL(12) local entity bound
  Hello bindNet/bindSid matched local entity
  snapshot len=984 received
  Q Dispatch accepted for local champion hook ids:
    YONE  hook=0x000D0032 anim=spell1_a1
    FIORA hook=0x00080032 anim=spell1
    EZREAL hook=0x000C0032 anim=spell1
  castFrame result for all three:
    gameplay=1 visual=1 legacy=0

Full-map telemetry:
  Server/Bin/Debug/WintersServer.exe --smoke-seconds=160
  Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=0 --smoke-champion=YONE --smoke-start --smoke-start-min-humans=1 --smoke-full-map --smoke-no-skill
  map init begin -> map init done ok=1
  InGameMap entity created
  bootstrap done player=31 champion=13
  snapshot len=984 received

Smoke options:
  --banpick-smoke      fast default: local-only roster, map init skip, FX mesh preload skip
  --smoke-human-roster spawn all human slots on client
  --smoke-full-roster  spawn server-filled 10-slot roster on client
  --smoke-full-map     include 336MB map init in smoke
  --smoke-full-ingame  include full InGame bootstrap
  --smoke-no-skill     disable automatic Q skill smoke

95초 후 Server + Client 3 alive 확인. Full-map smoke는 120초 후 Server + Client alive 확인.
```

### 필수 smoke matrix

```txt
Case 1: Direct local InGame
  [ ] selected champion spawns
  [ ] Sylas fallback bot spawns
  [ ] map renders
  [ ] local skill input works

Case 2: Server 1 + Client 1
  [x] host enters BanPick (hidden smoke)
  [x] host selects slot
  [x] host selects champion
  [x] host fills bots (StartGame server-side fills empty slots)
  [x] StartGame
  [x] InGame local entity champion matches selected champion (hidden telemetry)

Case 3: Server 1 + Client 3
  [x] Client 1 selects Yone
  [x] Client 2 selects Fiora
  [x] Client 3 selects Ezreal
  [x] all clients see same roster (server-side accepted roster + SpawnLobby 기준)
  [x] each client binds its own selected champion (hidden telemetry)
  [x] no client binds to host slot (Hello bindNet/bindSid matched)

Case 4: Legacy champions
  [ ] Irelia selectable and spawns
  [ ] Sylas selectable and spawns
  [ ] Viego selectable and spawns
  [ ] Kalista selectable and spawns
  [ ] Yasuo/Garen/Zed/Riven selectable and spawn
```

### Freeze 규칙

S1 통과 후:

```txt
No more direct champion code inside Scene_InGame.
No more BanPick hardcoded champion mapping.
No more SelectedChampion-only InGame spawn.
```

---

## 8. 보존해야 할 Gotchas

### G-1. Slot은 identity가 아니다

```txt
slotId는 좌석 좌표다.
sessionId/netId가 identity다.
authoritative identity가 없을 때만 slotId를 fallback으로 사용한다.
```

### G-2. Button index는 champion id가 아니다

```txt
BanPick UI index를 eChampion으로 cast하면 안 된다.
모든 버튼은 catalog에서 온 명시적 eChampion id를 들고 있어야 한다.
```

### G-3. Local fallback이 network roster를 오염시키면 안 된다

```txt
Direct InGame fallback은 slot 0 + slot 5를 합성해도 된다.
Network roster mode는 server-authored roster만 소비해야 한다.
```

### G-4. Engine은 LoL 챔피언을 알면 안 된다

```txt
Engine은 rendering/ECS/resource primitive를 제공한다.
Client가 champion-specific spawn 및 skill/FX policy를 소유한다.
```

### G-5. Register order는 명시적이어야 한다

```txt
RegisterAllLegacy와 champion self-registration은 BanPick이 catalog를 읽기 전에 끝나야 한다.
static registration을 쓰더라도 deterministic bootstrap call을 둔다.
```

### G-6. Scene split은 얇게 유지해야 한다

```txt
500줄짜리 블록을 새 파일로 옮기고 깨끗해졌다고 부르면 안 된다.
각 bridge는 하나의 변경 이유만 가져야 한다.
```

---

## 9. 권장 다음 작업

Fiber 루트 작업 완료 후 바로 다음은:

```txt
0. Client Debug 빌드로 Fiber merge 이후 기준선 확인 [x]
1. 7D-6B Irelia_Tuning 분리 [x]
2. 7D-6C Irelia E stage hook 분리 [x]
3. 7D-6D Irelia W stage hook 분리 [x]
4. 7D-6E Irelia R accepted hook 분리 [x]
5. 7D-7 Garen/Zed/Riven fallback 제거 [x]
6. 7E-1 InGameRenderBridge 추출 [x]
7. 7E-2 InGamePlayerControlBridge 추출 [x]
8. 7E-3 InGameCombatInputBridge 추출 [x]
9. S1 direct visual smoke freeze [next]
```

지금 시작하지 말 것:

```txt
Full skill migration
Full Engine/Shared boundary rewrite
All filters at once
```

이유:

```txt
Champion identity와 spawn 경로가 먼저 deterministic해져야 한다.
그 다음 skill/FX migration과 dependency cleanup은 훨씬 기계적으로 진행할 수 있다.
```

---

## 10. 구현 체크리스트

```txt
[ ] S0 full build/smoke baseline
[x] 7A ChampionCatalog files added
[x] 7A all champion defs readable through catalog
[x] 7A BanPick selectable list from catalog
[x] 7A no button-index-to-champion mapping
[x] 7B ChampionSpawnService files added
[x] 7B Scene_InGame::CreateECSChampion removed
[x] 7B SnapshotApplier and roster spawner use spawn service
[x] 7C BanPick/MatchLoading/InGame all consume GameContext.Roster
[x] 7D-1 Fiora cast-frame double-dispatch removed
[x] 7D-2 Yone gameplay/state hook moved to GameplayHook path
[x] 7D-3 Ezreal gameplay/visual hook split
[x] 7D Yone/Fiora/Ezreal hooks moved
[x] 7D-4A Kalista BA/Q/E fallback moved to champion SkillHook module
[x] 7D-4B Kalista tuning moved to Kalista_Tuning
[x] 7D-4C Kalista passive recovery moved to SkillHook
[x] 7D-5A Yasuo tuning moved to Yasuo_Tuning
[x] 7D-5B Yasuo Q keySwapHook moved to champion VisualHook module
[x] 7D-5C Yasuo Q/W accepted hook moved to champion SkillHook module
[x] 7D-5D Yasuo E/R accepted hook moved to champion SkillHook module
[x] 7D-6A Irelia Q accepted hook moved to champion SkillHook module
[x] 7D-6B Irelia tuning moved to Irelia_Tuning
[x] 7D-6C Irelia E stage hook moved
[x] 7D-6D Irelia W stage hook moved
[x] 7D-6E Irelia R accepted hook moved
[x] 7D Irelia/Yasuo/Kalista hooks moved
[x] 7D-7 Garen/Zed/Riven fallback hooks moved
[x] 7D-8 ChampionModuleBootstrap added
[x] 7D remaining legacy champion hooks moved
[x] 7E RenderBridge extracted
[x] 7E PlayerControlBridge extracted
[x] 7E CombatInputBridge extracted
[x] 7E DebugBridge extracted
[x] 7E ChampionStateBridge extracted
[x] 7E LifecycleBridge extracted
[x] 7E SkillDispatchBridge extracted
[x] 7E legacy player adapter isolated
[x] 7E BootstrapBridge extracted
[x] 7F Engine/Client/Shared dependency scan
[x] 7F F-2 Shared GameSim world adapter boundary
[x] 7F F-3 RenderComponent split from GameplayComponents
[x] 7F F-4 SkillDispatch/CombatInput bridge local runtime boundary cleanup
[x] 7F F-5 Scene local champion runtime implementation moved behind InGameChampionStateBridge
[x] 7G Client filters removed from current track
[x] 7G Server filters removed from current track
[x] 7G AssetConverter filters removed from current track
[x] RHI strategy: DX11 legacy / DX12+Vulkan main
[x] S1 implementation/telemetry freeze (hidden roster/start/local binding/snapshot/Q skill/full-map telemetry passed; visual gate remains user-owned)
```

---

## 11. 최종 완료 기준

```txt
1. BanPick이 catalog에서 구현된 모든 챔피언을 보여준다.
2. 선택된 모든 챔피언이 순수 ECS 경로로 스폰된다.
3. Scene_InGame에 direct legacy champion renderer member가 없다.
4. Scene_InGame에 champion-specific skill/FX implementation branch가 없다.
5. Scene_InGame은 대부분 bridge orchestration만 담당한다.
6. Client champion module이 LoL champion behavior를 소유한다.
7. Engine은 generic 상태로 남아 Elden에서도 재사용 가능하다.
8. Server 1 + Client 3 smoke가 서로 다른 챔피언 선택으로 통과한다.
9. Engine, Client, Server, AssetConverter Debug 빌드가 통과한다.
10. VS Solution Explorer filters가 실제 ownership과 충분히 일치한다.
```
