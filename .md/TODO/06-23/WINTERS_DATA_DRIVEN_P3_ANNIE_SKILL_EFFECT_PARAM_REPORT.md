# Winters DataDriven P3 Annie SkillEffectParam 결과 보고서

작성일: 2026-06-23

## 1. 목표

Annie Q/W/E/R의 스킬 효과 수치를 코드 상수 중심에서 `ServerPrivate` JSON 기반 정의팩으로 이동했다.

이번 단계의 본질은 다음 하나다.

```text
수치의 소유권은 데이터가 가진다.
실행 규칙은 GameSim 코드가 가진다.
Shared는 데이터가 어떤 원자 이름으로 조회되는지만 가진다.
```

## 2. 반영 범위

### 데이터

반영 파일:

- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

추가한 Annie 데이터:

- `skill.annie.q`
  - `baseDamage`
  - `damagePerRank`
  - `stunDurationSec`
- `skill.annie.w`
  - `baseDamage`
  - `damagePerRank`
  - `range`
  - `halfAngleCos`
  - `stunDurationSec`
- `skill.annie.e`
  - `shieldDurationSec`
  - `shieldBaseAmount`
  - `shieldAmountPerRank`
  - `shieldArmorPerRank`
  - `moveSpeedMul`
- `skill.annie.r`
  - `baseDamage`
  - `damagePerRank`
  - `range`
  - `radius`
  - `stunDurationSec`
  - `summonDurationSec`
  - `summonMoveSpeed`
  - `summonAttackRange`
  - `summonSightRange`
  - `summonAttackCooldownSec`
  - `summonBaseAttackDamage`
  - `summonAttackDamagePerRank`
  - `summonBaseHp`
  - `summonHpPerRank`
  - `summonRadius`

### Shared 원자 이름

반영 파일:

- `Shared/GameSim/Definitions/SkillAtomData.h`

추가한 원자:

- `HalfAngleCos`
- `Radius`
- `ShieldDurationSec`
- `ShieldBaseAmount`
- `ShieldAmountPerRank`
- `ShieldArmorPerRank`
- `SummonDurationSec`
- `SummonMoveSpeed`
- `SummonAttackRange`
- `SummonSightRange`
- `SummonAttackCooldownSec`
- `SummonBaseAttackDamage`
- `SummonAttackDamagePerRank`
- `SummonBaseHp`
- `SummonHpPerRank`
- `SummonRadius`

이 이름들은 Annie 전용 이름이 아니다. 스킬 효과 수치가 더 이상 `AnnieGameSim.cpp`의 하드코딩 필드로만 갇히지 않도록, 모든 챔피언이 공유할 수 있는 최소 단위 이름으로만 추가했다.

### 생성기

반영 파일:

- `Tools/LoLData/Build-LoLDefinitionPack.py`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`

생성팩은 `SkillEffectGameplayDefs.json`의 Annie 값을 읽어서 `LoLGameplayDefinitions.generated.cpp`에 포함한다.

최신 정의팩 해시:

```text
0x22933D1B
```

### 런타임

반영 파일:

- `Shared/GameSim/Champions/Annie/AnnieGameSim.cpp`

변경 내용:

- Q/W/R 피해량과 스턴 시간을 정의팩에서 조회한다.
- W의 사거리와 콘 각도를 정의팩에서 조회한다.
- E의 보호막 시간, 보호막량, 방어/MR 보정, 이동속도 배율을 정의팩에서 조회한다.
- R의 사거리, 반경, 피해량, 스턴 시간, Tibbers 소환 전투 수치를 정의팩에서 조회한다.
- `TibbersSpawnTuning`을 추가해서 Tibbers의 소환 수치를 한 묶음으로 해석한다.

## 3. 의심한 결과

### 정말 데이터여야 하는 값인가?

이번에 이동한 값은 전부 디자이너/밸런스 패치 대상이다.

- 피해량
- 계수
- 거리
- 반경
- 지속시간
- 이동속도 배율
- 소환체 HP/공격력/공격거리/시야/지속시간

따라서 코드가 아니라 데이터가 소유하는 것이 맞다.

### 아직 코드에 남긴 값은 왜 남겼나?

다음 값은 이번 단계에서 의도적으로 남겼다.

- `kStunThreshold`
- `kEShieldBuffDefId`
- `kTibbersRoleType`
- `kTibbersLane`

이 값들은 단순 스킬 효과 수치라기보다 패시브 조건, 버프 식별자, 소환체 정책에 가깝다.
즉 `SkillEffectParam`으로 억지 편입하면 원자 단위가 흐려진다.

다음 단계에서 별도 원자로 분리해야 한다.

```text
PassivePolicy
BuffPolicy
SummonPolicy
```

### fallback은 본질인가?

아니다.

현재 fallback 상수는 로컬 smoke나 정의팩이 없는 임시 실행 경로의 회귀 방지 장치다.
최종 DataDriven 구조에서는 `TickContext.pDefinitions`가 모든 GameSim 실행 경로에 들어가야 하고, 그 뒤 fallback은 삭제 대상이다.

## 4. 검증 결과

### 생성기 문법

```text
python -m py_compile Tools/LoLData/Build-LoLDefinitionPack.py
PASS
```

### 정의팩 생성

```text
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
PASS
hash=0x22933D1B
```

### 정의팩 최신성 체크

```text
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
PASS
```

### 전체 DataDriven 파이프라인

```text
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
PASS
```

검증에 포함된 단계:

- Definition pack freshness
- Legacy ownership audit
- Client visual timing parity
- `Shared/GameSim/Include/GameSim.vcxproj`
- `Server/Include/Server.vcxproj`
- `Client/Include/Client.vcxproj`
- SimLab deterministic replay
- Whitespace validation

SimLab 결과:

```text
same-seed replay OK: hash=67F2A97563B8DB04
seed sensitivity OK: seed+1 hash=5DA19645E291A29B
```

주의:

- 첫 전체 검증 중 Server 링크 단계에서 이전 중간 산출물 때문에 `CServerProjectileAuthority` 미해결 심볼이 보였다.
- Server를 clean/build한 뒤 전체 파이프라인을 재실행했고 최종 PASS를 확인했다.
- Client 빌드에는 기존 C4251/C4275/C4828 계열 경고가 남아 있다.
- 이번 Annie 파일은 빌드 가능한 UTF-8 상태로 정리했고, 깨진 한글 주석 한 줄은 ASCII 주석으로 교체했다.

## 5. 남은 리스크

현재 구조는 기능 회귀 없이 한 단계 더 DataDriven에 가까워졌다.
하지만 아직 완전한 컷오버는 아니다.

남은 리스크:

- GameSim 코드에 fallback 상수가 남아 있다.
- `SkillEffectParam`이 커지고 있어, 다음부터는 모든 값을 여기에 밀어 넣으면 안 된다.
- 소환체 정책, 버프 정책, 패시브 정책은 별도 데이터 원자로 나눠야 한다.
- 감사 스크립트가 아직 챔피언별 skill effect 하드코딩 잔존량을 직접 집계하지 않는다.

## 6. 다음 진행 방향

다음 단계는 값을 더 옮기는 것이 아니라, 값의 종류를 더 의심해서 원자 경계를 나누는 것이다.

우선순위:

1. `Collect-LoLLegacyDataAudit.ps1`에 `skillEffectHardcode` 계열 집계를 추가한다.
2. 로컬 smoke와 서버 실행 경로 모두 `TickContext.pDefinitions`를 보장한다.
3. fallback 상수 삭제 가능 여부를 챔피언별로 감사한다.
4. `PassivePolicy`, `BuffPolicy`, `SummonPolicy`의 최소 JSON/생성팩 구조를 설계한다.
5. Annie의 `kStunThreshold`, `kEShieldBuffDefId`, `kTibbersRoleType`, `kTibbersLane`을 새 정책 데이터로 이동한다.

최종 북극성:

```text
코드는 규칙을 실행한다.
데이터는 수치를 소유한다.
정책은 수치와 실행 사이의 의도를 이름 붙인다.
클라이언트는 시각화를 소유하고, 서버/GameSim은 판정 가능한 진실만 소유한다.
```
