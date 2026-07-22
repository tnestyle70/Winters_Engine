Session - Lee Sin W1과 Riven E 실드 WFX를 최신 초록 팔레트로 복구하고 재발 방지 계약을 연결했다.

# 2026-07-20 LeeSin/Riven WFX Tint Regression Result

## 0. 판정

- 데이터·자동 계약·Debug/Release Client build: `PASS`
- Release 실제 화면 확인: `USER_VERIFY_NEEDED`
- 실행 중인 Release 서버 두 프로세스는 종료하거나 재시작하지 않았다.

과거 흰색은 2026-07-14 공통 shield 작업의 의도된 결정이었다. 2026-07-20 사용자가 Lee Sin W1과 Riven E를 모두 초록 계열로 바꾸도록 명시했으므로, 이번 결과가 두 champion WFX palette에 한해 과거 결정을 supersede한다. 공통 gameplay shield, 3초 duration, 흰색 effective-health bar, Yasuo passive WFX는 변경하지 않았다.

## 1. 반영 결과

### Lee Sin W1

- floor ring, main bubble, outer glow를 초록·청록 RGB로 복구했다.
- outline과 interior의 기존 금색 accent를 복구했다.
- 5개 emitter의 alpha, texture, size와 3초 main shield lifetime은 유지했다.

### Riven E

- 기존 mesh, flare, rune, ground flash의 마지막 정상 초록 RGB를 복구했다.
- 공통 흰색 shield 작업에서 추가된 main bubble과 outer glow에도 기존 Riven main/flare 초록 RGB를 적용했다.
- 6개 emitter의 alpha, texture, lifecycle과 3초 duration은 유지했다.

## 2. 전수 감사와 재발 방지

- `Data/LoL/FX` 전체 150개 WFX / 516개 emitter를 JSON·emitter object·존재하는 RGBA shape 기준으로 검사했다.
- 전체 git WFX 이력의 emitter별 chromatic-to-grayscale 전이를 비교했다. 중복 브랜치 커밋을 제외하면 과거 공통 흰색 shield 작업의 Lee Sin W1과 Riven E 두 파일만 검출됐다.
- 일반 WFX의 합법적인 optional `name`/`color` 계약은 강제하지 않는다.
- Lee Sin 5개 + Riven 6개, 총 11개 보호 tint는 이름 중복·RGBA shape·exact value를 검사한다.
- `Verify-LoLDataDrivenPipeline.ps1`에 `Protected WFX tint regression` 단계를 연결했다.
- `.claude/gotchas.md`와 과거 white-shield 결과 문서에 최신 palette ownership을 기록했다.

## 3. 검증

| 검증 | 결과 |
|---|---|
| 독립 계획 재비평 | PASS, P0=0/P1=0 |
| Python syntax | PASS |
| `Test-WfxTintRegression.py` | PASS, 150 files / 516 emitters / 11 protected tints |
| 전체 WFX JSON parse | PASS, 150 files |
| Client Debug x64 build | PASS |
| Client Release x64 build | PASS |
| scoped `git diff --check` | PASS |
| 전체 LoL data-driven pipeline | BLOCKED before WFX stage by unrelated stale definition pack |

전체 pipeline은 다른 병렬 데이터 변경으로 `ChampionGameplayDefs.json`, `SkillGameplayDefs.json`, `DefinitionManifest.json`, server/client generated definitions, parity JSON이 stale여서 첫 freshness 단계에서 종료됐다. 이번 WFX는 generator 소유 데이터가 아니며, 병렬 변경을 덮지 않기 위해 generator를 실행하지 않았다. 신규 WFX 계약은 독립 실행으로 PASS했다.

## 4. Release 확인 방법

WFX registry는 client process에서 `Data/LoL/FX`를 한 번 preload한다. 수정 전에 실행 중이던 클라이언트는 재시작하거나 WFX tool에서 reload해야 한다. 검사 시점에는 `WintersGame.exe` 프로세스가 없었고 Release client를 새로 링크했으므로 다음 실행부터 새 팔레트를 읽는다.

Release에서 다음 두 장면만 확인하면 시각 gate가 닫힌다.

1. Lee Sin W1: 흰 구체가 아니라 초록·청록 bubble과 금색 accent가 3초간 Lee Sin을 따라간다.
2. Riven E: mesh/bubble/glow가 초록색이며 Additive 중첩에도 중심 전체가 흰색으로 포화되지 않는다.
