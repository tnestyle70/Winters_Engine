# Winters Server Simulation Exception Doctrine

작성일: 2026-07-12
적용 범위: LoL Server, Shared/GameSim, GameCommand, Champion AI, 상태 이상, 강제 이동, 스킬 단계, 투사체, 피해, 사망, Snapshot/Event

## 0. 북극성

Winters의 gameplay truth는 하나다.

~~~text
Client intent
-> GameCommand
-> Server 30 Hz GameSim
-> deterministic state transition
-> Snapshot/Event
-> Client presentation
~~~

챔피언, 미니언, 봇, 연습 도구가 결과 HP·위치·쿨다운·상태를 직접 쓰는 두 번째 진실을 만들면 안 된다. 예외 처리는 “특수 if를 어디에 더 넣을까”가 아니라 “어떤 불변식 아래 어느 전이 소유자가 이 규칙을 적용할까”로 판단한다.

## 1. 공개 사실과 Winters의 설계 추론

Riot가 공개한 자료에서 확인 가능한 범위:

- League는 전용 서버 권위 게임이다.
- 결정론 구현은 기록된 입력, 프레임 양자화, 통합 시계, gameplay-essential state를 중심으로 구성되었다.
- divergence를 찾기 위해 계층 상태 로그와 대량 자동화 게임을 사용했다.

참고:

- https://www.riotgames.com/en/news/determinism-league-legends-introduction
- https://www.riotgames.com/en/news/determinism-league-legends-implementation
- https://www.riotgames.com/en/news/determinism-league-legends-fixing-divergences
- https://www.riotgames.com/en/news/determinism-league-legends-unified-clock
- https://www.riotgames.com/en/news/the-tech-behind-swarm

Riot 내부의 실제 ability/state-machine 구현은 공개 자료만으로 확정할 수 없다. 아래 transaction, action generation, rule composition 구조는 Winters의 현재 충돌에서 도출한 설계다.

## 2. 왜 LoL형 상태 공간은 폭발하는가

한 행동은 단독으로 존재하지 않는다. 입력 승인, 타겟팅, 사거리, 시야, 자원, 쿨다운, action phase, 이동 정책, 상태 이상, 강제 이동, 사망, 변신, 소환, 투사체, 피해 면역, 네트워크 지연이 같은 tick에서 교차한다. 예외 수를 줄이는 방법은 조건문을 없애는 것이 아니라 조건문의 차원을 분리하는 것이다.

| 차원 | 질문 | 소유자 |
|---|---|---|
| Capability | 지금 이동/공격/시전 가능한가 | GameplayStateQuery |
| Command acceptance | 이 명령을 승인할 수 있는가 | CommandGate + skill validator |
| Action phase | windup/impact/recovery 중 어디인가 | ActionController |
| Motion | 일반 이동/대시/넉백/에어본 중 무엇이 위치를 소유하는가 | Motion systems |
| Effect | damage/status/projectile/summon 무엇을 예약하는가 | deterministic request queues |
| Lifetime | 사망/삭제/부활 후 참조가 유효한가 | lifetime/Entity identity |
| Replication | late join도 같은 truth를 재구성할 수 있는가 | Snapshot/Event contract |
| Presentation | animation/FX/sound를 언제 한 번 재생하는가 | Client presentation timeline |

## 3. 반드시 유지할 불변식

1. 한 gameplay 결과에는 한 권위 writer만 존재한다.
2. 명령 검증은 read-only이고, 승인 후 commit은 원자적으로 보이게 한다.
3. 같은 tick의 충돌은 명시적 우선순위와 안정적인 EntityID tie-break로 결정한다.
4. 과거 action의 impact는 새 action generation에서 발동할 수 없다.
5. 상태 이상은 capability를 바꾸며, champion if가 임의로 movement/attack writer를 끄지 않는다.
6. 강제 이동은 위치 writer를 독점하고 일반 이동과 동시에 위치를 쓰지 않는다.
7. 죽은 대상, 재사용된 entity, 만료된 target은 impact 시점에 다시 검증한다.
8. bot은 truth를 직접 수정하지 않고 사람과 같은 GameCommand를 생산한다.
9. Snapshot은 현재 상태를, Event는 한 번의 사건을 전달한다. late join truth를 Event만으로 복원하지 않는다.
10. 데이터/훅 누락은 개발 빌드에서 가시화한다. 알 수 없는 범용 피해로 조용히 성공시키지 않는다.
11. 안전 판정은 현재 공간 위치를 기준으로 하며 macro objective나 lane label로 위협을 제외하지 않는다.
12. Macro objective, tactical intent, mechanical plan, atomic command는 서로 다른 축이다. 하위 행동이 상위 목표의 이름으로 덮어써지지 않는다.
13. 방어·공성 objective에는 combat leash와 return boundary를 두어 가까운 웨이브나 표적이 목표를 무기한 끌고 가지 못하게 한다.

## 4. 권장 전이 파이프라인

~~~text
GameCommand
-> CommandGate
-> SkillModule.Validate (read-only)
-> SkillExecutionPlan
-> ActionController.Commit
-> Motion / Projectile / Damage / Status / Summon request queues
-> deterministic system flush
-> TransitionJournal
-> Snapshot + Event
~~~

Validate 단계가 cooldown과 action을 먼저 소비한 뒤 champion hook이 실패하는 현재 패턴은 장기적으로 제거한다. 실행 계획에는 비용, cooldown, action phase, movement policy, target set, effect requests가 들어가며 Commit이 한 번만 적용한다.

## 5. 충돌 우선순위

~~~text
Death / entity invalidation
-> hard CC and forced motion
-> already committed impact
-> interrupt/cancel policy
-> newly accepted skill or attack
-> queued move
-> ordinary move
-> idle presentation
~~~

챔피언 특수 규칙은 이 순서를 바꾸는 이름 기반 if가 아니라 “interruptible인가”, “impact 시 어떤 request를 만드는가” 같은 정책 데이터 또는 champion module로 제공한다.

## 6. 예외를 담는 네 가지 형태

- 데이터: 수치, 사거리, 비용, cooldown, phase duration, target mask.
- typed rule/atom: Damage, ApplyStatus, Dash, Knockback, Spawn, Mark, ConsumeMark.
- champion module: 비에고 빙의, 사일러스 탈취, 칼리스타 계약처럼 새로운 상태 수명과 상호 참조를 만드는 규칙.
- engine invariant: stale action, entity lifetime, deterministic order, writer conflict. 콘텐츠 예외가 아니므로 공용 시스템에서 한 번 막는다.

## 7. 상태 머신의 형태

하나의 거대한 enum으로 모든 조합을 만들지 않는다.

~~~text
LifeState       Alive / Dying / Dead / Respawning
ActionState     Idle / Windup / Impact / Recovery / Channel
MotionState     Grounded / PathMove / Dash / ForcedMotion
ControlState    Free / Stunned / Airborne / Silenced / Disarmed
FormState       Base / Transformed / Possessing
ObjectiveState  Lane / Defend / Siege / Recall
~~~

각 축의 writer와 우선순위를 고정하고 최종 capability는 GameplayStateQuery가 합성한다. 상태 전이에는 source, startTick, endTick, generation, cancel reason을 남긴다.

## 8. 150 챔피언 확장 규칙

- 공용 시스템은 champion 목록을 몰라야 한다.
- 새 챔피언은 definition + module registration + SimLab scenario의 세 묶음으로 들어온다.
- 공용 변경 전 “새 gameplay primitive인가, 기존 primitive 조합인가”를 판정한다.
- 한 챔피언 때문에 공용 if를 추가했다면 두 번째 챔피언이 재사용 가능한 contract인지 검토한다.
- champion module은 결과를 직접 쓰지 말고 execution request를 만든다.
- 데이터 fallback은 validator와 bounded diagnostic에서 가시화한다.

## 9. 사람다운 bot의 본질

사람다움은 매 tick 최적 행동을 고르는 것이 아니다. 제한된 시야, 낡아지는 기억, 반응 지연, 계획 고수, 위험 오차, 실행 가능성을 가진 agent가 일관된 목적을 추구하는 것이다.

~~~text
Authoritative World Fact
-> team-filtered Observation
-> Perception Memory (last seen, confidence, ETA envelope)
-> Situation Features
-> Candidate Generation
-> hard feasibility gates
-> Utility + Risk + Opportunity Cost
-> commitment / hysteresis / reaction budget
-> one Intent
-> champion tactics / combo plan
-> GameCommand
~~~

목적 함수는 단순한 나의 HP - 적 HP 하나가 아니다.

~~~text
Win probability proxy
= survival value
+ expected damage and kill value
+ XP/gold income
+ objective and map pressure
+ information value
- death risk
- missed farm and travel opportunity cost
- cooldown/summoner resource cost
- uncertainty penalty
~~~

HP 차이는 전투 국면 feature 중 하나다. 성장 곡선, 레벨업 임계치, item spike, 수적 우위, 포탑, 웨이브, 귀환/이동 시간을 별도 feature로 유지한다.

## 10. Fact와 Perception

bot이 서버 World 전체를 볼 수 있다는 사실과 bot이 알아야 하는 사실을 분리한다.

- visible enemy는 현재 위치/상태를 관측한다.
- hidden enemy는 lastSeenTick/position/velocity와 가능한 이동 반경만 유지한다.
- confidence는 시간, 경로, 시야 재확인으로 감소한다.
- ward, minion, turret, ally vision을 team visibility mask로 합성한다.
- unseen cooldown/item은 마지막 관측 이후 belief로 취급한다.

현재 Winters의 CanBeSeenBy는 team visibility mask보다 invisible flag 중심이라 bot이 사실상 전지적이다. 사람형 판단 전에 이 경계를 고쳐야 한다.

## 11. 행동 선택 계층

ATTACK, DEFEND, CLAIM, BA, Q/W/E/R, Combo1/2/3, RETREAT, FLASH는 같은 층의 enum 하나가 아니다.

| 층 | 예 |
|---|---|
| Macro objective | DEFEND_MID, CLAIM_WAVE, SIEGE, RECALL, ROTATE |
| Tactical intent | ATTACK, RETREAT, PEEL, CONTEST |
| Mechanical plan | Combo1/2/3, kite, chase |
| Atomic command | Move, BA, Q, W, E, R, Flash |

hard gate로 불가능한 후보를 제거한 뒤 효용을 비교해 intent를 고른다. intent가 combo plan을 만들고 atomic command를 순차 발행한다. 30 Hz는 관측과 안전 검사를 갱신하되 일반 숙고는 5~10 Hz로 제한하고 계획은 hysteresis로 유지한다.

## 12. macro 진화

1. S013: 아군 외곽 포탑이 하나라도 파괴되면 안전/commitment 뒤에 미드 방어로 집결.
2. 웨이브 가치: 도착 ETA와 유실 XP/gold를 비교해 사이드 웨이브를 먹고 합류.
3. 수적 우위: ally/enemy arrival ETA와 궁극기/스펠을 반영해 defend/contest/retreat 선택.
4. recall/purchase: gold spike와 objective spawn ETA로 귀환.
5. fog inference: last seen과 경로 envelope로 gank risk 추정.
6. teleport: 채널 안전, destination, 도착 후 전투 가치와 사이드 손실 비교.
7. rollout: 제한된 후보에만 짧은 horizon, 고정 deterministic budget.

## 13. 튜닝과 디버깅

가중치뿐 아니라 후보별 raw feature, hard gate/거절 이유, utility 항별 기여도, 최종 점수, commitment remaining, emitted command와 승인/거절을 캡처한다. F9는 snapshot evidence를 표시하고 runtime override는 typed debug command로 서버에 보낸다. override는 세션 한정이며 canonical JSON은 validate/cook/SimLab을 거친다.

## 14. 검증 피라미드

1. 단일 전이 probe: stun 중 cast 거절, BA windup 후 skill 교체 시 stale impact 없음.
2. scenario probe: 포탑 파괴 후 미드 방어, commitment 중 전환 금지.
3. same-seed decision hash: state/intent/action/command/target/position tick hash.
4. counterfactual batch: 한 feature/weight만 바꾼 비교.
5. LAN smoke: server 한 대 + clients의 snapshot/event/presentation 일치.
6. 대량 replay divergence: 최초 다른 tick과 계층 상태 자동 보고.

## 15. 현재 부채와 S013 경계

- CastSkill이 cooldown/action을 commit한 뒤 void hook이 실패할 수 있다.
- BasicAttack의 CombatAction과 skill의 ActionState가 분리되어 있었고, S013에서 owner action generation을 impact와 command-level Move 소비 양쪽에서 검증하도록 막았다. 두 action owner를 하나로 통합하는 일은 후속 부채다.
- 공용 executor/query의 champion switch가 증가 중이다.
- StatusEffectComponent와 legacy CC truth가 중복된다.
- AI가 team visibility를 충분히 반영하지 않아 전지적이다.
- 일반 decision은 5 Hz지만 기존 target context scan은 decision gate 앞에서 30 Hz로 수행된다. S013 macro 구조물 scan만 decision cadence로 제한했으며, 전체 Perception cache 분리는 후속 과제다.
- 기존 SimLab 장기 RunMatch는 Champion AI를 실행하지 않아 AI 결정론을 증명하지 못한다.
- Snapshot의 active status source/expiry와 forced-motion late-join truth가 부족하다.

S013은 stale BA를 action generation으로 차단하고 실제 AI 미드 방어 결정론 probe를 추가한다. 나머지는 transaction/vision/status consolidation 세션으로 분리한다.

## 16. 완료 정의

- home lane은 보존되고 active lane/objective만 미드 방어로 전환된다.
- hard safety와 active combo/dive가 macro보다 먼저 평가된다.
- 포탑 안전은 objective lane과 무관하게 현재 위치 기준으로 평가된다.
- macro objective와 tactical state/intent가 분리되고 방어 반경 밖에서는 앵커로 복귀한다.
- bot은 GameCommand 외 gameplay truth를 직접 쓰지 않는다.
- BA를 새 action으로 교체하면 이전 generation impact가 발생하지 않는다.
- 같은 seed의 실제 AI scenario가 같은 decision hash를 만든다.
- GameSim, SimLab, Server, Client 빌드와 F9 brain/macro 표시가 성공한다.
