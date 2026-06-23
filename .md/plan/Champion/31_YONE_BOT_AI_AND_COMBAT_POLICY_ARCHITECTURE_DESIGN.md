# 설계 계획서 — Champion 전투 스킬 결정 아키텍처(고정 콤보 → 반응형 스킬 정책) + Yone 적용

작성일: 2026-06-23
성격: **설계 계획서**(방향/구조 결정). 코드 줄단위 지시는 본 설계 승인 후 Phase별 계획서로 분해.
상위 가이드: [16_BOT_AI_HUMANLIKE_COMPLETION_PIPELINE_GUIDE.md](../ai/16_BOT_AI_HUMANLIKE_COMPLETION_PIPELINE_GUIDE.md)

```text
Goal - 챔피언 전투 스킬 결정을 "고정 콤보 재생"에서 "반응형 스코어 정책 + 챔피언 전술 훅"으로 진화시키고, 그 첫 수직 슬라이스로 Yone 봇을 의도대로 동작시킨다. 인게임에서 직접 튜닝하며 로직을 키우는 워크플로우를 전제로 한다.
```

---

## 1. 현행 방식 검토 (코드 근거)

현재 "Champion Attack 상태에서 정해진 콤보 수행"은 **세 갈래가 따로 공존**한다.

### 1-A. 데이터 콤보 플랜 (Jax/Fiora/Ashe/Riven)
- `ChampionAIComboPlan` = `ChampionAIComboStep[8]`. step = `{slot, itemId, minRange, maxRange, selfHpMinRatio, enemyHpMaxRatio, targetMode}` (`Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h:20`).
- 실행: `TryEmitAttackChampionCombo`(`ChampionAISystem.cpp:1327`)가 `ai.comboStep`을 들고 `index = comboStep % stepCount`로 **선형·순환** 진행. `CanUseComboStep`(`:204`)이 HP/사거리/쿨다운 게이트. 마지막 step 후 `CompleteChampionAICombo`(`:305`) → post-combo 평타 또는 FarmMinion 복귀.
- 데이터는 `GetChampionAIComboPlan`(`ChampionAIPolicy.cpp:499`)에 constexpr.

장점: 단순, 데이터 주도, 결정론적, 추가가 쉽다(새 챔프 = step 배열).
한계(본질적):
- **반응이 없다.** 컨텍스트가 바뀌어도 step을 순서대로 순환한다. "지금 적이 빠졌으니 평타 대신 갭클로즈", "상대가 CC 쓰면 E 아껴 회피" 같은 판단 불가.
- **스택/스테이지 무지.** Yone Q3(스택), Riven Q 체인, Yasuo Q스테이지처럼 시퀀스 의존 메커닉을 step 인덱스만으로는 정확히 못 맞춘다.
- **반응형 이탈 없음.** 콤보 도중 트레이드가 불리해져도 끝까지 밀고, 이탈 스킬(Yone E 귀환)을 콤보 흐름에 못 넣는다.

### 1-B. 하드코딩 챔피언 전술 (Yasuo)
- `TryExecuteYasuoChampionCombat`(`ChampionAISystem.cpp:1675`) + `TryExecuteYasuoUltimate`(`:1645`) + 미니언 갭클로즈/Q스테이지. 매 틱 **E활성 여부·Q쿨·사거리 밴드·공중 타겟**을 읽어 우선순위로 분기하는 **반응형 정책**이다(EQ after dash → E engage → Q → BA → E gapclose via minion).

장점: 행동이 좋다. 챔피언이 자기 킷을 "상황에 맞게" 쓴다(= 사용자가 원하는 그림).
한계:
- **공용 시스템 cpp 오염.** 챔피언 지식이 `ChampionAISystem`에 if-chain으로 박혀 로스터가 커질수록 비대해진다(북극성 위반 부채). `YasuoGameSim.h`를 공용 AI가 직접 include.
- **데이터/튜닝 불가.** 사거리·우선순위·임계값이 코드 상수라 인게임 튜닝 surface가 없다.

### 1-C. generic skillRules (나머지 14챔프, Yone 포함)
- `ChampionAIProfile.skillRules[4]`(`{slot, minRange, score}`)을 `TryEmitAttackChampionSkill`(`ChampionAISystem.cpp:1271`)이 사거리만 보고 단발로 쏜다.
- Yone = `MakeYoneProfile`(`ChampionAIPolicy.cpp:387`): skillRules `{Q, W}`, **콤보 플랜 없음(default 빈 플랜)**, 하드코딩 전술 없음.

### 1-D. 그래서 Yone이 "의도에 안 맞는" 이유 (핵심 진단)
Yone은 1-A(콤보)도 1-B(전술)도 없이 **1-C만** 탄다 → 교전 시 **Q/W를 사거리 들어오면 단발로 쏘는 것**이 전부. Yone 실제 킷(`YoneGameSim.cpp`):
- Q(Mortal Steel) range 4.75 / dmg 75, **Q3 = 대시+넉업**(스택), W(Spirit Cleave) range 6.0 / dmg 65 쉴드+콘, **E(Soul Unbound) 2스테이지**(`eYoneDashKind::SoulOut`↔`SoulReturn`, `ResolveEStage`, `YoneSimComponent`) — 영혼 분리로 진입→콤보→**본체로 귀환(이탈)**, R(Fate Sealed) range 10 / dmg 150 넉업.
- Yone의 정체성은 **E commit → 딜교 → 불리하면 E 귀환으로 이탈**이라는 **반응형/위치 판단**이다. 선형 콤보(1-A)로는 원천적으로 표현 불가, 단발 skillRules(1-C)로는 E/Q3/R을 아예 안 쓴다.

**결론**: 세 방식 중 어느 하나로 통일하면 안 된다. 1-A는 단순 로테이션 챔프에 충분하고, 1-B는 행동 품질이 목표지만 위치가 틀렸으며, 1-C는 부족하다. **필요한 건 1-B의 반응성을 데이터/훅으로 구조화하고, 1-A를 그 특수 케이스로 흡수하는 통합 모델**이다.

---

## 2. 설계 목표 (구체)

1. Yone 봇이 **의도대로**: Q 포크/Q3 넉업, E로 진입(commit)·콤보·**불리하면 E 귀환 이탈**, W 쉴드+딜, R 한타/킬각.
2. 챔피언 전술이 **공용 `ChampionAISystem` cpp를 오염시키지 않는** 자리(훅/데이터)에 산다. Yasuo/Jax 하드코딩 부채를 갚을 경로 포함.
3. 전투 스킬 선택이 **반응형**: 매 결정 틱마다 "지금 가장 좋은 다음 행동"을 컨텍스트로 고른다(고정 순환 아님).
4. 선형 콤보(1-A)는 **trivial case로 흡수**해 기존 4챔프 회귀 없이 공존.
5. 전 과정이 **인게임 튜닝 가능**: 선택된 스킬·점수·반응 오버라이드 사유가 trace/패널에 보이고 노브로 조정.
6. 북극성 준수: command-only, Shared/GameSim 소유, 결정론(`tc.pRng`/tick), 튜닝은 `AIDebugControl` 경유.

---

## 3. 설계 공간과 권고

| 옵션 | 내용 | 평가 |
|---|---|---|
| **A. 고정 콤보 개선** | step에 wait/reset/stage 조건 필드 추가 | 여전히 선형·반응 부족. Yone E귀환 표현 난망. **불충분** |
| **B. 순수 Utility(매 틱 스킬 스코어)** | 콤보 개념 버리고 매 틱 최고점 스킬 | 반응적이나 **시퀀스 응집/스택/캐스트락**을 놓쳐 콤보가 산만해짐 |
| **C. 반응형 스코어 정책 + 챔피언 전술 훅 (하이브리드)** | 반응 오버라이드 → 후보(데이터 ability rule + 챔피언 훅) → 스코어 → emit. 스택/스테이지 상태 인지. 선형 콤보는 ordered-scorer로 흡수 | **권고.** 1-B의 행동 품질 + 1-A의 데이터성 + 반응성·튜닝성 |

**권고 = C.** 이유: Yasuo 하드코딩이 이미 "반응형 우선순위 체인"이 옳다는 증거다. 그걸 **버리지 말고 일반화(데이터+훅)** 하면, 행동 품질을 유지하면서 구조 부채를 갚고 Yone 같은 신규 킷을 같은 틀로 표현한다.

---

## 4. 권고 아키텍처 상세

### 4.1 한 틱 = "최선 행동 선택" 루프 (AttackChampion/Dive intent 안에서)

```text
[결정 틱(decisionInterval로 throttle)]
1) 반응형 오버라이드 검사 (champion 훅 + generic)
     - 이탈/귀환(Yone E-return), 회피(추후 skillshot dodge), 캔슬/홀드, bail
     - 발동하면 즉시 emit하고 종료
2) 후보 행동 수집
     - 데이터 ability rule (프로파일) + 챔피언 훅이 기여
     - "지금 사용 가능"만: 쿨다운/사거리밴드/스테이지·스택/타겟상태/자원
3) 스코어링 (Utility within combat)
     - base weight × {사거리 적합, 다음 셋업, 저HP 처형 finisher, 콤보 컨텍스트 보너스, kiteBias}
4) 최고점 후보를 기존 Emit*Command로 방출 (command-only). 콤보/스테이지 메모리 갱신
5) castable 없음 & 교전 의지 → 추격/평타/카이팅(위치). nav가 충돌은 처리
```

기존 `decisionTimer`(0.20s)·`intentHoldTimer`·`IsChampionAIActionLocked`(시전 잠금) 게이트와 1틱 지연 모델은 그대로 따른다.

### 4.2 레이어와 seam

- **반응형 오버라이드**: 챔피언 훅 + generic(저HP/포탑위험은 이미 `ExecuteLaneCombat`의 EmergencyRetreat). Yone E-return은 훅이 소유.
- **후보/스코어 인프라(공용, Shared)**: `ChampionAISkillRule`을 확장한 `ChampionAIAbilityRule`(아래 4.4)과 스코어러. `TryEmitAttackChampionSkill`/`TryEmitAttackChampionCombo`를 이 스코어러로 수렴.
- **챔피언 전술 훅(신규 AI 인프라)**: `IChampionAICombatTactics`(또는 함수포인터 레지스트리). `GetChampionAIProfile`/`GetChampionAIComboPlan` 옆에 `GetChampionAICombatTactics(champion)`로 등록. 서버 GameSim의 `GameplayHookRegistry`(`YoneGameSim::RegisterHooks`) 패턴을 **AI측에 미러**. 공용 `ChampionAISystem`은 훅을 호출만 하고 챔피언 지식을 갖지 않는다.
- **시퀀스/스택 상태**: 새로 만들지 않는다. 기존 GameSim 상태를 read — Yone `YoneSimComponent`/`ResolveEStage`, Yasuo `ResolveQVariantStage`/`YasuoStateComponent.bEActive`. `ChampionAIComponent`에는 결정용 캐시(현재 콤보 goal/phase)만 둔다.

### 4.3 `ExecuteLaneCombat` 통합 위치
현행 셀렉터(`ChampionAISystem.cpp:2248`): `Jax다이브 → 저HP/포탑위험 후퇴 → 진행중 콤보 → 구조물시즈 → Yasuo전투 → 챔피언공격 → 미니언파밍 → 웨이브따라가기`.
→ "Jax다이브/Yasuo전투" 자리를 **단일 `TryExecuteChampionCombatTactics(champion 훅)` 호출**로 대체(훅 없으면 공용 스코어 정책으로 폴백). 셀렉터 자체는 유지(반응형 후퇴 우선순위 보존).

### 4.4 데이터 형태 (스케치, 확정은 구현 계획서에서)

```text
ChampionAIAbilityRule  (ChampionAISkillRule 확장)
  slot
  role           : Poke / Engage / Cc / Shield / Escape / Execute / Mobility / Finisher
  minRange,maxRange
  requiresStage  : (선택) 스테이지/스택 조건 (예: Yone Q3, E SoulOut 상태)
  targetMode     : TargetEntity / SkillshotLead / AwayFromTarget / Self
  selfHpMin, enemyHpMax
  baseScore, setupBonus
```
선형 콤보(1-A)는 "순서 보너스가 큰 ordered ability rule 묶음"으로 동등 표현 → Jax/Fiora/Ashe/Riven 데이터 무손실 흡수.

### 4.5 북극성 준수
- 훅·스코어러 전부 Shared/GameSim. truth 직접수정 금지, `Emit*Command`만.
- 난수(에임오차/실수)는 `tc.pRng->MakeSubSeed(tick,entity,skill)`만.
- 튜닝은 `eChampionAITuningId` 확장 + `AIDebugControl` 왕복(직접 set 금지).

---

## 5. Yone 수직 슬라이스 (첫 적용)

### 5.1 의도 동작 정의 (성공 기준의 말풀이)
- **포크**: 사거리 밖/중립이면 Q로 견제, Q 스택 쌓기.
- **진입(commit)**: 교전 의지 + 유리하면 E(SoulOut)로 진입 → Q3 넉업/W/평타 콤보.
- **이탈(반응형)**: 진입 후 트레이드 불리(HP 열위/포탑위험/적 합류)면 **E(SoulReturn)로 귀환**. 이게 Yone 정체성의 핵심.
- **R**: 한타/확정 킬각(넉업 연계 또는 다수 적 직선)에서.

### 5.2 Yone 훅 결정 스케치
```text
반응형 오버라이드:
  if E가 SoulOut 상태이고 (selfHp<=returnHpRatio or 적합류 or 트레이드손해): E(SoulReturn) 귀환  → 종료

후보/우선순위(컨텍스트 스코어):
  R  : enemy 다수 직선 or 처형각 & R ready & in R range
  Q3 : Q 스택==2(=Q3 준비) & target in Q range  (넉업 셋업)
  E  : commit 가능(HP우위 & 교전의지) & not already SoulOut & in E range  (진입)
  W  : 근접 & W ready  (쉴드+딜)
  Q  : in Q range  (포크/스택)
  BA : 사거리 내
  없으면 추격/카이팅
```
값(사거리 4.75/6.0/10, E 2스테이지)은 `GameplayDefinitionQuery`/`YoneSimComponent`에서 read. `returnHpRatio` 등 임계값은 튜닝 노브로 노출(5.4).

### 5.3 의존 사실 확인 필요
- Yone Q 스택→Q3 판정 API(`YoneGameSim`에 Q stage/stack resolver가 있는지, Yasuo `ResolveQVariantStage` 대응물). 없으면 `YoneSimComponent` 필드 read 또는 resolver 추가.
- E 재시전(SoulReturn) command 형태(같은 E slot 재캐스트로 stage 전이되는지) — `CommandExecutor` Yone E 처리 확인.
- R 타게팅(논타겟 직선) command 형태(direction/groundPos).

---

## 6. 인게임 튜닝 / 디버깅 / 검증 (사용자 워크플로우)

가이드 §4~5를 이 슬라이스에 적용. 모든 새 행동은 **trace에 남고 패널에서 보이고 슬라이더로 만져지게** 추가한다.

- **Trace 확장**: `ChampionAIDecisionTraceEntry`에 "선택된 ability + 점수 + 반응 오버라이드 사유(예: YoneE-Return:LowHp)" 기록. (기존 16링버퍼/`blockReason` 재사용 + 항목 추가.)
- **튜닝 노브**(`eChampionAITuningId` 확장 + `AIDebugPanel` 슬라이더): Yone `returnHpRatio`, `engageHpAdvantage`, `q3SetupBias`, `rMinEnemies`. (절차 = 가이드 §4.3, 30번 계획이 템플릿.)
- **DebugDraw**: E SoulOut 상태/귀환 지점, Q 스택, R 직선 미리보기.
- **검증(둘 병행)**:
  - 인게임 스모크(F9/F5): Yone 선택 → trace에 `E진입 → Q3/W/BA → (불리)E귀환` 순서가 보이는가. R이 다수 적/킬각에만 나가는가.
  - 시나리오 하니스(SimLab, 가이드 §5.1): 고정 seed 1v1 트레이드(유리/불리) 두 케이스 → "불리 시 E귀환" assert, "유리 시 commit" assert. parity 해시 유지.

---

## 7. 단계별 진행 (각 Phase는 verify 통과 후 다음, 인게임 튜닝 병행)

- **Y0 — 전술 훅 seam(신규 AI 인프라)**: `IChampionAICombatTactics` + `GetChampionAICombatTactics(champion)` 레지스트리 추가. `ExecuteLaneCombat`의 챔피언 분기를 훅 호출로(훅 없으면 기존 경로 폴백). **Yasuo/Jax는 건드리지 않음**(회귀 0). verify: 전 챔프 trace 무변화.
- **Y1 — Yone 전술 훅 구현**: 5.2 결정 로직을 Yone 훅에 구현(진입/콤보/E귀환/Q3/R). verify: 인게임 스모크 + 시나리오 2케이스.
- **Y2 — 공용 스코어 정책 일반화**: `ChampionAIAbilityRule` + 스코어러를 Yone 훅에서 추출해 공용화. 선형 콤보를 ordered-scorer로 재해석(back-compat). verify: Jax/Fiora/Ashe/Riven trace 동등.
- **Y3 — 부채 상환**: Yasuo/Jax 하드코딩을 훅/데이터로 이관. verify: Yasuo/Jax trace parity(이관 전후 동일 시퀀스).
- **Y4 — 튜닝 surface 완성**: Yone(및 공용) 노브를 패널/trace에 노출. verify: 슬라이더로 E귀환 임계값 바꾸면 다음 스냅샷에 반영·체감.

(가이드의 Phase A "brainType/난이도 배선"은 이 트랙과 독립. PlayerLike brain 확장은 Phase C에서. 본 설계는 "전투 스킬 결정 구조"에 한정.)

---

## 8. 안티패턴 / 리스크

- 새 챔피언 지식을 공용 `ChampionAISystem` if-chain에 추가 금지(훅으로). Yasuo/Jax를 따라 하지 말 것.
- 비권위 Engine MCTS/BT를 전투 결정에 끌어쓰지 말 것(가이드 §1-8).
- 결정론: 스코어 타이브레이크는 안정 키(slot/EntityID), 난수는 `tc.pRng`만.
- 과설계 경계(Karpathy): Y0~Y1은 Yone 하나로 좁게. 일반화(Y2)는 Yone이 인게임에서 "의도대로" 검증된 뒤. 데이터 외부화(JSON/Lua)는 가이드의 사다리대로 챔프 8+ 또는 기획 수정 필요 시까지 보류.
- 성능: 후보 스코어가 매 결정 틱 world 순회를 늘리지 않게(이미 `BuildChampionAIContext`가 수집한 값 재사용). 추가 순회는 scope/counter로 측정.

---

## 9. 확인 필요 결정

- **(a) 훅 형태**: `IChampionAICombatTactics` 가상 인터페이스 vs 함수포인터 레지스트리(`GameplayHookRegistry` 스타일). 후자가 GameSim 기존 패턴과 정합.
- **(b) Yone E 재시전 계약**: 같은 E slot 재캐스트로 SoulReturn 전이인지(`CommandExecutor`/`YoneGameSim` 확인 필요) → command 산출 방식 결정.
- **(c) Y0 착수 범위**: 훅 seam만 먼저(회귀 0) vs Yone 구현까지 한 번에.
- **(d) 콤보 흡수 시점**: 선형 콤보를 Y2에서 ordered-scorer로 흡수 vs 당분간 1-A 유지하고 신규 챔프만 훅.
```text
권고 기본값: (a) 레지스트리, (b) 확인 후, (c) 훅 seam(Y0)→Yone(Y1) 분리, (d) Y2까지 1-A 유지.
```
