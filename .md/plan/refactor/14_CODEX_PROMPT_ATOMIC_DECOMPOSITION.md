# Codex 지시 프롬프트 — God-file 본질 분해 (Atomic Decomposition)

> 이 문서의 "프롬프트 본문(복붙용)"을 그대로 Codex(또는 코딩 에이전트)에 붙여넣으면 바로 작업 시작.
> 첫 줄 `ROLE=`만 담당으로 바꿔 지시한다.
> 목표: God-file(한 파일이 여러 본질을 가진 거대 파일)을 북극성(Essence Boundary)에 따라 **회귀 0으로** 본질 단위로 쪼갠다. 방법은 `GameRoom.cpp`에서 검증된 2단계(Stage 1 verbatim 분리 -> Stage 2 원자 owner 추출).

---

## 사용법

1. 아래 **프롬프트 본문** 전체를 복사.
2. 첫 줄 `ROLE=`를 담당으로 설정 (SERVER_ATOMIZE / CLIENT_SCENE / CLIENT_MANAGERS / SHARED_SYSTEMS / ENGINE_RHI / ENGINE_DEPRODUCT).
3. Codex에 붙여넣고 실행.
4. Codex가 막히면(영구 실패·판단 필요) 사유를 분류해 보고하고, 나머지는 계속 진행.

---

## 프롬프트 본문 (복붙용)

```text
ROLE=CLIENT_SCENE   # SERVER_ATOMIZE / CLIENT_SCENE / CLIENT_MANAGERS / SHARED_SYSTEMS / ENGINE_RHI / ENGINE_DEPRODUCT

너는 Winters 엔진(C++ DX11, 서버 권위 MOBA)의 구조 정리를 담당하는 시니어 엔지니어다.
목표: God-file(한 파일이 여러 본질을 가진 거대 파일)을 북극성(Essence Boundary)에 따라 본질 단위로 쪼갠다.
방법은 Server/Private/Game/GameRoom.cpp에서 이미 검증된 2단계다.
회귀 0(동작 불변)을 절대 조건으로 두고, 자기 ROLE 범위를 작업 루프로 진행한다.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 빌드(자기 ROLE target만, Engine 미커밋 변경으로 전체 리빌드가 깨질 수 있어 BuildProjectReferences=false 권장):
  & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Server/Include/Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false
  & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false
- 서버 스모크: .\Server\Bin\Debug\WintersServer.exe --smoke-seconds=10
- 한글 주석 파일을 읽을 때는 UTF-8로 읽어 mojibake를 진짜 에러로 오판하지 마라.

[최상위 개념 — 이게 전부다]
가장 위의 개념은 'God-file을 쪼개기'가 아니라 'source-of-truth ownership(본질 소유권)'이다.
한 파일/모듈/폴더는 하나의 본질만 가진다. 파일 안 각 함수 묶음에 먼저 묻는다:
  - 서버가 판정하는 gameplay truth인가?            -> Server / Shared
  - 이미 정해진 결과를 보여주는 presentation인가?  -> Client
  - transport / adapter(packet send, session bind)인가?
  - authoring / editor(save/load, placement)인가?
  - debug / lab / tuner인가?
답이 다르면 다른 파일로 쪼갠다. owner가 틀린 코드는 삭제가 아니라 '이동'한다.
"한 코드가 두 가지 이유로 바뀐다면 아직 덜 쪼갠 것이다."

[2단계 방법론 — 반드시 이 순서, 한 번에 둘 다 하지 않는다]
Stage 1 (기계적 본질 분리, 동작 불변):
  - God-file을 GameRoomXxx.cpp 패턴으로 책임별 .cpp로 쪼갠다. 함수 본문은 라인 단위 그대로 이동(verbatim).
  - public 클래스 선언(.h)은 건드리지 않는다. cpp 정의만 이동한다(멤버 선언은 이미 헤더에 있다).
  - 시그니처/동작/호출순서 불변. 두 파일 이상이 공유하는 anonymous namespace 헬퍼/상수는 XxxInternal.h로 승격(중복정의 드리프트 차단).
  - 이동과 로직 수정을 같은 커밋에 섞지 않는다. diff는 순수 이동이어야 한다.
Stage 2 (원자 owner 추출, 상태 소유권 이동):
  - God-class를 orchestration shell로 줄이고, 상태/규칙을 원자 owner 클래스로 추출한다.
  - owner는 side effect를 실행하지 않고 XxxResult를 반환한다. packet send/spawn bootstrap/replay stop은 shell이 결과를 받아 처리.
  - 반드시 Stage 1으로 seam이 드러난 뒤에 한다.

[먼저 읽을 문서 — 순서대로 반드시 읽는다]
1. .md/plan/refactor/13_ATOMIC_DECOMPOSITION_PLAYBOOK.md   ← 방법론·대상지도·경계가드레일·게이트(최우선)
2. .md/plan/refactor/00_ESSENCE_BOUNDARY_REFACTOR.md       ← 북극성·본질판정·S0~S11
3. .md/plan/refactor/03_SERVER_GAMEROOM_ATOMIC_REFACTOR_PLAN.md ← Server Stage 2 정본(LobbyAuthority 패턴)
4. .md/architecture/WINTERS_CODEBASE_COMPASS.md, AGENTS.md, CLAUDE_Legacy.md ← 계층 경계·서버 권위
5. CLAUDE.md, .claude/gotchas.md, .md/계획서작성규칙.md   ← 코딩/계획서 규칙

[현재 코드베이스 사실 — 다시 만들지 마라]
- GameRoom.cpp는 원래 GameRoomTick/Commands/Replication/ChampionAI로만 부분 분리돼 있었고, Phase 0~5(commit 2c14a15)가 그 패턴을 같은 규율로 연장해
  GameRoomLobby / GameRoomNav / GameRoomSpawn / GameRoomMinionAI / GameRoomProjectiles + 공유 GameRoomInternal.h/.cpp + GameRoomSmokeRoster.h/.cpp를 추가했다.
  GameRoom.cpp는 403줄 shell로 축소됨. 이게 Stage 1의 레퍼런스 구현이다.
- GameRoom Stage 2는 03 계획이 정본(CLobbyAuthority부터, owner는 LobbyAuthorityResult 반환).
- .vcxproj는 명시적 ClCompile 목록(glob 아님). /utf-8 /FS는 프로젝트 전역. (작업 직전 rg/Read로 실제 라인 재확인.)

[너의 ROLE 작업 범위] (ROLE 변수에 따름 — 서로의 파일을 동시 수정하지 않는다)
- SERVER_ATOMIZE: GameRoom Stage 2. 03 계획대로 CLobbyAuthority부터 원자 owner 추출
  (WalkabilityAuthority / WorldBootstrap / ServerAICommandProducer / ReplicationEmitter / ServerProjectileAuthority).
- CLIENT_SCENE: Client/Private/Scene/Scene_InGame.cpp(6,588줄) Stage 1.
  seam: Scene_InGameLifecycle / Network / LocalSkills / Input / Render / ImGui + Scene_InGameInternal.h.
  핵심 주의: ApplyLocalPrediction/StartLocal*Dash/UpdateLocal*는 compass 허용 local-only prediction이다.
  snapshot-apply(OnAuthoritativeSnapshot/ApplyNetworkActorInterpolation)와 같은 파일에 섞지 마라(최대 경계위반).
  L138 anonymous namespace 공유 헬퍼는 Scene_InGameInternal.h로 승격(이게 최대 정확성 함정).
  Scene_InGame 완료 후 같은 ROLE에서 client network-apply God-file도 분리:
  SnapshotApplier.cpp(1,461 — EnsureEntity/OnHello/yaw-protect를 OnSnapshot apply에서 분리), EventApplier.cpp(1,193 — action/projectile/effect/combat event family별 applier + OnEvent dispatcher).
- CLIENT_MANAGERS: Minion_Manager.cpp(1,670, 우선)/Structure_Manager.cpp(437, 우선순위 낮음)/Jungle_Manager.cpp(590, 우선순위 낮음) Stage 1.
  주의: Minion_Manager::Tick은 client lab 시뮬 — 단일 verbatim unit으로 유지(m_vecSpawnedThisTick clear/read 순서).
  Structure/Jungle은 runtime vs authoring(save/load/add) seam 2분할. ChampionTable.cpp(149)는 분리하지 않는다(단일 책임).
  공유 anon helper는 Minion_ManagerInternal.h로 승격.
- SHARED_SYSTEMS: CommandExecutor.cpp(2,586)/ChampionAISystem.cpp(2,555) Stage 1.
  CommandExecutor: Handle* 핸들러별 TU(move/cast/attack/economy/recall/flash) + wire-decode 분리, dispatch는 routing shell.
  ChampionAISystem: Targeting/Siege/Context/Emitter/Combo/DebugTrace 헬퍼 분리, Execute()는 얇은 loop.
  Shared/GameSim는 Engine/Client/Renderer/UI/ImGui/DX include 금지를 절대 유지.
- ENGINE_RHI: DX12Device.cpp(2,006)/CDX11Device.cpp(1,291) Stage 1.
  resource factory(buffer/texture+sampler/shader/pipeline/bind group) TU 분리. device bring-up + frame/sync loop는 core 유지.
- ENGINE_DEPRODUCT: (고난도 — mechanical split 아님) UI_Manager.h/GameplayComponents.h/MinionAISystem.h/TurretAISystem.h/
  TurretProjectileSystem.h/BTNodes_Champion.h/FxMaterialDesc.h/GameInstance.h의 LoL 제품의미(eChampion/Champion/Minion/Turret/HUD/shop/LOL*Mode)를
  Engine -> Shared/GameSim/Client로 owner 이동. 의존 방향이 바뀌므로 04_INGAME_CHAMPION 계획의 dual-write 회귀방지로 접근.
  이 ROLE은 verbatim 이동이 아니다. 새 타입 추가 -> dual-write -> parity -> reader 전환 -> 삭제 순서로 간다.

[작업 루프 — 완료까지 반복]
1. 상태 파악: 내 ROLE God-file을 열어 함수 목록을 table-of-contents처럼 읽고 본질 클러스터로 분류해 보고.
2. 한 seam 진행: 경계 라인을 assert로 확인 -> 함수 본문 verbatim 이동 -> 공유 helper는 Internal.h -> 새 .cpp/.h를 vcxproj/.filters에 등록.
3. 검증: 빌드 + F5 스모크(서버 로그만으로 판정 금지) + git diff로 순수이동 확인.
4. 커밋: seam 1개 = 커밋 1개. 이동과 로직수정을 분리.
5. 막힌 부분(버그/판단 필요)만 사유 분류해 보고, 나머지는 계속 루프.

[반드시 지킬 규칙]
- 서버 권위: Client는 gameplay truth를 새로 만들지 않는다. local-only prediction/lab path는 이름으로 격리하고 snapshot-apply와 섞지 않는다.
- 의존 방향: Shared/GameSim는 Engine/Client/Renderer/UI/ImGui/DX include 금지. Server는 Client visual 의존 금지. Engine은 LoL/Server/GameSim 제품 타입을 새로 노출하지 않는다.
- DX 비노출: Client/Public 또는 Shared 헤더에 ID3D11*/IDXGI* 노출을 넓히지 않는다. IRHIDevice 추상을 유지한다.
- 공유 anon helper 중복정의 금지 -> XxxInternal.h로 단일화.
- /utf-8: 새 cpp/h의 한글 주석이 CP949로 깨지지 않게 프로젝트 전역 /utf-8에 의존. per-file AdditionalOptions로 /utf-8을 떨어뜨리지 않는다.
- 프로젝트 등록: 새 split .cpp/.h의 vcxproj/.filters 등록은 분해의 필수 단계로 직접 수행한다(미등록 시 링크 단계에서 함수 증발). 그 외 XML 구조는 건드리지 않는다.

[금지]
- Stage 1과 Stage 2를 한 커밋에 섞기.
- 이동과 로직 수정을 한 커밋에 섞기.
- Stage 1에서 public .h 시그니처를 바꾸기.
- EngineSDK/inc 직접 수정 — Engine public header 변경 시 UpdateLib.bat 동기화로만.
- 진행률 %만 보고 완료 선언 — 게이트(빌드+스모크+순수이동) 통과로만 판단.
- normal F5 roster/map/minion/champion/snapshot/UI/FX를 숨겨서 분리를 통과시키기.
- ChampionTable.cpp처럼 단일 책임·소형 파일을 의미 없이 쪼개기.

[검증 게이트 — 모든 ROLE 공통]
G1 빌드: 내 ROLE target Debug x64 통과.
G2 순수 이동: git diff가 이동/등록만(이동과 로직수정 분리 커밋).
G3 경계: forbidden-dependency 스캔 0(분리로 새 위반 없음).
   rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim Server
   rg -n "#include .*Server" Client Shared/GameSim
G4 런타임 스모크(F5, 서버 로그만으로 판정 금지): roster/map/minion/champion/snapshot/UI/FX 7개 미은닉.
G5 본질: 분리 후 각 파일이 한 가지 이유로만 바뀌는가.

[완료 기준 — 내 ROLE DoD]
- SERVER_ATOMIZE: CGameRoom이 RoomLifecycle+AuthorityOrchestration shell로 줄고, lobby/walkability/bootstrap/AI/replication/projectile이 원자 owner로 추출됨. owner는 Result 반환, side effect는 shell. F5 동작 불변.
- CLIENT_SCENE: Scene_InGame.cpp가 얇은 scene shell로 줄고, network/local-skill/input/render/imgui가 책임 파일로 분리됨. local-prediction과 snapshot-apply가 다른 파일. F5 7개 시스템 불변.
- CLIENT_MANAGERS: Minion/Structure/Jungle manager가 책임 파일로 분리, Tick 단일 unit 유지, 웨이브 스폰/이동/시각화/저장 동작 불변.
- SHARED_SYSTEMS: CommandExecutor/ChampionAISystem이 handler/helper TU로 분리, dispatch/Execute는 얇은 shell. Shared 의존 위반 0. 시뮬레이션 동작 불변.
- ENGINE_RHI: DX12/DX11 device가 core + resource factory로 분리, 렌더 동작 불변.
- ENGINE_DEPRODUCT: Engine public API에서 LoL/Server 제품 타입이 빠지고 owner가 Shared/GameSim/Client로 이동, parity 통과, F5 불변. (별도 세션 산출물.)

[시작]
지금 즉시: (1) 문서 1~3을 읽고, (2) 내 ROLE God-file을 함수목록으로 본질 클러스터 분류해 보고,
(3) 첫 seam 1개를 Stage 1로 분리해 G1/G4를 통과시킨 뒤 커밋하라.
막히면 사유를 (재시도/설계상 정상/버그)로 분류해 보고하고 나머지는 계속 진행하라.
```

---

## ROLE별 첫 슬라이스 예시 (Codex가 바로 실행)

### CLIENT_SCENE — Scene_InGame ImGui/debug seam부터 (가장 독립적)
```text
ROLE=CLIENT_SCENE
첫 슬라이스: Scene_InGame.cpp의 OnImGui/DrawReplayControlPanel 등 debug/replay UI 묶음을
Scene_InGameImGui.cpp로 verbatim 분리. ImGui/debug 패널 17종 include도 함께 이동.
공유 anon helper는 Scene_InGameInternal.h로 승격. 호출처/순서 불변. 빌드 + F5 패널 표시 확인.
다음 슬라이스: Network(snapshot apply/interp) seam을 Scene_InGameNetwork.cpp로. local-prediction은 섞지 않는다.
```

### SERVER_ATOMIZE — GameRoom Stage 2 LobbyAuthority부터
```text
ROLE=SERVER_ATOMIZE
첫 슬라이스: 03_SERVER_GAMEROOM_ATOMIC_REFACTOR_PLAN.md대로 CLobbyAuthority.h/.cpp 추가.
GameRoomLobby.cpp의 lobby rule 판단(slot/ready/start/bot edit)을 CLobbyAuthority로 이동,
owner는 LobbyAuthorityResult만 반환. packet send/hello/game start는 GameRoomLobby transport adapter에 남긴다.
빌드 + lobby join/leave/champion select/start가 기존과 같은 packet을 보내는지 확인.
```

### CLIENT_MANAGERS — Minion_Manager Tick/Visual 분리
```text
ROLE=CLIENT_MANAGERS
첫 슬라이스: Minion_Manager.cpp의 시각화 묶음(UpdateMinionVisual/TickVisuals/Render/network visual pool)을
Minion_ManagerVisual.cpp로 verbatim 분리. Tick(spawn 시뮬)은 단일 unit으로 그대로 둔다.
공유 anon helper/constexpr는 Minion_ManagerInternal.h로 승격. 빌드 + F5 미니언 웨이브 스폰/이동/애니 확인.
```

### SHARED_SYSTEMS — CommandExecutor handler 분리
```text
ROLE=SHARED_SYSTEMS
첫 슬라이스: CommandExecutor.cpp의 HandleMove/HandleBasicAttack를 각각 TU로 verbatim 분리,
ExecuteCommand는 dispatch shell로 유지. wire-decode(BuildServerCommand)도 별도 파일.
Shared 의존 위반 0 확인. 빌드 + 서버 스모크 + (가능하면) SimLab golden case 동작 불변 확인.
```

### ENGINE_RHI — DX12Device resource factory 분리
```text
ROLE=ENGINE_RHI
첫 슬라이스: DX12Device.cpp의 buffer 리소스 팩토리(CreateBuffer/DestroyBuffer/GetBufferNativeHandle)를
DX12BufferFactory.cpp로 verbatim 분리. device bring-up + frame/sync loop는 core에 유지.
빌드 + 렌더 동작 불변 확인. 다음 슬라이스: texture/sampler, shader, pipeline, bind group 순.
```

---

## 검증 명령 (모든 ROLE 공통)

```powershell
# 빌드 (자기 ROLE target만, Engine 리빌드 회피)
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Server/Include/Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false

# 순수 이동 확인 (--check는 whitespace 에러만 검출 — 이동 vs 로직수정 분리는 --stat + 내용 리뷰로 판단)
git diff --stat
git diff --check

# 서버 스모크
.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=10

# forbidden dependency
rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim Server
rg -n "#include .*Server" Client Shared/GameSim
```
