Session - Yone Bot AI를 코드베이스 기준으로 재검증하고 다음 구현 순서를 고정한다.

# Yone Bot AI Codebase Audit And Continuation Plan

작성일: 2026-06-24
성격: 코드베이스 실사 보고서 + 다음 구현 계획
대상: Winters LoL 서버 권위 Bot AI, Yone 전투 전술, Champion AI 전술 레지스트리

상위 문서:

- `.md/plan/Champion/31_YONE_BOT_AI_AND_COMBAT_POLICY_ARCHITECTURE_DESIGN.md`
- `.md/plan/ai/16_BOT_AI_HUMANLIKE_COMPLETION_PIPELINE_GUIDE.md`
- `.md/TODO/06-24/WINTERS_Y0_Y1_YONE_AI_REGISTRY_IMPLEMENTATION_REPORT.md`
- `CLAUDE_Legacy.md`

---

## 0. 결론

Y0/Y1의 방향은 맞다.

- Bot AI는 gameplay truth를 직접 수정하지 않고 `GameCommand`만 생산한다.
- `ExecuteLaneCombat`에는 챔피언별 전술을 레지스트리로 호출하는 seam이 생겼다.
- Yone 전술은 E 진입, E 복귀 판단, R/Q/W/평타 우선순위를 갖기 시작했다.
- 이 방향은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 북극성과 맞다.

하지만 현재 코드 기준으로는 Yone Bot이 "의도대로 완성"된 상태가 아니다.

가장 큰 문제는 Yone E 복귀 계약이다.

Yone AI는 E 복귀를 `cmd.itemId = 2u`인 2단계 스킬 재시전으로 발행한다. 그런데 현재 `skill.yone.e` 데이터는 stage count가 1이고 window가 0이다. `CommandExecutor`는 stage count 2, stage window, current stage를 모두 만족해야 `itemId=2`를 통과시키므로, 현재 데이터 상태에서는 AI가 발행한 E 복귀 명령이 `stage2-window`에서 거절될 수 있다.

따라서 다음 구현의 0순위는 Yone E 복귀 command contract를 실제 데이터/Executor/Sim 경로에서 닫는 것이다.

---

## 1. 실제 코드 기준 현재 구조

### 1.1 Bot AI의 본질 경로

현재 Bot AI는 아래 흐름으로 움직인다.

```text
Server GameRoom Tick
  -> Phase_ServerBotAI
  -> CChampionAISystem::Execute
  -> ChampionAIContext / score / intent / champion tactics
  -> GameCommand push
  -> CommandExecutor
  -> Champion GameSim hook
  -> Snapshot/Event
  -> Client Visual
```

이 구조에서 Bot AI의 본질은 "서버 권위 플레이어 입력 생산자"다.

Bot AI가 직접 해도 되는 일:

- 대상을 고른다.
- 현재 상황을 점수화한다.
- 어떤 행동을 시도할지 정한다.
- `Move`, `CastSkill`, `BasicAttack`, `Flash`, `Recall` 같은 `GameCommand`를 만든다.

Bot AI가 직접 하면 안 되는 일:

- Transform을 직접 바꾼다.
- HP, cooldown, skill state를 직접 바꾼다.
- 스킬 성공 여부를 AI 내부에서 확정한다.
- Client visual path를 gameplay truth처럼 사용한다.

### 1.2 챔피언 전술 레지스트리

현재 `ChampionAISystem.cpp`에는 다음 형태의 함수 포인터 seam이 있다.

```cpp
using ChampionCombatTacticsFn = bool_t (*)(
    CWorld&, const TickContext&, EntityID, ChampionAIComponent&,
    ChampionComponent&, const Vec3&, const ChampionAIContext&,
    std::vector<GameCommand>&);
```

현재 등록 항목:

- `eChampion::YASUO -> TryExecuteYasuoChampionCombat`
- `eChampion::YONE -> TryExecuteYoneChampionCombat`

의미:

- 공용 lane combat 흐름은 챔피언 전술 함수를 호출한다.
- 챔피언별 판단은 `GameCommand` 생산으로만 표현된다.
- 신규 챔피언 전술은 `ExecuteLaneCombat`의 if-chain을 늘리는 대신 레지스트리에 붙이는 방향으로 간다.

현재 한계:

- Yone/Yasuo 전술 함수 자체는 아직 같은 `ChampionAISystem.cpp` 안에 있다.
- `TryExecuteJaxDive`와 `TryExecuteYasuoMinionFarm`은 아직 레지스트리 바깥의 특수 경로다.
- 따라서 seam은 생겼지만 "공용 AI cpp가 챔피언 지식을 모른다"는 최종 상태까지는 아직 아니다.

### 1.3 Yone 전술 구현 상태

`TryExecuteYoneChampionCombat`의 현재 판단 순서는 다음이다.

```text
1. YoneSimComponent에서 SoulOut 상태 확인
2. SoulOut 중 위험/불리/타이머 만료 직전이면 E 복귀 command 발행
3. target이 없거나 교전 불가하면 실패
4. 적 HP 50% 이하 또는 SoulOut 중이면 R 시도
5. SoulOut이 아니고 적이 평타 거리보다 멀면 E 진입 시도
6. Q 시도
7. W 시도
8. 평타 시도
```

의미:

- 기존 Yone의 Q/W 단발 skillRules보다 훨씬 낫다.
- Yone의 정체성인 `E commit -> trade -> E return`이 전술로 표현되기 시작했다.
- 그러나 아직 점수 기반 후보 선택은 아니며, 하드코딩 우선순위 체인이다.

남은 핵심:

- Q3 스택/넉업 셋업 판단이 없다.
- R은 `enemyHpRatio <= 0.5f || bSoulOut` 기준이라 너무 넓게 나갈 수 있다.
- E 복귀 임계값은 `0.75f`, `reengageHpRatio`, `fChampionScoreMargin` 등 기존 공용 값과 하드코딩 상수에 기대고 있다.
- Yone 전용 tuning knob이 없다.
- trace에는 "왜 이 스킬이 선택됐는지"의 ability score/reason이 충분히 남지 않는다.

---

## 2. 노트북 설계/보고서와 실제 코드 대조

### 2.1 일치하는 부분

설계서의 핵심 방향은 실제 코드와 맞다.

- 고정 콤보만으로는 Yone을 표현할 수 없다.
- Yone에는 반응형 전술 훅이 필요하다.
- Bot AI는 command-only여야 한다.
- Yone E 복귀는 AI가 상태를 직접 바꾸지 않고 command로 요청해야 한다.
- 실제 상태 전이는 `CommandExecutor -> YoneGameSim::OnE -> StartSoulReturn` 경로가 담당해야 한다.

### 2.2 불일치하는 부분

`WINTERS_Y0_Y1_YONE_AI_REGISTRY_IMPLEMENTATION_REPORT.md`는 Yone E 복귀가 2단계 스킬 계약을 따른다고 적고 있다.

문서상 계약:

```text
cmd.kind = CastSkill
cmd.slot = E
cmd.itemId = 2u
cmd.groundPos = YoneSimComponent.anchorPosition
```

실제 AI 코드도 이 명령을 만든다.

하지만 실제 스킬 데이터는 아직 이 계약을 받지 못한다.

현재 `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`의 `skill.yone.e`:

```json
"stage": {
  "count": 1,
  "windowSeconds": 0.0,
  "lockSeconds": [
    0.75,
    0.6
  ]
}
```

`CommandExecutor`의 2단계 판정:

```text
bRequestedStage2 = cmd.itemId == 2u
bStage2 =
  bRequestedStage2 &&
  slot.currentStage == 1 &&
  slot.stageWindow > 0.f &&
  GameplayDefinitionQuery::IsSkillTwoStage(...)
```

그리고 `bRequestedStage2 && !bStage2`이면 reject:

```text
LogCastSkill("reject", "stage2-window", ...)
return
```

즉 현재 데이터 기준으로는 AI의 `itemId=2` 복귀 command가 `YoneGameSim::OnE`까지 도달하지 못할 수 있다.

이것은 문서/보고서와 코드베이스 사이의 가장 중요한 정정 사항이다.

---

## 3. 수행된 작업의 방식과 원리

### 3.1 Y0 - Champion combat tactics seam

수행 방식:

- `ExecuteLaneCombat` 내부의 챔피언별 전투 분기를 함수 포인터 레지스트리로 옮겼다.
- 기존 Yasuo 전술도 레지스트리 항목으로 등록했다.
- 새 champion hook을 만들 때 공용 lane combat 흐름을 다시 키우지 않는 방향을 열었다.

원리:

- 공용 AI는 "언제 챔피언 전술을 호출할지"만 안다.
- 챔피언 전술은 "내 킷을 어떻게 쓸지"만 안다.
- gameplay truth는 여전히 `CommandExecutor`와 Champion GameSim이 소유한다.

결과:

- Yone 전술을 붙일 seam이 생겼다.
- 그러나 `ChampionAISystem.cpp` 안의 챔피언 지식은 아직 완전히 분리되지 않았다.

### 3.2 Y1 - Yone combat tactics

수행 방식:

- `TryExecuteYoneChampionCombat`를 추가하고 레지스트리에 등록했다.
- Yone의 `YoneSimComponent`를 읽어 SoulOut 상태를 판단했다.
- E 복귀 조건을 만족하면 직접 `CastSkill` command를 만들었다.
- E 진입/R/Q/W/평타는 기존 `EmitSkillCommand`/`EmitBasicAttackCommand` 경로를 재사용했다.

원리:

- Bot AI는 "돌아가야 한다"는 판단까지만 한다.
- `cmd.itemId = 2u`는 "2단계 스킬 요청"이라는 기존 CommandExecutor 계약을 사용한다.
- 실제 복귀 성공 여부는 stage window와 skill definition을 통과한 뒤 Yone GameSim이 결정한다.

결과:

- Yone Bot의 전술적 형태가 생겼다.
- 하지만 현재 데이터가 2단계 계약과 맞지 않아 E 복귀 완성은 미검증/불완전 상태다.

---

## 4. 현재 결과와 검증 상태

### 4.1 동기화/빌드 보고서 기준 결과

노트북 handoff 및 데스크탑 동기화 과정에서 보고된 검증:

```powershell
git diff --check
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
powershell -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1
```

보고된 결과:

- `git diff --check`: 통과
- `GameSim Debug x64`: 오류 0
- `Server Debug x64`: 오류 0
- `Client Debug x64`: 오류 0
- `Verify-LoLDataDrivenPipeline.ps1`: `PASS`
- SimLab same-seed replay OK
- SimLab seed sensitivity OK

### 4.2 오늘 코드 실사 기준 결과

오늘 코드 실사에서는 빌드를 다시 돌리지 않고, Yone Bot 설계/구현 계약을 코드와 데이터 기준으로 대조했다.

확인된 사실:

- AI는 Yone E 복귀를 `itemId=2`로 발행한다.
- `CommandExecutor`는 2단계 skill definition과 stage window를 요구한다.
- 현재 `skill.yone.e`는 `count=1`, `windowSeconds=0.0`이다.
- `YoneGameSim::OnE`는 `YoneSimComponent.bSoulUnboundActive` 상태에서 호출되면 복귀를 수행한다.
- 따라서 문제는 Yone GameSim 자체보다 `AI command -> CommandExecutor stage2 gate -> OnE` 사이의 contract mismatch다.

### 4.3 검증 공백

아직 자동 검증으로 고정되지 않은 것:

- SoulOut 상태의 Yone Bot이 불리 상황에서 E return command를 발행하는지.
- 그 command가 `CommandExecutor`에서 `accept stage2`로 통과하는지.
- 통과 후 `[YoneSim] E return` 또는 상태 변화가 실제로 발생하는지.
- 같은 seed에서 이 행동이 deterministic하게 반복되는지.
- F5 인게임에서 trace와 visual이 동일하게 보이는지.

---

## 5. 다음 구현 계획

### S0 - Yone E 복귀 계약 고정

목표:

```text
yone-e-soul-return command
  -> CommandExecutor accept stage2
  -> YoneGameSim::OnE
  -> StartSoulReturn
  -> Snapshot/Event/Visual에서 복귀 확인
```

선택지:

1. 데이터 계약을 AI 설계에 맞춘다.
   - `skill.yone.e`를 2단계 skill로 선언한다.
   - `stage.count = 2`
   - `stage.windowSeconds = SoulOut 지속 시간과 같은 값` 또는 GameSim이 요구하는 최소 window
   - AI의 `itemId=2` 설계를 유지한다.

2. AI command를 일반 E cast로 바꾸고 `YoneGameSim::ResolveEStage`에 맡긴다.
   - `itemId=2`를 제거한다.
   - `CommandExecutor`의 Yone E special case가 `ResolveEStage`로 stage 2를 고르게 둔다.
   - 단, cooldown gate와 stage gate를 어떻게 해석할지 다시 정해야 한다.

권고:

- 1번을 우선한다.
- 이유: 현재 구현 보고서와 Bot AI 가이드가 이미 `itemId=2`를 "2단계 스킬" 신호로 정의하고 있다.
- 데이터가 이 계약을 받아야 CommandExecutor가 stage window를 검증하는 구조도 살릴 수 있다.

검증:

- `git diff --check`
- `GameSim Debug x64`
- `Server Debug x64`
- Yone E return targeted SimLab scenario
- F5/F9 trace에서 `yone-e-soul-return`, `accept stage2`, `YoneSim E return` 확인

### S1 - BrainType / Difficulty 런타임 배선

현재:

- `ChampionAIComponent.brainType` 기본값은 `RuleBased`다.
- `GameRoomSpawn.cpp`는 `ai.difficulty = slot.botDifficulty`만 설정한다.
- 따라서 `PlayerLike`/`Decision` brain은 일반 bot spawn에서 도달하지 않는다.

목표:

- bot difficulty 또는 slot 설정으로 `brainType`을 결정한다.
- F9 AI Debug Panel에서 brain type/difficulty가 보인다.
- PlayerLike와 RuleBased가 trace에서 구분된다.

원리:

- Bot의 "인간다움"은 새 프레임워크가 아니라 기존 `ChampionAIBrain` seam을 실제 runtime에 연결하는 일부터 시작한다.
- 난이도는 별도 AI를 만드는 것이 아니라 reaction, aggression, mistake, aim, tuning profile의 preset으로 표현한다.

### S2 - Yone 전술을 score policy로 정리

현재:

- R/E/Q/W/BA 하드 우선순위 체인이다.

목표:

- 반응형 override와 후보 스코어를 분리한다.
- Yone E return은 override로 최우선 처리한다.
- R/Q3/E/W/Q/BA는 후보 행동으로 모아 점수화한다.
- 선택 이유를 trace에 남긴다.

Yone 후보 예시:

```text
Override:
  EReturn - SoulOut 중 low hp, turret danger, timer expire, target lost, trade losing

Candidates:
  R       - kill angle, multi target line, SoulOut combo finish
  Q3      - Q stack ready, target in line/range
  EEngage - favorable trade, target just outside attack range, not SoulOut
  W       - close trade, shield value, target in cone/range
  Q       - poke/stack
  BA      - in attack range
```

### S3 - Champion tactics 분리 정리

목표:

- `ChampionAISystem.cpp`는 공용 selector와 command emission만 남긴다.
- champion-specific combat logic은 별도 tactics 파일 또는 champion-owned module로 이동한다.
- Jax dive와 Yasuo minion farm도 레지스트리 경로로 이동할 수 있게 한다.

우선순위:

1. Yone E contract fix 이후에 진행한다.
2. Yone score policy가 안정되면 Yasuo/Jax를 옮긴다.
3. 이동 전후 trace parity를 확인한다.

### S4 - Tuning / Trace / Designer surface

목표:

- 기획자/디자이너가 C++을 열지 않고 threshold를 확인하고 조정할 수 있는 최소 표면을 만든다.

Yone tuning 후보:

- `YoneReturnHpRatio`
- `YoneReturnTimerThreshold`
- `YoneEngageHpAdvantage`
- `YoneQ3SetupBias`
- `YoneRExecuteHpRatio`
- `YoneRMinTargets`

Trace 후보:

- selected ability
- selected score
- override reason
- rejected reason
- SoulOut timer
- trade delta
- target distance

---

## 6. 협업 분리 구조

### 6.1 기획자

기획자가 소유할 것:

- Yone Bot의 의도 문장
- "언제 들어가고 언제 돌아오는가"의 행동 기준
- 난이도별 성향
- 성공 기준: 예를 들어 "불리하면 돌아온다", "유리하면 끝까지 압박한다", "R은 킬각/다수 적중 때만 쓴다"

기획자가 소유하지 않을 것:

- Transform/HP/cooldown 변경 방식
- `GameCommand` wire format
- CommandExecutor validation detail

### 6.2 디자이너

디자이너가 소유할 것:

- tuning knob 기본값
- 인게임 F9 패널에서 체감 조정
- trace를 보고 threshold 재조정
- champion별 profile 값과 행동 weight

디자이너가 소유하지 않을 것:

- 서버 권위 성공 판정
- stage window/skill definition contract를 우회하는 임시 로직

### 6.3 개발자

개발자가 소유할 것:

- CommandExecutor 계약
- GameSim 상태 전이
- deterministic scenario test
- data definition과 runtime query 일치
- Snapshot/Event/Client visual 검증 경로

개발자가 피해야 할 것:

- 기획 의도를 C++ 상수로만 묻어두는 것
- debug panel 없이 tuning을 감으로 바꾸는 것
- client visual hook으로 gameplay truth를 보정하는 것

### 6.4 QA / 검증

QA가 확인할 것:

- 동일 seed replay parity
- Yone E return scenario pass
- F5 visual과 F9 trace 일치
- 난이도별 행동 차이
- 기존 Yasuo/Jax/Jax dive 회귀 없음

---

## 7. 문서 스타일 정리 원칙

기존 Legacy 문서는 넓은 배경과 포괄적 목표가 많다. 앞으로 Bot AI 문서는 아래처럼 나눈다.

### 7.1 Compass / Rule 문서

역할:

- 행동 규칙
- 아키텍처 경계
- 반복 실수 방지

예:

- `AGENTS.md`
- `.claude/gotchas.md`
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`
- `CLAUDE_Legacy.md`

원칙:

- 코드로 바로 알 수 있는 목록을 붙이지 않는다.
- 100줄을 넘기며 inventory가 되기 시작하면 도메인 문서로 분리한다.

### 7.2 Design 문서

역할:

- 왜 이 구조가 필요한지
- 선택지와 권고
- 기획/디자인/개발 협업 경계

예:

- `31_YONE_BOT_AI_AND_COMBAT_POLICY_ARCHITECTURE_DESIGN.md`
- 이 문서

원칙:

- 실제 코드와 어긋나면 즉시 정정 문서를 남긴다.
- "희망 구조"와 "현 코드 상태"를 섞지 않는다.

### 7.3 Implementation Plan 문서

역할:

- 구현자가 바로 적용할 줄단위 변경 지시

원칙:

- `.md/계획서작성규칙.md`를 따른다.
- 기존 코드/아래에 추가/아래로 교체/삭제 형태로 쓴다.
- 확인 불가능한 부분은 `CONFIRM_NEEDED`로 남긴다.

### 7.4 Report 문서

역할:

- 어떤 작업을 했다.
- 어떤 원리로 했다.
- 어떤 검증을 통과했다.
- 무엇이 아직 남았다.

원칙:

- 완료와 미완료를 섞지 않는다.
- "PASS"와 "미검증"을 분리한다.
- 문서상 완료라고 적혀 있어도 코드/데이터가 반박하면 코드/데이터를 우선한다.

---

## 8. 다음 액션

즉시 진행 순서:

```text
1. S0 Yone E stage-2 contract fix 계획서를 .md/계획서작성규칙.md 형식으로 분해
2. skill.yone.e data / generated pack / CommandExecutor 경로 수정
3. Yone E return targeted scenario 추가
4. GameSim + Server + SimLab + F5/F9 수동 검증
5. 통과 후 S1 brainType/difficulty 배선 진행
```

이번 문서 작성은 코드 변경이 아니다. 현재 목적은 다음 코드 변경 전에 "무엇이 실제로 맞고, 무엇이 아직 착각인지"를 고정하는 것이다.
