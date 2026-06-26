# Session - Winters DOD Goal Loop And Yasuo P3 Report

## 1. 결론

루프는 메인이 아니다.

이번 반영의 핵심은 `07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`의 북극성을 기준으로, 루프가 다음 목표를 고르고 실제 코드/데이터 변경이 그 목표 수치를 줄였는지 확인하는 검증 장치를 만든 것이다.

이번 루프에서 실제 목표 감소가 확인됐다.

- P3 `Champion skill gameplay/balance literals are removed`
- 직전 추적 기준: `170`
- 현재 추적 기준: `118`
- 감소량: `52`
- 전체 검증: `PASS`

즉, 루프는 상태판으로 남고 실질 목표 달성이 본체라는 방향이 유지됐다.

## 2. 북극성 기준

북극성 문서:

- `.md/plan/collab-pipeline/07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`

현재 루프의 목표는 다음 순서다.

1. P3: 스킬 gameplay/balance literal 제거
2. P4: gameplay authority가 visual timing/yaw를 읽지 않게 분리
3. P5: bot AI tactic/rank policy 데이터화
4. P6: minion/wave/jungle/structure/object value 데이터화
5. P7: network identity를 stable `DefinitionKey` 경계로 전환
6. P8: legacy value owner runtime reader 제거

이번 기준에서 다음 목표는 여전히 P3다.

```text
P3SkillEffectHardcode
current=118
targetMax=0
```

## 3. 루프 승격 반영

추가/수정한 검증 구조:

- `Data/LoL/SharedContract/DataDrivenGoalCriteria.json`
- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`
- `Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1`
- `Tools/LoLData/Run-LoLDataDrivenGoalLoop.ps1`
- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
- `.md/plan/collab-pipeline/07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`
- `.md/plan/2026-06-24_DATA_DRIVEN_GOAL_LOOP_PLAN.md`

루프의 원칙:

```text
Measure -> choose earliest unfinished phase -> implement one reversible slice -> verify full pipeline -> write status -> repeat
```

중요한 결정:

- `Verify-LoLDataDrivenPipeline.ps1` 안에 goal status 생성을 공식 단계로 넣었다.
- 아직 incomplete 상태를 hard fail로 만들지는 않았다.
- 이유: P3~P8이 완료되기 전까지는 incomplete가 실패가 아니라 정상 진행 상태이기 때문이다.
- 최종 cutover 이후에는 `Get-LoLDataDrivenGoalStatus.ps1 -FailWhenIncomplete`를 hard gate로 승격하면 된다.

## 4. 이번 실제 P3 반영

Yasuo 스킬 runtime 값 일부를 `ServerPrivate` gameplay data로 이동했다.

반영 데이터:

- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

추가한 gameplay atom:

- `skill.yasuo.q`
- `skill.yasuo.e`
- `skill.yasuo.r`

Yasuo 코드 변경:

- `Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp`

핵심 변경:

- Q 기본 projectile 값
  - `speed`
  - `effectDurationSec`
  - `radius`
  - `baseDamage`
  - `stackWindowSec`
- E dash/damage 값
  - `effectDurationSec`
  - `gap`
  - `dashDistance`
  - `dashDurationSec`
  - `baseDamage`
- R target hold/landing/damage 값
  - `airborneDurationSec`
  - `gap`
  - `baseDamage`
- Tornado airborne duration
  - Q의 `airborneDurationSec`를 읽도록 변경

생성팩:

- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`
- build hash: `0x3851F548`

## 5. 검증 결과

실행한 파이프라인:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\LoLData\Run-LoLDataDrivenGoalLoop.ps1
```

결과:

- Definition pack freshness: PASS
- Legacy ownership audit: PASS
- Data-driven goal status: PASS
- Raw product asset path audit: PASS, non-blocking
- Client visual timing parity: PASS
- GameSim build: PASS
- Server build: PASS
- Client build: PASS
- SimLab deterministic replay: PASS
- SimLab seed sensitivity: PASS
- Whitespace validation: PASS
- Full data-driven pipeline: PASS
- Loop status: INCOMPLETE

`INCOMPLETE`는 오류가 아니다. 현재 목표가 남아 있으므로 정상적인 다음 작업 신호다.

SimLab 결과:

```text
same-seed replay OK: hash=C6B6B27562331CD3
seed sensitivity OK: seed+1 hash=6873AC62CDF97D65
```

`git diff --check` 결과:

- whitespace error 없음
- CRLF 변환 warning만 존재

빌드 warning:

- 기존 코드 페이지 warning `C4828`
- 기존 EngineSDK DLL-interface warning `C4275`
- 이번 Yasuo P3 변경으로 인한 error 없음

## 6. 현재 남은 목표 상태

현재 생성된 상태 파일:

- `.md/TODO/06-24/LOL_DATA_DRIVEN_AUDIT.json`
- `.md/TODO/06-24/LOL_DATA_DRIVEN_GOAL_STATUS.json`
- `.md/TODO/06-24/LOL_DATA_DRIVEN_GOAL_STATUS.md`

현재 goal status:

```text
P3: 118
P4: 905
P5: 208
P6: 30
P7: 513
P8: 193
```

다음 루프의 첫 목표는 P3 유지다.

## 7. 다음 원자 단위

계속 의심한 결과, Yasuo에 남은 P3 값은 단순한 기본 Q/E/R 값이 아니다.

남은 성격:

- Q3 tornado projectile
- E 중 Q를 쓴 EQ variant
- airborne lift
- hit delay/debug boundary

본질 판단:

- `skill.yasuo.q` 하나에 모든 변형 값을 억지로 넣으면 다시 의미가 섞인다.
- `secondaryDamage`, `secondaryRadius` 같은 이름은 원자 단위가 아니라 임시 상자다.
- 다음 원자는 `slot` effect 하나가 아니라 `variant effect key`다.

다음 계획:

1. 생성기와 runtime query에 variant effect 조회 경로를 추가한다.
2. `skill.yasuo.q.tornado`, `skill.yasuo.q.eq` 같은 별도 gameplay effect key를 공식 데이터로 만든다.
3. Yasuo Q stage 3/4가 해당 variant effect를 읽게 한다.
4. fallback constant는 reader가 붙은 뒤에만 삭제한다.
5. full loop를 다시 돌려 P3 숫자가 실제로 감소하는지 확인한다.

## 8. 승격 판단

루프는 정식 검증 파이프라인으로 승격해도 된다.

단계별 승격:

1. 현재 단계: `Verify-LoLDataDrivenPipeline.ps1` 안에서 non-blocking status report 생성
2. P3 완료 후: P3만 hard gate 가능
3. P3~P8 완료 후: 전체 `-FailWhenIncomplete` hard gate 가능

현재는 1단계가 맞다.

이유:

- 아직 완료되지 않은 목표가 많다.
- incomplete를 실패로 취급하면 개발 루프 자체가 막힌다.
- 대신 매 루프마다 수치가 줄지 않으면 작업 방향을 의심할 수 있다.

따라서 지금의 정답은 다음이다.

```text
Loop is official observability now.
Goal completion becomes official gate after cutover.
```
