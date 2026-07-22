Session - 리 신·야스오·미니언 gameplay closure 적용 결과
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-20_LEESIN_YASUO_MINION_GAMEPLAY_CLOSURE_PLAN.md

## 1. 예측 vs 실측

- PLAN 독립 비평은 4차에서 `PASS`, 잔존 P0 0 / P1 0으로 구현 gate를 통과했다.
- 리 신 Q 예측은 충족했다. server projectile은 speed `19.2`, terrain collision `false`, projectile barrier `true`이며 real projectile `UnitHit`가 source-owned mark와 Q2 stage/window를 재무장한다. Q2는 caller target을 무시하고 server가 정확히 하나인 유효 mark를 선택하며, mark 부재/모호 시 `ChampionRuleBlocked`로 stage/window를 보존한다.
- 리 신 E/R 예측은 충족했다. E1은 반경 data(3.5)의 적 mobile unit에 canonical physical skill damage request와 3초 Tempest mark를 만들고, E2는 살아 있는 source-owned E1 hit 대상만 slow한다. R은 Lee→target 10 world units의 desired landing을 walkable query로 clamp/resolve한다.
- 이동 예측은 충족했다. W는 transit terrain clamp를 유지하고 Q2만 transit clamp를 건너뛰되 공통 arrival walkable snap을 유지한다.
- Bot AI 경계 예측은 충족했다. Lee probe에서 AI는 Q1/Q2/BasicAttack `GameCommand`만 생산하며 mark, 피해, dash, status truth를 직접 변경하지 않는다.
- 시각/데이터 구현은 반영했다. ward는 작은 Lee Q ground mark, Kalista W는 cone만 남겼고 blue/red siege만 authored/generated visual multiplier 1.5를 갖는다. minion collision/spatial 값은 변경하지 않았다.
- 포탑 인접 2 melee 교착 후보는 production과 SimLab이 같은 primary→static-tangent 선택 helper를 사용하도록 반영했다. mixed static+two-soft fixture 및 soft-only 결정성은 PASS했으나 exact red-mid-outer 좌표 live trace는 없다.
- 전 챔피언 variant flat damage `ByRank[5]` F4 확장은 추가 사용자 결정에 따라 병렬 세션 소유로 분리했다. 이 결과는 Yasuo E 0.1과 현재 Yasuo/Lee Q 편집 경로의 계약 검증만 기록한다.

### 자동 검증 실측

- `python Tools/LoLData/Build-LoLDefinitionPack.py --check` — PASS, pack hash `0xE1A001DE`.
- `python Tools/ChampionData/build_champion_game_data.py --check` — PASS, pack hash `0x6820D4E0`.
- `python Tools/LoLData/Test-F4BalanceContracts.py --root .` — PASS.
- `MSBuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1` — PASS.
- `Tools/Bin/Debug/SimLab.exe --leesin-closure-only` — PASS.
- `Tools/Bin/Debug/SimLab.exe` — PASS. same-seed hash `C0ABF1ACB012D797`, seed+1 hash `52176FDFF218D713`.
- `powershell -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomProjectileIntegrationProbe.ps1` — PASS. 기존 passive/turret/projectile 계약과 `leesin_q=1`을 함께 통과했다.
- `MSBuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1` — PASS.
- `MSBuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1` — PASS.
- Shared boundary 검사 — PASS.
- 전체 `git diff --check` — 기존 범위 밖 `Client/Private/Scene/Scene_InGameInput.cpp:244` trailing whitespace 1건 때문에 non-zero. 이 slice의 대상 파일에는 새 whitespace 오류가 없다.

### 검증 중 발견·수정

- 최초 SimLab compile은 `DeterministicEntityIterator` include 누락으로 실패했고 include 추가 후 PASS했다.
- 최초 GameRoom probe는 fixture에 canonical champion identity/stat과 active definition pack이 없어 passive/Q projectile가 실패했다. production 코드를 우회하지 않고 fixture를 실제 spawn 계약에 맞춘 뒤 기존·신규 case가 모두 PASS했다.
- F4 정적 검사에는 현행 변수명이 `kRuntimeFlatOnlySkills`인데 오래된 `kCustomFlatSkills`를 기대하는 fixture가 있었다. 실제 구현 이름에 맞춰 정정 후 PASS했다.

### 수동 시각 검증

- `.md/build/2026-07-20_gameplay-closure/objective-fx/`와 필수 Baron/Elder 6개 캡처는 현재 없다.
- 따라서 ward Q-mark 표현, Kalista cone-only, siege 체감 크기, Baron/Elder attach/follow/death-prune는 Client build 성공과 별개로 `미검증`이다.
- Yasuo Q3 sink/E target-death 시각도 이 slice에서 새 F5 캡처를 만들지 않았으므로 `미검증`이다.

## 2. 판결

- **코드·자동 회귀·Debug build: PASS.** 리 신 Q/E/R server authority, Bot command-only 경계, siege data/runtime 배율, minion deterministic resolver가 계획된 자동 gate를 통과했다.
- **전체 gameplay closure: PARTIAL.** 필수 F5 objective 6캡처와 exact red-mid-outer live 재현이 없으므로 시각/현장 항목을 완료로 판정하지 않는다.
- dirty worktree와 replay 변경은 보존했으며 별도 worktree, reset, checkout, 병렬 MSBuild를 사용하지 않았다. 마지막 점검에 `MSBuild/cl/link`는 없었고 기존 `devenv.exe`와 선행 세션의 `WintersServer.exe`(PID 22392, 00:25 시작)가 실행 중이었다. 소유 불명 runtime process는 종료하지 않았다.

## 3. 대가 갱신

- checkpoint 대상에 `LeeSinTempestMarkComponent`가 추가됐다.
- Q1 hit client stage mirror가 projectile hit dedupe 뒤에 추가됐다. authoritative command result/snapshot이 이후의 최신 truth이며 replay rebase 경로는 기존 소유권을 유지한다.
- siege role 배율은 ClientPublic visual definition의 기본 1.0 필드가 되었고 local/network spawn 및 tuner refresh가 같은 resolver를 사용한다. Baron minion snapshot 배율은 render matrix의 별도 추가 곱으로 유지한다.
- minion 교착 완화는 static blocker가 존재하고 primary clamp가 실패/무이동일 때만 deterministic tangent 후보를 한 번 더 시도한다. allied minion을 hard blocker로 바꾸지 않았다.
- 남은 수동 비용은 objective 6캡처, ward/Kalista/siege F5 판독, exact red-mid-outer 좌표 trace다. 이 증거가 생기기 전에는 해당 항목을 PASS로 승격하지 않는다.
