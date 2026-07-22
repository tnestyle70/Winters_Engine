Session - F4 챔피언 랭크·공격 형태별 데미지 편집을 서버 권위와 Release cook까지 일치시켰다.

# 2026-07-20 F4 Champion Ranked Variant Damage Closure Result

## 0. 판정

- 결과: `PASS`
- 독립 계획 비평: 3차 비평에서 P0 0건, P1 0건으로 통과한 뒤 구현했다.
- 생성된 gameplay definition pack hash: `0xDA9C4394`
- 적용 범위: 17 champions / 85 skill rows / 323 rank cases와 별도 공격 형태 5종.

사용자가 지정한 Yasuo Q 값은 다음과 같이 서버 권위 JSON, 생성 pack, GameSim 조회, F4 편집 행에 동일하게 반영됐다.

| 공격 형태 | Rank 1 | Rank 2 | Rank 3 | Rank 4 | Rank 5 |
|---|---:|---:|---:|---:|---:|
| Q1/Q2 Flat Damage | 60 | 80 | 100 | 120 | 140 |
| Q3 Tornado Flat Damage | 100 | 120 | 140 | 160 | 180 |
| EQ Flat Damage | 70 | 90 | 110 | 130 | 150 |

## 1. 구현 결과

### 1-1. F4 편집 모델

- `Balance > Skills`의 선택 범위를 `Passive / Basic Attack`, `Q`, `W`, `E`, `R`로 확장했다.
- 일반 스킬 피해는 기존 canonical `damage.*ByRank` 행에서 편집한다.
- 같은 스킬 안에서 실제 공격 형태만 다른 피해는 동일 랭크 테이블에 별도 행으로 표시한다.
- 기존의 중복 `Runtime Damage Params` 편집 영역은 제거했다.
- `Save & Hot Load`의 ranked variant JSON 저장과 Debug server overlay 적용이 같은 배열 shape를 사용하도록 맞췄다.
- Release는 Debug hot-load 값을 읽는 구조가 아니라, JSON을 cook하여 생성된 pack을 링크하는 구조다. 이번 결과에는 cook 및 Release 재빌드까지 포함했다.

### 1-2. 별도 공격 형태 배열

| 챔피언 | canonical 피해 | 별도 ranked 피해 |
|---|---|---|
| Yasuo | Q1/Q2 | Q3 Tornado, EQ |
| Lee Sin | Q1 | Q2 Recast |
| Kalista | E Rend Base | Damage Per Spear |
| Ezreal | R Champion/Epic | R Non-Epic |

배열이 허용되는 effect parameter는 위 5개 정확한 `(champion, slot, key)` 조합으로 제한했다. 그 밖의 scalar mechanics가 실수로 배열로 바뀌면 generator와 runtime overlay가 거부한다.

### 1-3. 중복 데미지 원본 제거

- Ashe Q, Fiora E, Jax W/R, Kindred E 강화 기본 공격은 중복 `params.baseDamage` 대신 해당 스킬의 canonical rank flat damage를 사용한다.
- Zed R missing-health ratio도 중복 scalar parameter가 아니라 canonical rank ratio를 사용한다.
- Kindred E는 발동 시점의 랭크를 저장해 이후 기본 공격 적중 시 같은 랭크 값을 소비한다.
- scalar parameter의 기존 의미는 유지했다. ranked resolver에 scalar를 넣으면 모든 요청 랭크에서 rank 1 값으로 clamp된다.

### 1-4. 실행 코드가 없는 항목의 처리

- Riven Q/W와 Master Yi Q/E는 현재 서버 GameSim에 damage request 실행 자체가 없다.
- 이 항목은 F4에서 숫자를 바꿀 수 있는 척하지 않고 `Server damage execution: NOT_IMPLEMENTED`와 비활성 피해 행으로 표시한다.
- Master Yi W는 의도된 회복/channel 스킬이므로 non-damaging으로 표시한다.
- Lee Sin E와 Riven R처럼 canonical damage path가 이미 존재하는 항목은 정상 편집 대상으로 유지했다.

이는 F4 데이터 편집 범위를 닫은 결과이며, Riven Q/W 또는 Master Yi Q/E의 신규 전투 구현은 별도의 gameplay 기능 범위다.

## 2. 권위 흐름

```text
Debug F4 edit
  -> typed balance command / runtime overlay
  -> Server GameSim ranked resolver
  -> DamageRequest
  -> DamagePipeline

Release authoring
  -> SkillEffectGameplayDefs.json
  -> generator validation/cook
  -> LoLGameplayDefinitions.generated.cpp
  -> Server GameSim ranked resolver
  -> DamageRequest
  -> DamagePipeline
```

클라이언트 F4는 값을 보여주고 편집 요청을 만들 뿐 실제 피해 결과의 소유자가 아니다.

## 3. 검증 결과

| 검증 | 결과 |
|---|---|
| Python generator syntax compile | PASS |
| `SkillEffectGameplayDefs.json` parse | PASS |
| `Test-F4BalanceContracts.py --root .` | PASS |
| generator freshness `--check` | PASS, hash `0xDA9C4394` |
| GameSim Debug build | PASS, 0 errors |
| SimLab Debug build | PASS, 0 errors |
| Debug SimLab `--f4-balance-only` | PASS |
| Debug full SimLab | PASS |
| Server Debug build | PASS, 0 errors |
| Client Debug build | PASS, 0 errors |
| SimLab Release build | PASS, 0 errors |
| Release SimLab `--f4-balance-only` | PASS |
| Release full SimLab | PASS |
| Server Release full link | PASS, isolated output |
| Client Release full link | PASS, isolated output |

SimLab의 ranked variant probe는 원본 pack 조회와 public gameplay query 양쪽에서 Yasuo Q3/EQ, Lee Sin Q2, Kalista E spear, Ezreal R non-epic의 전 랭크 값을 비교한다. 기존 damage-authority matrix도 17/85/323 전 케이스를 계속 통과한다.

Release 서버의 정상 출력 파일은 실행 중인 PID 22392가 잠그고 있어 해당 프로세스를 종료하지 않았다. 동일 소스와 설정을 격리 출력 폴더로 full link하여 검증했고, Release 클라이언트는 이후 정상 출력 경로에도 다시 링크해 통과했다.

- Server: `.md/build/f4-ranked-release-server/WintersServer.exe`
- Client isolated evidence: `.md/build/f4-ranked-release-client/WintersGame.exe`
- Client normal output: `Client/Bin/Release/WintersGame.exe`

기존 C4251/C4275 및 isolated link의 PDB 관련 LNK4020 경고는 남아 있으나 컴파일·링크 오류는 0건이다.

## 4. Diff 위생과 수동 확인

- 본 변경 파일의 whitespace 오류는 없다.
- 전체 worktree `git diff --check`에는 다른 병렬 작업 소유 파일 `Client/Private/Scene/Scene_InGameInput.cpp:244`의 trailing whitespace 1건이 남아 있어 이 세션에서는 수정하지 않았다.
- 자동 검증은 데이터 shape, overlay, query, 실제 GameSim 소비, Debug/Release 생성 pack을 모두 닫았다.
- 실제 F4 창에서 마우스로 값을 바꾸고 Save/Hot Load를 누르는 UX smoke는 실행 중인 Release 세션을 보존하기 위해 수행하지 않았다. 다음 Debug room-host 수동 확인에서는 Yasuo Q3 한 랭크를 변경해 서버 적중 피해가 즉시 바뀌고, Reload JSON 후 원복되는지만 확인하면 된다.

## 5. 변경 파일 요약

- 데이터·생성: `SkillEffectGameplayDefs.json`, `Build-LoLDefinitionPack.py`, generated gameplay pack, `DefinitionManifest.json`
- 공용 계약·조회: `SkillAtomData.h`, `GameplayDefinitionQuery.h/.cpp`
- Debug overlay: `RuntimeGameplayDefinitionOverlay.cpp`
- F4 UI: `ChampionTuner.cpp`
- GameSim 소비: Yasuo, Lee Sin, Kalista, Ezreal, Ashe, Fiora, Jax, Kindred, Zed
- 회귀: `Test-F4BalanceContracts.py`, `Tools/SimLab/main.cpp`
- 안정 규칙: `WINTERS_CODEBASE_COMPASS.md`
