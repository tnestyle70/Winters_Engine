# S013 Server State / Humanlike Bot Foundation Validation

검증일: 2026-07-12

## 범위

- BasicAttack CombatAction과 ActionState의 generation 충돌 차단
- 아군 외곽 포탑 파괴 후 PlayerLike bot의 미드 방어 macro
- home lane과 active lane 분리
- 생존 미드 포탑, Nexus 방향, EntityID formation 기반 집결 위치
- Snapshot AI debug flags와 Client F9 Brain/Macro 표시
- 실제 Champion AI -> GameCommand -> CommandExecutor -> MoveSystem 결정론 probe

## 자동 검증 결과

### Shared/GameSim

명령:

~~~text
MSBuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
~~~

결과:

~~~text
PASS
Shared boundary audit PASS
WintersGameSim.lib 생성
~~~

### SimLab

명령:

~~~text
MSBuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
Tools/Bin/Debug/SimLab.exe 1800 42
~~~

결과:

~~~text
PASS [ActionGeneration]
same-tick Kalista BasicAttack -> E -> Move preserves Move
stale BasicAttack impact discarded after action replacement
target HP and DamageRequest remain unchanged

PASS [ChampionAI][MidDefense]
deterministic hash=CBC15147CFE16804
home lane preserved
PlayerLike bot grouped mid
active combo commitment gate preserved
allied wave does not pull defenders out of formation
cross-lane turret danger forces retreat
last mid turret loss falls back to Nexus formation

PASS full SimLab
same-seed replay hash=DB0DC85E451999AD
seed+1 hash=57A9B2394575042A
~~~

### Server

명령:

~~~text
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
~~~

결과:

~~~text
PASS
Server/Bin/Debug/WintersServer.exe 생성
기존 C4275 DLL-interface warning만 관측
~~~

### Client

명령:

~~~text
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
~~~

결과:

~~~text
PASS
Client/Bin/Debug/WintersGame.exe 생성
기존 C4251/C4275 DLL-interface warning만 관측
~~~

### 전체 Bot AI harness

명령:

~~~text
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Debug
~~~

결과:

~~~text
PASS
ChampionAI dependency boundary
Yone E stage-2 contract
LoL data-driven pipeline
SimLab regression
~~~

생성 보고서:

~~~text
.md/build/2026-07-12_170939_BOT_AI_VALIDATION_HARNESS_REPORT.md
~~~

### Diff hygiene

명령:

~~~text
git diff --check
~~~

결과:

~~~text
PASS (exit 0)
기존 working-copy LF -> CRLF 경고만 출력
~~~

## 코드에서 고정된 불변식

1. BasicAttack 시작 시 CombatAction이 현재 ActionState sequence를 소유 generation으로 기록한다.
2. impact 전뿐 아니라 command-level Move 소비 전에도 ActionState generation을 검증한다.
3. bot macro는 hard safety, action lock, debug override, Kalista 계약, retreat 처리 뒤에서 평가된다.
4. active combo 또는 dive가 있으면 미드 방어로 전환하지 않는다.
5. macro는 lane을 덮어쓰지 않고 activeLane만 Mid로 바꾼다.
6. AI gameplay 결과는 기존 GameCommand 경로로만 적용한다.
7. 외곽 포탑/미드 앵커 구조물 scan은 30 Hz 매 tick이 아니라 AI decision cadence에서만 갱신한다.
8. turret safety는 objective lane이 아니라 현재 위치 기준 모든 적 포탑을 검사한다.
9. macro objective는 bMidDefenseActive에, tactical state/intent는 LaneCombat/Attack/Retreat 축에 분리한다.

## 수동 인게임 확인 필요

- F9 Selected Champion AI에서 Brain=PlayerLike, Macro=HomeLane/DefendMid 표시
- 아군 외곽 포탑 파괴 후 안전한 bot의 GroupMidDefense 전환
- combo 중에는 완료 후 전환
- 여러 bot이 생존 미드 포탑 뒤쪽에 EntityID formation으로 분산
- LAN의 다른 Client에서도 같은 위치/state snapshot 표시

## 남은 구조 부채

- CastSkill의 Validate -> SkillExecutionPlan -> Commit 원자화
- team visibility mask 기반 Observation과 last-seen/confidence/ETA memory
- StatusEffectComponent와 legacy CC truth 통합
- TransitionJournal 및 first-divergence 계층 로그
- 웨이브/귀환/구매/정글/시야/텔레포트 macro utility
- 포탑 entity를 즉시 제거하는 정책이 도입될 경우 match-level destroyed objective fact
