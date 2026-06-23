# InGameScene Thinning Design — `CScene_InGame`을 IScene 가상함수만 남기는 본질 분해 설계

작성일: 2026-06-23
북극성: `.md/plan/refactor/00_ESSENCE_BOUNDARY_REFACTOR.md` (한 파일 = 하나의 본질), `.md/architecture/WINTERS_CODEBASE_COMPASS.md` (계층 소유권·의존 규칙)
방법론 정본: `.md/plan/refactor/13_ATOMIC_DECOMPOSITION_PLAYBOOK.md` (Stage 1 verbatim → Stage 2 원자 owner)
Codex 핸드오프 프롬프트: `.md/plan/refactor/16_CODEX_PROMPT_INGAME_SCENE_THINNING.md`
보존할 흐름: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FxCue -> Client Visual`

> **진행 상태(2026-06-13)**: **Stage 1 완료** — `Scene_InGame.cpp` 6,588 → 760줄, 책임 TU 8개 + `Scene_InGameInternal.h/.cpp` 분리(빌드 green, G3 0, 예측↔snapshot-apply 파일 분리). 남은 **Stage 2(원자 owner 추출)**의 owner 지도·순서·Client-presentation 규율은 `.md/plan/refactor/17_INGAME_SCENE_STAGE2_OWNER_DESIGN.md`, Codex 프롬프트는 `.md/plan/refactor/18_CODEX_PROMPT_INGAME_SCENE_STAGE2.md`로 이관됐다. 이 문서(15)의 §6 Stage 2 표는 17이 actual TU 이름·authority 패턴으로 대체한다.

---

## 0. 이 문서의 위치와 목적

이 문서는 **코드 변경 지시서가 아니라 방향·소유권·경계·검증을 고정하는 설계 문서**다. `13_ATOMIC_DECOMPOSITION_PLAYBOOK`의 일반 방법론을 `CScene_InGame` 한 대상에 적용해, "무엇을 어디로 옮겨 scene을 IScene 6함수만 남길 것인가"를 정한다. 각 seam의 실제 적용 지시는 작업 직전 `.md/계획서작성규칙.md` 형식으로 따로 쓴다.

원래 목표는 한 문장이었다: **`CScene_InGame`은 IScene 가상함수만 남기고, 데이터·로직은 전부 본질 소유자로 이전한다.** 이 문서는 그 목표가 북극성의 어디에 위치하는지, 이번 세션에서 구조가 왜 정반대로 보이게 바뀌었는지, 그리고 그 변화가 사실은 올바른 출발점인 이유를 정리한 뒤, 실행 단계를 고정한다.

---

## 1. 북극성 — 이 설계가 절대 위반하면 안 되는 불변식

`13`/`14`/`04`/`12`/Compass에서 추출한 불변식. 모든 seam 결정은 이 목록을 통과해야 한다.

1. **본질 소유권이 최상위 개념이다.** "한 코드가 두 가지 이유로 바뀐다면 아직 덜 쪼갠 것이다." owner가 틀린 코드는 삭제가 아니라 **이동**한다. (`14:36-45`, `04:8-9`)
2. **판정 가능한 진실은 Server/GameSim이 만들고, Client는 presentation으로 해석한다.** `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FxCue -> Client Visual` 흐름을 깨지 않는다. (`04:13-15`, `13:5`)
3. **의존 방향은 이것만 허용한다.** `Server -> Shared/GameSim`, `Client -> Shared/GameSim`, `Client -> Engine`, `Shared/GameSim -> Engine primitive(기존 관례 범위)`, `Engine -> 제품 의존 없음`. (`04:124-131`, Compass:57/78)
4. **Shared/GameSim은 Engine/Client/Renderer/UI/ImGui/DX를 include하지 않는다.** (Compass:57, `13:112`)
5. **Client는 authoritative truth(cooldown/hit/damage/action lock/HP)를 새로 만들지 않는다.** 유일한 예외는 **이름으로 격리된 local-only prediction/lab path**다. (Compass:90, `04:151`)
6. **local-only prediction과 snapshot-apply를 같은 파일에 섞지 않는다 — 이것이 최대 경계 위반이다.** `ApplyLocalPrediction`/`StartLocal*Dash`/`UpdateLocal*`/`ApplyLocalChampionDamage`(허용된 예측)와 `OnAuthoritativeSnapshot`/`ApplyNetworkActorInterpolation`(snapshot-apply)은 **다른 파일**에 둔다. (`13:114-115`, `14:77-78`)
7. **DX concrete type을 Client/Public·Shared·EngineSDK/inc로 노출하지 않는다.** scene은 추상 `IRHIDevice*`를 쓴다. 분리를 빌드시키려고 DX11 device를 public 헤더로 올리지 않는다. (Compass:91/126, `13:117`)
8. **Engine public header에 LoL/Server/GameSim 제품 타입을 새로 노출하지 않는다.** Map/static-mesh batching·material grouping·draw submission 최적화는 Engine generic 계층 소유이고, Client는 view data를 만들어 generic API를 호출한다. (Compass:114-115)
9. **2단계 분해를 한 번에 둘 다 하지 않는다.** Stage 1(기계적 verbatim 이동, 동작·시그니처 불변) → Stage 2(원자 owner 추출, 상태 소유권 이동). Stage 1로 seam이 드러난 뒤에만 Stage 2를 한다. (`13:16-43`)
10. **결정성·호출 순서 불변, F5 normal runtime 은닉 금지.** roster/map/minion/champion/snapshot/UI/FX 7개를 숨겨서 분리를 통과시키지 않는다. 서버 로그만으로 visual을 판정하지 않는다. (`13:139`, Compass:116)
11. **EngineSDK/inc(=Derived)는 직접 수정하지 않는다.** Engine public header 변경은 `UpdateLib.bat` 동기화로만. `.vcxproj`는 명시적 `ClCompile` 목록이라 새 split 파일 등록은 분해의 필수 단계다(미등록 시 링크 단계에서 함수 증발). 프로젝트 전역 `/utf-8`에 의존한다. (`13:118-124`)

---

## 2. 현재 상태 — 정직한 진단

| 항목 | 값 |
|---|---|
| `Client/Private/Scene/Scene_InGame.cpp` | **6,588줄** |
| `Client/Public/Scene/Scene_InGame.h` | **580줄** |
| 익명 namespace 자유 함수 (cpp 138~1318) | ~1,180줄, ~60개 |
| 다른 모든 Client scene 헤더 | 31~104줄 (`Scene_Editor.h`가 최대 104줄) |

`CScene_InGame`은 다른 scene보다 한 자릿수 큰 god-file이다. 이번 세션에서 `InGame*Bridge` 10개 클래스(20개 파일)를 전부 이 한 파일로 인라인한 직후 상태다.

### 2.1 책임 버킷 (멤버/메서드 실측)

| 버킷 | 멤버수 | cpp 줄수(추정) | 이미 존재하는 대응 시스템 |
|---|---|---|---|
| **ChampionLocalSim** (Kalista passive dash·Yasuo E/R·범용 dash·EndTransition·스킬프레임훅) | 24 | **~1,340** | **없음** — scene에만 존재 (최대 부채) |
| **Network/Prediction** (보간 상태·MovePrediction·ActionAnim·세션 배선) | 19 | **~700** | applier 3종은 분리됨, 보간/예측 상태는 scene 산재 |
| **Champion/ECS bootstrap** (스폰·바인드·alias) | 16 | ~500 | `CWorld`+`CSystemSchedular`, `ChampionSpawnService`, Manager 3종 |
| **Map/Nav** (NavGrid 빌드·surface 샘플링·경로) | 6 | ~430 | `CNavGrid`/`CMapSurfaceSampler`/`CMapWalkableBaker`/`CPathfinder` (본체 Engine) |
| **Render** (OnRender 본문 오케스트레이션) | 13 | ~260 | `CNormalPass`/`CSSAOPass`/`CFogOfWarRenderer`/`CPlaneRenderer` 등 본체 존재 |
| **Input/Combat** (hover·ping·attack-intent) | 18 | ~330 | `CInput` + 패널들, 로직은 scene 직접 |
| **FX** (Irelia blade·WindWall·Yasuo/Kalista projectile 등 9종) | 11 | (OnUpdate/OnRender 분산) | 9개 시스템 클래스 모두 존재, scene은 소유+수동 tick |
| **DebugUI 토글** | 21 | ~100 | 패널 분리 완료, scene은 토글 bool + 디스패치 |
| **Replay** | 5 | ~80 | `CReplayPlayer` 독립 |
| **Camera** | 1 | — | `CDynamicCamera` 독립 |
| **AmbientProps** (새/오리) | 1 | ~74 | **없음** — scene 직접 소유 (의도적) |
| **Tuner 스칼라** | 8 | — | — |

줄 수 기준 최중량 메서드 Top 5: `OnUpdate`(~458) · `OnEnter`(~365) · `UpdateNetworkChampionLocomotion`(~316) · `UpdatePlayerControl`(~304) · `OnRender`(~258).

**핵심 진단**: 무게의 절대 다수가 **ChampionLocalSim(~1,340) + Network/Prediction(~700)** 두 버킷에 몰려 있고, 이 둘은 *대응 시스템이 없어* scene에만 산다. Render/FX/Map-Nav/Camera/Replay/DebugUI는 *이미 본체 시스템이 따로 있고* scene은 오케스트레이션 코드만 들고 있다.

### 2.2 우리는 어떻게 여기에 왔나 (Bridge → Inline → ?)

이 god-file은 두 단계를 거쳤다.

1. **이전**: 로직이 `InGame*Bridge` 10개 facade 클래스에 흩어져 있었다. 각 bridge는 `static` 메서드로 함수 본문만 들고, 상태(`m_*`)는 여전히 `CScene_InGame`에 두고 `friend`로 reach-in했다.
2. **이번 세션**: 사용자가 "Bridge가 너무 많고 죽은 코드가 많다"고 판단해 10개 facade를 전부 scene으로 인라인했다. 동시에 죽은 코드(무출력 sprintf, `if(false)` 트레이스, placeholder cube, 호출자 없는 `OnSnapshot` 등)를 제거했다.

겉보기엔 "분리의 반대"지만, **북극성 기준으로는 올바른 출발점**이다. 그 이유는 §3에서 설명한다.

---

## 3. 왜 Bridge는 틀렸고 Owner는 맞나 — 이 설계의 핵심

> **Bridge는 함수 본문만 옮기고 상태는 scene에 남겼다. Owner는 상태와 규칙을 같이 가져간다. 분리의 가치는 후자에만 있다.**

`InGame*Bridge`의 구조적 문제:

- **상태를 소유하지 않았다.** bridge는 `static Foo(CScene_InGame& scene, ...)` 시그니처로 `scene.m_*`를 직접 읽고 썼다. 상태가 scene에 남았으므로 scene은 여전히 뚱뚱했고, bridge는 *간접 호출 한 겹만 추가*했다.
- **`friend`로 캡슐화를 뚫었다.** 10개 bridge가 전부 `friend class`였다. scene의 모든 private 멤버가 10개 파일에 노출됐다 — "한 코드가 두 가지 이유로 바뀐다" 그 자체.
- **두 본질을 한 facade에 섞었다.** 예: `InGamePlayerControlBridge`가 *클라 로컬 예측 이동*과 *서버 스냅샷 동기화*를 같은 클래스에서 만졌다. 이는 북극성 불변식 6(예측 ↔ snapshot-apply 분리)의 정면 위반이었다.

따라서 인라인은 **잘못된 추상(facade)을 녹여 없앤** 것이고, `13_ATOMIC_DECOMPOSITION_PLAYBOOK`이 Stage 1 직전에 원하는 **"읽을 수 있는 단일 god-file"** 상태를 만들었다. 실제로 13번 playbook은 `Scene_InGame.cpp`(6,588줄)를 2순위 분해 대상으로 이미 지목하고 있다.

**앞으로 가는 길은 Bridge로 되돌아가는 게 아니다.** 두 단계로 간다.

- **Stage 1 (기계적 multi-TU 분리)**: `CScene_InGame`의 메서드를 책임별 `.cpp`로 verbatim 이동한다. **facade 클래스를 새로 만들지 않는다.** `GameRoomXxx.cpp` 패턴과 동일하게 *같은 클래스의 멤버 정의*를 여러 TU에 나누고, 공유 anon helper는 `Scene_InGameInternal.h`로 승격한다. `.h` 시그니처·동작·호출 순서 불변. → 6,588줄을 읽을 수 있게 만들고 Stage 2 seam을 드러낸다.
- **Stage 2 (원자 owner 추출)**: 상태와 규칙을 *실제로 소유하는* owner 클래스로 옮긴다. owner는 side effect를 직접 실행하지 않고 `Result`를 반환하며, scene은 owner를 **소유(주입받지 않고 직접 보유)**하되 멤버 접근은 owner의 public API로만 한다. `friend` 없음. → scene이 orchestration shell로 줄어든다.

Bridge와 Owner의 차이를 한 줄로:

```text
Bridge:  scene이 상태 소유 + bridge가 friend로 reach-in (분리 가치 0, 캡슐화 파괴)
Owner :  owner가 상태 소유 + scene이 owner를 보유하고 public API로만 호출 (진짜 분리)
```

---

## 4. 목표 구도 — "IScene 가상함수만"의 정직한 정의

"IScene 6함수만, 멤버 0개"는 문자 그대로는 불가능하다. scene은 **무언가를 조립(compose)하고 그 수명을 소유**해야 한다. 따라서 현실적인 thin-scene 목표를 이렇게 정의한다.

```text
CScene_InGame (목표)
= IScene 6 가상함수 (OnEnter/OnExit/OnUpdate/OnLateUpdate/OnRender/OnImGui)
+ 소수의 owner 소유 멤버 (composition root, unique_ptr 6~8개)
+ 외부 UI 패널이 요구하는 얇은 view-state accessor (§7)
```

각 가상함수는 owner들에게 **위임만** 한다. gameplay·prediction·network·nav·render 로직은 owner가 소유한다. 데이터는 owner·ECS·Manager·GameSim·Engine resource 계층으로 이전된다.

목표 composition root (소유권 에이전트 권고):

```text
CScene_InGame
├─ m_pScheduler           : CSystemSchedular   ── ECS sim phase. FX/스킬 sim 시스템 흡수(버킷 FX)
├─ m_pInGameNetcode       : CInGameNetcode      ── applier 3종 소유 + 보간/예측 상태 (Client 전용)
├─ m_pLocalPrediction     : CLocalChampionPrediction ── 클라 dash 약예측 (Client 전용)
├─ m_pMapNavService       : CMapNavService      ── NavGrid 빌드 / map surface 샘플링
├─ m_pSceneInstaller      : CInGameSceneInstaller ── OnEnter 부트스트랩 주입(facade 아님, 주입 후 소유는 scene)
├─ (CAmbientProp_Manager::Get())  ── 새/오리 장식 (Manager 싱글톤)
└─ m_World / m_pCamera / m_Map ... ── 남는 1차 소유 자원 (Champion 렌더러·맵 메시·카메라)
```

---

## 5. 버킷 → Owner 소유권 맵

각 버킷을 (a) 기존 스케줄러 흡수 / (b) 신규 전용 owner / (c) ECS·GameSim(=Shared) / (d) Manager 중 어디로 보내는가. **(c)는 Client presentation에 금지** — Shared가 Client를 include하게 되는 의존성 역전이다.

| # | 버킷 | 타겟 | 이전 위치 | 북극성 위반 리스크 |
|---|---|---|---|---|
| 1 | 챔피언 FX/스킬 sim (Irelia blade, WindWall, Yasuo/Kalista projectile, PendingHit, Rend) | **(a) 스케줄러 흡수** | `m_pScheduler->RegisterSystem` | **낮음.** 이미 `Execute(world, dt)` 시그니처 = ISystem 동형. ISystem 상속 + `GetPhase` 부여 후 등록. presentation FX라 Client 소유 유지 |
| 2 | FX 렌더 update (`m_pFxSystem/Beam/Mesh->Update`) | **(a) 스케줄러 흡수** (Render는 OnRender 유지) | OnUpdate/OnLateUpdate 수동 호출 | **낮음.** Update만 시스템화. Render는 스케줄러가 아니라 OnRender |
| 3 | 로컬 챔피언 예측 (UpdateDash, Yasuo/Kalista dash 상태머신, ApplyLocalPrediction) | **(b) `CLocalChampionPrediction`** (Client 전용) | scene 멤버 ~30개 + 메서드 다수 | **중간.** GameSim 중복 아님 — 클라 weak prediction. **(c) Shared로 보내면 의존성 역전 위반.** 반드시 Client. snapshot-apply와 다른 파일/owner |
| 4 | 네트워크 보간/예측 배선 (`m_NetworkActorInterpStates`, MovePrediction, OnAuthoritativeSnapshot, 세션 콜백) | **(b) `CInGameNetcode`** (Client 전용, applier 3종 소유) | `InitializeNetworkSession` 람다 + scene 상태 | **중간.** applier는 분리됨. 보간/예측은 presentation이라 (c) 금지. applier 콜백이 authoritative truth를 새로 만들지 않게 경계 유지 |
| 5 | AmbientProps (새/오리 장식) | **(d) `CAmbientProp_Manager`** | scene 직접 소유 | **낮음.** 게임플레이 무관 장식. `Minion_Manager` 싱글톤 패턴 복제 |
| 6 | 부트스트랩 (OnEnter 시스템 등록·맵·네비·렌더헬퍼·FX preload) | **(b) `CInGameSceneInstaller` + 데이터 기반 system 테이블** | OnEnter 인라인 ~365줄 | **중간.** facade 회귀 주의 — Installer는 scene 멤버에 **주입**(소유는 scene). system 등록의 `if(!네트워크권위)` 분기를 `{phase, factory, condition}` 데이터 테이블로 |
| 7 | NavGrid 빌드/맵 surface 샘플링 | **(b/d) `CMapNavService`** | scene 메서드 + `m_pNavGrid`/`m_pPathNavGrid` | **낮음.** NavigationSystem이 grid 소비. 빌드/베이크를 전용 owner로, scene은 결과만 |
| 8 | 디버그 UI / 튜너 (~40 getter-setter, OnImGui) | **(b) 전용 패널 또는 OnImGui 유지** | scene 멤버 + OnImGui | **낮음(우선순위 낮음).** 순수 dev 도구. Engine UI panel은 view-state로만(Compass:79) |

**전 버킷 공통 가드**:
- (c) ECS 컴포넌트/Shared GameSim로 **절대 보내면 안 되는 것**: 버킷 3(로컬 예측)·4(보간) — 둘 다 Client presentation/weak prediction. Shared로 올리면 `Shared/GameSim`이 Client를 include하는 의존성 역전.
- (b) 신규 owner는 **facade 금지** — scene 멤버를 감싸 재노출하는 게 아니라 상태와 로직을 실제 소유.
- 버킷 1 FX를 스케줄러에 넣을 때 ISystem(Engine) 상속은 OK지만 Yasuo/Kalista 구체 타입을 Engine public으로 밀어올리지 말 것.

---

## 6. 2단계 실행 계획

### Stage 1 — 기계적 multi-TU 분리 (verbatim, 동작 불변)

`GameRoomXxx.cpp` 패턴. `CScene_InGame` 멤버 정의를 책임별 TU로 옮기되 **facade 클래스를 만들지 않는다.** 공유 anon helper/상수는 `Scene_InGameInternal.h`로 승격(= 가장 큰 정확성 함정, ~60개 helper의 사용처 전수 분류 후 배치). `.h` 시그니처 불변, 이동과 로직 수정 분리 커밋, seam 1개 = 커밋 1개.

권장 slice 순서 (독립도 높은 것부터):

| slice | 새 TU | 옮길 묶음 | 주의 |
|---|---|---|---|
| S1-0 | `Scene_InGameInternal.h` | 2개 이상 TU가 공유하는 anon helper/상수/struct | helper 사용처 전수 분류 선행 |
| S1-1 | `Scene_InGameImGui.cpp` | `OnImGui`, `DrawReplayControlPanel`, 디버그 패널 디스패치, 17종 ImGui include | 가장 독립적 — 첫 slice |
| S1-2 | `Scene_InGameNetwork.cpp` | `InitializeNetworkSession`, `OnAuthoritativeSnapshot`, `ApplyNetworkActorInterpolation`, `UpdateNetworkChampionLocomotion`, 보간/예측 상태 | **local prediction을 절대 섞지 않는다** |
| S1-3 | `Scene_InGameLocalSkills.cpp` | `ApplyLocalPrediction`, `StartLocal*Dash`, `UpdateLocal*`, `UpdateDash`, dash 상태머신 | S1-2와 **다른 파일** (불변식 6) |
| S1-4 | `Scene_InGameInput.cpp` | `UpdateCombatInput`, `UpdateTargeting`, ping, hover, attack-intent | — |
| S1-5 | `Scene_InGameRender.cpp` | `OnRender` 본문 + render anon helper | OnRender 시그니처 불변 |
| S1-6 | `Scene_InGameMapNav.cpp` | `CreateMapNavGrid`, `BakeMapWalkableNavGrid`, `Mark_StructuresOnNavGrid`, surface 샘플링, 부쉬 시드 | NavGrid 결과는 scene 멤버 유지 |
| S1-7 | `Scene_InGameLifecycle.cpp` | `OnEnter`/`OnExit` 부트스트랩·셧다운 | 시스템 등록 순서 불변 |

Stage 1 종료 시: `Scene_InGame.cpp`는 `OnUpdate` orchestration + 생성자/소멸자 + 얇은 위임만 남고, 나머지는 책임 TU로 분산된다. 동작은 완전히 불변.

### Stage 2 — 원자 owner 추출 (상태 소유권 이동)

Stage 1이 드러낸 seam을 따라 owner를 추출한다. owner는 상태를 소유하고 side effect 대신 `Result`를 반환한다. 우선순위는 **무게 × 대응 시스템 부재**:

| 순서 | owner | 흡수할 Stage 1 TU | 비고 |
|---|---|---|---|
| S2-1 | `CLocalChampionPrediction` | `Scene_InGameLocalSkills.cpp` | 최대 부채(~1,340줄), 대응 시스템 없음. Client 전용 |
| S2-2 | `CInGameNetcode` | `Scene_InGameNetwork.cpp` | applier 3종 소유 이전 + 보간/예측 상태. Client 전용 |
| S2-3 | FX/스킬 sim → `m_pScheduler` | (OnUpdate 수동 tick) | ISystem 상속 + 등록. 데이터 기반 system 테이블 |
| S2-4 | `CMapNavService` | `Scene_InGameMapNav.cpp` | NavGrid 빌드/surface owner |
| S2-5 | `CAmbientProp_Manager` | (AmbientProps) | Manager 싱글톤 패턴 |
| S2-6 | `CInGameSceneInstaller` | `Scene_InGameLifecycle.cpp` | 부트스트랩 주입. facade 아님 |

S2-6의 system 등록은 데이터 지향 북극성과 연결한다: 현재 `OnEnter`의 `if(!m_bNetworkAuthoritativeGameplay)` 분기 다발을 `{phase, factory, condition}` 테이블로 바꿔, "어떤 시스템을 어떤 조건에 등록하나"를 코드 분기가 아니라 데이터로 만든다.

Stage 2 종료 시: `CScene_InGame`은 §4의 composition root + 6 위임 가상함수 + view-state accessor만 남는다.

---

## 7. 보존해야 하는 외부 인터페이스 (thin화해도 살아남는 surface)

`Client/` 전역에서 `CScene_InGame` public API를 호출하는 곳. thin-scene으로 가도 이 surface는 유지하거나 owner로 forwarding해야 한다.

| 호출처 | 호출하는 멤버 | 비고 |
|---|---|---|
| `Scene_BanPick/Editor/CustomMode.cpp` | 생성자/소멸자만 (`new CScene_InGame`) | 생성 경로 유지 |
| `AIDebugPanel.cpp` | `IsNetworkAuthoritativeGameplay`, `GetNetworkView`, `GetCommandSerializer`, `GetNavGrid`/`GetPathNavGrid` | Network/Nav owner로 forward |
| `RenderDebug.cpp` | SSAO 8종, Dbg 토글 16종, `RebuildMapWalkableNavGridForDebug`, `Is/SetShowRenderDebug` | Render/Nav owner로 forward |
| `DebugDrawSystem.cpp` | `IsDbgShow*` 7종, `GetNavGrid`/`GetPathNavGrid`, `GetCameraPtr`, `GetDbgNavRadius` | — |
| `EffectTuner.cpp` / `WfxEffectToolPanel.cpp` | `GetWorld`, `GetPlayerEntity`, `GetFxMeshRenderer`, `ResolveMouseMapSurfacePos` | World/FX/Map owner로 forward |
| `MapTunerPanel.cpp` | `Is/SetShowMapTuner`, `Get/SetMapRotation` | — |
| `CombatDebugPanel.cpp` | `Is/SetShowCombatDebug`, hover/Sylas/hit 게터·세터 | — |
| `ChampionTuner.cpp`, `SkillTimingPanel.cpp` | **없음 (빈 스텁)** | 무시 가능 |

원칙: Stage 1에서는 이 surface를 건드리지 않는다(`.h` 시그니처 불변). Stage 2에서 owner가 상태를 가져간 뒤, scene의 accessor는 owner로 위임하는 1줄 forwarder로 남긴다. UI panel은 render만 한다는 규칙(Compass:79/107)을 깨지 않는다 — 패널에 truth 판정을 떠넘기지 않는다.

---

## 8. 가드레일 & 검증 게이트

### 가드레일 (전 slice 공통)
- 서버 권위: Client는 gameplay truth 신규 생성 금지. local prediction/lab은 이름으로 격리, snapshot-apply와 분리.
- 의존 방향: `Shared/GameSim` ↛ Engine/Client/Renderer/UI/ImGui/DX. Server ↛ Client visual. Engine ↛ LoL/Server/GameSim 제품 타입.
- DX 비노출: `Client/Public`·`Shared`·`EngineSDK/inc`에 `ID3D11*`/`IDXGI*` 신규 노출 금지. `IRHIDevice` 추상 유지.
- 공유 anon helper 중복정의 금지 → `Scene_InGameInternal.h` 단일화.
- `/utf-8` 전역 의존, per-file로 떨어뜨리지 않음. 새 split `.cpp/.h`는 `.vcxproj`/`.filters`에 직접 등록(그 외 XML 구조 불변).
- EngineSDK/inc 직접 수정 금지 — `UpdateLib.bat` 동기화로만.

### 검증 게이트 (G1~G5)
- **G1 빌드**: `Client.vcxproj` Debug x64 통과 (Engine 미커밋 변경 시 `/p:BuildProjectReferences=false`).
- **G2 순수 이동**: `git diff`가 이동/등록만. 이동과 로직 수정 분리 커밋.
- **G3 경계**: forbidden-dependency `rg` 스캔 0.
- **G4 F5 스모크**: roster/map/minion/champion/snapshot/UI/FX 7개 미은닉. 서버 로그만으로 판정 금지. 회귀 확인 동작: player/minion/jungle Idle·Run, BasicAttack, SkillQ/W/E/R, Yasuo R·Kalista passive dash, Recall, DeathStart/Dead, cast/recovery FX, cooldown UI.
- **G5 본질**: 분리 후 각 파일이 한 가지 이유로만 바뀌는가.

### 검증 명령
```powershell
git diff --stat ; git diff --check
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false
rg -n "#include .*(Server)" Client Shared/GameSim
rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim
```

---

## 9. Codex 핸드오프

이 설계의 실행은 Codex 프롬프트 `.md/plan/refactor/16_CODEX_PROMPT_INGAME_SCENE_THINNING.md`로 넘긴다. 그 프롬프트는 본 문서(설계·소유권·경계)와 `13_ATOMIC_DECOMPOSITION_PLAYBOOK`(방법론 정본)을 가리키는 얇은 부트스트랩이며, 실제 코드 변경 지시의 진실은 각 slice 직전에 작성하는 `.md/계획서작성규칙.md` 형식 계획서에 둔다.

문서 역할 분리:
```text
15 (이 문서)  설계·소유권 맵·단계 계획·경계      ← 의도와 경계의 정본
16            Codex 복붙 프롬프트                ← 실행 진입점 (15·13을 가리킴)
slice별 계획서 (작업 직전 작성, 계획서작성규칙)  ← 실제 코드 변경 지시의 진실
```
