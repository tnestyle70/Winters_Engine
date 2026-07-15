# S021 AI 선택 근거 + Chrono branch timeline 검증 보고서

Session - S020 Claude lane을 기능 단위로 감사한 뒤, bot 선택의 원인과 executor 결과를 F9에서 추적하고 S015 Chrono branch와 함께 비교할 수 있는 bounded vertical slice를 반영했다.

날짜: 2026-07-13. 계획: `.md/plan/2026-07-13_S021_AI_CHRONO_DECISION_TRACE_PLAN.md`. Packet: `2026-07-13_s021_ai_chrono_decision_trace`.

## 1. 결론

요청 범위의 코드 반영과 자동 빌드·결정론 검증을 완료했다. 기존 dirty 변경은 reset/revert하지 않았고 commit도 만들지 않았다.

- S020의 미니맵 직교/등길이 기저와 F9 접힘 구조는 그대로 유지했다.
- S020의 5초 skill policy가 executor 제출 시점에 소비되던 P1 결함을 고쳤다. 이제 stage-1 CastSkill이 authoritative executor에서 Accepted일 때만 interval을 장전한다.
- mana/state/target 등으로 Rejected된 cast는 interval을 소비하지 않고, 제출 전에 전진했던 combo step을 matching command sequence의 trace step으로 복구한다.
- 실제 skill cooldown과 bot policy interval을 `RuntimeSkillCooldown` / `PolicyCastInterval`로 분리하고, 5초 값을 15번째 runtime tuning(0..15초)으로 승격했다.
- low-HP emergency가 Retreat 도달 Recall보다 먼저 return하던 순서를 고쳐, retreat anchor 도달 시 Recall command를 제출하고 executor Accepted까지 검증했다.
- F9에 Retreat/Fight/Farm/Siege 점수, legal/illegal candidate mask, hard block, HP·거리·turret evidence, command sequence, executor Accepted/Rejected와 정확한 reason을 노출했다.
- server의 authoritative 16행 ring은 유지하되 snapshot wire는 최신 1행만 전송한다. 기존 최대 16행/봇/snapshot 대비 상세 trace row 수를 93.75% 줄였다.
- F9가 열린 동안 선택 bot만 1..300 tick sampling, 16..512행 local bounded history를 축적한다. 각 행은 server tick, timeline epoch/branch, tool revision, decision/executor evidence를 함께 보존한다.
- F9에서 1..1800 tick(1/30초..60초) rewind 요청을 보낼 수 있다. server는 seconds를 먼저 자르지 않고 `round(seconds * 30)`으로 변환한다.
- rewind 성공으로 epoch/branch가 바뀌면 이전 branch의 local history는 남고, 실제 restored tick에서 새 branch 행이 다시 축적된다.

주요 코드 앵커:

- AI trace/tuning 계약: `Shared/GameSim/Components/ChampionAIComponent.h:80`, `:104`, `:260`
- decision evidence와 Recall 전이: `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp:939`, `:4128`
- executor 승인 commit/reject rollback: `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp:1763`, `:1787`
- append-only wire: `Shared/Schemas/Snapshot.fbs:38`
- 최신 1행 snapshot: `Server/Private/Game/SnapshotBuilder.cpp:695`
- Client mapping: `Client/Private/Network/Client/SnapshotApplier.cpp:1371`
- F9 evidence/Chrono UI: `Client/Private/UI/AIDebugPanel.cpp:737`, `:914`
- tick 단위 rewind 변환: `Server/Private/Game/GameRoomCommands.cpp:688`
- 회귀 probe: `Tools/SimLab/main.cpp:3323`, `:5370`

## 2. S020 감사 결과와 조치

| 감사 항목 | 결과 | S021 조치 |
| --- | --- | --- |
| S020 Debug x64/SimLab 결과 | 문서와 산출물 시각이 일치해 자동 검증 증거는 신뢰 가능 | 새 변경 후 전체 솔루션을 재빌드하고 SimLab을 재실행 |
| 미니맵 기저 | u/v가 직교·등길이. 수동 화면 gate만 남음 | 코드 변경 없음 |
| F9 접힘 구조 | 기능상 정상. 접힘은 wire 비용을 줄이지 않음 | server wire 16행→최신 1행, 선택 bot local history로 전환 |
| 5초 gate commit 시점 | executor 전 outCommands push에서 timer 장전 | Accepted stage-1 cast에서만 timer commit |
| rejected combo | timer/step을 소비할 수 있음 | exact sequence legacy/rich trace close + rejected cast step rollback |
| cooldown 원인 | policy interval과 runtime cooldown이 모두 `SkillCooldown` | 두 block reason 분리 + executor reason 노출 |
| 기존 tuning 수 | 문서의 15개와 달리 실제 14개 | `SkillCastMinInterval`을 실제 15번째 tuning으로 추가 |
| Retreat→Recall | emergency low HP branch가 도달 전이를 가림 | 도달 전이를 emergency 앞에 배치 + accepted Recall probe |

## 3. “왜 이 선택인가”를 읽는 순서

F9의 한 행은 이제 다음 다섯 층을 연결한다.

1. `State / Intent / Action`: 실제로 고른 macro·candidate·행동.
2. `Retreat / Fight / Farm / Siege`: 같은 decision에서 비교된 최종 utility 점수.
3. `legal / illegal mask`와 `blockReason`: 후보가 점수 비교에 들어갈 수 있었는지, hard safety·policy·runtime gate 중 어디서 막혔는지.
4. `self/enemy HP`, `distance`, `turret danger`, runtime threshold/interval: 선택 당시의 핵심 perception/effective tuning.
5. `commandSequence -> executorState / executorReason`: 의사결정이 명령 제출에 그쳤는지, 실제 server gameplay에서 승인/거절됐는지와 정확한 사유.

이 구조로 예를 들어 “Fight 점수가 높았는데 왜 스킬을 안 썼는가”를 `PolicyCastInterval`, `RuntimeSkillCooldown`, `InsufficientResource`, `OutOfRange`, `StateBlocked`로 분리해서 볼 수 있다. Rewind 전/후 같은 bot의 행은 epoch/branch로 구분되므로 branch A와 branch B의 점수·gate·executor 결과를 한 패널에서 비교할 수 있다.

## 4. 검증 결과

### Schema

- `Shared\Schemas\run_codegen.bat`: PASS
- C++ 및 Go `AIDebugTraceRow` accessor/builder 생성 확인
- 기존 `AiEpisodeV1` schema/ABI는 변경하지 않음

### Python AI Research

```text
python -m unittest discover -s Tools\AIResearch\tests -p "test_*.py"
Ran 61 tests in 0.575s
OK
```

### SimLab 1,800 tick / seed 42

- 전체 probe: PASS
- 새/확장 회귀:
  - `[ChampionAI] ... retreat->Recall`: PASS
  - `[CommandOutcome] ... accepted cast commits cadence; rejected cast preserves timer/step`: PASS
- keyframe transactional/restore replay: PASS
- 동일 seed: `DB0DC85E451999AD`
- seed+1: `57A9B2394575042A`

### 빌드

- GameSim Debug x64: 오류 0
- SimLab Debug x64: 오류 0
- Server Debug x64: 오류 0
- Client Debug x64: 오류 0
- `Winters.sln` Debug x64 직렬 전체 빌드: 오류 0, 경고 144

경고는 기존 C4251/C4275 계열 DLL-interface 등을 포함한 누적 warning이며 새 컴파일·링크 오류는 없다.

### Diff

- S021 owned paths `git diff --check`: PASS
- codegen의 기존 dirty Go 산출물과 S017/S018/S020 누적 변경을 보존
- stage/commit/push: 수행하지 않음

## 5. 완료로 주장하지 않는 경계

이번 완료는 “diagnostic summary + branch-tagged local timeline”이다. 다음은 아직 별도 구현이다.

- `ChampionAIValuation`의 각 raw×weight contribution을 단일 계산 경로에서 내보내는 DecisionLedger V2
- 모든 early return을 decision당 정확히 1행으로 닫는 full why-not ledger
- server가 F9 open/selected bot을 아는 typed subscribe/unsubscribe observation op
- keyframe에서 arbitrary requested tick까지 command journal을 faithful replay하는 exact Chrono A/B
- branch-aware WRPL segment/replay format
- keyframe interval/window, bytes, capture/restore μs와 p95 budget 계측
- Recall/DefendMid를 `AiEpisodeV1` learned candidate로 확장하는 일
- PyTorch BC·DAgger·PPO, league/self-play, ShadowCoach 학습

특히 현재 WRPL v2 player는 non-monotonic tick을 거부한다. rewind 뒤 같은 recorder에 낮은 tick이 이어지는 기존 경계는 이번 local AI timeline과 별개이며, rewind가 포함된 WRPL을 branch-aware replay로 사용할 수 있다고 주장하지 않는다. 다음 exact Chrono packet에서 recorder segment 또는 branch-aware format으로 먼저 막아야 한다.

또한 F9는 최신 summary 1행을 모든 bot에 보내고 긴 history만 선택 bot client-local로 제한한다. 완전한 selected-only network subscription은 후속이다.

## 6. 수동 gate

자동·헤드리스 검증과 전체 빌드는 완료했다. 창을 띄우는 수동 확인은 다음만 남는다.

- F9에서 15번째 `Skill Cast Min Interval` slider가 server value와 왕복하는지
- Rewind 버튼 후 paused 착지, epoch/branch 증가, requested target과 observed restored tick 표시
- 이전 branch 행을 보존한 채 restored tick부터 새 행이 누적되는지
- bot 선택 변경 시 local history가 섞이지 않는지

이는 컴파일/결정론 완료 조건이 아니라 UI 사용성 gate다.
