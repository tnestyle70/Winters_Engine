Session - 비에고 W 사거리 보정·원거리 미니언 사거리 F4·챔피언 AI 라인 정체 복구와 선택 봇 실시간 튜너 결과

계획: `2026-07-19_VIEGO_W_MINION_RANGE_AI_LIVE_TUNER_STALL_PLAN.md`

## 1. 결과 상태

- 상태: `COMPLETE_WITH_MANUAL_QA_REMAINING`
- 독립 계획 비평: `PASS`, 미해결 P0/P1 없음.
- 자동 검증: Debug/Release 전체 파이프라인, AI 전용 SimLab, F4 계약, definition pack, 10-bot SnapshotBuilder probe, Services Go 테스트, 최종 diff 검사 통과.
- 남은 확인: 실제 Debug F5에서 F9 드래그·더블클릭·서버 echo와 새 ranged minion 사거리 hot-load를 사람이 화면으로 확인하는 수동 QA. 자동 검증을 수동 화면 검증으로 과장하지 않는다.

## 2. 최종 동작

### 2.1 비에고 W

- 과도했던 Viego W 최대 charge range `12.5`를 원래 authored 값 `5.0`으로 복구했다.
- 최소 charge scale 0.5는 유지되므로 최소 이동 거리는 `2.5`, 최대 이동 거리는 `5.0`이다.
- definition pack을 canonical JSON에서 다시 생성했다.
- SimLab은 5.0/2.5를 새 고정 단언으로 만들지 않고 현재 pack의 `rangeMax`와 charge scale에서 기대 이동 거리를 계산해, F4 편집값과 실제 transform 도착점이 일치하는지 검증한다.

### 2.2 F4 원거리 미니언 공격 사거리

- `Minions -> Ranged -> Attack Range`를 F4 drag/double-click 편집 필드로 추가했다.
- 허용 범위는 `0.1..100.0`, drag speed는 `0.1`이다.
- `Save & Hot Load` 성공 시 canonical JSON 저장과 runtime definition reload 뒤, 현재 살아 있는 lane minion의 `MinionStateComponent.attackRange`도 새 definition 값으로 갱신된다.
- 이 경로는 F4의 기존 HP/AD와 같은 영구 authoring 경로다. JSON에 저장되며 이후 cook/빌드 입력으로도 남는다.

### 2.3 LeeSin 및 라인 봇 정체 원인과 복구

기존 정체의 직접 원인은 aggression 수치가 아니었다. `MoveToOuterTurret`/`WaitForWave` 상태가 다음 중 하나일 때만 `LaneCombat`으로 전환했다.

- 적 챔피언이 보임.
- 아군 웨이브가 고정 `waveJoinRange = 8.0` 안에 있음.

따라서 봇이 공격 가능한 적 미니언을 이미 인지하거나 8m 밖의 아군 웨이브를 찾았어도 상태 전이문을 통과하지 못했다. Debug 표면에는 `FarmMinion`이 보이지만 executor는 `MoveToSafeAnchor`를 유지할 수 있었고, 챔피언 target이 없다는 이유만으로 `NoTarget`도 잘못 표시됐다.

수정 결과:

- 보이는 적 미니언이 있으면 앵커 상태에서 `LaneCombat`으로 들어간다.
- 아군 웨이브 탐색은 고정 8m가 아니라 선택 봇의 `Follow Wave Search` 유효값을 사용한다.
- 찾은 아군 웨이브와 거리를 authoritative evidence로 기록한다.
- `Farm Priority`가 실제 farm utility 가중치에 연결된다.
- `NoTarget`은 target이 필요한 실행이 실제 실패했을 때만 남고, 이후 명령 성공 시 지워진다.
- 강제 AttackMinion/Champion/Structure에서 target이 없을 때는 정확한 `NoTarget`을 유지한다.

### 2.4 F9 행동 우선순위와 계산 근거

기본 `Live Tuning` 탭 상단에 서버가 계산한 네 거시 후보를 표시한다.

1. Retreat
2. Fight
3. Farm
4. Structure

표시 규칙:

- legal 후보 우선, 그 안에서 score 내림차순으로 정렬한다.
- 같은 authoritative tick에 실제 선택 trace가 있으면 `SELECTED THIS TICK`을 표시한다.
- 새 선택이 없는 tick의 현재 intent에는 `ACTIVE / HELD`를 표시한다.
- raw score 1등과 실제 선택이 다르면 hard gate, margin, 기존 intent hold가 개입했음을 경고한다.
- `Score Calculations`를 열면 각 후보를 구성하는 항을 `raw × weight = contribution`으로 보여주고, contribution 합과 최종 score를 함께 보여준다.
- Retreat는 health pressure, turret risk, incoming combo risk를 사용한다.
- Fight는 positive opportunity, turret risk, incoming combo risk와 clamp/threshold adjustment를 사용한다.
- Farm은 farm opportunity에 `Farm Priority`를 곱한다.
- Structure는 structure exposure에 structure weight를 곱한다.

정직한 범위는 중요하다. 숫자로 상호 경쟁하는 것은 위 네 거시 utility다. `FollowWave`, `AttackMinion`, 스킬 선택 등 하위 행동은 조건 gate와 executor 순서로 결정되므로 존재하지 않는 개별 utility score를 만들지 않았다. 대신 target, wave distance, last move, executor, block reason으로 통과·차단 근거를 보여준다.

### 2.5 F9 실시간 튜닝과 hot-load 의미

F9의 실시간 적용 경로는 다음과 같다.

```text
선택 봇 DragFloat 드래그/더블클릭
-> 편집 종료 시 AIDebugControl command 1회 전송
-> Debug host Server 검증·clamp
-> 선택한 ChampionAIComponent override만 변경
-> decisionTimer=0으로 다음 판단 즉시 요청
-> authoritative snapshot으로 유효값 echo
-> Client pending 해제 또는 2초 timeout 시 서버값으로 복원
```

- 다른 봇과 JSON profile은 바뀌지 않는다.
- `Reset Selected Bot`은 해당 봇만 runtime JSON/profile 기본값으로 되돌린다.
- keyframe round-trip에서도 선택 봇 override는 보존된다.
- F9의 hot-load는 **선택 entity에 대한 Debug 세션 오버라이드**다. 파일 저장이 아니므로 서버 재시작 뒤에는 남지 않는다.
- F4 `Save & Hot Load`는 **JSON을 저장하는 영구 authoring 경로**다. 두 의미를 같은 저장 버튼으로 섞지 않았다.
- Snapshot의 상세 후보·기여도 evidence는 Debug 진단 데이터이며, stale tick이면 서버가 생략하고 Client도 이전 값을 먼저 지운다.

## 3. 주요 변경 경계

- `Data/Gameplay/ChampionGameData/champions.json`: Viego W canonical range.
- `Client/Private/UI/ChampionTuner.cpp`: ranged minion Attack Range drag/double-click.
- `Server/Private/Game/GameRoomCommands.cpp`: living minion attackRange refresh와 AI tuning validation.
- `Shared/GameSim/Components/ChampionAIComponent.h`: append-only tuning id, 중앙 resolver, effective/evidence 값.
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`: anchor->lane 전이, follow-wave range, farm weight, target/block evidence.
- `Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.cpp`: 네 utility breakdown의 raw/weight/contribution.
- `Client/Private/UI/AIDebugPanel.cpp`: 우선순위 표, 계산 상세, per-bot draft/pending/echo UI.
- `Shared/Schemas/Snapshot.fbs`, `Server/Private/Game/SnapshotBuilder.cpp`, `Client/Public/Network/Client/AIDebugEvidenceDecoder.h`: Debug evidence 전달과 stale 제거.
- `Tools/SimLab/main.cpp`, `Tools/Harness/GameRoomBotMatchSoak.cpp`: behavior/tuning/decoder/snapshot 회귀 검증.

## 4. 검증 결과

### 4.1 데이터와 계약

- `python Tools/LoLData/Test-F4BalanceContracts.py --root .`: PASS.
- `python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check`: PASS, pack `0x24349C4A`, champions 17, skills 85, summoner spells 1.
- 연속 2회 schema codegen C++/Go SHA-256 일치.
- `go test ./...` in `Services`: PASS.

### 4.2 AI 및 게임플레이 fixture

- 모든 17 tuning id가 중앙 resolver에 유일하게 연결되고 `Count`는 거절됨.
- 선택 bot A의 Follow/Farm/Turret override가 bot B에 전파되지 않음.
- keyframe 보존과 reset-to-profile 통과.
- 적 미니언만 보이는 WaitForWave가 LaneCombat과 command로 전환되고 false `NoTarget` 없음.
- 20m 아군 웨이브 fixture에서 follow range 10은 대기, 80은 추종 이동.
- farm priority 0과 3이 farm/fight 순위를 뒤집음.
- 강제 missing target은 `NoTarget`, 이후 성공 command가 이를 clear.
- current-tick evidence decode와 next-tick stale clear 통과.
- Viego W 현재 pack 기반 최소 2.5/최대 5.0과 최대 charge 실제 도착점 통과.

### 4.3 빌드와 통합

- Debug 전체 AI validation pipeline: PASS. 보고서 `2026-07-19_062540_BOT_AI_VALIDATION_HARNESS_REPORT.md`.
- Release 전체 AI validation pipeline: PASS. 보고서 `2026-07-19_063037_BOT_AI_VALIDATION_HARNESS_REPORT.md`.
- 최종 UI 배치 변경 뒤 Client Debug x64 `/m:1`: PASS.
- 최종 UI 배치 변경 뒤 Client Release x64 `/m:1`: PASS.
- Server-linked 10-bot SnapshotBuilder probe: baseline 21,264 bytes, evidence 21,632 bytes, delta 368 bytes. 계획 ceiling 6 KiB 이내.
- GameRoom bot soak Debug, 1,800 ticks, seed 42: PASS. 10/10 bot command active, world hash `2139EF7BAECC60CB`.
- `git diff --check`: PASS. 기존 dirty tree의 LF/CRLF 경고만 있고 whitespace error 없음.

## 5. 실패와 교정 기록

- Release pipeline에서 `run_codegen.bat`의 괄호 loop가 간헐적으로 인자 없는 `flatc`를 호출했다. per-schema loop는 유지하고 `call :generate_cpp`/`call :generate_go` subroutine으로 변경한 뒤 연속 codegen과 Release MSBuild를 통과했다.
- 기존 SimLab gameplay formula probe가 사용자가 F4로 편집한 damage를 과거 고정값 300/120과 비교해 실패했다. 현재 generated formula에서 expected damage를 계산하도록 바꿔 data-driven 실행 검증으로 복구했다.
- PowerShell이 native soak executable stderr를 `NativeCommandError`로 승격했다. executable 구간만 bounded `ErrorActionPreference=Continue`로 실행하고 exit code를 별도로 검증하도록 수정했다.

## 6. 수동 QA 체크리스트

자동 검증은 닫혔지만 다음 화면·체감 확인은 실행하지 않았다.

- Debug F5에서 F9를 열었을 때 첫 봇 자동 선택과 `Decision Ranking` 기본 노출.
- Follow Wave Search/Farm Priority/Turret Danger 값을 드래그 및 더블클릭 편집한 뒤 같은 선택 봇의 snapshot echo 확인.
- 2초 timeout이나 reset에서 로컬 draft가 서버 유효값으로 복원되는지 확인.
- LeeSin이 적 미니언 또는 20m 내 아군 웨이브를 보고 정지하지 않고 이동·파밍하는지 실제 map/nav 환경에서 확인.
- F4 Ranged Attack Range 저장 후 현재 살아 있는 원거리 미니언의 공격 거리 변화 확인.

수동 화면 검증에서 실패하면 이 결과서를 완료로 과장하지 않고 해당 재현 tick의 F9 target/move/block evidence를 기준으로 후속 수정한다.

## 7. 2026-07-19 보정 재개 결과

상태: `COMPLETE_WITH_MANUAL_QA_REMAINING`.

이 절은 앞 절의 과거 Viego 12.5 결과를 명시적으로 폐기하고 최종 보정 결과를 기록한다.

### 7.1 F9와 F4 편집 표면

- F9 `RenderTuningDrag`를 label/value 2열 table로 바꿔 좁은 창에서도 오른쪽 label이 잘리지 않게 했다. 기존 drag, 더블클릭 정확값 입력, pending, 서버 snapshot echo 계약은 유지한다.
- F4 모든 챔피언 스킬에 `Skill Range (m)`를 추가했다. `skills[].rangeMax` 하나가 서버 cast/dash 판정과 다음 cue geometry의 길이 입력이 된다.
- present mechanics에 `Effect Radius / Half Width (m)`, `Delay (sec)`, `Heal / Damage Ratio`를 추가했다. 값은 기존 `Save & Hot Load` JSON 저장·검증·권위 서버 1회 reload 경로를 그대로 사용한다.
- Fiora W도 custom flat-damage 제외 목록에서 제거해 ranked base damage를 F4에서 직접 조절할 수 있다.

### 7.2 수치와 FX 보정

- Viego W `rangeMax`: `12.5 -> 5.0`, 최소 charge `2.5`.
- Ranged minion `attackRange`: `8.0 -> 5.6`.
- Ashe Q의 불투명 diffuse ground quad를 제거했다. additive glow/mesh는 유지했고 `w_hit.wfx`의 0 크기는 `1.2 x 1.2`로 복구했다.
- Sylas Q는 cursor 위치에 0.5초 뒤 반경 1.65 폭발을 만들고, 동일 authoritative tick에 stage-2 cue와 Magic damage를 낸다. client는 stage-1 cast와 stage-2 explosion을 분리 재생한다.
- Sylas W는 canonical Magic damage가 확정된 뒤 `finalAmount * 0.5`만큼 회복하고 최대 HP로 clamp한다. HP와 champion snapshot mirror도 같은 값으로 맞춘다.

### 7.3 Fiora W의 최종 원리

Fiora W는 더 이상 WFX의 그림과 서버 판정이 별도 고정 숫자를 갖지 않는다.

```text
F4 Skill Range = 6.5
F4 Effect Radius / Half Width = 0.8
F4 ranked Base Damage = 80 / 80 / 80 / 80 / 80
            │
            ▼
Server OnW: mouse direction 저장 + Fiora yaw 회전
            │
            ├─ authoritative rectangle: length 6.5, full width 1.6
            ├─ release damage: rectangle 내부의 가장 가까운 적 1명
            └─ EffectTrigger: endpoint + direction + length + halfWidth
                                      │
                                      ▼
Client WFX: caster와 endpoint의 중점에 6.5 x 1.6 decal
            release beam은 caster 현재 위치 -> endpoint world segment
```

- 서버 판정은 원형/부채꼴이 아니라 `0 <= forwardDistance <= length`, `abs(lateralDistance) <= halfWidth`인 방향 직사각형이다.
- 뒤, 폭 0.8 밖, 길이 6.5 밖의 대상은 맞지 않는다. 안쪽 후보 중 deterministic nearest 1명만 canonical W formula로 피해를 받는다.
- cast/success ground decal만 world anchor로 바꾸고 중점에 놓는다. glow/flash는 계속 Fiora에 붙는다.
- decal local X 길이 축과 월드 진행 방향을 맞추기 위해 두 cue 모두 `YawFromDirection - π/2`를 사용한다.
- 따라서 WFX는 미술 기본 모양을 제공하고, 실제 런타임 길이·폭·방향은 서버가 F4 데이터에서 읽어 event로 전달한다. F4 hot-load 뒤 다음 W부터 판정과 보이는 크기가 함께 바뀐다.

### 7.4 자동 검증

- GameSim Debug, Server Debug, Client Debug, SimLab Debug: PASS.
- Client Release 단독 빌드: PASS, warning 0/error 0.
- Bot AI validation Debug/Release: PASS.
- `SimLab --fiora-w-only`: mouse direction, 6.5 x 1.6 event geometry, inside hit, lateral/range/behind miss PASS.
- `SimLab --sylas-qw-only`: Q pre-delay/exact detonate tick/inside-outside, W post-mitigation 50% heal PASS.
- `SimLab --stage-input-only`, `--f4-balance-only`: PASS.
- 전체 SimLab 600 ticks, seed 1234: same-seed hash `AE3888BC9E831695`, PASS.
- GameRoom bot soak 1,800 ticks, seed 42: PASS.
- definition pack `--check`, F4 contract, schema isolated 연속 2회 동일 hash, Services `go test ./...`, `git diff --check`: PASS.

첫 Release AI harness는 `WintersGame.lib` 순간 파일 잠금으로 LNK1114가 났다. 실행 중 Winters 바이너리가 없고 파일이 read-only가 아님을 확인한 뒤 Client Release 단독 재링크가 통과했고, 같은 Release harness 재실행도 PASS했다. 스키마/소스 실패로 처리하지 않는다.

### 7.5 남은 수동 화면 QA

실제 Debug F5 화면 조작은 이 세션에서 수행하지 않았다. 다음만 사람이 확인하면 된다.

- F9 폭 560/700에서 label/value가 모두 보이고 drag/더블클릭/server echo가 유지되는지.
- F4에서 Fiora W range/half-width/damage를 바꿔 hot-load한 뒤 다음 cast의 회전, decal, endpoint, 실제 피격이 동시에 바뀌는지.
- Sylas Q stage-2 cue와 damage text가 같은 순간인지, W 회복이 실제 피해의 절반인지.
- Ashe Q 검은 quad가 사라지고 W hit가 보이는지.
