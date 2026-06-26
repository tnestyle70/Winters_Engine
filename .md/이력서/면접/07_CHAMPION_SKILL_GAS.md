# 면접 대비 — 챔피언/스킬 시스템 (GAS 등가) 도메인

> 대상: Winters 엔진 `Shared/GameSim`의 챔피언·스킬 권위 시뮬레이션
> 근거 문서: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` §7 (정직성 경계 준수)
> 작성 원칙: 구현된 것은 파일:라인으로 증명, 미구현은 "계획"으로 명시. 과장 금지.

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: "LoL류 PvP의 *서버 권위 + 결정론* 요구를 만족시키기 위해, UE5 GAS의 통찰(스킬=데이터+작은 trigger, 상태=tag, 효과=effect)을 1급 Tag/Effect 객체 없이 ECS로 재해석한 스킬 시스템이다. 모든 스킬 시전은 단일 서버 게이트(`CommandExecutor::HandleCastSkill`)를 통과하고, champ×variant **함수포인터 디스패치 테이블**로 챔피언별 로직에 분기한다."

**현재 성숙도 (working, 완성도 편차 큼)**:
- **production/working**: 단일 캐스트 게이트, 함수포인터 훅 레지스트리, cooldown/learned-rank/CC/타겟·사거리/2단스테이지 서버 판정, 15챔피언 등록, 데이터팩 기반 파라미터 조회(fallback 잔존), 결정론 골든 테스트(SimLab).
- **부분/진행 중**: 데이터주도 cutover (모든 resolver가 `ChampionGameDataDB` 상수 fallback 유지), 챔피언 완성도 편차 (Zed/Yasuo/Viego 풀 구현, Riven Q/W만, **MasterYi는 빈 스텁**).
- **planned-only**: 1급 GameplayTag/AttributeSet/GameplayEffect 객체, AbilityTask 빌딩블록, 오프라인 codegen 산출 테이블의 완전 cutover, validator/cook 파이프라인, client prediction/reconciliation.

> 정직성 핵심: 나는 "GAS를 만들었다"고 말하지 않는다. "GAS의 통찰을 ECS로 등가 구현했고, Tag/Effect를 1급 객체로 만들지 않은 것은 *의도적 선택*"이라고 말한다. 코드에 `GameplayTag`/`AttributeSet`이라는 명명은 0건이다.

---

## 1. 핵심 개념 (본질)

### 1.1 왜 "스킬 시스템"이 어려운가 — 조합 폭발 (first principles)

문제의 1차 원리: 챔프 150 × 스킬 4 = 600개, 각 스킬은 cooldown·cost·range·damage·CC·timing·조건을 가진다. 여기에 패시브·아이템·룬·버프까지 곱해진다. 이걸 `if (champ == Zed) cast()`로 짜면 600함수 + 수만 if가 되어 유지 불가능하다 (`08_Ch8_GAS.md:9~17`).

**GAS의 통찰 5가지** (`08_Ch8_GAS.md:19~24`):
1. 스킬 = **데이터 + 작은 trigger 코드**의 조합.
2. 스탯(HP/MP/AD/AP) = **dirty 추적되는 attribute**.
3. 상태(stun/silence/airborne) = **tag** (계층·오너십 있는 string).
4. 효과(데미지/회복/슬로우) = **GameplayEffect** (적용·제거가 1급 연산).
5. 스킬 = **AbilityTask**(projectile/channel/dash 같은 빌딩블록)의 시퀀스.

이 5가지로 600개를 "데이터 + 30~50개 빌딩블록 함수"로 압축하는 것이 GAS의 본질이다.

### 1.2 PvP가 추가로 강제하는 제약 — 서버 권위 + 결정론

LoL류 PvP에서 스킬 시스템은 단독으로 존재할 수 없다. 두 가지 근본 제약이 설계를 지배한다:

- **서버 권위(server authority)**: "쿨다운이 돌았는지, 사거리 안인지, 맞았는지, 데미지가 얼만지"는 **클라이언트가 주장하면 안 된다**. 클라가 위치/결과를 보내면 스피드핵·텔레포트의 공격 표면이 열린다. 그래서 클라는 "의도(GameCommand)"만 보내고 서버 GameSim이 결과를 확정한다 (`14_..._SERVER_AUTH_PIPELINE.md:14~22`).
- **결정론(determinism)**: 같은 seed + 같은 입력이면 모든 머신에서 bit-identical 결과가 나와야 한다. 이게 깨지면 스냅샷 복제·리플레이·회귀 테스트가 전부 무너진다. → xorshift/FNV 고정 RNG, EntityID 정렬 순회(`DeterministicEntityIterator`), 고정 tick(30Hz)이 강제된다.

> 그래서 내 스킬 시스템의 모든 설계 결정은 "이게 서버에서만 확정되는가? 결정론을 깨지 않는가?"라는 두 질문을 통과해야 했다. UE5 GAS가 가진 **client prediction이 1급**이라는 특성(`08_Ch8_GAS.md:178~190`)을 일부러 뒤로 미룬 것도 이 때문이다 — 먼저 권위·결정론을 세우고, 예측은 그 위에 얹는다.

### 1.3 ECS에서 "tag"와 "effect"는 무엇이 되는가

ECS는 데이터 지향(컴포넌트=연속 메모리, system=로직)이다. GAS의 개념을 ECS로 옮기면:
- **tag(상태)** → 비트플래그. `GameplayStateComponent.stateFlags`의 `u32_t` 비트 (`kGameplayStateStunnedFlag = 1u<<0` … `kGameplayStateAirborneFlag = 1u<<8`, `GameplayComponents.h:379~388`). `HasTag("Status.Stun")` 대신 `(stateFlags & kStunnedFlag) != 0`.
- **effect(효과)** → `StatusEffectComponent`의 고정 크기 인스턴스 배열. 각 인스턴스는 `effectId/stateFlags/fRemainingSec/stackPolicy`를 들고, 매 틱 `RebuildGameplayState`가 살아있는 인스턴스들의 `stateFlags`를 OR-fold해서 `GameplayStateComponent`를 재구성한다 (`StatusEffectSystem.cpp:228~276`).
- **attribute(스탯)** → `StatComponent` 등 평범한 ECS 컴포넌트. clamp/보정은 ECS system이 처리.

핵심: GAS의 string tag-tree + reflection 기반 AttributeSet의 **유연성을 포기**하는 대신, **결정론·캐시 지역성·서버 단순성**을 얻는 트레이드다.

---

## 2. 왜 이 선택인가 — 기술 스택 + Trade-off

### 2.1 1급 GAS 객체 vs 함수포인터 훅 + ECS 컴포넌트

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **UE5 GAS 1급 객체 포팅** (GameplayTag tree / AttributeSet reflection / GameplayEffect Spec) | 디자이너 자율성 최강, 데이터로 cancel/block, 검증된 설계 | string tag 해시·reflection은 결정론/직렬화에 위험, ASC가 무겁고 UObject 의존, 신입 1인이 충실 포팅 = 수개월 | **버림** |
| **함수포인터 디스패치 + tag=비트플래그 + effect=컴포넌트 배열** (현재) | 결정론 친화(비트연산), 캐시 지역성, 서버 단순, 1인 범위 적합 | tag 계층 매치 없음(flat 비트), 효과 종류 확장 시 비트/컴포넌트 수기 추가, 디자이너 자율성 낮음 | **선택** |
| **virtual IChampionController 다형성** (로드맵 Phase 4) | OOP 친화, 챔프별 캡슐화 | vtable 간접호출, 챔프 인스턴스마다 객체 수명 관리, 16개 정도엔 과설계 | **로드맵에만**(`00_ROADMAP.md:56~76`) |

근본 트레이드: **"유연성/디자이너 자율성"(GAS)을 버리고 "결정론/단순성/구현 비용"을 산다.** champ×variant를 `(champ<<16)|variant`의 정수 hookId로 만들어 256×256 함수포인터 2D 테이블(`GameplayHookRegistry.h:34~37`)에 O(1) 디스패치하는 건, 신입 1인이 16챔프를 서버 권위로 실제 굴리는 데 가장 빠르고 디버깅 쉬운 경로였다.

### 2.2 offline codegen 데이터팩 vs runtime JSON vs 하드코딩

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **하드코딩 테이블** (`ChampionRuntimeDefaults.cpp`) | 즉시, 빌드 보장 | 150챔프 협업 불가, 리뷰 단위 없음 | 레거시(fallback로 잔존) |
| **runtime JSON 파싱** | hot reload 가능 | 매 프레임 string lookup, 파싱 견고성/결정론 위험 | 버림 |
| **offline codegen 불변 데이터팩** (현재 진행) | frame 경로는 dense index만, string lookup 0, 결정론 안전, champion별 파일=리뷰 단위 | hot reload 없음, cook 단계 필요 | **선택(부분 cutover)** |

데이터팩(`SkillGameplayDefs.json`)은 `definitionKey`(stable 해시) + `defId`(dense 정수) + `buildHash`를 갖는다 (`SkillGameplayDefs.json:2~9`). 런타임은 `SkillLoadoutComponent`의 dense id로 `pack->FindSkill(...)`만 조회한다 (`GameplayDefinitionQuery.cpp:123~130`). 

> **정직성**: cutover는 미완이다. 모든 resolver가 `if (데이터팩에서 찾으면) return 값; else return ChampionGameDataDB::Resolve*(...)`로 **상수 fallback을 유지**한다 (`GameplayDefinitionQuery.cpp:181, 198, 254, 272, 289` 등). 이건 "데이터주도 100%"가 아니라 "진행 중"이다.

### 2.3 왜 client prediction을 안 했나 (의도적 후순위)

GAS는 prediction이 1급이지만, 나는 **먼저 권위 루프를 닫고 결정론을 증명**하는 걸 택했다. 예측 없이 "권위 스냅샷을 직접 적용"하면 입력 지연이 보이지만, 그 대신 *truth가 단 하나*라 디버깅·결정론 검증이 단순하다. 예측/reconciliation은 그 위에 얹을 계획(§6)이지 지금 없다 (`HONEST_MAP §6 레드플래그`).

---

## 3. 실제 구현 (코드 근거)

### 3.1 단일 캐스트 게이트 — `CDefaultCommandExecutor::HandleCastSkill`

모든 스킬 시전은 이 한 함수(`CommandExecutor.cpp:1932~2231`)를 통과한다. 서버가 순서대로 확정하는 게이트:

1. **선행 검사** (`:1935~1961`): `SkillStateComponent` 존재 → slot<5 → `CanCast`(CC 차단) → 타겟 생존 → `CanBeTargetedBy`(은신/무적/팀).
2. **스킬 정체성 해석** (`:1963~1973`): `CSpellbookFormOverrideSystem::ResolveSkill`로 *실제로 시전될* champ/slot을 해석. Viego 빙의·Sylas 탈취 시 hookChampion ≠ 본인 챔피언이 된다.
3. **learned-rank 검사** (`:1976~1980`): `IsSkillLearned` 실패 시 `reason="unlearned"` reject.
4. **2단 스테이지 판정** (`:1982~2040`): `GameplayDefinitionQuery::IsSkillTwoStage`로 stage2 입력 윈도우 검사. Yasuo Q variant/R 공중타겟, Yone E 변형 같은 비자명 분기.
5. **쿨다운 검사** (`:2055~2059`): `slot.cooldownRemaining > 0.f`면 `reason="cooldown"` reject.
6. **사거리 검사** (예: Annie Q, `:2061~2089`): `DistanceSqXZ > effectiveRange²` (거리제곱 비교, sqrt 회피)면 **AttackChase**로 전환(거리 안으로 붙은 뒤 자동 재시전).
7. **쿨다운/스테이지 소유** (`:2093~2128`): 서버가 cooldown을 slot에 기록. Sylas 탈취 캡처는 쿨 0.
8. **훅 디스패치** (`:2181~2198`): `BuildPrimarySkillHookId(champ, slot)`로 hookId 생성 → `DispatchGameplayHookIfAvailable`. 미등록이면 `EnqueueFallbackSkillDamage`.
9. **복제 이벤트** (`:2200~2231`): `SkillCast` + `EffectTrigger` ReplicatedEvent를 큐에 넣어 클라가 1회 재생.

핵심: **클라가 보낸 `cmd`는 issuer/slot/target/direction "의도"일 뿐**이고, 위 1~9는 전부 서버 월드 상태 기준으로 판정된다.

### 3.2 함수포인터 디스패치 테이블 — `CGameplayHookRegistry`

- 자료구조: `HookFn m_table[256][256]` (champ×variant 2D, `GameplayHookRegistry.h:34~37`). `HookFn = void(*)(GameplayHookContext&)`.
- hookId 인코딩: `MakeGameplayHookId(champ, variant) = (champ<<16)|variant` (`.h:39~42`). variant는 슬롯×단계 의미 (예: `Q_CastFrame=0x0032`, `R_OnCastAccepted=0x0025`, `.h:44~69`).
- 디스패치: `Dispatch(hookId, ctx)` = champ/variant 추출 → `m_table[champ][variant]` 호출, 미등록이면 false (`.cpp:16~25`). O(1), 분기 없음.
- 컨텍스트: `GameplayHookContext`는 `pWorld/casterEntity/casterTeam/casterChampion/skillRank/pDef/pCommand/pTickCtx`를 묶어 훅에 넘긴다 (`.h:9~19`).

챔피언 등록 (예: Zed, `ZedGameSim.cpp:737~752`):
```cpp
CGameplayHookRegistry::Instance().Register(
    MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::Q_CastFrame), &OnQ);
// W/E/R 동일. s_bRegistered 가드로 1회만.
```
전 챔피언 등록은 `SimLab/main.cpp:136~153`의 `RegisterAllChampionHooks()`에서 15개를 호출(서버는 `GameRoom.cpp`에서 동일 호출).

훅 본문 (예: Zed OnQ, `ZedGameSim.cpp:450~514`): 방향 해석 → 회전/이동정지 → `GameplayDefinitionQuery`로 range/speed/radius/damage 파라미터 조회 → `SpawnZedShuriken` (그림자 있으면 2발). **수치는 데이터팩에서, 메커니즘은 코드에서.**

### 3.3 CC(군중제어) = tag-비트 OR-fold

- 시전 차단(`GameplayStateQuery.cpp:116~122`): `CanCast` = `!(stateFlags & (cannotCast|stunned))`. `CanMove`/`CanAttack`도 동일 패턴.
- 효과 적용(`StatusEffectSystem.cpp:58~129`): `ApplyStatusEffect` → `UpsertEffect`(stack policy: KeepLongest/Refresh/AddIndependent) → `RebuildGameplayState`.
- 상태 재구성(`:228~276`): 살아있는 모든 `StatusEffectInstance.stateFlags`를 `state.stateFlags |= ...`로 OR-fold + 슬로우는 `fMoveSpeedMul` min-fold. `StunComponent`가 있으면 stun+cannotMove+cannotAttack+cannotCast 합성.

이게 "GAS의 tag aggregation"을 비트연산으로 등가 구현한 부분이다. 디버그도 비트마스크 하나만 보면 된다.

### 3.4 쿨다운/스테이지 윈도우 tick — `CSkillCooldownSystem::Execute`

`SkillCooldownSystem.cpp:148~209`: `DeterministicEntityIterator<SkillStateComponent>::CollectSorted`로 EntityID 정렬 순회 → 5슬롯 각각 `cooldownRemaining -= dt`, 0이하면 clamp. `currentStage==1 && stageWindow>0`면 윈도우 감소, 만료 시 stage 리셋. 정렬 순회가 **결정론을 보장**하는 지점.

### 3.5 데이터주도 파라미터 조회 — `GameplayDefinitionQuery`

`GameplayDefinitionQuery.cpp` 전체가 "데이터팩 우선, 못 찾으면 상수 fallback" 패턴. 예: `ResolveSkillCooldown`(`:186~201`): `FindSkill` 성공 시 `skill->cooldown.cooldownSec`, 실패 시 `ChampionGameDataDB::ResolveSkillCooldown(...)`. `FindSkill`(`:110~138`)은 `SkillLoadoutComponent`의 dense id → `pack->FindSkill` → 없으면 챔피언 def의 skillLoadout[slot]. **string lookup 0건**.

### 3.6 폼 오버라이드 — Viego 빙의 / Sylas 탈취

`CSpellbookFormOverrideSystem::ResolveSkill`이 `hookChampion/hookSlot/sourceRank/bConsumeSpellbookOnAccept`를 반환(`CommandExecutor.cpp:1965~1973`). Sylas R 탈취는 캡처 시점(`:2042~2053`)에 대상 궁을 가져오고 쿨 0으로 처리(`:2124~2128`), Viego는 처치 시 FormOverride 컴포넌트로 적 챔피언 훅으로 디스패치된다. → **함수포인터 테이블 + 폼 해석 레이어**로 "남의 스킬 쓰기"를 1급 메커니즘으로 구현.

---

## 4. 검증 — 동작을 어떻게 증명했나

### 4.1 결정론 골든 테스트 (SimLab) — 1차 회귀 게이트

`Tools/SimLab/main.cpp`: 헤드리스 5v5 스크립트 매치를 30Hz로 돌려 per-tick state hash를 비교 (`:1~4`).
- 해시: FNV-1a basis `1469598103934665603`로 tick·position·HP·dead·mana·level·gold·**RNG state**를 fold (`:420~438`). RNG state까지 넣어 결정론 누수를 잡는다.
- 판정(`:616~653`):
  - **runA vs runB (same seed)**: 불일치 시 첫 divergent tick 출력 + `exit 1` (`:634`).
  - **runC (seed+1)**: A와 동일 해시면 "해시가 sim 상태를 안 잡음" 실패(`:649`). 즉 *해시 자체의 민감도*도 검증.
- exit code로 CI 게이트화. 같은 seed+커맨드 → 동일 hash가 "스킬 시스템이 결정론적으로 동작한다"의 판정 기준.

### 4.2 accept/reject 사유 로깅

`HandleCastSkill`의 모든 거부 경로가 `LogCastSkill("reject", reason, ...)`로 사유 코드를 남긴다 (unlearned/cooldown/state-blocked/dead-target/untargetable/no-airborne/no-hijack-target/stage2-window). accept도 stage 정보와 함께 로깅(`:2138`). → "왜 시전이 씹혔나"를 코드 읽지 않고 로그로 진단.

### 4.3 측정 범위의 정직한 한계
- SimLab은 `FlatWalkable`(navgrid 없는 평면) GameSim-only 미러다. 실게임의 pathfinding/맵 충돌은 포함 안 됨.
- 골든 해시는 위 7개 필드만 잡는다. 스킬별 세부(투사체 위치 등)는 직접 해시에 안 들어가므로, 그쪽은 인게임 수동 확인 + accept 로그로 본다.
- 자동 "스킬 단위 단위테스트"는 없다 → "결정론 게이트 + 사유 로그 + 인게임 수동 검증"이 현재 검증 수단이다.

---

## 5. 최적화

### 5.1 실제로 한 것
- **함수포인터 2D 테이블 O(1) 디스패치**: champ×variant를 정수 hookId로 만들어 분기/string 비교 없이 `m_table[champ][variant]` 단일 인덱싱(`GameplayHookRegistry.cpp:16~25`). 챔프 추가가 디스패치 비용을 키우지 않음.
- **거리제곱 비교**: 사거리 검사에서 sqrt 회피, `DistanceSqXZ > range²` (`CommandExecutor.cpp:2376, 2081`).
- **데이터팩 dense index 조회**: frame 경로 string lookup 제거 (`GameplayDefinitionQuery.cpp`).
- **CC = 비트 OR-fold**: 상태 합성을 비트연산으로 (`StatusEffectSystem.cpp:247`).
- **stateFlags 캐싱**: 매번 효과 배열을 순회하지 않고 `RebuildGameplayState`가 변경 시 한 번 fold해서 `GameplayStateComponent`에 캐싱, 시전 게이트는 캐싱된 비트만 읽음.

> **정직성**: 위는 "설계상 효율적"이지 "측정으로 X% 빨라졌다"가 아니다. 챔피언/스킬 경로 단독 프로파일 수치는 **측정 예정**이다. 정량 수치를 댈 수 있는 건 도메인 #12(프로파일러)이고, 거긴 9.54ms/94드로우콜 같은 캡처가 있지만 이 스킬 경로만 분리한 budget은 아직 없다.

### 5.2 계획 중인 최적화
- 데이터팩 완전 cutover로 fallback 분기 자체를 제거(분기 예측 안정화 + 코드 경로 단일화).
- 스킬 시전 경로에 전용 프로파일러 scope를 박아 "캐스트 게이트 + 훅 디스패치"의 per-tick 비용을 측정 → budget 설정.

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

> 이 도메인의 미구현 부분은 **실제로 구현할 로드맵**이다. "그건 안 했죠?"에 "네, 안 했고 이렇게 할 겁니다"로 답할 수 있어야 한다.

### 6.1 1급 GameplayTag (계층 매치) — Ch8 Stage1

- **무엇/왜**: 지금 tag는 flat `u32_t` 비트라 32종이 상한이고 계층 매치(`Status` ⊃ `Status.Stun`)가 없다. 챔프가 늘면 "은신을 푸는 모든 효과", "이동불가 계열 전체" 같은 *집합 질의*가 필요해진다.
- **어떻게**: `struct GameplayTag { u32_t hash; }` + 부모 해시 테이블 (`08_Ch8_GAS.md:305~307`). 계층은 **오프라인 cook 시 부모 비트마스크로 펼쳐** 런타임은 여전히 비트 AND로 매치 → 결정론·속도 유지하면서 계층 의미만 얻는다.
- **Trade-off**: cook 복잡도 증가 vs 런타임 비용 0. 32종 상한은 `u64_t` 2개 또는 bitset으로 확장.
- **검증**: tag 매치 단위테스트(부모 질의가 자식 전부 매치) + SimLab 해시 불변(런타임 표현이 비트면 결정론 유지).

### 6.2 1급 GameplayEffect (Instant/Duration/Periodic) — Ch8 Stage3~4

- **무엇/왜**: 지금 데미지/CC는 챔피언 코드가 직접 `ApplyStatusEffect`/DamageRequest를 호출한다. dot(도트딜)·주기 효과·매그니튜드 스케일을 **데이터로** 표현하면 챔프 코드가 더 얇아진다.
- **어떻게**: `GameplayEffectDef{ durationPolicy, duration, period, modifiers[], grantedTags }` (`08_Ch8_GAS.md:309~317`). `ApplyEffect(source, target, effectId)` → ECS의 `StatusEffectComponent`/타이머에 인스턴스 등록, periodic은 쿨다운 system이 tick. 즉 **지금의 `StatusEffectInstance`를 데이터팩에서 cook**해 채우는 형태.
- **Trade-off**: 표현력↑ vs cook/스키마 부담. 결정론을 위해 effect 적용 순서는 EntityID 정렬·고정 우선순위로.
- **검증**: dot 효과의 총 데미지·만료 tick을 골든 해시에 추가, accept 로그에 effect id 기록.

### 6.3 AbilityTask 빌딩블록 — Ch8 Stage6

- **무엇/왜**: 지금 각 챔프 OnQ/OnW가 projectile spawn·dash·channel을 직접 작성한다(중복). `SpawnProjectile/Dash/WaitTarget/ApplyEffect`를 **데이터 시퀀스**로 (`08_Ch8_GAS.md:300~303`).
- **어떻게**: `AbilityDef.tasks: vector<AbilityTaskInstance>`. 캐스트 게이트가 task 시퀀스를 순차 실행. 30~50개 빌딩블록으로 600스킬 압축.
- **Trade-off**: 디자이너 자율성↑·코드 중복↓ vs task VM/스케줄러 복잡도. 비동기 task(channel/wait)는 결정론 tick 기반 상태머신으로.
- **검증**: 기존 챔프 한 명을 task 시퀀스로 재작성해 **골든 해시가 기존과 동일**한지 비교(behavior-preserving 리팩터링 증명).

### 6.4 validator/cook codegen 완전 cutover — 서버권위 파이프라인 S5~S6

- **무엇/왜**: 지금 fallback 상수가 잔존하고 cook은 부분적. 150챔프 협업엔 champion별 authoring 파일 + validator가 필수 (`14_..._PIPELINE.md:174~207`).
- **어떻게**: `Data/ChampionGameData/<Champion>.json` (챔프=리뷰 단위) → validator(id 유일성/슬롯 완전성/stage 유효성/finite 값/visual·gameplay 필드 분리/schema version/hash) → `ChampionGameData.gen.h/.cpp` 생성 → `ChampionGameDataDB`가 generated table 읽음 → fallback 분기 제거.
- **Trade-off**: cook 인프라 구축 비용 vs 협업 확장성·리뷰 단위·hot tuning 경로.
- **검증**: server/client가 같은 data hash를 boot log로 출력(불일치 탐지), validator exit code 게이트.

### 6.5 client prediction / reconciliation — Ch8 Stage8

- **무엇/왜**: 지금은 권위 스냅샷 직접 적용이라 입력→시각 지연이 보인다. 반응성을 위해 클라가 낙관적으로 cast를 미리 재생하고 서버 결과로 보정.
- **어떻게**: PredictionKey 발급 → 클라 낙관 실행 → 서버 같은 `CanActivate` 검사 → 결과 도착 시 일치하면 무시/불일치면 rollback (cooldown 회복·fx 취소) (`08_Ch8_GAS.md:178~190`). **권위는 그대로**, 클라에 예측 레이어만 추가.
- **Trade-off**: 반응성↑ vs rollback 복잡도·미스예측 시 시각 튐. 서버 권위 불변식은 유지(클라는 *표현*만 예측).
- **검증**: 인위적 지연 주입 후 미스예측 rollback이 시각적으로 복구되는지, 서버 결과와 클라 최종 상태 일치 확인.

### 6.6 MasterYi 등 미완 챔피언 채우기

- **무엇/왜**: MasterYi는 빈 스텁(`MasterYiGameSim.cpp:5~7`은 `RegisterHooks(){}` 본문 0), Riven은 Q/W만(`RivenGameSim.cpp:166~175`).
- **어떻게**: 기존 풀구현 챔프(Zed)의 OnQ/W/E/R 패턴 복사 → 메커니즘 치환 + 데이터팩 엔트리. 파이프라인 정착 후 챔프당 목표 ~2시간(`00_ROADMAP.md:96`).
- **검증**: 등록 후 SimLab에 해당 챔프 커맨드 추가 → 결정론 유지 확인.

---

## 7. 면접 예상 질문 & 모범 답변

**Q1 (기본). GAS가 뭔지, 왜 LoL 같은 게임에 필요한가?**
A. GAS의 핵심 통찰은 "스킬 = 데이터 + 작은 trigger 코드"입니다. 챔프 150 × 스킬 4 = 600개를 if문으로 짜면 유지 불가라서, 상태를 tag로, 효과를 GameplayEffect로, 스탯을 attribute로, 스킬을 task 시퀀스로 데이터화해 30~50개 빌딩블록으로 압축하는 설계입니다. LoL은 이 조합 폭발이 가장 심한 장르라 이 추상화가 필수입니다.

**Q2 (기본). 너의 스킬 시전은 어디서 어떻게 판정되나?**
A. 단일 게이트 `CommandExecutor::HandleCastSkill` 한 곳입니다. 클라는 "Q를 이 타겟에" 같은 의도(GameCommand)만 보내고, 서버가 SkillState 존재→CanCast(CC)→타겟 생존/타게팅→learned-rank→2단 스테이지→쿨다운→사거리 순으로 전부 서버 월드 기준으로 확정합니다. 통과하면 함수포인터 훅으로 챔프 로직에 디스패치하고, SkillCast/EffectTrigger 이벤트를 복제합니다.

**Q3 (설계 의도). 왜 함수포인터 테이블인가? virtual이나 std::function이 더 깔끔하지 않나?**
A. 세 가지 이유입니다. (1) champ×variant를 `(champ<<16)|variant` 정수로 만들면 256×256 2D 테이블에 O(1) 인덱싱이라 챔프가 늘어도 디스패치 비용이 안 늘고 분기 예측 부담이 없습니다. (2) raw 함수포인터는 객체 수명·heap 할당이 없어 결정론·디버깅이 단순합니다. (3) std::function은 type erasure로 간접호출+할당 비용이 있습니다. virtual IChampionController는 로드맵 Phase4에 있지만, 16챔프 규모엔 과설계라 판단했습니다.

**Q4 (설계 의도). CC(stun/silence)는 어떻게 구현했나?**
A. tag를 1급 객체 대신 `u32_t` 비트플래그로 등가 구현했습니다. 효과는 `StatusEffectComponent`의 인스턴스 배열에 들어가고, 매 틱 `RebuildGameplayState`가 살아있는 인스턴스의 stateFlags를 OR-fold해서 `GameplayStateComponent`에 캐싱합니다. 시전 게이트의 CanCast는 `!(stateFlags & (stunned|cannotCast))` 비트 AND 한 번입니다. GAS의 tag aggregation을 비트연산으로 바꾼 거죠.

**Q5 (심화). 결정론은 어떻게 보장하고 어떻게 증명하나?**
A. 보장은 세 가지로 합니다 — 고정 30Hz tick, xorshift 결정론 RNG, `DeterministicEntityIterator`의 EntityID 정렬 순회(쿨다운/효과 tick이 전부 정렬 순회). 증명은 SimLab 골든 테스트입니다. 헤드리스 5v5를 두 번 돌려(same seed) per-tick FNV 해시가 bit-identical인지 보고, seed+1로 한 번 더 돌려 해시가 *달라지는지*도 봅니다 — 해시 자체가 sim 상태를 안 잡으면 그것도 잡으려고요. divergence면 exit 1로 CI에서 막힙니다.

**Q6 (adversarial). "GAS를 만들었다"고 했는데, 코드에 GameplayTag도 AttributeSet도 없던데요?**
A. 정확합니다. 저는 "GAS를 만들었다"가 아니라 "GAS의 *통찰을 ECS로 등가 구현*했다"고 말합니다. Tag/Effect를 1급 객체로 만들지 **않은 건 의도적 선택**입니다 — UE5의 string tag-tree와 reflection AttributeSet은 결정론·직렬화에 위험하고 ASC가 무거워서, 서버 권위 PvP에선 tag=비트플래그, effect=컴포넌트 배열, dispatch=함수포인터가 더 맞습니다. 1급 GameplayTag 계층 매치는 Ch8 Stage1로 설계해 뒀고, 계층을 cook 시 비트마스크로 펼쳐 런타임은 비트 AND를 유지하는 방향입니다.

**Q7 (adversarial). 15챔피언 다 됐다고요? 직접 코드 보여주세요.**
A. "15명 배선, 완성도 편차 있음"이 정확한 표현입니다. Zed/Yasuo/Viego는 QWER 풀구현이고요 — 예를 들어 ZedGameSim은 OnQ/W/E/R 4개를 다 등록하고 그림자 분신까지 처리합니다. 반대로 **Riven은 Q/W만 등록**돼 있고(`RivenGameSim.cpp:166~175`에 E/R Register 없음), **MasterYi는 빈 스텁**입니다(`RegisterHooks(){}` 본문 0줄). 이건 제 README 구현상태 표에도 그어 둔 경계입니다. 파이프라인이 정착됐으니 미완 챔프는 Zed 패턴 복사로 챔프당 ~2시간이 목표입니다.

**Q8 (adversarial). Ezreal, Garen도 구현했나요? 클라에 있던데.**
A. 아니요, 그건 오히려 반례로 말씀드립니다. Ezreal/Garen은 클라이언트측 PoC고 서버에 배선이 안 돼 있어서, 제 서버 권위 규칙을 *위반하는* 케이스입니다. 구현 챔피언으로 카운트하지 않습니다 — 서버 GameSim의 `RegisterAllChampionHooks`(`SimLab/main.cpp:139~152`)에 등록된 15명만 권위 챔피언입니다. 이 구분 자체가 제 권위 모델이 살아있다는 증거예요.

**Q9 (adversarial). 데이터주도라면서 코드에 상수가 남아있던데, 데이터주도 맞나요?**
A. "부분 cutover, 진행 중"이 정직한 답입니다. `GameplayDefinitionQuery`의 모든 resolver가 "데이터팩에서 찾으면 그 값, 없으면 `ChampionGameDataDB` 상수 fallback" 구조라(예: `ResolveSkillCooldown` `:193~200`) fallback 상수가 잔존합니다. 70KB `SkillGameplayDefs.json`이 `definitionKey`+dense `defId`+`buildHash`로 cook돼 있고 frame 경로는 string lookup이 0이지만, validator/codegen 완전 cutover와 fallback 제거는 서버권위 파이프라인 S5~S6 계획입니다.

**Q10 (심화). client prediction은요? LoL은 그게 핵심 아닌가요?**
A. 맞습니다, 그래서 *의도적으로 후순위*에 뒀습니다. 먼저 권위 루프를 닫고 결정론을 증명하는 게 우선이었어요 — 예측을 먼저 넣으면 truth가 둘(예측/권위)이 돼 디버깅·결정론 검증이 복잡해집니다. 지금은 권위 스냅샷 직접 적용이라 입력 지연이 보이지만 truth가 하나라 단순합니다. 예측은 PredictionKey + 미스예측 rollback으로 *표현 레이어에만* 얹을 계획이고(Ch8 Stage8), 서버 권위 불변식은 그대로 유지합니다.

**Q11 (심화). Viego 빙의나 Sylas 궁 탈취처럼 "남의 스킬 쓰기"는 어떻게 하나?**
A. `CSpellbookFormOverrideSystem::ResolveSkill`이 캐스트 게이트 안에서 *실제 시전될* hookChampion/hookSlot/sourceRank를 해석합니다(`CommandExecutor.cpp:1965~1973`). 함수포인터 테이블이 champ×variant라 hookChampion만 바꾸면 적 챔피언의 OnQ가 그대로 디스패치됩니다 — Sylas R은 캡처 시점에 대상 궁을 가져오고 쿨 0으로 처리(`:2124~2128`), Viego는 처치 시 FormOverride 컴포넌트로 적 훅을 가리키게 합니다. 1급 객체 없이도 폼 해석 레이어 하나로 비자명 메커니즘이 됩니다.

**Q12 (확장). 150챔프로 확장하면 이 설계 어디가 먼저 깨지나?**
A. 두 곳입니다. (1) tag가 flat 32비트라 상태 종류가 32개를 넘으면 한계 — bitset 확장 + 계층 cook이 필요합니다. (2) 챔프 코드가 OnQ/W/E/R에서 projectile/dash를 직접 작성하는 중복 — AbilityTask 빌딩블록(Ch8 Stage6)으로 데이터 시퀀스화해야 합니다. 그리고 협업 측면에선 fallback 상수 + 하드코딩이 merge 충돌을 키우니 champion별 authoring 파일 + validator(서버권위 파이프라인 S5~S6)가 선결입니다. 이건 제가 로드맵에 이미 단계로 그어 둔 부분입니다.

---

## 8. 30초 엘리베이터 피치

"LoL 스킬 시스템은 챔프 150 × 스킬 4의 조합 폭발이 본질입니다. UE5 GAS의 통찰 — 스킬은 데이터+작은 trigger, 상태는 tag, 효과는 effect — 을 가져오되, 서버 권위와 결정론이 강제되는 PvP라서 1급 Tag/Effect 객체는 일부러 안 만들고 ECS로 등가 구현했습니다. 모든 시전은 단일 서버 게이트를 통과해 쿨다운·사거리·CC·랭크를 서버에서만 확정하고, champ×variant 함수포인터 테이블로 O(1) 디스패치합니다. CC는 비트 OR-fold, 데이터는 string lookup 없는 dense 데이터팩이고요. 15챔프를 배선했는데 완성도 편차는 README에 솔직히 그어 뒀습니다 — Zed/Yasuo/Viego는 풀, MasterYi는 스텁. 그리고 같은 seed면 bit-identical하다는 걸 FNV 골든 테스트로 매번 증명합니다. 화려한 기능 목록보다, 권위·결정론을 먼저 세우고 측정으로 검증하는 루프를 보여드리고 싶습니다."
