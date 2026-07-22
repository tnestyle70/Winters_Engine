Session - Irelia W release 회귀를 자동 재현·복구하고 전 챔피언 Data-Driven 행동 계약의 안전 이관 경계를 확정
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_PLAN.md, 2026-07-18_DATA_DRIVEN_GAMEPLAY_REGRESSION_AUDIT_RESULT.md

# 1. 예측 vs 실측

## 1-1. 핵심 가설

| ID | 예측 | 실측 | 판정 |
|---|---|---|---|
| P0 | W1 전송 성공과 `stageCount==2`면 client stage가 arm된다 | 사용자 BP에서 `currentStage=1`, window=4.0 설정 후 `true` 반환 | 적중. 이 재현의 근본은 stageCount 붕괴가 아님 |
| P1 | 한 프레임 `IsKeyReleased` 의존은 focus/ImGui/early return에서 유실 가능 | 코드상 edge는 한 프레임이며 사용자 BP에서는 release 분기 미진입 | 원인 범위 확정. 실제 `WM_KEYUP` trace 재현은 미실행 |
| P2 | W2가 서버에 수락되기 전 E는 W1 action에 막힌다 | SimLab에서 W1 뒤 E가 `ActionBlocked` | 적중 |
| P3 | W2를 `Allow`로 바꾸면 W2와 같은 tick에 Move/E가 수락된다 | `RunIreliaWReleaseRegressionProbe`에서 W2, Move, E 모두 수락 | 적중 |
| P4 | Ezreal E의 Direction/Ground 계약이 어긋난다 | 수동 등록은 Direction, server는 ground를 소비 | 적중. client 등록을 `GroundTarget`으로 정렬 |
| P5 | 2단계 수치만으로 Viego W charge가 완성되지는 않는다 | server에 hold 시작 tick/charge ratio/curve 상태가 없음 | 적중. 이번 slice에서는 임의 구현하지 않음 |

## 1-2. 자동 전수 검사

- Release SimLab의 DataContract: **17 champions / 85 skills / 13 two-stage contracts PASS**.
- 13개 모두 출력상 `activation=command-itemId`다. 즉 stage command는 있지만 `PressRelease`/`PressRecast` 의미는 아직 데이터에 없다.
- Irelia W 회귀 probe: W1은 E를 차단하고, W2 수락 뒤 Move와 E는 같은 tick에 수락됨을 확인했다.
- 전체 Release SimLab `120 ticks / seed 42`: 기존 status, shield, projectile, AI, keyframe, formula, rank, replay hash를 포함해 PASS.
- canonical source의 raw authored stage 수는 **97**이다. 85 skill 중 13개가 2단계라 완전 물질화 기대치는 **98**이며, 한 stage가 암묵 기본값에 의존한다.
- `Conditional` authored target은 **30개**다. 이는 활성화 타입이 아니라 target schema 손실 신호다.

## 1-3. 생성·빌드 실측

```text
ChampionGameData --check: PASS, hash 0x780B63F7
LoL Definition Pack --check: PASS, hash 0x8C5D9212
Client Debug: PASS, 0 errors
SimLab Release: PASS, 0 errors
Client Release: PASS, 0 errors
Server Release: PASS, 0 errors
git diff --check (회귀 관련 파일): PASS, line-ending warnings only
```

Release Client 첫 시도에서 `flatc`가 인자 없이 종료됐지만 `Shared/Schemas/run_codegen.bat` 단독 실행은 PASS했고, 직렬 재시도에서 Client 전체 compile/link가 PASS했다. 소스 실패가 아니라 코드젠 실행의 일시적 실패로 판정한다.

`Verify-LoLDataDrivenPipeline.ps1 -RequireComplete`는 **11/12**로 실패했다. 남은 항목은 `Shared/GameSim/Champions/Zed/ZedGameSim.cpp`의 gameplay tuning literal 1개다. 이 11/12는 데이터 소유권 게이트이며 activation/hold 행동 완성도 수치가 아니다.

# 2. 판결: 수정 반영, 전면 이관은 보류

최종 방향은 사용자 설명과 완전히 일치한다. Data Driven 자체가 잘못된 것이 아니라, 수치 소유권을 먼저 옮기면서 activation·stage별 target·action policy·charge·server accept 결과가 코드 여러 곳에 남은 과도기 계약 불일치가 회귀를 만들었다.

이번 slice에서 유지할 수정은 다음과 같다.

- W release를 한 프레임 edge만이 아니라 `pending && !WDown` 상태 전이로 복구한다.
- network command를 실제로 보내지 못하면 client stage를 arm/clear하지 않는다.
- Irelia W2 action policy를 `Allow`로 하여 수락 직후 이동/E를 허용한다.
- Irelia W hold FX active flag를 실제 재생 결과로 보존한다.
- Ezreal E와 Viego R의 client payload shape를 authored/server 계약에 맞춘다.
- Client bootstrap에 17×85 registry/game-atom 감사를 추가하고, SimLab에 17×85/13-stage 및 Irelia W 회귀 probe를 추가한다.
- `[WGate][ClientInput]`, `[StageGate][ClientDispatch]`, `[StageGate][ServerAction]` bounded trace를 남긴다.

그러나 현재 상태를 “전 챔피언 완전 복구” 또는 “100% 행동 Data Driven”으로 판정하지 않는다. 특히 non-Zed W 공통 입력 분기는 pending stage가 있으면 key-up을 W2로 보내므로, 두 번째 press가 맞는 Lee Sin W 같은 스킬의 activation을 아직 구분하지 못한다.

## 2-1. 회귀 없이 완전 이관하는 순서

1. **행동 기준선 동결**: 17×85 skill의 실제 stage 전체를 `activation / target / command payload / move policy / lock / charge / cancel` 행렬로 만든다. 13개 2단계는 `PressRelease 2개(Irelia W, Viego W)`와 `PressRecast 11개`로 명시한다.
2. **스키마 선추가, 소비자 미전환**: `activationMode`, stage별 `targetMode`, `movePolicy`, `commandLockSec`, charge/cancel spec을 JSON과 generated type에 먼저 추가한다. 누락 stage와 기본값은 generator error로 만든다.
3. **공통 원본 parity**: 두 생성기가 같은 원본에서 같은 의미를 냈는지 client/server generated 결과를 기계 비교한다. 85 skill과 기대 98 authored stage에서 mismatch 0이 gate다.
4. **shadow read**: 기존 수동 계약과 새 generated 계약을 동시에 읽어 차이만 기록하고 gameplay에는 아직 기존 값을 쓴다. 차이가 0이 될 때까지 owner를 바꾸지 않는다.
5. **계약 축별 cutover**: 수치 → stage target → activation → action policy → charge 순으로 한 축씩 전환한다. 여러 축과 여러 챔피언을 한 번에 바꾸지 않는다.
6. **server truth 완성**: charge 시작/해제/timeout/death/stun/disconnect를 server state로 만들고 Client와 Bot은 `GameCommand`만 생산한다. send 성공과 server accept를 구분하는 command result/authoritative stage도 추가한다.
7. **legacy 삭제는 마지막**: generated parity, command-contract, SimLab, Debug/Release build, 정상 F5 matrix가 모두 통과한 뒤에만 수동 target/stage/action fallback을 삭제한다.

각 단계는 이전 owner를 그대로 둔 rollback 경계를 가진다. 새 필드가 존재한다는 이유만으로 즉시 권위를 넘기지 않는 것이 이번 회귀를 반복하지 않는 핵심이다.

# 3. ⑤ 갱신: 남은 대가와 다음 세션 인수인계

현재 hotfix의 대가는 manual target과 champion-specific action policy가 계속 권위라는 점이다. 하지만 완전하지 않은 schema가 기존 행동을 덮어쓰는 것보다 안전하다. 다음 조건이 충족되면 이 선택은 틀리므로 generated 계약으로 넘겨야 한다.

- `activationMode`와 stage별 target이 13개 2단계 전부에 물질화되고 client/server parity mismatch가 0일 때.
- authored stage 97이 기대 98로 채워지고 암묵 stage/default가 0일 때.
- Viego W charge ratio/curve 및 모든 cancel 경로가 server authoritative test를 통과할 때.
- server accept/reject가 sequence별로 client pending state에 반영될 때.
- hard-coded `GameplayDefinitionQuery` champion switch와 non-Zed W 공통 release 관례를 삭제할 수 있을 때.

남은 검증/부채:

1. 실제 InGame에서 `[WGate]`를 한 번 수집해 `WM_KEYUP`, ImGui/focus, animation/FX 통합을 확인한다. 서버 규칙은 자동 검증됐지만 OS 입력 통합은 아직 미검증이다.
2. Lee Sin W를 포함한 11개 `PressRecast`의 client input contract test가 없다. 현재 가장 중요한 다음 자동화다.
3. Client `AuditDataDrivenContracts`는 Debug bootstrap에서만 실행되며 이번 세션에는 실제 게임 부팅 로그를 수집하지 않았다.
4. `RequireComplete` 11/12의 Zed literal 1개는 이번 stage-input slice와 분리해 처리한다.
5. Claude CLI OAuth 만료로 독립 2-pass 리뷰는 `CLAUDE_REVIEW_PENDING`이다. 완료했다고 표시하지 않는다.

다음 세션은 `2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_PLAN.md`의 V2부터 시작한다. 먼저 13개 activation command-contract test를 만들고, 그 테스트가 red인 상태에서 schema를 추가한 뒤 green으로 만드는 순서를 지킨다. 추가 gameplay 구조 변경 전에 현재 PASS 해시와 본 문서의 build 결과를 baseline으로 사용한다.

# 4. 2026-07-18 최종 마감 판정

이 절은 위 1~3절의 **당시 중간 판정과 수치를 대체하는 최종 판정**이다. 이후 추가 작업에서 activation, charge, command result, stage 물질화, registration 권위 제거, presentation stage 분리까지 완료했으므로 `11/12`, authored stage `97`, activation 미물질화, Viego W charge 미구현이라는 과거 문구를 현재 상태로 읽으면 안 된다.

## 4-1. 결론

- Data-Driven gameplay 소유권 게이트는 **12/12 complete**다.
- canonical gameplay 계약은 **17 champions / 85 skills / 98 ordered stages**다.
- 2단계 gameplay skill은 **13개**이며 activation은 `PressRelease` 2개(Irelia W, Viego W), `PressRecast` 11개로 명시됐다.
- Client champion registration에서 `targetMode`, `stage2TargetMode`, `stageCount`, `rotate`, `stage2Rotate` gameplay 권위 대입을 제거했다. registration은 animation key, hook, FX 같은 presentation/접착 정보만 유지한다.
- generated JSON 계약이 gameplay 권위다. 남아 있는 legacy `SkillDef` fallback은 generated data가 없는 비정상/과도기 상황을 위한 bridge이지 정상 gameplay의 두 번째 권위가 아니다.
- 자동화로 증명 가능한 이번 이관 범위는 완료됐다. 단, OS key-up/focus/ImGui와 실제 애니메이션·FX 감각을 포함한 **InGame 수동 acceptance는 별도 최종 승인 항목**이다.

따라서 “Data Driven 때문에 특수 스킬이 일반 Use로 평탄화됐다”는 회귀 원인은 제거됐다. Data Driven 방향 자체의 실패가 아니라, v1 이관에서 데이터 필드를 먼저 만들고 activation·stage action·server ACK·charge lifecycle 소비자를 동시에 완성하지 못했던 계약 불일치가 원인이었다.

## 4-2. 최종 권위와 데이터 구조

현재 canonical stage는 target/facing/activation/move/lock/command lock/stage window를 함께 가진다. Client는 이를 입력·표현에 소비하고, Shared/Server GameSim은 명령 수락과 gameplay 결과에 소비한다.

```text
Canonical JSON
  -> ChampionGameData generated contract
  -> Client input/presentation adapter
  -> GameCommand
  -> Server GameSim authoritative action/charge/lock
  -> Snapshot commandResults[slot]
  -> Client pending-stage reconciliation
```

이전 `SkillTable` 방식처럼 C++ registration이 gameplay shape를 다시 선언하지 않는다. stage별 계약을 JSON에서 수정하면 generator/schema/frozen-contract gate를 통과해야 하며, 정상 런타임의 Client와 GameSim은 같은 generated 의미를 읽는다.

## 4-3. stage와 presentation 분리

- gameplay authored stage: **98**
- gameplay two-stage skill: **13**
- visual timing stage: **101**
- visual two-stage skill: **16**
- presentation-only stage2: **3** — Riven, Sylas, Zed basic attack

presentation-only stage는 gameplay의 추가 입력 단계가 아니다. 강화 평타처럼 gameplay는 한 번의 command지만 다른 animation/timing을 재생해야 하는 경우다. `ResolveVisualStageCount`는 gameplay stage 수와 `stage2AnimKey`를 분리해 계산하며, Riven basic attack stage2 timing도 `playback=1.0`, `cast=6`, `recovery=14`로 명시했다. visual timing export 결과는 **17 champions / 101 stages / mismatch 0**이다.

## 4-4. 회귀별 최종 처리

### Irelia W와 즉시 이동/E

- W1은 `PressRelease` charge 시작으로 해석된다.
- key release 한 프레임 edge만 보지 않고 `release pending && !WDown` 상태 전이로 W2를 복구한다.
- W2는 `Allow` action move policy를 사용한다.
- SimLab은 W1 동안 E가 `ActionBlocked`인 것, W2 수락 뒤 **같은 tick의 Move와 E가 모두 수락**되는 것을 함께 검사한다.
- max hold auto release는 server sequence를 가지며 CC/death cleanup과 함께 server-authoritative lifecycle로 처리한다.

사용자가 말한 “딜레이 없이 E”의 이번 회귀 기준은 Ezreal E의 authored cast time을 0으로 바꾸는 뜻이 아니라, Irelia W2 직후 E input이 이전 W1 action lock에 남아 막히지 않아야 한다는 뜻으로 구현·검증했다.

### Viego W

- `PressRelease` activation, hold start tick, max hold, charge ratio/curve, release/timeout/cancel 경로가 generated data와 server state에 연결됐다.
- Client는 key-up command를 만들 뿐 charge 결과를 권위 있게 계산하지 않는다.

### Ezreal E

- Client command shape를 `GroundTarget`으로 맞춰 server가 소비하는 cursor ground payload와 일치시켰다.
- landing clamp/우선순위는 SimLab에서 검증한다.
- `castTimeSec=0.25`는 기존 authored gameplay tuning으로 유지한다. 이번 회귀 수정의 “즉시 E” acceptance와 혼동해 임의로 0으로 바꾸지 않았다.

### PressRecast 11개

non-Zed W 공통 key-up 관례를 데이터 activation으로 대체했다. Lee Sin W처럼 두 번째 press가 맞는 스킬은 key-up으로 stage2를 보내지 않는다. 13개 two-stage 전체가 activation contract로 열거되고 ordered stage 계약에 포함된다.

## 4-5. ACK와 authoritative reconciliation

과거 snapshot의 단일 command result는 같은 tick에 W1과 E 같은 여러 명령이 처리되면 마지막 결과가 앞 결과를 덮을 수 있었다. 현재는 다음 구조다.

- server session은 slot별 `SkillCommandFeedback[5]`를 보존한다.
- snapshot은 `commandResults` vector로 슬롯별 ACK/sequence/result를 전달한다.
- 기존 scalar 필드는 wire compatibility fallback으로 유지한다.
- Client는 vector가 있으면 vector를 우선하고, 구 snapshot이면 scalar를 읽는다.
- SimLab wire probe는 W/E 두 feedback의 동시 보존과 legacy fallback을 검사한다.

이로써 “Client가 보냈으니 stage가 성공했다”가 아니라 server accept/reject 결과로 pending stage를 정리한다.

## 4-6. 동결된 98-stage 기준선

SimLab ordered contract hash는 다음 필드를 순서까지 포함해 고정한다.

```text
champion / slot / stage / stageCount
target / facing / activation / movePolicy
createsAction / presentationLoop
lockDuration / commandLock / stageWindow
```

현재 기대 hash는 **`0xC2EE91F37A8E34B0`**다. Client의 generated-vs-generated audit만으로는 구 `SkillTable` 행동 기준선을 증명할 수 없으므로, 이 frozen 98-stage 계약과 별도 exact 예외 검사가 이관 후 drift 방지 기준선 역할을 한다.

HEAD 대비 lock 판정은 다음처럼 정정한다. 이전 인수인계 메모의 “4개 delta”는 기존 stage 변경과 신규 stage를 섞어 센 수치라 사용하지 않는다.

| 계약 | HEAD 대비 최종 판정 |
|---|---|
| Irelia W1 | `lockDurationSec=5.0`은 유지하고 effective `commandLockSec`만 5.0초(30Hz 150 tick) -> 4.0초(120 tick), 의도된 release window 정렬 |
| Irelia W2 | `lockDurationSec=0.4`는 유지하고 legacy Queue cap 8 tick -> `Allow`/`commandLockSec=0`의 0 tick, 의도된 즉시 이동/E 복구 |
| Zed R2 | 기존 stage 변경이 아니라 98번째로 새로 명시된 stage, 0.25초 Forced |
| Yone E2 | HEAD와 현재 모두 0.6초 Forced, delta 없음 |

legacy move policy 대비 의도된 delta는 **Irelia W2의 QueueUntilUnlock -> Allow 한 건**이다. facing 기본 규칙과 명시 예외 12개도 exact list로 고정한다.

## 4-7. 실제 실행한 최종 검증

다음 생성/검증을 최신 변경 뒤 실행했다.

```text
python Tools/ChampionData/build_champion_game_data.py
  PASS: 17 champions, hash 0x773C4102

python Tools/ChampionData/build_champion_game_data.py --check
  PASS

python Tools/LoLData/Build-LoLDefinitionPack.py
  PASS: 17 champions / 85 skills, hash 0x5B3B9BD5

python Tools/LoLData/Build-LoLDefinitionPack.py --check
  PASS

python Tools/LoLData/Test-ChampionGameDataSchema.py
  PASS: canonical + ByRank accepted;
        nested/stage/charge/target mutation rejected

Shared/Schemas/run_codegen.bat
  PASS

Export-LoLChampionVisualTimingSeed.ps1
Export-LoLChampionVisualTimingSeed.ps1 -Check
  PASS: 17 champions / 101 visual stages / mismatch 0

Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug -RequireComplete
  PASS: 12/12, GameSim/Server/Client/SimLab build,
        1800-tick deterministic SimLab, git diff --check

Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Release -RequireComplete
  PASS: 12/12, GameSim/Server/Client/SimLab build,
        1800-tick deterministic SimLab, git diff --check
```

최종 SimLab의 핵심 결과:

```text
DataContract: 17 champions / 85 skills / 98 stages
OrderedContractHash: C2EE91F37A8E34B0
TwoStage: 13
FacingOverrides: 12
IreliaWRelease: PASS
ChargeLifecycle: PASS
CommandResultWire: PASS
ActionLock: PASS
SameSeedReplayHash: 9A3697750E07FC00
SeedPlusOneReplayHash: 4FBCE8EA9099DA65
Overall: PASS
```

FlatBuffers codegen은 `Shared/GameSim` 단일 소유로 정리했고 stamp뿐 아니라 생성 header/Go output과 `flatc.exe`를 MSBuild input/output에 포함했다. Client/Server가 중복 생성하지 않는다. 최신 Debug와 Release 전체 pipeline에서 compile/link까지 통과했다.

## 4-8. 자동 완료와 InGame 최종 승인 경계

코드·schema·generator·contract·SimLab·Debug/Release build 기준으로는 이번 Data-Driven 행동 이관을 완료 판정한다. 사용자가 InGame에서 직접 확인해야 하는 것은 원인을 다시 찾기 위한 수동 디버깅이 아니라, 자동화가 볼 수 없는 아래 통합/감각 acceptance다.

1. Irelia W tap/hold/release/max-auto-release 후 즉시 이동과 E, hold loop FX 종료.
2. Viego W early/max release, W1 중 이동 정책, charge별 range/stun 체감.
3. Ezreal E cursor ground, 사거리 clamp, 벽 근처 partial landing, animation/FX 체감.
4. Lee Sin W 두 번째 press recast, key-up만으로는 recast되지 않음.
5. 13개 two-stage 대표 동작: Irelia W/E, Kalista R, Zed W/R, Riven R, Jax E, Lee Sin Q/W/E, Viego W, Yone E, Sylas E.
6. Riven/Sylas/Zed 강화 basic attack의 presentation-only stage2 animation/timing.
7. 창 focus 이동, ImGui capture, 실제 `WM_KEYUP` 환경에서도 release pending이 유실되지 않음.

여기서 실패하면 bounded `[WGate]`, `[StageGate]` trace와 slot별 ACK를 BP 기준점으로 사용하면 된다. 데이터 계약을 다시 추측할 필요 없이 `OS input -> Client dispatch -> Server accept -> snapshot ACK -> visual playback` 중 어느 경계가 깨졌는지 바로 분리할 수 있다.

## 4-9. 독립 교차 검증 상태

Claude CLI로 98-stage/ACK/presentation/codegen 최종 read-only 리뷰를 시도했으나 로컬 OAuth session 만료로 인증에 실패했다.

```text
Failed to authenticate: OAuth session expired and could not be refreshed
```

따라서 Claude 교차 리뷰는 완료로 기록하지 않는다. 이 항목은 코드/빌드 gate의 실패가 아니라 외부 리뷰 채널의 인증 blocker다. 재로그인 뒤 동일 dirty diff를 read-only로 한 번 더 검토하면 된다.
