# 2026-07-18 Champion AI live safe-turret recall anchor RESULT

관련 계획: `2026-07-18_CHAMPION_AI_LIVE_SAFE_TURRET_RECALL_ANCHOR_PLAN.md`

## 1. 예측 대 실측

| 예측 | 실측 | 판정 |
|---|---|---|
| 파괴된 Outer는 후보에서 빠지고 살아있는 동일 라인 Inner가 선택된다. | SimLab 탑 애니 `MoveToOuterTurret` fixture가 stale `(-13,1,0)` 대신 Inner 뒤 `(-23,1,0)` Move를 확인했다. | PASS |
| 미드 저체력 퇴각은 살아있는 포탑 수명에 따라 목적지를 바꾼다. | 5% HP Annie가 Inner `(-23,1,0)` → Nexus-tier 포탑 `(-35,1,0)` → Nexus `(-40,1,0)` 순으로 3틱 연속 갱신했고, 매 단계 `safeAnchor == retreatGoal == Move.groundPos`였다. | PASS |
| 파괴 다음 AI 틱부터 stale cache를 사용하지 않는다. | `BuildChampionAIContext()`가 decision cadence 조기 반환과 emergency 판정보다 먼저 live resolver를 실행한다. 30 Hz 기준 다음 틱에 반영되는 구조다. | PASS |
| Server 초기값과 Shared 런타임이 하나의 알고리즘을 사용한다. | `CGameRoom::ResolveChampionAISafeAnchor()`가 `CChampionAISystem::ResolveSafeAnchor()`로 위임하고, 기존 Server 전용 103줄 resolver는 제거됐다. | PASS |
| Shared 권위 경계와 데이터 계약은 유지된다. | `Check-SharedBoundary.ps1` PASS. Bot AI는 Move/Recall `GameCommand`만 생산하며 구조물 사망·Targetable·이동 truth를 변경하지 않는다. 데이터/schema/생성물 변경 없음. | PASS |
| Debug GameSim/Server/Client/SimLab과 전체 AI 회귀가 통과한다. | `Run-BotAiValidation.ps1 -Configuration Debug` 전체 PASS, LoL data-driven pipeline + SimLab exit 0. 보고서: `.md/build/2026-07-18_195519_BOT_AI_VALIDATION_HARNESS_REPORT.md`. | PASS |
| 직접 SimLab에서 신규 회귀와 same-seed 결정론이 유지된다. | `SimLab.exe 600 1234` exit 0. MidDefense hash `1735AF9283C9F02E`, runA/runB hash `D651F4627B66C963`, seed+1 `FA20657F5EA5F6D9`. | PASS |
| 실제 GameRoom 1800틱 soak가 생존한다. | Debug 1800 ticks/seed 42/runs 1 exit 0, `RESULT status=PASS`, replay hash `290ADD9933800908`, 10/10 bots command-active, respawn AI/mana failure 0, p99 `23.701 ms`로 33.333 ms 예산 안이다. | PASS |
| 3개 별도 프로세스의 raw world hash도 동일하다. | 세 실행은 모두 개별 `RESULT status=PASS`이고 replay hash도 동일했지만 raw world hash는 달랐다. 차이는 `ChampionAssistCreditComponent`와 `CombatActionComponent` POD padding에 있었고, 작업 전 `release_ticks_1800_seed_42_20260718_170534_425_6bfa0b10` 증거도 같은 현상이다. | PRE-EXISTING INFRA FAIL |

## 2. 결론

기능·빌드·직접 회귀·실제 서버 soak 기준 **PASS**다. 미드와 탑 봇이 파괴된 포탑 좌표를 계속 안전 지점으로 사용하는 근본 원인인 spawn-time cache를 제거했고, 살아있는 동일 라인 포탑을 최우선으로 사용한다. 모든 라인 포탑이 죽은 경우에도 살아있는 Nexus-tier 포탑, 마지막으로 Nexus까지 명시적으로 후퇴한다.

3-run harness의 raw world hash 게이트는 이번 변경의 AI 상태나 명령 차이가 아니다. replay stream은 세 번 완전히 동일했고, byte diff는 변경하지 않은 두 checkpoint POD의 프로세스별 padding으로 국소화됐다. 이 작업에서 world hash 검사를 약화하거나 무관한 keyframe codec을 수정하지 않았다.

## 3. 갱신된 트레이드오프

- 선택한 live scan은 component/schema를 늘리지 않고 rewind·잔해 보존과 자연스럽게 맞는다. 현재 10봇/30구조물 soak에서 30 Hz p99 예산을 통과했지만 resolver 단독 비용은 별도 계측하지 않았다.
- `MoveToOuterTurret` 상태명은 호환성을 위해 유지했다. 실제 의미는 이제 “살아있는 home-lane safe turret로 이동”이다.
- 후속 독립 과제는 `ChampionAssistCreditComponent`와 `CombatActionComponent`의 canonical keyframe codec 또는 padding zeroing이다. 이것은 기능 수정과 분리해야 하며, raw world hash 게이트를 다시 신뢰 가능하게 만드는 검증 인프라 작업이다.
