Session - 챔피언 AI 공격성·미드 합류·요네 복귀 후 R·미니언 정체 개선 결과
좌표: 신규 좌표 후보 · 축: C1 기준계, C8 검증이 병목
관련: 2026-07-18_CHAMPION_AI_AGGRESSION_MID_GROUP_YONE_COMBO_MINION_STUCK_PLAN.md

## 1. 예측 vs 실측

- 적중 — 근접 챔피언의 Fight 점수가 BA 사거리만 보던 것이 원인이었다. 현재 combo의 첫 legal hostile entry 사거리를 사용하자 요네·피오라·사일러스의 거리 감점이 해소됐고, 20% 체력 열세에서는 교전을 허용하되 30%를 넘는 심각 열세는 차단했다. Farm hold와 탑/미드 이동 anchor도 관측한 적 챔피언이 있으면 즉시 LaneCombat으로 전환한다.
- 적중 — 적 미드 외곽 포탑 파괴, 미드 반경 18 안 적 2명 이상, 아군 수 열세를 같은 decision cadence snapshot으로 계산하면 side-lane 봇의 home lane을 보존한 채 active lane만 Mid로 바뀌었다. 아군 미드 웨이브를 합류 anchor로 사용하고, 이동 중 적 챔피언은 교전하되 side-lane 미니언만으로는 합류가 취소되지 않았다. 동일 seed 공세 fixture와 전체 MidDefense hash `84CDE905725E55C0`가 결정적으로 일치했다.
- 빗나감 후 수정 — authored 요네 순서 `E-Q-W-BA-BA-BA-E2-R`은 맞지만 기본 공격 속도에서는 고정 5초 E 창 안에 세 번째 BA까지 완료할 수 없었다. 정상 timer return은 뒤의 `E2-R` tail을 찾아 남은 BA만 포기하도록 수정했고, 기본 속도에서 `E-Q-W-BA-BA-E2-귀환 완료-R`을 실측했다. target-lost/retreat 안전 복귀는 R 예약을 남기지 않았다.
- 적중 — 거절된 BasicAttack이 comboStep을 건너뛰던 executor 피드백 비대칭도 발견해 모든 거절 command가 제출 전 comboStep으로 복원되게 했다. accepted stage-1 cast만 fresh-cast cooldown을 확정하는 기존 계약은 유지했다.
- 적중 — 미니언 soft depenetration은 raw push의 역방향 성분을 제거하고 entity parity 기반 lateral+forward 방향을 선택했다. pure-policy probe는 동일 입력 결정성과 `dot(result, forward) > 0`을 통과했고, blocked frame을 phase 진입마다 0으로 지우던 경로도 제거했다.
- 검증 실측 — LoL 데이터 생성/`--check` PASS, 전체 `Winters.sln` Debug x64 빌드 exit 0, SimLab 두 번 exit 0, same-seed hash `AE525C1392315007`, seed+1 hash `E4F925CE98C2465C`, 최종 봇 하네스 Overall PASS다. 보고서는 `.md/build/2026-07-18_233616_BOT_AI_VALIDATION_HARNESS_REPORT.md`다.
- 분리된 실패 — `Tools/AIResearch/RunValidation.ps1`은 80개 중 2 failure/6 error로 exit 1이었다. 모두 이번 런타임 변경과 무관한 기존 correction sidecar `source AiEpisode SHA-256 mismatch` fixture이며, AI 경계·데이터 생성·SimLab·하네스 PASS와 섞어 성공으로 표기하지 않는다.
- 미검증 — 실제 5v5에서 요청 roster의 체감 공격성, bottom 3-wave 장기 정체 해소, 미드 합류 그림은 replay가 없으므로 `CONFIRM_NEEDED`다. 자동 pure-policy 검증을 인게임 완전 재현으로 과장하지 않는다.

## 2. 판결

수정 반영. 단순 aggression 상향이 아니라 거리 기준계, 중복 HP gate, anchor 선점, 공세 macro snapshot, 요네 시간창 적응, command feedback, 전진 보존 분리를 각각 고쳤고 전체 빌드·결정성·봇 하네스가 통과했다. Bot AI는 계속 GameCommand 생산자이며 피해·이동 결과의 서버 권위는 변경하지 않았다.

## 3. ⑤ 갱신

- 30% 심각 체력 열세 gate는 무모한 진입을 막는 대신, 특정 챔피언의 확정 처형 콤보까지 보수적으로 차단할 수 있다. 챔피언별 lethal 계산이 들어오면 이 공통 gate가 틀릴 수 있다.
- 공세 미드 합류의 `enemy>=2`, 반경 18, 6초 hold는 안정적인 한타 그림을 얻는 대신 split push 기회를 일부 포기한다. 실제 5v5에서 과집결·왕복 진동이 보이면 수치가 아니라 objective/웨이브 가치 항을 추가해야 한다.
- 기본 속도 요네는 시간창 만료 시 세 번째 BA보다 E2-R을 우선한다. 공격 속도가 충분하면 authored 3 BA를 모두 실행하지만, E 지속시간·공격 cadence가 바뀌면 tail 탐색 fixture도 함께 갱신해야 한다.
- forward-safe 분리는 soft minion 겹침에만 적용한다. 정적 지형·동적 blocker를 무시하지 않으므로 실제 장기 stuck이 남는다면 별도 path/flow fallback의 원인을 추적해야 한다.
- AIResearch correction fixture 실패는 이번 범위 밖으로 남겼다. 해당 fixture가 갱신되기 전에는 전체 연구 파이프라인 PASS를 주장할 수 없다.
