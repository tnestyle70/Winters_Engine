# Session - Winters DOD Migration P3a SummonPolicy Split

작성일: 2026-06-24

## 1. 목표

이번 컷의 목표는 ECS를 늘리는 것이 아니라, DOD 기준으로 데이터의 의미를 더 작게 분리하는 것이다.

- `SkillEffectSpec`: 스킬이 즉시 발생시키는 피해, 상태이상, 판정 수치만 가진다.
- `SummonPolicySpec`: 스킬이 생성한 소환체가 살아가는 정책만 가진다.
- 런타임 소유권은 유지한다. 새 ECS ownership을 만들지 않는다.
- 서버 권위 흐름은 유지한다.
  - Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual

## 2. 왜 이게 더 본질인가

기존 `eSkillEffectParamId::Summon*` 값들은 이름부터 이미 `SkillEffect`가 아니었다.

- Annie R의 Tibbers는 단발 피해 효과가 아니라 소환체다.
- Kalista W의 Sentinel은 피해 효과가 아니라 정찰 소환체다.
- 따라서 수명, 이동 속도, 시야, 반경, 공격 수치, 체력 수치는 `SkillEffectSpec` 안에 있으면 의미가 섞인다.

이번 컷은 이 혼합을 끊고, "즉시 효과"와 "소환체 정책"을 별도 atom으로 분리했다.

## 3. 반영 내용

### 3.1 Shared atom 추가

수정 파일:

- `Shared/GameSim/Definitions/SkillAtomData.h`
- `Shared/GameSim/Definitions/SkillGameplayDef.h`

반영:

- `eSkillEffectParamId`에서 `Summon*` 항목 제거
- `eSummonPolicyParamId` 추가
- `SummonPolicyParam` 추가
- `SummonPolicySpec` 추가
- `FindSummonPolicyParam`, `ResolveSummonPolicyParam` 추가
- `SkillGameAtomBundle`과 `SkillGameplayDef`에 `summonPolicy` 추가

본질 검증:

- 즉시 효과 조회와 소환 정책 조회가 서로 다른 배열을 본다.
- 메모리 구조는 여전히 flat fixed array다.
- 런타임 polymorphism, ECS ownership, virtual owner를 추가하지 않았다.

### 3.2 Query 경계 추가

수정 파일:

- `Shared/GameSim/Definitions/GameplayDefinitionQuery.h`
- `Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp`

반영:

- `GameplayDefinitionQuery::ResolveSummonPolicyParam(...)` 추가

본질 검증:

- GameSim 코드가 JSON이나 generated cpp를 직접 알지 않는다.
- Champion handler는 "내 스킬의 소환 정책 값 하나"만 요청한다.
- fallback은 기존 하드코딩 상수를 유지해 회귀 위험을 낮췄다.

### 3.3 Generator cutover

수정 파일:

- `Tools/LoLData/Build-LoLDefinitionPack.py`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`
- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
- definition pack hash 관련 파생 JSON

반영:

- `SkillEffectGameplayDefs.json`의 `summonPolicy` 블록 파싱
- `summonPolicyParams` 정규화
- 서버용 `SkillGameplayDefs.json`에 `summonPolicy.params` 방출
- 서버 generated cpp에 `def.summonPolicy` 방출
- definition pack hash 갱신: `0xC1442036`

본질 검증:

- 사람이 편집하는 원본 JSON -> 정규화된 서버 JSON -> generated cpp가 같은 의미 구조를 유지한다.
- 예전 `summonRadius`, `summonSightRange`, `summonDurationSec` 같은 이름은 생성물에서도 제거됐다.

### 3.4 Champion runtime read 변경

수정 파일:

- `Shared/GameSim/Champions/Annie/AnnieGameSim.cpp`
- `Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp`

반영:

- Annie R Tibbers:
  - duration
  - moveSpeed
  - attackRange
  - sightRange
  - attackCooldownSec
  - baseAttackDamage
  - attackDamagePerRank
  - baseHp
  - hpPerRank
  - radius
- Kalista W Sentinel:
  - duration
  - moveSpeed
  - sightRange
  - radius

위 값들은 `ResolveSkillEffectParam` 대신 `ResolveSummonPolicyParam`으로 읽는다.

## 4. 아직 더 의심해야 하는 지점

이번 컷이 최종 본질은 아니다.

남은 의심 지점:

- Annie R의 `roleType`, `lane`은 현재 `summonPolicy` 데이터에는 실렸지만 런타임은 기존 상수를 유지한다.
- `roleType`, `lane`을 f32 param으로 두는 것은 최종 본질이 아니다. 다음 컷에서는 강타입 필드 또는 `SpawnObjectGameplayDef` 참조로 바꾸는 편이 더 맞다.
- Kalista W의 `range`, `halfAngleCos`는 이번 컷에서 유지했다. `range`는 이미 skill range와 중복될 수 있으므로 다음 컷에서 단일 소유자를 정해야 한다.
- `SummonPolicyParam`이 generic f32 key-value 구조인 것도 완전한 본질은 아니다. 값 종류가 안정되면 typed struct field로 승격할 후보다.

이번 컷은 회귀 위험을 낮추기 위해 "예전 summon 접두 값을 즉시 효과에서 제거"하는 데 집중했다.

## 5. 검증

실행 결과:

- `python Tools\LoLData\Build-LoLDefinitionPack.py --root .`
  - PASS
  - Generated LoL definition pack `0xC1442036`
- `python Tools\LoLData\Build-LoLDefinitionPack.py --root . --check`
  - PASS
- old summon param 검색
  - `SummonDurationSec`, `SummonRadius`, `summonDurationSec`, `summonRadius` 등 예전 이름 잔존 없음
- `git diff --check`
  - PASS
- `MSBuild Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
  - PASS
  - 기존 `CommandExecutor.cpp` UTF-8 문자 경고 C4828 존재
- `powershell -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1`
  - PASS
  - Definition pack freshness PASS
  - Client visual timing parity mismatch 0
  - GameSim build PASS
  - Server build PASS
  - Client build PASS
  - SimLab build PASS
  - SimLab deterministic regression PASS
  - same-seed replay hash: `C6B6B27562331CD3`

## 6. Dirty Worktree 주의

이번 작업 시작 전부터 다른 세션/이전 작업의 변경이 섞여 있었다.

대표적으로 다음 범위는 이번 DOD 컷의 직접 목적이 아니며, 되돌리지 않았다.

- Client input/lifecycle/minimap/UI 변경
- Engine turret/UI manager 변경
- 여러 champion dash/skill cooldown 관련 변경
- 기존 plan/result 문서
- `Shared/GameSim/Systems/Move/DashArrival.h`

이번 컷의 직접 변경 축은 `SummonPolicySpec`, definition generator, Annie/Kalista summon read path다.

## 7. 다음 컷 제안

P3b는 `SummonPolicySpec`를 더 본질로 줄이는 방향이 맞다.

1. `roleType`, `lane`을 f32 param에서 제거하거나 강타입 필드로 승격한다.
2. 소환체 종류는 가능하면 `SpawnObjectGameplayDef` 참조 하나로 묶는다.
3. Annie Tibbers의 role/lane/agent setup도 data query로 읽을지, spawn object definition으로 흡수할지 결정한다.
4. Kalista W의 `range` 중복 소유자를 정리한다.
5. 그 다음 남은 champion hardcode candidate를 "효과 수치", "상태 정책", "이동 정책", "소환 정책"으로 나눠 계속 제거한다.
