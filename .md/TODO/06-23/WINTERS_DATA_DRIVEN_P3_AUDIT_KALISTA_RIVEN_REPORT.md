# Winters DataDriven P3 Audit + Kalista/Riven 결과 보고서

작성일: 2026-06-23

## 1. 북극성 유지 여부

유지되고 있다.

이번 라운드에서도 기준은 `07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`의 canonical flow다.

```text
authoring JSON
-> Build-LoLDefinitionPack.py
-> generated immutable pack
-> GameplayDefinitionQuery read-only query
-> GameSim execution
```

이번 변경은 이 흐름 밖으로 새 경로를 만들지 않았다.

## 2. 이번 라운드 목표

이번 라운드는 값을 많이 옮기는 것보다, 이후 컷오버가 흔들리지 않도록 P3 계기판을 먼저 만들고 작은 런타임 reader를 추가하는 데 집중했다.

반영 목표:

1. `Collect-LoLLegacyDataAudit.ps1`이 skill effect 하드코딩 후보를 별도로 집계하게 만든다.
2. Kalista E slow 수치를 `ServerPrivate` JSON으로 이동한다.
3. Riven Q/W 판정 수치를 `ServerPrivate` JSON으로 이동한다.
4. 생성팩과 전체 빌드/SimLab 회귀를 통과시킨다.

## 3. 감사 계기판 추가

반영 파일:

- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`

추가한 항목:

- `legacyLineCounts.skillEffectHardcodeCandidates`
- `legacyLineCounts.skillEffectDataQueryReaders`
- `skillEffectCutover.hardcodeCandidatesByFile`
- `skillEffectCutover.dataQueryReadersByFile`

현재 결과:

```text
skillEffectHardcodeCandidates = 156
skillEffectDataQueryReaders   = 55
```

의미:

- `hardcodeCandidates`는 아직 코드에 남은 `constexpr f32_t` skill effect 후보다.
- `dataQueryReaders`는 정의팩을 통해 값을 읽는 runtime reader다.
- 현재 hardcode 후보가 그대로 남는 이유는 fallback 상수를 아직 회귀 방지용으로 유지하기 때문이다.

따라서 지금 당장 156이 0이 아닌 것은 실패가 아니다.
다음 큰 목표는 모든 정상 실행 경로에 `TickContext.pDefinitions`를 보장한 뒤 fallback을 삭제하는 것이다.

## 4. Kalista 반영

반영 파일:

- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
- `Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp`

추가 데이터:

```json
{
  "key": "skill.kalista.e",
  "params": {
    "moveSpeedMul": 0.55,
    "slowDurationSec": 2.0
  }
}
```

런타임 변경:

- Kalista E slow duration을 `SlowDurationSec`로 조회한다.
- Kalista E slow move speed multiplier를 `MoveSpeedMul`로 조회한다.
- 기존 fallback 값은 유지한다.

의심 결과:

- `slowDurationSec`는 `stunDurationSec`와 같은 원자가 아니다.
- 그래서 새 원자 `SlowDurationSec`를 추가했다.
- `moveSpeedMul`은 slow와 haste 모두에서 “이동속도 배율”이라는 같은 원자라 재사용했다.

## 5. Riven 반영

반영 파일:

- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
- `Shared/GameSim/Champions/Riven/RivenGameSim.cpp`

추가 데이터:

```json
{
  "key": "skill.riven.q",
  "params": {
    "airborneDurationSec": 0.75,
    "radius": 2.25,
    "stackWindowSec": 4.0
  }
}
```

```json
{
  "key": "skill.riven.w",
  "params": {
    "radius": 2.5,
    "stunDurationSec": 0.75
  }
}
```

런타임 변경:

- Riven Q3 radius를 정의팩에서 조회한다.
- Riven Q3 airborne duration을 정의팩에서 조회한다.
- Riven Q stack window를 정의팩에서 조회한다.
- Riven W radius를 정의팩에서 조회한다.
- Riven W stun duration을 정의팩에서 조회한다.

의심 결과:

- `AirborneDurationSec`는 `StunDurationSec`와 다르다.
- `StackWindowSec`는 피해/CC 수치가 아니라 스킬 상태 유지 창이지만, Q skill effect의 실행 판정 수치라 이번 P3에 포함했다.
- Riven E/R의 타이머, 표식, 상태 유지 값은 아직 정책 경계가 섞여 있어 이번 slice에 억지로 넣지 않았다.

## 6. Shared 원자 추가

반영 파일:

- `Shared/GameSim/Definitions/SkillAtomData.h`
- `Tools/LoLData/Build-LoLDefinitionPack.py`

추가한 원자:

- `SlowDurationSec`
- `AirborneDurationSec`
- `StackWindowSec`

이 원자들은 챔피언 전용 이름이 아니다.
스킬 효과 수치를 더 작은 의미 단위로 나눈 이름이다.

## 7. 생성팩 결과

정의팩 생성:

```text
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
Generated LoL definition pack 0xBCCF654A
Champions: 17, skills: 85, summoner spells: 1
```

정의팩 최신성 확인:

```text
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
PASS
```

생성팩 확인:

- `skill.kalista.e` 포함
- `skill.riven.q` 포함
- `skill.riven.w` 포함
- `eSkillEffectParamId::SlowDurationSec` 포함
- `eSkillEffectParamId::AirborneDurationSec` 포함
- `eSkillEffectParamId::StackWindowSec` 포함

## 8. 전체 검증 결과

실행:

```text
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

결과:

```text
[LoLDataDriven] PASS
```

통과 단계:

- Definition pack freshness
- Legacy ownership audit
- Client visual timing parity
- GameSim Debug x64 build
- Server Debug x64 build
- Client Debug x64 build
- SimLab Debug x64 build
- SimLab deterministic regression
- Whitespace validation

SimLab:

```text
same-seed replay OK: hash=67F2A97563B8DB04
seed sensitivity OK: seed+1 hash=5DA19645E291A29B
```

주의:

- Client 빌드 중 기존 C4251/C4275/C4828 경고가 남아 있다.
- 복사 단계에서 `공유 위반입니다.` 메시지가 한 번 출력됐지만 전체 파이프라인은 PASS했다.
- `git diff --check`는 CRLF 경고만 출력했고 trailing whitespace 실패는 없었다.

## 9. 현재 상태 요약

이번 라운드 후 P3 reader 상태:

```text
Annie   : skill effect reader 전환됨
Ashe    : W/R reader 전환됨
Jax     : Q/E reader 전환됨
Kalista : E reader 전환됨
Riven   : Q/W reader 전환됨
```

아직 남은 큰 후보:

```text
Fiora   : Q/W/R 수치
LeeSin  : Q/E/R 수치
Viego   : Q/W/E/R + soul 수치
Yone    : Q/W/E/R 수치
Zed     : Q/W/E/R + mark/shadow 수치
Yasuo   : Q/W/E/R 수치
Irelia  : Q/W/E/R 수치
Kindred : Q/W/E/R 수치
Sylas   : E 수치
```

## 10. 다음 방향

다음에도 같은 순서로 간다.

```text
1. 값이 진짜 skill effect 수치인지 의심한다.
2. 기존 원자로 표현 가능한지 확인한다.
3. 기존 원자가 거짓말을 하면 새 원자를 만든다.
4. ServerPrivate JSON에 byte-identical 값을 적는다.
5. generator로 immutable pack을 만든다.
6. runtime reader만 pack/query로 바꾼다.
7. fallback은 회귀 방지용으로 남긴다.
8. 전체 Verify 파이프라인으로 회귀를 확인한다.
```

다음 우선순위:

1. Fiora Q/W/R 수치 컷오버
2. LeeSin Q/E/R 수치 컷오버
3. Viego 또는 Yone처럼 수치가 많은 챔피언을 한 번에 묶지 말고 skill 단위로 분리
4. 모든 정상 실행 경로에 `TickContext.pDefinitions` 주입 보장
5. fallback 상수 삭제 감사로 `skillEffectHardcodeCandidates` 감소 시작

## 11. 결론

북극성은 유지되고 있다.

이번 라운드는 구조를 옆으로 늘리지 않았고, 기존 P3 흐름을 더 증명 가능하게 만들었다.
이제부터는 “얼마나 옮겼는가”보다 “어떤 값이 어떤 원자인가”를 계속 의심하면서 reader를 하나씩 줄이면 된다.
