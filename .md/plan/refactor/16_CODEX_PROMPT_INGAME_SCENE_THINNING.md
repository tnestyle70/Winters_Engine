# Codex 지시 프롬프트 — `CScene_InGame`을 IScene 가상함수만 남기는 본질 분해

> **상태(2026-06-13)**: 이 프롬프트의 **`STAGE=STAGE1_SPLIT`은 완료**됐다(8 TU 분리, 빌드 green). 남은 **Stage 2(owner 추출)는 `.md/plan/refactor/18_CODEX_PROMPT_INGAME_SCENE_STAGE2.md`로 이관**됐다 — Stage 2를 할 땐 18을 쓰라. 이 문서는 Stage 1 기록/맥락으로 보존한다.
>
> 이 문서의 **프롬프트 본문(복붙용)**을 그대로 Codex에 붙여넣으면 바로 작업 시작.
> 첫 줄 `STAGE=`만 단계로 바꿔 지시한다.
> 목표: `Client/Private/Scene/Scene_InGame.cpp`(6,588줄 god-file)를 북극성(Essence Boundary)에 따라 **회귀 0으로** 쪼개고, 최종적으로 `CScene_InGame`을 IScene 6 가상함수 + 소수 owner 소유 + 얇은 accessor만 남는 orchestration shell로 만든다.
> 방법은 `GameRoom.cpp`(6,077 → 403줄)에서 검증된 2단계: Stage 1 verbatim 분리 → Stage 2 원자 owner 추출.

---

## 사용법

1. 아래 **프롬프트 본문** 전체를 복사한다.
2. 첫 줄 `STAGE=`를 현재 단계로 설정한다 (`STAGE1_SPLIT` / `STAGE2_OWNERS`). 처음에는 반드시 `STAGE1_SPLIT`.
3. Codex에 붙여넣고 실행한다.
4. Codex가 막히면(영구 실패·판단 필요) 사유를 `(a)재시도 가능 / (b)설계상 정상 / (c)버그`로 분류해 보고하고, 나머지는 계속 진행한다.

설계·소유권 맵·단계 계획의 정본은 `.md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md`다. 이 프롬프트는 그것과 `13_ATOMIC_DECOMPOSITION_PLAYBOOK`을 가리키는 실행 진입점이다.

---

## 프롬프트 본문 (복붙용)

```text
STAGE=STAGE1_SPLIT   # STAGE1_SPLIT = 기계적 multi-TU verbatim 분리 / STAGE2_OWNERS = 원자 owner 추출

너는 Winters 엔진(C++ DX11, 서버 권위 MOBA)의 Client scene 구조 정리를 담당하는 시니어 엔지니어다.
목표: Client/Private/Scene/Scene_InGame.cpp(6,588줄 god-file)를 본질 단위로 쪼개고, 최종적으로 CScene_InGame을
IScene 6 가상함수(OnEnter/OnExit/OnUpdate/OnLateUpdate/OnRender/OnImGui) + 소수 owner 소유 멤버 + 얇은 accessor만
남는 orchestration shell로 만든다. 방법은 Server/Private/Game/GameRoom.cpp에서 검증된 2단계다.
회귀 0(동작 불변)을 절대 조건으로 두고, 현재 STAGE 범위를 작업 루프로 진행한다.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 빌드(Engine 미커밋 변경으로 전체 리빌드가 깨질 수 있어 BuildProjectReferences=false 권장):
  & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false
- 런타임 스모크: F5 / WintersGame.exe 로 normal 경로 실행 (roster/map/minion/champion/snapshot/UI/FX 7개 미은닉 확인).
- 한글 주석 파일을 읽을 때는 UTF-8로 읽어 mojibake를 진짜 에러로 오판하지 마라.

[최상위 개념 — 이게 전부다]
가장 위의 개념은 'God-file 쪼개기'가 아니라 'source-of-truth ownership(본질 소유권)'이다.
각 함수 묶음/멤버에 먼저 묻는다:
  - 서버가 판정하는 gameplay truth인가?            -> Server / Shared (Client로 새로 만들지 마라)
  - 이미 정해진 결과를 보여주는 presentation인가?  -> Client (여기가 scene의 본질)
  - 클라 약예측(weak prediction)/lab인가?          -> Client, 단 이름으로 격리하고 snapshot-apply와 다른 파일
  - transport / adapter(packet send, session bind)인가?
  - debug / lab / tuner인가?
답이 다르면 다른 파일/owner로 '이동'(삭제 아님)한다.
"한 코드가 두 가지 이유로 바뀐다면 아직 덜 쪼갠 것이다."

[과거 실패 — 다시 만들지 마라]
이 파일은 한때 InGame*Bridge 10개 facade 클래스로 흩어져 있었다. 그 bridge들은 static 메서드로 함수 본문만 들고
상태(m_*)는 scene에 남긴 채 friend로 reach-in했다. 분리 가치가 0이었고(상태가 scene에 그대로), 캡슐화만 파괴했다.
그래서 전부 scene으로 인라인해 읽을 수 있는 단일 god-file로 만들었다. 지금 상태가 분해의 올바른 출발점이다.
=> 절대 facade 클래스(static Foo(CScene_InGame& scene, ...) + friend)로 되돌아가지 마라.
   Stage 1은 '같은 CScene_InGame 클래스의 멤버 정의'를 여러 .cpp로 나눈다(GameRoomXxx.cpp 패턴).
   Stage 2는 상태를 '실제로 소유하는' owner 클래스로 옮긴다(owner는 Result 반환, scene은 owner를 보유+public API로만 호출, friend 없음).

[2단계 방법론 — 반드시 이 순서, 한 번에 둘 다 하지 않는다]
Stage 1 (STAGE1_SPLIT, 기계적 본질 분리, 동작 불변):
  - Scene_InGame.cpp를 Scene_InGameXxx.cpp 패턴으로 책임별 .cpp로 쪼갠다. 함수 본문은 라인 단위 그대로 이동(verbatim).
  - public 클래스 선언(Scene_InGame.h)은 건드리지 않는다. cpp 정의만 이동한다(멤버 선언은 이미 헤더에 있다).
  - 시그니처/동작/호출순서 불변. 2개 이상 .cpp가 공유하는 anonymous namespace 헬퍼/상수/struct는
    Scene_InGameInternal.h로 승격한다(중복정의 드리프트 차단 — 이게 최대 정확성 함정. ~60개 helper 사용처 전수 분류 후 배치).
  - 이동과 로직 수정을 같은 커밋에 섞지 않는다. diff는 순수 이동이어야 한다.
Stage 2 (STAGE2_OWNERS, 원자 owner 추출, 상태 소유권 이동):
  - CScene_InGame을 orchestration shell로 줄이고, 상태/규칙을 원자 owner 클래스로 추출한다.
  - owner는 side effect를 직접 실행하지 않고 Result를 반환한다. packet send/spawn bootstrap/replay stop은 scene이 받아 처리.
  - 반드시 Stage 1으로 seam이 드러난 뒤에 한다.

[먼저 읽을 문서 — 순서대로 반드시 읽는다]
1. .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md  ← 이 작업의 설계·소유권 맵·slice 순서·보존 surface(최우선)
2. .md/plan/refactor/13_ATOMIC_DECOMPOSITION_PLAYBOOK.md  ← 2단계 방법론·경계 가드레일·게이트 정본
3. .md/plan/refactor/00_ESSENCE_BOUNDARY_REFACTOR.md      ← 북극성·본질판정
4. .md/architecture/WINTERS_CODEBASE_COMPASS.md, CLAUDE_Legacy.md ← 계층 경계·서버 권위·의존 규칙
5. CLAUDE.md, .claude/gotchas.md, .md/계획서작성규칙.md   ← 코딩/계획서 규칙(각 slice 직전 계획서를 이 형식으로 쓴다)

[현재 코드베이스 사실 — 다시 만들지 마라. 라인 번호는 변동 가능 — 작업 직전 rg/Read로 재확인]
이미 있는 것(흡수·소비만, 새로 만들지 마라):
  - ECS 스케줄러 패턴: m_pScheduler->RegisterSystem(std::move(...)) (OnEnter), 매 프레임 m_pScheduler->Execute(m_World, dt) (OnUpdate).
    등록 시스템: Transform/SpatialHash/StatusEffect/Vision/Navigation/MinionAI/MinionPerformance/Turret*/BehaviorTree/YoneSoul/MCTS.
  - 네트워크 applier 분리 클래스: CSnapshotApplier / CEventApplier / CCommandSerializer / CClientNetwork / EntityIdMap (Network/Client/*).
  - Manager 싱글톤 패턴: CMinion_Manager / CStructure_Manager / CJungle_Manager — CWorld* 참조만, Initialize/Tick/Render/Shutdown 생애주기.
  - FX 시스템 9종(CFxSystem/Beam/Mesh/IreliaBlade/WindWall/Yasuo·Kalista projectile/PendingHit/Rend): 전부 Execute(world, dt) 시그니처 = ISystem 동형.
  - Map/Nav 본체: CNavGrid/Engine::CNavGrid/CMapSurfaceSampler/CMapWalkableBaker/CPathfinder (Manager/Navigation/*).
  - Render 본체: CNormalPass/CSSAOPass/CFogOfWarRenderer/CPlaneRenderer/CRHIFxSpriteRenderer/CFxStaticMeshRenderer (Renderer/*).
  - debug 패널: AIDebugPanel/RenderDebugPanel/CombatDebugPanel/MapTunerPanel/DebugDrawSystem/EffectTuner/WfxEffectToolPanel/MinimapPanel.
    ChampionTuner/SkillTimingPanel은 이미 빈 스텁 — 신경 쓰지 마라.
빈자리(= 네가 만들 것):
  - 대응 시스템이 없는 최대 부채 2개: (1) 로컬 챔피언 예측(UpdateDash/Yasuo·Kalista dash 상태머신/ApplyLocalPrediction, ~1,340줄),
    (2) 네트워크 보간/예측 상태머신(m_NetworkActorInterpStates/MovePrediction/OnAuthoritativeSnapshot/ApplyNetworkActorInterpolation, ~700줄).
  - .vcxproj는 명시적 ClCompile 목록(glob 아님). /utf-8 /FS는 프로젝트 전역.

[STAGE1_SPLIT 작업 범위] (현재 STAGE에 따름)
Scene_InGame.cpp를 아래 책임 TU로 verbatim 분리한다. 독립도 높은 것부터. seam 1개 = 커밋 1개.
  S1-0 Scene_InGameInternal.h     : 2개 이상 TU 공유 anon helper/상수/struct 승격 (먼저, 사용처 전수 분류)
  S1-1 Scene_InGameImGui.cpp      : OnImGui + DrawReplayControlPanel + 디버그 패널 디스패치 + ImGui include (가장 독립적, 첫 slice)
  S1-2 Scene_InGameNetwork.cpp    : InitializeNetworkSession/OnAuthoritativeSnapshot/ApplyNetworkActorInterpolation/UpdateNetworkChampionLocomotion + 보간/예측 상태
  S1-3 Scene_InGameLocalSkills.cpp: ApplyLocalPrediction/StartLocal*Dash/UpdateLocal*/UpdateDash + dash 상태머신  ★ S1-2와 절대 같은 파일에 두지 마라
  S1-4 Scene_InGameInput.cpp      : UpdateCombatInput/UpdateTargeting/ping/hover/attack-intent
  S1-5 Scene_InGameRender.cpp     : OnRender 본문 + render anon helper
  S1-6 Scene_InGameMapNav.cpp     : CreateMapNavGrid/BakeMapWalkableNavGrid/Mark_StructuresOnNavGrid/surface 샘플링/부쉬 시드
  S1-7 Scene_InGameLifecycle.cpp  : OnEnter/OnExit 부트스트랩·셧다운 (시스템 등록 순서 불변)
핵심 주의:
  - ApplyLocalPrediction/StartLocal*Dash/UpdateLocal*/ApplyLocalChampionDamage는 compass가 허용한 local-only prediction이다.
    snapshot-apply(OnAuthoritativeSnapshot/ApplyNetworkActorInterpolation)와 같은 파일에 섞지 마라(최대 경계위반).
  - 익명 namespace 공유 헬퍼(cpp 상단 ~60개: Network 애니/yaw, Map/Nav 경로·회피, ChampionLocalSim 스킬 브리지)를
    Scene_InGameInternal.h로 승격하는 것이 최대 정확성 함정. 두 곳 이상이 쓰는 것만 올린다.

[STAGE2_OWNERS 작업 범위] (Stage 1 완료 후에만)
Stage 1이 드러낸 seam을 따라 owner를 추출한다. 우선순위 = 무게 × 대응 시스템 부재:
  S2-1 CLocalChampionPrediction (Client 전용) ← Scene_InGameLocalSkills.cpp   최대 부채, GameSim 중복 아님(클라 weak prediction)
  S2-2 CInGameNetcode           (Client 전용) ← Scene_InGameNetwork.cpp        applier 3종 소유 이전 + 보간/예측 상태
  S2-3 FX/스킬 sim → m_pScheduler             ← OnUpdate 수동 tick             ISystem 상속 + GetPhase + RegisterSystem. system 등록을 {phase,factory,condition} 데이터 테이블로
  S2-4 CMapNavService                         ← Scene_InGameMapNav.cpp
  S2-5 CAmbientProp_Manager                   ← AmbientProps(새/오리)          Minion_Manager 싱글톤 패턴 복제
  S2-6 CInGameSceneInstaller                  ← Scene_InGameLifecycle.cpp      부트스트랩 주입. facade 아님(주입 후 소유는 scene)
절대 금지: 버킷 S2-1(로컬 예측)/S2-2(보간)를 Shared/GameSim(ECS 컴포넌트/시스템)로 보내기 — Shared가 Client를 include하는 의존성 역전.
  둘 다 Client presentation/weak prediction이므로 반드시 Client 소유.

[보존해야 하는 외부 surface — Stage 1에서 .h 시그니처 불변, Stage 2에서 owner로 forwarding]
  AIDebugPanel/RenderDebug/DebugDrawSystem/EffectTuner/WfxEffectToolPanel/MapTunerPanel/CombatDebugPanel이 호출하는 게터·세터:
  IsNetworkAuthoritativeGameplay/GetNetworkView/GetCommandSerializer/GetSnapshotApplier/GetEntityIdMap, SSAO 8종, Dbg* 토글 16종,
  GetWorld/GetPlayerEntity/GetFxMeshRenderer/ResolveMouseMapSurfacePos, GetNavGrid/GetPathNavGrid/GetCameraPtr,
  Get/SetMapRotation, hover/Sylas/hit 게터·세터, RebuildMapWalkableNavGridForDebug. (ChampionTuner/SkillTimingPanel은 빈 스텁 — 무시)

[작업 루프 — 완료까지 반복]
1. 상태 파악: Scene_InGame.cpp 함수 목록을 table-of-contents처럼 읽고 본질 클러스터로 분류해 보고.
2. 한 slice 진행: 경계 라인을 assert/rg로 확인 -> 함수 본문 verbatim 이동 -> 공유 helper는 Scene_InGameInternal.h
   -> 새 .cpp/.h를 Client.vcxproj/.filters에 직접 등록.
3. 검증: 빌드(G1) + F5 스모크(G4, 서버 로그만으로 판정 금지) + git diff로 순수이동(G2) 확인.
4. 커밋: slice 1개 = 커밋 1개. 이동과 로직 수정을 분리.
5. 막힌 부분(버그/판단 필요)만 사유 분류해 보고, 나머지는 계속 루프.

[반드시 지킬 규칙]
- 서버 권위: Client는 gameplay truth를 새로 만들지 않는다. local-only prediction/lab은 이름으로 격리, snapshot-apply와 다른 파일.
- 의존 방향: Shared/GameSim는 Engine/Client/Renderer/UI/ImGui/DX include 금지. Server는 Client visual 의존 금지.
  Engine은 LoL/Server/GameSim 제품 타입(eChampion/Champion/Minion/Turret 등)을 새로 노출하지 않는다.
- DX 비노출: Client/Public 또는 Shared/EngineSDK/inc 헤더에 ID3D11*/IDXGI* 노출을 넓히지 않는다. IRHIDevice 추상 유지.
- 공유 anon helper 중복정의 금지 -> Scene_InGameInternal.h로 단일화.
- /utf-8: 새 cpp/h의 한글 주석이 CP949로 깨지지 않게 프로젝트 전역 /utf-8에 의존. per-file AdditionalOptions로 떨어뜨리지 않는다.
- 프로젝트 등록: 새 split .cpp/.h의 Client.vcxproj/.filters 등록은 분해의 필수 단계로 직접 수행(미등록 시 링크 단계에서 함수 증발). 그 외 XML 구조는 건드리지 않는다.

[금지]
- facade 클래스(static Foo(CScene_InGame&, ...) + friend)로 되돌아가기 — 과거 InGame*Bridge의 실패를 반복하지 마라.
- Stage 1과 Stage 2를 한 커밋에 섞기. 이동과 로직 수정을 한 커밋에 섞기.
- Stage 1에서 Scene_InGame.h public 시그니처를 바꾸기.
- local prediction과 snapshot-apply를 같은 파일/owner에 합치기.
- 로컬 예측/보간을 Shared/GameSim로 올리기(의존성 역전).
- EngineSDK/inc 직접 수정 — Engine public header 변경 시 UpdateLib.bat 동기화로만.
- 진행률 %만 보고 완료 선언 — 게이트(빌드+스모크+순수이동) 통과로만 판단.
- normal F5의 roster/map/minion/champion/snapshot/UI/FX를 숨겨서 분리를 통과시키기.
- 단일 책임·소형 파일을 의미 없이 쪼개기.

[검증 게이트 — 공통]
G1 빌드: Client.vcxproj Debug x64 통과.
G2 순수 이동: git diff가 이동/등록만(이동과 로직수정 분리 커밋).
G3 경계: forbidden-dependency 스캔 0.
   rg -n "#include .*(Server)" Client Shared/GameSim
   rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim
G4 런타임 스모크(F5, 서버 로그만으로 판정 금지): roster/map/minion/champion/snapshot/UI/FX 7개 미은닉 +
   player/minion/jungle Idle·Run, BasicAttack, SkillQ/W/E/R, Yasuo R·Kalista passive dash, Recall, DeathStart/Dead, cast/recovery FX, cooldown UI 불변.
G5 본질: 분리 후 각 파일이 한 가지 이유로만 바뀌는가.

[완료 기준 — STAGE DoD]
- STAGE1_SPLIT: Scene_InGame.cpp가 OnUpdate orchestration + 생성자/소멸자 + 얇은 위임만 남고,
  network/local-skill/input/render/imgui/mapnav/lifecycle이 책임 .cpp로 분리됨. local-prediction과 snapshot-apply가 다른 파일.
  Scene_InGame.h 시그니처 불변. F5 7개 시스템 + 회귀 동작 불변.
- STAGE2_OWNERS: CScene_InGame이 IScene 6 가상함수 + composition root(owner unique_ptr 6~8개) + 얇은 view accessor만 남는 shell.
  CLocalChampionPrediction/CInGameNetcode/CMapNavService/CAmbientProp_Manager/CInGameSceneInstaller가 상태를 실제 소유,
  owner는 Result 반환, scene은 위임만. 로컬예측/보간은 Client 소유. F5 동작 불변.

[시작]
지금 즉시: (1) 문서 1~3을 읽고, (2) Scene_InGame.cpp를 함수목록으로 본질 클러스터 분류해 보고,
(3) STAGE1_SPLIT이면 S1-0(Internal.h 공유 helper 분류) → S1-1(ImGui) 순으로 첫 slice를 verbatim 분리해 G1/G4 통과 후 커밋.
막히면 사유를 (a)재시도 가능 (b)설계상 정상 (c)버그로 분류해 보고하고 나머지는 계속 진행하라.
```

---

## STAGE별 첫 slice 예시 (Codex가 바로 실행)

### STAGE1_SPLIT — Internal.h + ImGui seam부터 (가장 독립적)
```text
STAGE=STAGE1_SPLIT
첫 slice: (1) Scene_InGame.cpp 익명 namespace ~60개 helper의 사용처를 전수 분류해, 2개 이상 .cpp가 쓸 것만
Scene_InGameInternal.h로 승격. (2) OnImGui/DrawReplayControlPanel/디버그 패널 디스패치 + ImGui include 17종을
Scene_InGameImGui.cpp로 verbatim 분리. 호출처/순서 불변. Client.vcxproj/.filters 등록. 빌드 + F5 패널 표시 확인.
다음 slice: Network(snapshot apply/interp/세션) seam을 Scene_InGameNetwork.cpp로. local-prediction은 절대 섞지 않는다.
```

### STAGE2_OWNERS — LocalChampionPrediction부터 (최대 부채)
```text
STAGE=STAGE2_OWNERS
첫 slice: Scene_InGameLocalSkills.cpp(Stage 1 산출)의 dash 상태머신과 ApplyLocalPrediction/StartLocal*Dash/UpdateLocal*를
CLocalChampionPrediction(Client 전용, .h/.cpp)로 상태째 이동. owner는 입력(커서/타겟/dt)을 받아 Result/예측 transform을 돌려주고,
scene이 그 결과를 m_World/렌더러에 적용한다. snapshot-apply(OnAuthoritativeSnapshot)와 절대 합치지 마라.
Shared/GameSim로 올리지 마라(의존성 역전). 빌드 + F5 Yasuo R/Kalista passive dash/Irelia Q dash 동작 불변 확인.
```

---

## 검증 명령 (공통)

```powershell
# 빌드 (Engine 리빌드 회피)
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false

# 순수 이동 확인 (--check는 whitespace만 — 이동 vs 로직수정은 --stat + 내용 리뷰로 판단)
git diff --stat
git diff --check

# forbidden dependency (0이어야 함)
rg -n "#include .*(Server)" Client Shared/GameSim
rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim
```
