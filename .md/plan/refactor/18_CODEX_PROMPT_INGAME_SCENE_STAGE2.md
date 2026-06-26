# Codex 지시 프롬프트 — InGameScene Stage 2 (원자 owner 추출)

> 이 문서의 **프롬프트 본문(복붙용)**을 그대로 Codex에 붙여넣으면 시작.
> 첫 줄 `OWNER=`를 추출할 owner로 바꾼다.
> 목표: Stage 1로 8개 책임 TU로 쪼개진 `CScene_InGame`에서 **상태 소유권을 owner 클래스로 옮겨** scene을 IScene 6 가상함수 + owner 조립 shell로 만든다.
> 방법은 `GameRoom.cpp`(Stage 2 진행 중)에서 검증된 stateful/static authority 패턴. **회귀 0이 절대 조건.**

---

## 사용법

1. 아래 **프롬프트 본문** 전체 복사.
2. 첫 줄 `OWNER=`를 현재 추출 대상으로 설정 (`AMBIENT` / `MAPNAV` / `NETCODE` / `PREDICTION` / `INPUT` / `INSTALLER` / `RENDERER`). 처음에는 `AMBIENT`(가장 안전, 패턴 검증).
3. Codex에 붙여넣고 실행.
4. 막히면 사유를 `(a)재시도 가능 / (b)설계상 정상 / (c)버그`로 분류 보고하고 나머지는 계속 진행.

설계·owner 지도·소유 상태 목록의 정본은 `.md/plan/refactor/17_INGAME_SCENE_STAGE2_OWNER_DESIGN.md`. 방법론·authority 패턴 정본은 `13_ATOMIC_DECOMPOSITION_PLAYBOOK.md`.

---

## 프롬프트 본문 (복붙용)

```text
OWNER=AMBIENT   # AMBIENT / MAPNAV / NETCODE / PREDICTION / INPUT / INSTALLER / RENDERER

너는 Winters 엔진(C++ DX11, 서버 권위 MOBA)의 Client scene 구조 정리를 담당하는 시니어 엔지니어다.
목표: Stage 1로 8개 책임 TU(Scene_InGame{,Network,LocalSkills,Input,MapNav,Render,ImGui,Lifecycle}.cpp + Internal)로
쪼개진 CScene_InGame에서, 지정된 OWNER의 상태/규칙을 원자 owner 클래스로 추출해 scene을
IScene 6 가상함수 + owner 조립 shell로 만든다. 방법은 Server/Private/Game/GameRoom.cpp Stage 2에서 검증된 패턴.
회귀 0(F5 동작 불변)을 절대 조건으로 두고 현재 OWNER 하나를 완료까지 진행한다.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 빌드(Engine 미커밋 변경 회피 시 BuildProjectReferences=false):
  & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false
- 런타임 스모크: F5 / WintersGame.exe 로 normal 경로 (roster/map/minion/champion/snapshot/UI/FX + 새/오리 미은닉 확인).
- 한글 주석 파일은 UTF-8로 읽어 mojibake를 진짜 에러로 오판하지 마라.

[최상위 개념 — 이게 전부다]
가장 위의 개념은 'God-file 쪼개기'가 아니라 'source-of-truth ownership'이다.
Stage 1은 함수를 파일로 나눴을 뿐 상태(m_*)는 아직 CScene_InGame이 소유한다. 각 TU 메서드가 scene.m_x를 직접 만진다.
Stage 2는 그 상태를 owner로 옮기고 scene을 조립자로 만든다. "한 코드가 두 가지 이유로 바뀌면 아직 덜 쪼갠 것이다."

[Client presentation owner의 규율 — GameRoom과 다르다. 반드시 숙지]
GameRoom owner는 서버 gameplay truth를 지키려고 Result를 반환하고 shell이 side effect를 실행한다.
InGameScene owner는 Client presentation이다:
  - 그대로: (1) owner가 자기 상태를 소유(scene 아님). (2) scene은 owner의 public API로만 호출 — friend 금지
    (과거 InGame*Bridge가 friend로 reach-in한 실패를 반복하지 마라). (3) owner는 한 이유로만 바뀐다.
  - 다른 점: owner는 visual transform 기록/애니 재생/draw 제출 같은 presentation 효과를 직접 실행해도 된다(truth 아님).
    단, intent 반환형이 더 명료하면 Result 패턴을 써라(예: InputController가 intent 반환, prediction이 실행).
  - 절대 금지(presentation이어도):
    * Client는 authoritative truth(cooldown/hit/damage/action lock/HP)를 새로 만들지 않는다. local-only prediction은
      compass 허용 예외이며 이름으로 격리(CLocalChampionPrediction).
    * local prediction과 snapshot-apply를 같은 owner/파일에 합치지 마라(CLocalChampionPrediction ≠ CInGameNetcode).
    * 로컬 예측/보간을 Shared/GameSim으로 올리지 마라 — Shared가 Client를 include하는 의존성 역전. 두 owner는 Client/ 소유.
    * Client/Public·Shared·EngineSDK/inc에 ID3D11*/IDXGI* 노출 금지. IRHIDevice 추상 유지.

[먼저 읽을 문서 — 순서대로 반드시 읽는다]
1. .md/plan/refactor/17_INGAME_SCENE_STAGE2_OWNER_DESIGN.md  ← OWNER별 소유 상태 목록·흡수 파일·인터페이스·순서(최우선)
2. .md/plan/refactor/13_ATOMIC_DECOMPOSITION_PLAYBOOK.md      ← stateful/static authority 패턴·게이트·GameRoom 레퍼런스
3. .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md       ← Stage 1 맥락·보존해야 할 외부 accessor surface(§7)
4. .md/architecture/WINTERS_CODEBASE_COMPASS.md, CLAUDE_Legacy.md ← 계층 경계·서버 권위
5. CLAUDE.md, .claude/gotchas.md, .md/계획서작성규칙.md       ← 코딩/계획서 규칙(각 owner 직전 계획서를 이 형식으로)

[현재 코드베이스 사실 — 다시 만들지 마라. 라인 번호 변동 가능, rg/Read로 재확인]
- Stage 1 완료: Scene_InGame.cpp 757줄 shell + 8 TU + Scene_InGameInternal.h/.cpp(공유 helper). 빌드 green.
- S2-A 완료: CAmbientProp_Manager(Client/Public/Manager + Client/Private/Manager, DECLARE_SINGLETON) 추출 끝. 다시 만들지 마라. 다음은 OWNER=MAPNAV.
- **신규 Client 매니저/owner 헤더는 맨 앞에 #include "Defines.h"** 를 둔다. ModelRenderer.h의 const wstring&/uint32가 using namespace std(=Defines.h) 전제라 빠지면 미정의 빌드 실패. Minion_Manager.h/AmbientProp_Manager.h 패턴 그대로.
- 각 TU 메서드는 CScene_InGame 멤버 정의이고 scene.m_*를 직접 접근. owner 추출 = 그 멤버 + 메서드를 owner로 이동. presentation owner는 RHI snapshot 경로(AppendRenderSnapshotMeshes)도 흡수할 수 있다(AmbientProp가 예시).
- 참조 패턴(흡수·소비만): GameRoom Stage 2 owner(CLobbyAuthority=stateful, CWorldBootstrap/CWalkabilityAuthority=static),
  Manager 싱글톤(CMinion_Manager: Initialize/Tick/Render/Shutdown), ECS 스케줄러(m_pScheduler->RegisterSystem).
- 외부 UI 패널이 호출하는 scene accessor(15 §7: GetWorld/GetPlayerEntity/GetNavGrid/GetCameraPtr/GetFxMeshRenderer/
  SSAO·Dbg 토글/Get·SetMapRotation/hover·Sylas·hit 게터세터)는 owner로 forward해 유지한다(시그니처 보존).

[OWNER별 작업 범위] (17 문서의 owner 지도 참조 — 한 번에 한 owner)
- AMBIENT     : CAmbientProp_Manager (싱글톤, Minion_Manager 패턴). m_AmbientProps+SpawnMapAmbientProps+update/render/clear 이동. 가장 안전 — 먼저.
- MAPNAV      : CMapNavService. m_pNavGrid/m_pPathNavGrid/m_pMapSurfaceSampler+nav튜닝 소유, Scene_InGameMapNav.cpp 흡수. calc는 입력받아 결과 반환.
- NETCODE     : CInGameNetcode. applier 3종+보간/예측/액션애니/locomotion 상태 소유, Scene_InGameNetwork.cpp 흡수. snapshot-apply 경계.
- PREDICTION  : CLocalChampionPrediction. dash/skill-runtime/flash/로컬sim 시스템 소유, Scene_InGameLocalSkills.cpp 흡수. compass local-only, Shared 금지, NETCODE와 분리.
- INPUT       : CInGameInputController. hover/공격의도/ping 소유, Scene_InGameInput.cpp 흡수. intent 반환 패턴 권장.
- INSTALLER   : CInGameSceneInstaller(static). OnEnter 부트스트랩 흡수, scene/owner에 주입. 시스템 등록을 {phase,factory,condition} 데이터 테이블로. FX Update는 ISystem으로 m_pScheduler 등록.
- RENDERER    : CInGameRenderer. 렌더 리소스 소유, OnRender 위임. 후순위(보존 accessor forward 비용 큼).

[작업 루프 — 완료까지 반복]
1. 상태 파악: 17 문서의 OWNER 소유 상태 목록을 Scene_InGame.h/해당 TU에서 rg로 재확인해 보고.
2. owner 추가: <Owner>.h/.cpp 신규(Client/Public/Scene + Client/Private/Scene). 상태 멤버를 owner로 이동, 메서드를 owner 멤버로 이동.
3. scene 위임: CScene_InGame은 owner를 unique_ptr로 보유. 6 가상함수/accessor가 owner public API로 위임. friend 제거.
   외부 TU(다른 Scene_InGame*.cpp)의 scene.m_x 접근을 scene.owner->x 또는 owner 메서드로 교체.
4. 등록: 새 .h/.cpp를 Client.vcxproj/.filters에 직접 등록.
5. 검증: 빌드(G1) + F5 스모크(G4, 서버 로그 금지) + git diff(G2: 이동/위임/등록만).
6. 커밋: owner 1개 = 커밋 1개. owner 이동과 동작 변경을 분리.
7. 막힌 부분만 사유 분류 보고, 나머지 계속.

[반드시 지킬 규칙]
- owner가 상태 소유, scene은 public API 위임, friend 금지.
- 서버 권위/의존 방향/DX 비노출/utf-8/프로젝트 등록 (13 경계 가드레일과 동일).
- 로컬 예측·보간은 Client 소유 — Shared/GameSim로 올리지 않는다.
- local prediction(PREDICTION)과 snapshot-apply(NETCODE)를 같은 owner에 합치지 않는다.
- 외부 UI accessor 시그니처 보존(owner로 forward).

[금지]
- friend 기반 facade로 되돌아가기(InGame*Bridge 실패 반복).
- owner 이동과 로직/동작 변경을 한 커밋에 혼합.
- 한 번에 여러 owner 동시 추출.
- 진행률 %로 완료 선언 — 게이트(빌드+F5+diff) 통과로만.
- normal F5의 7개 시스템 + 새/오리를 숨겨서 통과시키기.
- EngineSDK/inc 직접 수정 — UpdateLib.bat 동기화로만.

[검증 게이트 — 공통]
G1 빌드: Client.vcxproj Debug x64 통과.
G2 위임 diff: 이동/위임/등록만(이동과 동작변경 분리 커밋).
G3 경계: rg -n "#include .*Server" Client Shared/GameSim ; rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim  → 0.
G4 F5 스모크(서버 로그 금지): roster/map/minion/champion/snapshot/UI/FX/새·오리 미은닉 +
   player/minion/jungle Idle·Run, BasicAttack, SkillQ/W/E/R, Yasuo R·Kalista passive dash·Irelia Q dash, Recall, DeathStart/Dead, cast/recovery FX, cooldown UI 불변.
G5 본질: owner가 한 이유로만 바뀌고 scene은 조립/위임만. friend 0.

[완료 기준 — OWNER DoD]
- 지정 OWNER의 상태/메서드가 owner 클래스로 이동, CScene_InGame은 unique_ptr 보유 + 위임만.
- 다른 Scene_InGame*.cpp의 해당 상태 직접 접근이 owner API 경유로 전환.
- 외부 accessor 시그니처 보존. F5 동작·회귀 불변. friend 미사용.
- (전체 목표) 모든 OWNER 완료 시 CScene_InGame = IScene 6 가상함수 + owner unique_ptr 조립 + 얇은 forward accessor.

[시작]
지금 즉시: (1) 문서 1~3 읽기, (2) 내 OWNER의 소유 상태/메서드를 rg로 집계해 보고,
(3) AMBIENT면 CAmbientProp_Manager부터 owner 추가 → scene 위임 → 등록 → G1/G4 통과 후 커밋.
막히면 (a)재시도/(b)설계상 정상/(c)버그로 분류 보고하고 나머지는 계속.
```

---

## OWNER별 첫 슬라이스 예시

### AMBIENT — ✅ 완료 (참고용, 다시 하지 마라)
```text
완료됨: CAmbientProp_Manager(DECLARE_SINGLETON) 추출. Spawn(mapWorld,mapYaw,projectCb)/Tick/Render/
AppendRenderSnapshotMeshes/Shutdown. scene 위임: OnEnter Spawn, OnUpdate Tick, OnRender Render(+RHI append), OnExit Shutdown.
이 owner가 나머지 S2-B~G의 레퍼런스 — 같은 형태(헤더 맨 앞 Defines.h, owner 상태 소유, scene 위임, friend 0)로 진행.
```

### MAPNAV — 다음 (첫 슬라이스)
```text
OWNER=MAPNAV
CMapNavService.h/.cpp 추가(헤더 맨 앞 #include "Defines.h"). m_pNavGrid/m_pPathNavGrid/m_pMapSurfaceSampler + nav 튜닝을
service로 이동, Scene_InGameMapNav.cpp의 메서드를 service 멤버로. calc(TryResolveWalkableMoveTarget/TryProjectToMapSurface/
IsWalkableMoveSegment/ResolveMouseMapSurfacePos)는 입력받아 결과 반환. scene/다른 TU의 m_pNavGrid 직접 접근을 service API로 교체.
GetNavGrid/GetPathNavGrid accessor(외부 패널·DebugDraw가 호출, 15 §7)는 service로 forward해 시그니처 보존.
빌드 + F5에서 클릭 이동/경로/구조물 차단/미니언 이동 불변 확인.
```

### PREDICTION — 최대 부채 (NETCODE 다음, 분리 유지)
```text
OWNER=PREDICTION
CLocalChampionPrediction.h/.cpp(Client 전용) 추가. Scene_InGameLocalSkills.cpp의 dash/skill-runtime/flash 상태(~30 멤버)와
메서드를 owner로 이동. owner가 예측 transform/애니를 직접 적용(local-only, 이름 격리). Shared로 올리지 마라.
snapshot-apply(CInGameNetcode/Scene_InGameNetwork.cpp)와 절대 합치지 마라. scene은 m_pPrediction 보유, OnUpdate/입력경로가 위임.
빌드 + F5 Yasuo R/Kalista passive dash/Irelia Q dash/플래시/스킬 캐스트 프레임 훅 불변 확인.
```

---

## 검증 명령 (공통)

```powershell
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false
git diff --stat ; git diff --check
rg -n "#include .*Server" Client Shared/GameSim
rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim
```
