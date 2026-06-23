# Winters Data-Driven P3 Fiora/LeeSin SkillEffectParam 결과 보고서

## 1. 결론

이번 반영은 Data-Driven 북극성인 `JSON -> Generated Definition Pack -> GameplayDefinitionQuery -> GameSim 소비` 흐름을 유지한 채, Fiora와 LeeSin의 스킬 효과 수치를 코드 하드코딩의 소유물에서 서버 전용 JSON 정의의 소비 대상으로 옮긴 작업이다.

검증 결과 전체 DataDriven 파이프라인은 통과했다.

```text
[LoLDataDriven] PASS
```

생성된 LoL definition pack hash:

```text
0x1D9A3BE9
```

## 2. 이번에 남긴 원자 단위

### 2.1 새로 확정한 원자

`DashDistance`를 `eSkillEffectParamId`에 추가했다.

이 값은 `Range`와 다르다.

- `Range`: 스킬이 판정하거나 탐색할 수 있는 거리
- `DashDistance`: 시전자가 실제로 이동하는 거리
- `DashDurationSec`: 그 이동이 걸리는 시간

Fiora Q는 특히 이 셋이 같은 개념이 아니다. 그래서 `Range` 하나로 뭉치지 않고 `DashDistance`를 별도 원자로 분리했다.

### 2.2 JSON으로 이동한 효과 수치

`Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`에 아래 정의를 추가했다.

- `skill.fiora.q`: 피해량, 돌진 거리, 돌진 시간, 탐색 반경, 탐색 거리
- `skill.fiora.w`: 투사 거리, 판정 반경, 둔화 시간, 이동속도 배율
- `skill.fiora.r`: 피해량
- `skill.leesin.q`: Q2 피해량, 대상 근접 간격, 돌진 시간
- `skill.leesin.e`: 판정 반경, 둔화 시간, 이동속도 배율
- `skill.leesin.r`: 피해량, 에어본 시간

코드는 이제 이 값을 직접 소유하지 않고 `GameplayDefinitionQuery`로 읽는다.

## 3. 코드 반영 범위

### 3.1 공용 데이터 원자

- `Shared/GameSim/Definitions/SkillAtomData.h`
  - `eSkillEffectParamId::DashDistance` 추가

### 3.2 생성기

- `Tools/LoLData/Build-LoLDefinitionPack.py`
  - JSON 키 `dashDistance`를 C++ enum `DashDistance`로 매핑

### 3.3 서버 전용 게임플레이 데이터

- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
  - Fiora/LeeSin 스킬 효과 수치 추가

### 3.4 GameSim 소비 코드

- `Shared/GameSim/Champions/Fiora/FioraGameSim.cpp`
  - `ResolveFioraSkillEffectParam(...)` 추가
  - Q/W/R 효과 수치를 JSON definition pack에서 조회
  - 정의가 없을 때는 기존 상수를 fallback으로 사용해 회귀를 막음

- `Shared/GameSim/Champions/LeeSin/LeeSinGameSim.cpp`
  - `ResolveLeeSinSkillEffectParam(...)` 추가
  - Q2/E/R 효과 수치를 JSON definition pack에서 조회
  - Q2 돌진 함수가 `gap`, `durationSec`를 외부에서 받은 값으로 처리하도록 변경
  - 정의가 없을 때는 기존 상수를 fallback으로 사용해 회귀를 막음

## 4. 일부러 아직 옮기지 않은 값

이번 작업에서 모든 숫자를 무리하게 JSON으로 옮기지는 않았다.

### 4.1 LeeSin Q 표식 지속시간

`ApplySonicWaveMark(CWorld&, EntityID, EntityID)`는 현재 `TickContext`나 definition pack 포인터를 받지 않는다. 이 값을 지금 억지로 데이터화하려면 함수 경계를 같이 바꿔야 한다.

따라서 이번 범위에서는 남겼다.

다음 단계에서 선택지는 둘이다.

- `ApplySonicWaveMark`에 `TickContext` 또는 definition pack 접근 경로를 전달한다.
- Q 표식 지속시간을 SkillEffectParam이 아니라 별도 상태/표식 정책 데이터로 분리한다.

### 4.2 Fiora의 상태 정책성 지속시간

Fiora E/R 일부 지속시간은 단순 효과 수치라기보다 상태 유지 정책에 가깝다. 이번 단계에서는 `SkillEffectParam`으로 섞지 않았다.

의심 기준은 이것이다.

```text
이 값이 즉시 적용되는 효과 수치인가?
아니면 특정 상태/표식/창(window)의 생명주기인가?
```

후자라면 `SkillEffectParam`이 아니라 `SkillStatePolicy`, `BuffPolicy`, `MarkPolicy` 쪽 원자로 분리하는 편이 더 본질에 가깝다.

## 5. 검증 결과

### 5.1 Python 생성기 문법 검증

```powershell
python -m py_compile Tools/LoLData/Build-LoLDefinitionPack.py
```

결과: PASS

### 5.2 Definition pack 생성

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
```

결과:

```text
Generated LoL definition pack 0x1D9A3BE9
Champions: 17, skills: 85, summoner spells: 1
```

### 5.3 Definition pack freshness check

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
```

결과: PASS

### 5.4 Legacy data audit

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
```

결과:

```text
skillEffectHardcodeCandidates = 156
skillEffectDataQueryReaders = 75
```

`skillEffectHardcodeCandidates`가 줄지 않은 이유는 기존 상수를 fallback으로 남겼기 때문이다. 지금 단계에서 fallback을 지우면 정의 누락 시 즉시 회귀할 수 있으므로, 실제 런타임 경로가 전부 definition pack을 받는지 확인한 뒤 제거해야 한다.

### 5.5 전체 DataDriven 파이프라인

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

결과: PASS

주요 확인:

- GameSim build PASS
- Server build PASS
- Client build PASS
- SimLab deterministic regression PASS
- same-seed replay hash: `67F2A97563B8DB04`
- seed+1 hash: `5DA19645E291A29B`
- whitespace validation PASS

검증 중 기존 빌드 경고와 runtime asset copy 단계의 `공유 위반입니다.` 로그가 있었지만 최종 파이프라인은 실패하지 않았다.

## 6. 본질 검토

이번 반영은 아직 완전 컷오버가 아니다.

하지만 방향은 맞다.

```text
코드: 효과를 실행한다.
JSON: 효과 수치를 소유한다.
Generated Pack: JSON을 런타임 불변 데이터로 만든다.
GameSim: pack을 조회해서 서버 권위 판정을 수행한다.
Fallback 상수: 컷오버 중 회귀 방지 장치로만 남는다.
```

아직 의심해야 하는 지점은 fallback 상수다. fallback은 안전장치지만, 오래 남으면 다시 코드가 데이터의 주인이 된다. 따라서 다음 단계의 핵심은 fallback을 바로 삭제하는 것이 아니라, 모든 정상 런타임 호출 경로가 definition pack을 안정적으로 받는지 먼저 닫는 것이다.

## 7. 다음 단계

1. LeeSin Q 표식 지속시간 경계 정리
   - `ApplySonicWaveMark`가 definition pack을 읽을 수 있도록 호출 경계를 바꾼다.
   - 이 값이 SkillEffectParam인지 MarkPolicy인지 다시 의심한다.

2. Fiora 상태 정책 데이터 분리
   - E/R 지속시간류를 SkillEffectParam으로 섞지 말고 상태/표식 정책 원자로 분류한다.

3. 남은 챔피언 효과 수치 컷오버
   - Viego, Yone, Zed, Irelia 등에서 즉시 효과 수치와 상태 정책 수치를 분리해 같은 방식으로 이동한다.

4. fallback 제거 조건 정의
   - 정상 서버/GameSim/SimLab 경로가 모두 definition pack을 받는 것이 확인된 뒤 fallback 삭제를 시작한다.
   - 삭제 단계에서는 `skillEffectHardcodeCandidates` 감소를 실제 성과 지표로 본다.
