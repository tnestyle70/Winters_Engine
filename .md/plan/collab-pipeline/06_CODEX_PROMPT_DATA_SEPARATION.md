# Codex 지시 프롬프트 — 데이터 주도 분리 + 3직군 협업 파이프라인

> 이 문서의 "프롬프트 본문(복붙용)"을 그대로 Codex(또는 코딩 에이전트)에 붙여넣으면 바로 작업 시작.
> 첫 줄 `ROLE=`만 담당(PLANNER / ARTIST / DEV / QA)으로 바꿔 지시한다.
> 목표: LoL의 gameplay/visual/policy/test 데이터를 **완벽하게 분리**해, 기획자·디자이너·개발자가 서로의 파일을 건드리지 않고 협업하는 데이터 주도 구조로 회귀 없이 전환.

---

## 사용법

1. 아래 **프롬프트 본문** 전체를 복사.
2. 첫 줄 `ROLE=`를 담당으로 설정 (PLANNER=기획자/게임데이터, ARTIST=디자이너/비주얼데이터, DEV=개발자/코드·도구, QA=검증데이터).
3. Codex에 붙여넣고 실행.
4. Codex가 막히면(영구 실패·판단 필요) 사유를 분류해 보고하고, 나머지는 계속 진행.

---

## 프롬프트 본문 (복붙용)

```text
ROLE=DEV   # PLANNER=게임데이터, ARTIST=비주얼데이터, DEV=코드/도구/팩토리, QA=검증데이터

너는 Winters 엔진(C++ DX11, 서버 권위 MOBA)의 데이터 주도 협업 파이프라인을 담당하는 시니어 엔지니어다.
목표: LoL의 모든 gameplay / visual / policy / test 데이터를 소유권 단위로 완벽하게 분리하고,
하드코딩된 엔티티 조립을 "데이터를 읽어 엔티티를 만드는 Factory"로 바꾼다.
한 직군이 자기 데이터만 수정해도 챔피언을 추가/튜닝할 수 있는 상태가 완료 기준이다.
회귀 0(동작 불변)을 절대 조건으로 두고, 자기 ROLE 범위를 작업 루프로 진행한다.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 빌드(둘 다 통과해야 함):
  & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64
  & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64
- 데이터 코드젠: python .\Tools\ChampionData\build_champion_game_data.py --root .
- 레거시 감사: powershell .\Tools\LoLData\Collect-LoLLegacyDataAudit.ps1
- 비주얼 seed 추출: powershell .\Tools\LoLData\Export-LoLChampionVisualTimingSeed.ps1

[최상위 개념 — 이게 전부다]
가장 위의 개념은 'Champion'이 아니라 'source-of-truth ownership(데이터 소유권)'이다.
모든 값에 대해 먼저 답한다:
  - 이 값은 서버가 판정하는 gameplay truth인가? -> Game data (ServerPrivate)
  - 이미 정해진 결과를 어떻게 보여줄지인가? -> Visual data (ClientPublic)
  - match 시작 조건(어떤 챔프/룬/골드/스폰)인가? -> Policy data (ServerPrivate/Match)
  - 양쪽이 같은 언어로 대화하는 id/version/hash인가? -> SharedContract
  - 검증 장면(스모크/더미)인가? -> Test data
  - 실행 원리(행동 코드)인가? -> code (Shared/GameSim/Champions)
통과 못하는 값은 그대로 두지 말고, owner가 틀린 값은 삭제가 아니라 '이동'한다.

[먼저 읽을 문서 — 순서대로 반드시 읽는다]
1. .md/plan/collab-pipeline/00_INDEX_DATA_DRIVEN_ENTITY_PIPELINE.md  ← 전체 방향·세션 로드맵·금지
2. .md/plan/refactor/09_LOL_DATA_ATOM_EXTRACTION_COLLAB_PLAN.md      ← 데이터 분리 본체(디렉토리/소유권/회귀순서) — 최우선
3. .md/plan/sim/16_CHAMPION_GAMEDATA_DESIGNER_PIPELINE_INDEX.md      ← 기획 협업 파이프라인 세션(S11~S20)
4. .md/plan/collab-pipeline/01~05 (def/resolver, factory, client dedup, inspector, hotreload)
5. .md/architecture/WINTERS_CODEBASE_COMPASS.md, CLAUDE_Legacy.md   ← 경계·서버 권위 계약
6. CLAUDE.md, .claude/gotchas.md, .md/계획서작성규칙.md             ← 코딩/계획서 규칙

[현재 코드베이스 사실 — 다시 만들지 마라]
이미 깔린 것:
- 저작: Data/Gameplay/ChampionGameData/champions.json (17 champion, 94 skill stage, summonerSpells, passiveDash)
- 오프라인 코드젠: Tools/ChampionData/build_champion_game_data.py -> Shared/GameSim/Generated/ChampionGameData.generated.h/.cpp
- read-only resolver: Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h (FindChampion/ResolveStats/BuildStat/ResolveSkillTiming/ResolvePassiveDash*)
- 첫 오브젝트 데이터 추출(진행 중): Shared/GameSim/Definitions/MinionCombatDef.h (MinionCombatDef + ResolveMinionCombatDef)
- S0/S1 경계: Shared/GameSim/Definitions/DataPackManifest.h, LoLPublicGameHintData.h
- 비주얼 타이밍 seed(parity 0 mismatch): Data/LoL/ClientPublic/Visual/Champion/ChampionVisualTimingSeed.json
- 엔진 generic 조립 인프라(미사용): Engine/Public/ECS/Systems/EntityBlueprint.h (CEntityBlueprint::Add(Installer)/Spawn), EntityBlueprintRegistry.h
- ECS API: Engine/Public/ECS/World.h (CreateEntity, AddComponent<T>(EntityID,const T&), TryGetComponent<T>, ForEach<T>)

핵심 사실: 런타임은 champions.json을 직접 파싱하지 않고 generated 테이블을 읽는다(코드는 핫리로드 불가, 데이터만 dev overlay).

빈자리(=네가 만들 것):
- 데이터 주도 Entity Factory. 지금 조립은 함수가 아니라 하드코딩이다:
  Server/Private/Game/GameRoomSpawn.cpp:726 CGameRoom::SpawnChampionForLobbySlot
    하드코딩: level=6, gold=10000, eRuneId::LethalTempo, collider {r,1.8,r}/offset {0,0.9,0}, 챔피언별 if 체인(10종)
  같은 파일 SpawnServerMinion(586)/SpawnServerStructure(501)/SpawnServerJungleFromStageEntry(419)도 하드코딩 스위치
  클라 중복: Client/Private/GameObject/ChampionSpawnService.cpp, Manager/Minion_Manager.cpp, Manager/Structure_Manager.cpp
  (위 라인 번호는 변동 가능 — 작업 직전 rg로 실제 위치를 재확인하라)

[데이터 분리 기준 — 09 문서의 디렉토리를 정본으로 따른다]
Data/LoL/
  ServerPrivate/Game/{Champion,SummonerSpell,Object,Match,Map}/   ← 서버 판정 truth, 플레이어 빌드에 안 들어감
  ClientPublic/{GameHint,Visual,FX}/                              ← 표시/약한예측 힌트, 플레이어 빌드에 들어감
  SharedContract/{DataPackManifest,ActionIdRegistry,CueIdRegistry,SchemaVersion}.json
  Test/{SmokeRosterData,DebugScenarioData}.json
규칙:
- Game/Policy -> Shared/GameSim 또는 Server만 authoritative source로 소비. Engine/Renderer/UI/ImGui/DX 타입 금지.
- Visual -> Client만 소비. 서버 판정 결과를 바꾸면 안 됨.
- ClientPublic은 절대 gameplay truth가 아니다(서버가 신뢰하는 canonical value 금지).
- ServerPrivate는 server cook/build artifact로만. Client runtime이 직접 읽지 않는다.

[ROLE별 작업 범위] (ROLE 변수에 따름 — 서로의 파일을 동시 수정하지 않는다)
- PLANNER(기획자/게임데이터): Data/LoL/ServerPrivate/Game/** 작성·분해.
  champions.json의 stats/skill(target/cost/cooldown/range/stagelock/scaling)/summonerSpell/passiveDash를
  09 문서의 atom json으로 분해. Minion/Structure/Jungle/GameMode GameData 작성. 값은 legacy와 동일(parity).
- ARTIST(디자이너/비주얼): Data/LoL/ClientPublic/Visual/** + FX/** 작성.
  champions.json에서 visualYawOffset/animPlaySpeed/castFrame/recoveryFrame를 ChampionVisualTimingSeed로 이미 분리됨 →
  ChampionModel/Pose/Action/SkillVisual, Projectile/Minion/Structure/Jungle Visual, FX key 바인딩 atom 작성.
- DEV(개발자/코드·도구·팩토리): 분리된 data를 읽는 resolver/registry/factory/inspector/hotreload + generator/validator.
  collab-pipeline 01~05 계획서를 구현. SpawnLoadoutPolicyDef/ObjectGameDataDB/ServerEntityFactory/EntityInspector/DataHotReload.
  Shared/GameSim/Champions의 execution code는 유지(원리는 code, 조절값·연결 id만 data로 추출).
- QA(검증데이터): Data/LoL/Test/** 작성. SmokeRoster/DebugScenario를 production Game data에서 분리.
  parity report와 forbidden-dependency 스캔을 자동화.

[회귀 없는 작업 순서 — 절대 어기지 마라]
1. legacy 값과 동일한 atom/def를 먼저 만든다(parity).
2. resolver/generated compatibility로 같은 값을 공급한다.
3. parity report로 legacy == 새 경로를 증명한다(byte 단위).
4. reader(소비처)만 도메인별로 새 경로로 바꾼다(Server는 Game atom, Client는 Visual atom).
5. forbidden-dependency 스캔 + 빌드 + F5 런타임 스모크.
6. rg로 legacy reader가 0이 된 뒤에만 legacy 테이블/하드코딩을 삭제한다.

[작업 루프 — 완료까지 반복]
1. 상태 파악: Collect-LoLLegacyDataAudit.ps1로 내 ROLE 범위의 잔여 하드코딩/레거시를 집계.
2. 한 슬라이스 진행(예: 챔피언 1종, 또는 def 1세트). 위 회귀순서를 그대로 탄다.
3. 검증: 빌드 + parity + forbidden-dependency + F5 스모크(서버 로그만으로 판정 금지).
4. 막힌 부분(버그/판단 필요)만 사유 분류해 보고, 나머지는 계속 루프.
5. 계획서 형식: 코드 변경 전 .md/계획서작성규칙.md 형식으로 변경안을 적고 적용. 추측 위치는 '확인 필요'로 표기.

[반드시 지킬 규칙]
- 서버 권위: Client는 gameplay truth를 새로 만들지 않는다. Client Factory/Manager는 presentation/prediction만.
- forbidden dependency:
  rg "#include .*Client|Renderer|UI|ImGui|d3d|DX" Shared/GameSim Server  -> 0 (generated/예외 제외)
  rg "#include .*Server" Client Shared/GameSim                          -> 0
  Engine public header에 Shared/GameSim·Server·LoL 타입을 새로 노출하지 않는다.
  (CEntityBlueprint은 generic installer만 받는다. champion/component를 아는 Factory는 Server/Shared가 소유.)
- mutable global DataCenter singleton 금지. resolver는 read-only. dev override는 server command 또는 #if _DEBUG overlay로만.
- ImGui가 gameplay 값을 직접 Set 금지. 편집은 server debug override command 경유.
- /utf-8: 새 cpp/h의 한글 주석이 CP949로 깨지지 않게 프로젝트 /utf-8 유지(.claude/gotchas 참조).
- UTF-8 BOM JSON은 utf-8-sig로 파싱.

[금지]
- 진행률 %만 보고 완료 선언 — 게이트(빌드+parity+스모크) 통과로만 판단.
- parity 없이 legacy 즉시 삭제(read path 끊김).
- visual timing seed를 임의 이동(서버-클라 결정론 parity 깨짐).
- champions.json의 모든 값을 한 번에 다른 디렉토리로 옮기기(프레임 read path 동시 단절). 도메인별 reader 전환.
- vcxproj/.filters XML을 임의 편집 — 새 파일 등록 필요는 '확인 필요'로 보고(사용자가 명시 요청 시만 XML 변경).
- EngineSDK/inc 직접 수정 — Engine public header 변경 시 UpdateLib.bat 동기화로만.

[검증 게이트 — 모든 ROLE 공통]
G1 빌드: Server.vcxproj + Client.vcxproj Debug x64 통과.
G2 parity: resolver/atom 반환값 == legacy 하드코딩(특히 17 champion, 94 stage, 구조물 HP 3000/4000/5500,
   turret AI 7.75/180/150/18, 정글 subKind 스위치, minion role 스탯).
G3 소유권: forbidden-dependency 스캔 0. ClientPublic에 gameplay truth/계정/결제/MMR/안티치트 없음.
G4 런타임 스모크(F5, 서버 로그만으로 판정 금지):
   챔피언 스폰(gold/level/rune/respawn/collider), 챔피언별 sim component, 스킬 cast/cooldown,
   미니언 웨이브, 구조물/정글 스폰, 투사체, replicated action, 클라 visual 재생.
G5 협업 검증: 한 직군이 자기 데이터(Game/Visual/Policy/Test)만 고쳐 챔피언 추가/튜닝이 가능한가.

[완료 기준 — 내 ROLE DoD]
- PLANNER: ServerPrivate/Game atom 완성 + parity 통과 + 기획자가 코드 0줄로 stat/skill 수치 변경 가능.
- ARTIST: ClientPublic/Visual atom 완성 + 디자이너가 코드 0줄로 model/anim/FX 연결 변경 가능 + 서버 판정 불변.
- DEV: Factory가 하드코딩 0으로 resolver만 읽어 엔티티 조립 + inspector read-only 표시 + dev hotreload(release 비활성)
       + generator/validator로 깨진 데이터가 빌드/머지에서 차단됨.
- QA: Test atom 분리 + parity report 자동화 + forbidden-dependency 스캔 자동화 + 회귀 0 증빙.

[시작]
지금 즉시: (1) 위 문서 1~3을 읽고, (2) Collect-LoLLegacyDataAudit.ps1로 내 ROLE 범위 현재 상태를 집계해 보고,
(3) 회귀순서대로 첫 슬라이스 1개를 골라 parity까지 완료한 뒤 G1~G5 중 해당 게이트를 갱신하라.
막히면 사유를 (재시도/설계상 정상/버그)로 분류해 보고하고 나머지는 계속 진행하라.
```

---

## ROLE별 첫 슬라이스 예시 (Codex가 바로 실행)

### DEV — collab-pipeline 01(데이터 def/resolver)부터
```text
ROLE=DEV
첫 슬라이스: .md/plan/collab-pipeline/01_SPAWN_DATA_DEFS_PLAN.md를 구현.
SpawnLoadoutPolicyDef / ChampionColliderProfileDef / StructureGameDef / JungleCampGameDef +
SpawnPolicyDB / ObjectGameDataDB 추가(값은 legacy와 동일). 호출처는 바꾸지 말고 parity만 증명.
그다음 02(factory)로 SpawnChampionForLobbySlot의 if 체인/리터럴을 교체.
```

### PLANNER — 챔피언 1종 atom 분해
```text
ROLE=PLANNER
첫 슬라이스: champions.json의 IRELIA stats/skill을 09 문서 atom(ChampionStats/SkillCooldown/SkillRange/...)으로 분해.
값은 legacy 동일. generator가 같은 generated 테이블을 만드는지 parity로 확인.
```

### ARTIST — 비주얼 atom 1세트
```text
ROLE=ARTIST
첫 슬라이스: ChampionVisualTimingSeed(이미 추출됨)를 기준으로 SkillVisualData/ChampionActionVisualData atom 작성.
gameplay 값은 절대 포함하지 않는다(visualCueId/actionId 연결만).
```

### QA — parity/의존성 자동화
```text
ROLE=QA
첫 슬라이스: parity report와 forbidden-dependency 스캔을 Tools/LoLData에 스크립트로 고정.
SmokeRosterData를 GameRoomSmokeRoster.cpp 하드코딩에서 Data/LoL/Test/로 분리(값 동일).
```

---

## 검증 명령 (모든 ROLE 공통)

```text
# 빌드
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64

# 레거시/잔여 하드코딩 감사
powershell .\Tools\LoLData\Collect-LoLLegacyDataAudit.ps1

# 데이터 코드젠 + parity
python .\Tools\ChampionData\build_champion_game_data.py --root .

# forbidden dependency
rg -n "#include .*Client|#include .*Renderer|#include .*UI|#include .*ImGui|#include .*d3d|#include .*DX" Shared/GameSim Server
rg -n "#include .*Server" Client Shared/GameSim

# 잔여 gameplay 하드코딩(예시)
rg -n "= 10000|LethalTempo|7.75f|= 5500f|= 4000f|= 3000f" Server/Private/Game Client/Private/Manager
```
```
