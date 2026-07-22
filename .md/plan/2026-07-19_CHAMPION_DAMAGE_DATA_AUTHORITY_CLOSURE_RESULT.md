Session - 챔피언 수치 권위 경로 정합화 결과

## 1. 예측 vs 실측

- 적중: 최신 authoring JSON이 generated pack보다 앞서 있었다. 적용 전 F4 parity는 Ezreal Q rank 3 total AD에서 canonical `2.0` / generated `5.0` 불일치를 검출했고, 두 generator `--check`도 `STALE`이었다.
- 적중: 사용자 수치를 되돌리지 않고 generator를 직렬 실행한 결과 definition pack은 `0x8B7942C0`, ChampionGameData는 `0xAEE644A0`으로 갱신됐다. generator가 읽는 canonical 11개 입력의 실행 전후 SHA-256은 모두 동일했다.
- 적중: `SkillEffectGameplayDefs.json -> SkillGameplayDefs.json`의 85스킬·323 rank case·여섯 damage 배열은 불일치 0건이고, 두 generator `--check`가 PASS했다.
- 적중: direct pack oracle과 `BuildSkillDamageRequest`, `DamageQueue` 적용 피해를 분리한 SimLab은 17 champions / 85 skills / 323 rank cases를 PASS했다. custom-flat 4종은 sentinel flat 37 보존, F4 ratio override 5종은 최종 피해 반영을 각각 PASS했다.
- 적중: Kindred W는 실제 `W_CastFrame` rank 4를 캡처한 뒤 live W rank를 1로 바꿔도 request rank 4 / slot W / source kind Skill을 유지했다.
- 적중: 낡은 Ezreal Q `70/1.3/0.4` 기대값은 제거하고 현재 pack의 rank 배열/type/flags를 읽어 검증하도록 바꿨다.
- 범위 분리: 전체 F4 정적 스크립트의 `gold.text.center` expected 707 / actual 734는 소스 기본값 회귀가 아니다. `ActorHUDPanel` 기본값은 여전히 `[707,142]`이고 ignored runtime `Client/Bin/Resource/UI/hud_irelia_layout.json`에만 사용자 튜너 저장값 `[734,148.5]`가 남아 있다. 계약 기대값 707은 유지하며 로컬 override mismatch로 분리한다.
- 빌드 완료: current generated pack 기준 Debug x64 `GameSim`, `SimLab`, `Server`, `Client`가 모두 PASS했다.
- 챔피언 수치 전용 런타임 검증 완료: current `SimLab.exe --f4-balance-only`에서 `[DamageAuthority] 17 champions / 85 skills / 323 rank cases + live overrides + Kindred cast rank`, FormulaData, CommandResultWire, PracticeMinionAD, CooldownReload가 모두 PASS했다.
- 공유 트리 재검증: 병렬 세션이 JungleAI include, Irelia header, UI tuner 시그니처 blocker를 해소했고 Debug x64 네 타깃 빌드와 Irelia Q 전체/targeted 회귀를 PASS했다. same-seed world keyframe 불일치도 `ChampionAssistCreditComponent` implicit padding을 ABI 크기 유지 explicit-zero reserved field로 교정한 뒤 Debug 1800틱 x2 replay/world hash 일치를 확인했다. generated definition pack은 추가 재생성하지 않았다.
- 통합 후 재실행: 최신 Debug `SimLab.exe --f4-balance-only`를 다시 실행해 DamageAuthority 17/85/323, FormulaData, CommandResultWire, PracticeMinionAD, CooldownReload 및 최종 `[SimLab] PASS`, exit 0을 확인했다.
- 형식 검증 완료: `git diff --check` exit 0. 출력은 기존 공유 작업 트리 파일들의 LF→CRLF 경고뿐이며 whitespace error는 없다.

전수 변경 보존 결과:

- `champions.json`: 17개 챔피언 record 모두 현재 값 보존, HEAD 대비 numeric 변경 60건. Irelia/Viego AD·성장 AD, 전 챔피언 BA windup/resource regen, cooldown/mana/range 변경을 generator에 승격했다.
- `SkillEffectGameplayDefs.json`: 변경 record 16개, numeric 변경 134건. Ashe R, Ezreal E/Q/W, Fiora W, Irelia E/Q/R/W, Kalista E/Q, Sylas Q/W, Viego Q/R/W의 현 값을 보존했다.
- `params.*`와 `damage.*ByRank`가 중복되는 스킬에서 최종 일반 스킬 피해의 canonical은 `damage.*ByRank`다. Yasuo Q, Kalista E, Lee Sin Q, Ezreal R만 custom flat을 유지하고 canonical ratio를 합친다.

미구현 분리:

- Riven Q/W, Master Yi Q/W/E, Lee Sin E 등 damage request 자체가 없는 기능은 수치 전달 경로 수정으로 새 gameplay를 만들지 않았다. 해당 값은 `NOT_IMPLEMENTED`로 남는다.
- production rank clamp helper 독립 복제와 Kindred keyframe round-trip은 이번 범위에서 미검증이다.

## 2. 판결

수정 반영 및 챔피언 수치 범위 종료. 데이터 소유권·F4 ratio override·Kindred W rank·낡은 Ezreal 테스트·generated freshness 결함을 교정했고, current pack으로 네 타깃 빌드와 전용 SimLab 행렬을 통과했다. 이전 Irelia Q/통합 빌드 blocker도 owner 세션 재검증에서 해소됐다. 남은 HUD 정적 mismatch는 소스 기본값을 바꿀 사유가 아니라 ignored runtime 사용자 override를 정리하거나 의도적으로 다시 저장할 때 해소할 로컬 상태다.

## 3. ⑤ 갱신

두 피해 표현을 즉시 합치지 않은 대가는 남는다. F4/코드에서 `params`를 편집하더라도 일반 스킬 최종 피해는 canonical rank 배열이 소유하므로, 이후 UI는 중복 필드를 소유권별로 비활성화하거나 schema migration으로 제거해야 한다. 반면 이번 행렬은 수치 자체를 하드코딩하지 않아 다음 밸런스 조정이 테스트 회귀로 오인되는 문제를 제거했다.

## 4. 2026-07-19 Release 실플레이 재개방

- 이전 종료 판결은 20:02 generated pack 시점에는 맞았지만, 22:42~22:43 F4 저장 이후 Release cook이 다시 수행되지 않아 현재 상태에는 적용할 수 없다.
- 실측: authoring은 Yasuo Q total AD 2.0, Lee Sin Q total AD 1.0, Yasuo E cooldown 0.1인데 generated `SkillGameplayDefs.json`은 각각 0.0, 0.0, 3.0이다. 두 generator `--check`는 모두 `STALE`이다.
- 판결: `수정 반영 및 범위 종료`를 일시 재개방한다. flat 누락이나 skill cooldown clamp가 아니라 Debug authoring/hot-load와 Release compiled pack 사이의 미승격이 원인이다.
- 다음 완료선: §5 델타 독립 비평 통과 → 두 generator 직렬 cook → Release SimLab/Server/Client 빌드 → named-field parity/`--check`/Release damage-authority PASS → 실클라 체감 확인.
## 2026-07-19 Release cook·재빌드 최종 결과

- 사용자 추측 중 `flat damage가 비어 있다`는 부분은 기각됐다. Yasuo Q는 flat 60, Lee Sin Q는 rank별 flat 55~155가 이미 존재했다.
- 실제 원인은 Release generated pack이 stale이어서 Yasuo Q total AD ratio 2.0과 Lee Sin Q total AD ratio 1.0이 0으로 남아 있던 것이다.
- Yasuo E 0.1초를 3초로 올리는 서버 cooldown clamp는 없었다. 별도 규약인 0.4초 action lock과 동일 대상 10초 lockout은 유지된다.
- Debug F4 hot-load는 `_DEBUG` 전용이다. Release는 저장 JSON을 직접 읽지 않고 generator cook과 재빌드를 거쳐야 한다.

### 반영된 generated truth

- ChampionGameData hash: `0x6820D4E0`.
- LoL definition pack hash: `0x4496BB4B`.
- Yasuo Q total AD ratio `2.0`, Lee Sin Q total AD ratio `1.0`, Yasuo E cooldown `0.1초`.

### 검증

- 두 generator `--check` PASS.
- `Test-F4BalanceContracts.py --root .` PASS. 런타임에서 사용자가 저장한 HUD 튜너 위치는 유효한 값인지 검사하고, canonical 기본 좌표의 정확값은 source default에서 별도로 검사하도록 계약을 바로잡았다.
- Release SimLab `--f4-balance-only` PASS: 17 champions / 85 skills / 323 rank cases, live ratio overrides, Kindred rank, formula data, cooldown reload.
- Release SimLab 전체에서 DamageAuthority, FormulaData, IreliaQReset, StructureRemnant는 PASS. 전체 exit 1의 유일한 선행 실패는 별도 범위인 `[Shield] FAIL: minion damage triggered Yasuo passive`다.
- Release Server와 Client 링크 PASS.

판정: Yasuo/Lee Sin Q AD 계수 및 Yasuo E Release data authority는 완료. 별도 Yasuo passive shield 회귀는 이 결과의 잔여 외부 결함으로 분리한다.
