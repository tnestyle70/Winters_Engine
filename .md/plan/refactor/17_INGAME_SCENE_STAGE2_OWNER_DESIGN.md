# InGameScene Stage 2 — `CScene_InGame`을 IScene shell + owner 조립으로 만드는 원자 owner 추출 설계

작성일: 2026-06-13
북극성: `.md/plan/refactor/00_ESSENCE_BOUNDARY_REFACTOR.md` (한 파일 = 하나의 본질)
방법론 정본: `.md/plan/refactor/13_ATOMIC_DECOMPOSITION_PLAYBOOK.md` (Stage 2 = 원자 owner 추출, GameRoom이 증명한 stateful/static authority 패턴)
Stage 1 설계·맥락: `.md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md`
Codex 핸드오프 프롬프트: `.md/plan/refactor/18_CODEX_PROMPT_INGAME_SCENE_STAGE2.md`
보존할 흐름: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FxCue -> Client Visual`

---

## 0. 현재 상태 — Stage 1 완료 + Stage 2 S2-A 완료 (실측 2026-06-13)

Stage 1(기계적 verbatim 분리)은 끝났다(빌드 green, G3 경계 0, 예측↔snapshot-apply 파일 분리). **Stage 2 첫 owner `CAmbientProp_Manager`(S2-A) 추출 완료** — owner-소유 + scene 위임 패턴이 빌드 green으로 검증됐다. 아래 줄수는 병렬 RHI(`CRHISceneRenderer`) 작업 반영 후 실측이다.

| TU / owner | 줄수 | 책임 | Stage 2 owner / 상태 |
|---|---|---|---|
| `Scene_InGame.cpp` (shell) | 757 | OnUpdate 오케스트레이션 + OnLateUpdate + GetPlayerChampionId + player-transform 어댑터 9 | (shell로 유지) |
| `Scene_InGameLocalSkills.cpp` | 2,489 | 로컬 예측·스킬 디스패치·대시·플레이어 컨트롤·플래시 | **CLocalChampionPrediction** (stateful) — S2-D |
| `Scene_InGameNetwork.cpp` | 1,190 | 스냅샷 적용·보간·예측·세션 | **CInGameNetcode** (stateful) — S2-C |
| `Scene_InGameLifecycle.cpp` | 1,001 | OnEnter 부트스트랩·OnExit·ECS 조립 | **CInGameSceneInstaller** (static) — S2-F |
| `Scene_InGameInput.cpp` | 794 | 타게팅·전투 입력·핑·공격 의도·사망 입력락 | **CInGameInputController** (stateful) — S2-E |
| `Scene_InGameRender.cpp` | 691 | OnRender + RHI snapshot 수집(AppendRenderSnapshotMeshes) | **CInGameRenderer** (stateful, 후순위) — S2-G |
| `Scene_InGameMapNav.cpp` | 611 | NavGrid 빌드·맵 surface·이동 타겟 | **CMapNavService** (stateful 리소스 + static calc) — S2-B |
| `Scene_InGameImGui.cpp` | 210 | 디버그/리플레이 UI | (shell 위임 유지, 후순위) |
| `Scene_InGameInternal.h/.cpp` | 19/44 | 2+ TU 공유 helper | (공유 — 유지) |
| **`CAmbientProp_Manager`** (`Client/.../Manager`, 신규) | 37/160 | 새/오리 앰비언트 프롭 소유·Tick·Render·RHI snapshot | **✅ S2-A 완료 (빌드 green)** |

Stage 1은 **함수 정의를 파일로 나눴을 뿐, 상태(`m_*`)는 여전히 `CScene_InGame`이 소유**한다. 각 TU의 메서드는 `scene.m_x`를 직접 만진다(같은 클래스 멤버라 가능). Stage 2는 그 상태를 owner로 옮기고 scene을 조립자(shell)로 만든다. **S2-A가 그 패턴(owner가 상태 소유 + scene은 public API 위임 + friend 0)을 검증했다.**

---

## 1. Stage 2의 핵심 — Client presentation owner는 GameRoom과 규율이 다르다

`13` 플레이북의 owner 패턴은 **서버 gameplay truth**를 지키려고 만들어졌다. GameRoom owner는 `Result`를 반환하고 shell이 packet send/spawn 같은 side effect를 실행한다 — truth 변경을 감사 가능하게.

InGameScene owner는 **Client presentation**이다. 차이를 명확히 못박는다:

- **그대로 가져오는 규율**:
  1. owner가 **자기 상태를 소유**한다 (scene이 아니라). scene은 owner를 `unique_ptr`로 보유한다.
  2. scene은 owner의 **public API로만** 호출한다. `friend` 금지 (과거 `InGame*Bridge`가 friend로 reach-in한 실패를 반복하지 않는다).
  3. owner가 한 가지 이유로만 바뀐다.
- **다른 점 (presentation이라 허용)**:
  - owner는 visual transform 기록·애니메이션 재생·draw 제출 같은 **presentation 효과를 직접 실행**해도 된다. 이건 gameplay truth가 아니므로 GameRoom식 "Result 반환 후 shell이 실행" 강제가 불필요하다. (단, 인터페이스가 명료해지면 Result를 써도 좋다 — 예: input controller가 intent를 반환하고 scene이 dispatch.)
- **절대 위반 금지 (북극성, presentation이어도 동일)**:
  - **Client는 authoritative truth(cooldown/hit/damage/action lock/HP)를 새로 만들지 않는다.** local-only prediction은 compass가 허용한 예외이며 **이름으로 격리**한다(`CLocalChampionPrediction`).
  - **local prediction과 snapshot-apply를 같은 owner/파일에 합치지 않는다** (`CLocalChampionPrediction` ≠ `CInGameNetcode`). Stage 1이 이미 파일로 갈라놨으니 owner도 갈라 유지한다.
  - **로컬 예측·보간을 `Shared/GameSim`으로 올리지 않는다** — Shared가 Client를 include하는 의존성 역전. 두 owner는 `Client/` 소유.
  - `Client/Public`/`Shared`/`EngineSDK/inc`에 `ID3D11*`/`IDXGI*` 노출 금지. `IRHIDevice` 추상 유지.
  - mutable global singleton 금지(`CAmbientProp_Manager`는 기존 `Minion_Manager` 싱글톤 패턴을 따르되 그 범위 안에서).

---

## 2. owner 지도 — 버킷 → owner(패턴) + 소유 상태

각 owner가 `CScene_InGame`에서 가져갈 멤버와 메서드. (멤버명은 현재 `Scene_InGame.h` 기준, 작업 직전 재확인.)

### 2-1. `CLocalChampionPrediction` (stateful, Client 전용) — 최대 부채
- **소유 상태**: 범용 dash(`m_bDashActive`/`m_fDashElapsed`/`m_fDashDuration`/`m_vDashStart`/`m_vDashEnd`/`m_DashTargetEntity`), Kalista passive dash 14종(`m_bKalistaPassiveDash*`/`m_vKalistaPassiveDash*`/`m_uKalista*`), Yasuo E/R 9종(`m_bYasuoDashActive`/`m_bYasuoRActive`/`m_vYasuoDash*`/`m_fYasuoR*`/`m_iYasuoRHitsFired`/`m_YasuoRTarget`), end-transition(`m_pPendingEndAnim`/`m_fEndTransitionTimer`/`m_bEndTransitionMoving`), active skill(`m_ActiveSkillDefStorage`/`m_ActiveSkillCommandStorage`/`m_pActiveSkillDef`/`m_fActivePrevFrame`/`m_bCastFrameFired`/`m_bRecoveryFrameFired`/`m_pLastDispatchedSkill`), flash(`m_fFlash*`), 로컬 스킬 sim 시스템(`m_pIreliaBladeSystem`/`m_pWindWallSystem`/`m_pYasuo*`/`m_pKalista*`/`m_pPendingHitSystem`).
- **흡수 파일**: `Scene_InGameLocalSkills.cpp` 전체.
- **인터페이스(예)**: `DispatchSkillInput(slot,stage)`, `UpdatePrediction(dt)`, `UpdatePlayerControl(dt, netActive, ...)`, `TriggerFlash`, `ApplyLocalChampionDamage`. presentation 효과(애니/transform) 직접 적용 허용.
- **주의**: compass 허용 local-only prediction. `Shared`로 올리지 말 것. `CInGameNetcode`와 합치지 말 것.

### 2-2. `CInGameNetcode` (stateful, Client 전용)
- **소유 상태**: 세션/applier(`m_pEntityIdMap`/`m_pNetwork`/`m_pNetworkView`/`m_bUsingSharedNetwork`/`m_pSnapshotApplier`/`m_pEventApplier`/`m_pCommandSerializer`), 보간(`m_NetworkActorInterpStates`/`m_uNetworkActorInterpSnapshotTick`/`m_bNetworkActorInterpolationEnabled`), 예측(`m_NetworkMovePredictions`/`m_uLastAckedMovePredictionSeq`/`m_fLocalCorrectionBlendSec`), 액션 애니(`m_NetworkActionAnimStates`), locomotion 캐시(`m_NetworkChampionPrevPos`/`m_NetworkChampionMoveGraceSec`/`m_NetworkChampionMoving`).
- **흡수 파일**: `Scene_InGameNetwork.cpp` 전체.
- **인터페이스(예)**: `Initialize(...)`, `Pump()`, `ApplySnapshot(bytes,len)`, `BeginInterpForSnapshot(tick)`/`ApplyInterpolation(dt)`, `UpdateChampionLocomotion(dt)`, `OnAuthoritativeSnapshot(...)`. 보간 transform을 World에 직접 기록(presentation) 허용.
- **주의**: applier 콜백이 server snapshot을 visual로 적용만 — authoritative truth 신규 생성 금지. snapshot-apply 경계라 `CLocalChampionPrediction`과 분리 유지.

### 2-3. `CMapNavService` (stateful 리소스 소유 + static-style calc)
- **소유 상태**: `m_pNavGrid`/`m_pPathNavGrid`/`m_pMapSurfaceSampler` + nav 튜닝(`m_fNav*`).
- **흡수 파일**: `Scene_InGameMapNav.cpp` 전체.
- **인터페이스(예)**: `BuildFromStage(...)`, `TryProjectToMapSurface(ioPos)`, `TryResolveWalkableMoveTarget(...)`, `IsWalkableMoveSegment(...)`, `ResolveMouseMapSurfacePos(camera, input)`, `ProjectGameplayActorsToMapSurface(world)`. GameRoom `CWalkabilityAuthority` + `IWalkableQuery`의 클라 대응.
- **주의**: 계산 메서드는 입력을 받아 결과 반환(static authority 성격). 그리드 리소스는 service가 보유.

### 2-4. `CInGameInputController` (stateful, Client 전용)
- **소유 상태**: hover(`m_HoveredEntity`/`m_OutlinedHoverEntity`/`m_HoveredTeam`), 공격 의도 statics(현재 `Scene_InGameInput.cpp` anon ns의 `s_NetworkAttackTarget` 등 → controller 멤버로), ping 상태, attack-range 토글(`m_bShowAttackRange`).
- **흡수 파일**: `Scene_InGameInput.cpp` 전체.
- **인터페이스(예)**: `UpdateTargeting(scene-view)`, `UpdateCombatInput(out intent)`, `UpdatePingWheel`. **intent 반환형이 자연스러우면 Result 패턴 채택** — controller가 `{skill dispatch, move target, flash}` intent를 내고 scene/prediction이 실행.
- **주의**: 사망 입력락은 presentation gate. truth 아님.

### 2-5. `CAmbientProp_Manager` (stateful manager, `Minion_Manager` 패턴) — ✅ S2-A 완료
- **결과 파일**: `Client/Public/Manager/AmbientProp_Manager.h`(37) + `Client/Private/Manager/AmbientProp_Manager.cpp`(160). `DECLARE_SINGLETON` 싱글톤.
- **소유 상태**: `struct Prop{ unique_ptr<ModelRenderer>, CTransform }` 벡터 `m_props`. .wamb 로드·record/asset 상수는 owner .cpp anon ns로 이동.
- **실제 인터페이스**: `Spawn(const Mat4& mapWorld, f32_t mapYaw, const std::function<void(Vec3&)>& projectToSurface)` / `Tick(dt)` / `Render(vp, cameraWorld)` / `AppendRenderSnapshotMeshes(RenderWorldSnapshot&, vp)` / `Shutdown()`.
- **scene 위임 지점**(검증됨): OnEnter `Spawn(...)`(Lifecycle), OnUpdate `Tick`(shell), OnRender `Render` + RHI 경로 `AppendRenderSnapshotMeshes`(Render TU), OnExit `Shutdown`(Lifecycle).
- **핵심 결정**: `TryProjectToMapSurface`(MapNav/scene)를 직접 부르지 않고 **`projectToSurface` 콜백으로 위임** — owner가 MapNav/scene에 결합되지 않는다. RHI snapshot 경로(`AppendRenderSnapshotMeshes`)도 owner가 흡수해 `CRHISceneRenderer`와 자연 합류.
- **함정(기록)**: 신규 Client 매니저 헤더는 **맨 앞에 `#include "Defines.h"`** 를 둬야 한다. `Renderer/ModelRenderer.h`의 `const wstring&`/`uint32`가 `using namespace std;`(=`Defines.h` 제공) 전제라, 안 넣으면 미정의 빌드 실패. `Minion_Manager.h` 패턴을 그대로 따른다. **남은 S2-B~G 신규 헤더에 동일 적용.**

### 2-6. `CInGameSceneInstaller` (static authority)
- **책임**: OnEnter 부트스트랩 — 스케줄러/시스템 등록, Manager init, 맵 메시·MapSurfaceSampler, NavGrid 로드, 렌더 헬퍼(NormalPass/SSAO/FoW/AttackRange/ContactShadow/RHI), FX 시스템 6종 + preload, replay/ready.
- **흡수 파일**: `Scene_InGameLifecycle.cpp`의 OnEnter 본문.
- **패턴**: GameRoom `CWorldBootstrap`처럼 **무상태**. scene(또는 위 owner들)에 빌드 결과를 **주입**하고 소유는 scene/owner가 가진다. facade 아님(주입 후 reach-in 안 함).
- **데이터 주도 연결**: 시스템 등록의 `if(!m_bNetworkAuthoritativeGameplay)` 분기 다발을 `{phase, factory, condition}` 테이블로 — "어떤 시스템을 어떤 조건에 등록" = 코드 분기가 아니라 데이터(`13` 데이터 주도 트랙과 합류).

### 2-7. FX 시스템 → `m_pScheduler` 흡수 (a)
- `m_pFxSystem`/`Beam`/`Mesh`의 `Update`와 로컬 스킬 sim 시스템(`Irelia blade`/`WindWall`/`Yasuo`/`Kalista`)은 이미 `Execute(world, dt)` 시그니처 = ISystem 동형. ISystem 상속 + `GetPhase` 부여 후 `m_pScheduler->RegisterSystem`. **Update만** 시스템화(Render는 OnRender 유지). `CLocalChampionPrediction`이 소유하던 스킬 sim 시스템과 겹치면 prediction owner가 보유하고 scheduler 등록은 installer가.

### 2-8. `CInGameRenderer` (stateful, 후순위)
- 렌더 리소스(`m_pNormalPass`/`m_pSSAOPass`/`m_pFogOfWarRenderer`/plane/cube/attackrange)를 owner로. OnRender가 위임. SSAO/Dbg 토글 getter-setter(외부 패널이 호출, `15` §7 보존 surface)는 renderer로 forward. 가치 대비 touch가 커서 마지막.

---

## 3. 목표 shell

```cpp
class CScene_InGame final : public IScene
{
public:
    bool OnEnter() override;        // m_pInstaller->Install(*this, ...) 위임
    void OnExit() override;         // owner들 teardown
    void OnUpdate(f32_t dt) override;   // netcode->Pump → prediction/input/nav/scheduler 오케스트레이션
    void OnLateUpdate(f32_t dt) override;
    void OnRender() override;       // m_pRenderer->Render(view) 위임
    void OnImGui() override;        // 디버그 패널 디스패치

    // 외부 UI 패널 보존 accessor (15 §7) — owner로 forward
    // GetWorld/GetPlayerEntity/GetNavGrid/GetCameraPtr/GetFxMeshRenderer/...
private:
    CWorld m_World;
    unique_ptr<CDynamicCamera> m_pCamera;
    unique_ptr<CInGameNetcode>            m_pNetcode;
    unique_ptr<CLocalChampionPrediction>  m_pPrediction;
    unique_ptr<CMapNavService>            m_pMapNav;
    unique_ptr<CInGameInputController>     m_pInput;
    unique_ptr<CInGameRenderer>           m_pRenderer;
    unique_ptr<CInGameSceneInstaller>     m_pInstaller;
    // CAmbientProp_Manager::Get() (싱글톤)
};
```

scene은 `CWorld`/camera 같은 1차 자원과 owner들을 보유하고, 6 가상함수는 owner에게 위임만 한다.

---

## 4. 권장 순서 (안전 → 무게)

GameRoom Stage 2가 그랬듯 **가장 깨끗한 경계부터** 추출해 패턴을 검증하고, 무거운 것을 뒤로 둔다. owner 1개 = 빌드+스모크 게이트 1회 = 커밋 1회.

```text
S2-A  CAmbientProp_Manager     [✅ 완료, 빌드 green] 장식, Minion_Manager 패턴. owner 패턴 검증됨 (런타임 F5 확인 권장)
S2-B  CMapNavService           [다음] 리소스+calc 경계 명확. Network/LocalSkills/Input이 grid를 query만
S2-C  CInGameNetcode           (applier+보간+예측 상태. snapshot-apply 경계)
S2-D  CLocalChampionPrediction (최대 부채. compass local-only prediction, Shared 금지)
S2-E  CInGameInputController    (hover+공격의도+ping. intent 반환 패턴)
S2-F  CInGameSceneInstaller + FX→scheduler  (OnEnter 부트스트랩 데이터화)
S2-G  CInGameRenderer          (후순위 — 보존 accessor forward 비용 큼)
```

각 단계 후 scene shell이 그만큼 줄고, 마지막에 §3 목표 shell에 도달한다. 중간 단계는 항상 빌드+F5 동작 불변이어야 한다(verbatim이 아니라 owner 이동이므로 **런타임 회귀 확인이 Stage 1보다 중요**).

---

## 5. 가드레일 & 검증 게이트 (`13`과 동일)

- **G1 빌드**: `Client.vcxproj` Debug x64 (`/p:BuildProjectReferences=false`).
- **G2 위임 diff**: owner 이동 + scene 위임 + vcxproj 등록만. owner 이동과 동작 변경을 한 커밋에 섞지 않는다.
- **G3 경계**: `rg` forbidden-dependency 0 — 특히 `Shared/GameSim`에 Client 누출 없음(prediction/netcode가 Shared로 새지 않음).
- **G4 F5 스모크 (서버 로그만으로 판정 금지)**: roster/map/minion/champion/snapshot/UI/FX 7개 미은닉 + player/minion/jungle Idle·Run, BasicAttack, SkillQ/W/E/R, Yasuo R·Kalista passive dash·Irelia Q dash, Recall, DeathStart/Dead, cast/recovery FX, cooldown UI, **새/오리 앰비언트** 불변.
- **G5 본질**: owner가 한 가지 이유로만 바뀌고, scene은 조립/위임만 하는가. `friend` 0.

---

## 6. Codex 핸드오프

실행은 `.md/plan/refactor/18_CODEX_PROMPT_INGAME_SCENE_STAGE2.md`로 넘긴다. 그 프롬프트는 `13`(방법론·authority 패턴)·`15`(Stage 1 맥락)·이 문서(owner 지도)를 가리키는 진입점이며, 각 owner 추출의 실제 코드 지시는 작업 직전 `.md/계획서작성규칙.md` 형식 계획서에 둔다.
