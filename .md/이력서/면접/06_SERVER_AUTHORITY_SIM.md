# 06. 서버권위 시뮬레이션 / GameRoom — 면접 대비 세션

> 도메인 정직성 등급: **working** (정직성 지도 §6)
> 근거 코드: `Server/Private/Game/*`, `Server/Public/Game/*`, `Shared/GameSim/Systems/*`
> 근거 계획: `.md/plan/sim/00_SHAREDGAMESIM_MASTER_PLAN.md`, `.md/build/2026-06-23_GAMEROOM_SERVER_PROJECTILE_AUTHORITY_REPORT.md`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: 클라이언트는 "의도(intent)"만 보내고, **위치·HP·쿨다운·투사체 명중·미니언·타워 같은 게임 상태는 전부 서버의 30Hz 고정-dt 결정론 ECS tick 루프가 단독으로 판정**하는 권위 시뮬레이션 코어. `Client Input → GameCommand → Server GameSim → Snapshot/Event → Client 시각`이 닫힌 루프.

**현재 성숙도(정직하게)**:
- **working(돌아감)**: 30Hz tick 루프, 입력→커맨드 변환, 세션→엔티티 anti-spoof 바인딩, 시뮬 시스템 체인, 투사체/미니언/터렛 권위 판정, 풀 스냅샷 복제, 사망/리스폰. localhost 스모크로 round-trip 동작.
- **미완(정직선)**:
  - **승패/매치 종료 없음** — `eRoomPhase`는 `SeatSelect / Loading / InGame`까지만(`Server/Public/Game/LobbyAuthority.h:15`). 넥서스 파괴·PostGame 전이 코드 0줄. "1판 완결"은 **다음 단계**.
  - **랙 보정 미적용** — 히스토리 기록은 하지만 명령 처리 시 `cmd.rewindTicks = 0`(`Server/Private/Game/GameRoomCommands.cpp:26`). 인프라만 있고 적용 미연결.
  - **클라 예측/reconciliation 없음** — 계획(Sim-5)만, 코드 0. 현재는 권위 스냅샷 직접 적용.

이 세 가지는 면접에서 **먼저 자백**한다. 자백이 메타인지 신호다.

---

## 1. 핵심 개념 (본질)

### 1.1 왜 "서버 권위"인가 — first principles

멀티플레이 PvP의 근본 문제: **두 클라이언트가 같은 세계를 보지만, 누구의 계산을 진실로 믿을 것인가?** 클라가 자기 위치·데미지를 계산해 서버에 "통보"하면, 악성 클라가 "나는 적 옆으로 순간이동했고 데미지 9999를 줬다"고 거짓말할 수 있다. 클라를 신뢰하는 순간 게임은 핵에 무너진다.

해법은 **single source of truth를 서버에 둔다**: 클라는 *결과*가 아니라 *의도*("저 지점으로 이동하고 싶다", "이 대상에게 Q를 쓰고 싶다")만 보내고, 서버가 그 의도가 합법인지 검증한 뒤 *서버가* 위치·데미지를 계산한다. 클라 화면은 서버가 내려준 스냅샷의 시각 표현일 뿐이다. 이게 LoL/오버워치류 경쟁 게임의 공통 골격이다.

### 1.2 왜 "결정론(determinism)"이 필요한가

권위 서버라도, **같은 입력에 같은 결과**가 나오지 않으면 두 가지가 깨진다:
1. **클라 예측·reconciliation 불가** — 클라가 서버와 같은 시뮬 코드를 돌려 미래를 예측하려면, 같은 seed·같은 tick에서 bit 단위로 같은 결과가 나와야 한다(Sim-5의 전제).
2. **리플레이·회귀 테스트 불가** — 입력 로그만 저장해 재생하려면 재생이 원본과 같아야 한다.

결정론을 깨는 3대 비결정성 원천과 내 대응:
- **부동소수 / 시간**: `std::chrono`·`GetTickCount` 추방. 시뮬 안의 시간은 `tickIndex × kFixedDt(1/30)`로만 계산(`DeterministicTime`).
- **난수**: `std::rand`/`mt19937` 금지, seed 주입 xorshift `DeterministicRng`(seed `0xC0FFEE`, `GameRoom.h:203`).
- **컨테이너 순회 순서**: `unordered_map` 순회 결과로 시뮬을 결정하면 플랫폼별로 갈라진다. 그래서 시뮬 결과를 만드는 모든 순회는 `DeterministicEntityIterator<T>::CollectSorted()`로 **EntityID 오름차순 정렬 후** 순회(`GameRoom.cpp:172`).

### 1.3 왜 "고정 dt(fixed timestep)"인가

가변 dt(프레임 시간만큼 시뮬)는 프레임률에 따라 물리·이동 결과가 달라져 결정론을 깬다. 그래서 시뮬은 **항상 1/30초 고정 스텝**으로 전진하고, 실시간 페이싱은 tick 스레드가 `sleep_until`로 맞춘다(`GameRoomTick.cpp:72`, period 33333µs). 렌더 프레임률과 시뮬 tick률을 분리하는 게 핵심.

### 1.4 스냅샷 복제 모델

서버는 매 tick 권위 상태를 **풀 스냅샷**(엔티티별 hp/mana/pos/yaw/쿨다운/버프마스크 등)으로 직렬화해 모든 세션에 전송한다. 클라는 받은 스냅샷을 ECS에 적용해 화면을 그린다. (델타 스냅샷·AOI는 스키마 필드만 있고 항상 full — planned.)

---

## 2. 왜 이 선택인가 — 기술 스택 선택 + Trade-off

### 2.1 권위 모델: 풀 권위 서버 vs 대안

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **풀 서버 권위(택)** | 핵 표면 최소, 진실 단일화 | 입력 지연 체감(예측 없으면), 서버 CPU 부담 | LoL류 경쟁 게임의 정답. 핵 방어가 1순위라 택 |
| 클라 권위 + 검증 | 즉각 반응 | 클라 신뢰 = 핵 천국 | 거부 |
| P2P lockstep | 서버 비용 0 | 한 명만 거짓말해도 전원 붕괴, 치트 검증 불가 | MOBA엔 부적합, 거부 |

### 2.2 시뮬 코어 위치: Shared 공용 코어 vs 서버 전용

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **`Shared/GameSim` 공용 코어(택)** | 서버·클라 예측·AI·SimLab이 *같은 코드* 실행 → 결정론 일관 | 의존성 경계 관리 비용 | 예측/리플레이/봇이 전부 같은 진실을 돌려야 해서 필수 |
| 서버에만 시뮬 작성 | 단순 | 클라 예측 시 시뮬 두 벌 → 미스매치 지옥 | 거부 |

이 결정이 마스터플랜 §0 C-1(Hook 2 분리)의 핵심: 게임플레이(월드 변경)는 `Shared`, 시각(anim/FX)은 `Client`로 갈라 서버가 같은 hook을 돌리게 한다.

### 2.3 issuer 결정: 서버 추론 vs 클라 전송 (anti-spoof 근본)

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **sessionId → controlledEntity 서버 결정(택)** | 클라가 "나는 X다"라고 위조 불가 | lobby authority 결합 | 스푸핑 공격 표면 자체를 제거. 무조건 택 |
| 클라가 issuerEntity 송신 | 구현 간단 | 다른 플레이어 행세 가능 | **즉시 거부**(마스터플랜 C-2) |

그래서 와이어 패킷(`GameCommandWire`)에는 issuerEntity 필드가 **없다**. 서버가 `CSessionBinding::ResolveControlledEntity`로 lobby 권위에서만 도출한다(`SessionBinding.cpp:54`).

### 2.4 결정론 컨테이너: 정렬 순회 vs 해시 순회

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **EntityID 정렬 후 순회(택)** | 플랫폼 독립 결정론 | tick마다 sort 비용 O(n log n) | 결정론이 협상 불가라 비용 수용 |
| `unordered_map` 직접 순회 | 빠름 | bucket 순서 비결정 → 결과 갈라짐 | 시뮬 경로에선 거부 |

**신입 1인 프로젝트 범위에서 왜 합리적인가**: MOBA 한 룸 엔티티 수는 수십~수백 규모라 sort 비용이 병목이 아니다. 반면 결정론을 잃으면 예측·리플레이·SimLab 회귀 테스트가 *전부* 무너진다. "지금 안 느린데 미래 기능 셋을 지킨다" — 옳은 트레이드오프.

---

## 3. 실제 구현 (코드 근거)

### 3.1 30Hz 권위 tick 루프 — `GameRoomTick.cpp`

스레드가 `sleep_until(next += 33333µs)`로 고정 페이싱(`GameRoomTick.cpp:72`). 매 tick:

```
Tick()  (GameRoomTick.cpp:82)
  std::lock_guard(m_stateMutex)          // 네트워크 콜백과 tick 직렬화
  if (!IsInGamePhase()) return;          // InGame 단계에서만 시뮬
  ++m_tickIndex; m_visibleTickIndex.store(...)
  TickContext tc{ tickIndex, kFixedDt, TickToSec(tickIndex), &m_rng, &m_entityMap, ... }
  Phase_DrainCommands(tc)        // 입력 → 커맨드
  Phase_ServerBotAI(tc)          // 봇은 GameCommand만 생산(가짜 플레이어 입력)
  Phase_ExecuteCommands(tc)
  Phase_SimulationSystems(tc)    // 시스템 체인 (아래)
  m_pLagCompensation->RecordHistory(world, tick)  // 기록만
  Phase_BroadcastEvents(tc)
  Phase_BroadcastSnapshot(tc)
```

핵심: **TickContext가 결정론 핸들 묶음**. `m_rng`, `kFixedDt`, `tickIndex` 기반 시간만 들어가고 wall-clock은 안 들어간다(`GameRoomTick.cpp:93-98`).

### 3.2 시뮬 시스템 체인 — `Phase_SimulationSystems` (`GameRoomTick.cpp:120-163`)

고정 순서 DAG로 시스템 실행: StatusEffect → SpellbookFormOverride → AreaAura → Rune → Stat → Buff → SkillCooldown → Recall → WaypointPatrol → CombatAction → Move → JungleAI → AttackChase → (챔피언별 GameSim 16종 Tick) → MinionWave → UnitAI → MinionDepenetration → TurretAI → **Projectiles** → DamageQueue → Stat(재계산) → Death → DeathAndRespawn.

순서가 고정인 게 결정론의 일부(마스터플랜 C-3 "System 실행 DAG"). 데미지는 시스템들이 `DamageRequest`를 큐에 쌓고 `CDamageQueueSystem`이 한 곳에서 처리 → 데미지 공식이 hook에 흩어지지 않음(마스터플랜 §1 원칙).

### 3.3 입력 인제스트 + anti-spoof — `CommandIngress.cpp` / `SessionBinding.cpp`

**수신 경계**(`CommandIngress.cpp:12` `AcceptCommandBatch`):
- 시퀀스 검증 `session.TryAcceptSequence(seq, bSuspicious)` — anti-replay. 의심 시 `FlagSuspicious()`(집계만, kick 미연결 — 정직선).
- 와이어를 `GameCommandWire`로 변환(issuerEntity 없음).
- Move 커맨드는 같은 세션의 기존 Move를 **coalescing**(덮어쓰기, `CommandIngress.cpp:74-85`) → 큐 폭주 방지.

**드레인 + issuer 확정**(`GameRoomCommands.cpp:9` `Phase_DrainCommands`):
- `DrainSorted()`가 `(acceptedTick, sessionId, sequenceNum)` 키로 **stable_sort** → 결정론 처리 순서(`CommandIngress.cpp:98`).
- `ResolveControlledEntity(sessionId, ...)`로 서버가 issuer 결정. 실패면 `continue`(드롭).
- `cmd.issuedAtTick = tc.tickIndex; cmd.rewindTicks = 0;`(`GameRoomCommands.cpp:25-26`) — **여기 `rewindTicks=0`이 랙 보정 미적용의 물증**.

**세션→엔티티 바인딩**(`SessionBinding.cpp:54`): 먼저 캐시(`TryGetAlive`)에서 살아있는 바인딩을 찾고, 없으면 lobby authority의 슬롯에서 `bHuman && slot.sessionId==sessionId && netId`인 슬롯의 `netId→EntityID`만 신뢰. 즉 **제어권은 lobby 권위에서만 도출**되어 클라가 위조 불가.

### 3.4 단일 캐스트 게이트 — `CommandExecutor::HandleCastSkill` (`CommandExecutor.cpp:1932`)

모든 스킬 캐스트가 통과하는 **하나의 검증 깔때기**. 거부는 전부 reason 코드와 함께 로깅:

| 검사 | 코드 | reason |
|---|---|---|
| SkillState 존재 | `:1935` | `no-skill-state` |
| slot < 5 | `:1940` | `invalid-slot` |
| CanCast(상태 차단: 기절/침묵) | `:1945` | `state-blocked` |
| 대상 생존 | `:1950` | `dead-target` |
| 타게팅 가능(은신/무적) | `:1956` | `untargetable` |
| 스킬 학습 여부 | `:1976` | `unlearned` |
| 2단 스테이지 윈도우 | `:2036` | `stage2-window` |
| 쿨다운 | `:2055` | `cooldown` |
| 사거리(거리² vs range²) | `:1163`, `:2068` | (범위 밖 거부) |

`ExecuteCommand`(`:1665`)는 진입 시 issuer 생존을 먼저 막고(`:1668`) kind별 핸들러로 분기. 사거리는 `DistanceSqXZ > range*range`로 sqrt 없이 검사. 쿨다운/사거리/학습을 **서버가 소유**하는 게 안티치트 1차 방어의 실체.

### 3.5 투사체 권위 — `CServerProjectileAuthority` (`ServerProjectileAuthority.h`)

1500줄 비대 GameRoom에서 투사체 판정 책임을 순수 authority 모듈로 분리(2026-06-23 리포트). 책임: 미니언 원거리 투사체 판별, 스킬 투사체 히트 타겟 탐색(`FindSkillProjectileHitTarget`, 세그먼트 충돌), spawn/hit `ReplicatedEvent` DTO 생성, `DamageRequest` DTO 생성. **순수 함수만**(`= delete` 생성자) — Session/Packet/FlatBuffers 의존 0(리포트 rg 검증). GameRoom은 사이드이펙트 실행 셸(enqueue/destroy/net id)로 남김.

### 3.6 사망/리스폰 — `Phase_ServerDeathAndRespawn` (`GameRoom.cpp:167`)

`DeterministicEntityIterator<RespawnComponent>::CollectSorted` 정렬 순회. hp≤0이면 사망 처리·`TargetableTag` 제거·이동 타겟 클리어, `respawnDelaySec`(데이터팩에서) 타이머 시작. 챔피언은 죽고 살아나지만 — **챔피언이 다 죽어도 게임이 안 끝남**(§0 레드플래그).

---

## 4. 검증 — 동작을 어떻게 증명했나

1. **SimLab 결정론 골든 게이트**: 같은 seed + 같은 커맨드 시퀀스로 A/B/C 재현 비교, per-tick 상태 FNV 해시 일치 검사. divergence 시 exit 1. (정직선: SimLab은 navgrid 없는 `FlatWalkable` 평면 미러라 "서버 시뮬 코어 결정론"으로 한정. 실맵 navgrid 경로는 미포함.)
2. **localhost 스모크**: `WintersServer.exe --smoke-seconds=10` 종료 코드 0(2026-06-23 리포트). round-trip(입력→스냅샷→클라 적용) 동작 확인.
3. **빌드/경계 게이트**: authority 모듈 추출 시 `rg`로 네트워크 의존 0 검증(리포트), MSBuild 성공.
4. **ReplayRecorder**: 서버 입력/이벤트 기록 → 재생.
5. **결정론 grep 12종**(마스터플랜 §3.7): `std::chrono`/`std::rand`/시뮬 경로 `unordered_map` = 0 hit 강제.

**무엇으로 "됐다"를 판정했나**: "같은 seed가 같은 해시를 내는가(결정론)" + "스모크가 0으로 끝나는가(크래시 없음)" + "스냅샷이 클라에 적용되는가(round-trip)". **정량 FPS/동접 수치는 없음** — 부하 테스트 안 했으니 "구조상 다중 accept 가능, 실측은 localhost 스모크 수준"이라고 정직하게 말한다.

---

## 5. 최적화

**실제로 한 것**:
- **Move 커맨드 coalescing**(`CommandIngress.cpp:74`): 같은 세션의 중복 Move를 덮어써 큐/처리 비용 절감. 이동 명령은 최신 것만 의미 있으니 합당.
- **사거리 거리² 비교**: `DistanceSqXZ > range*range`로 sqrt 제거(`CommandExecutor.cpp:1163`).
- **stable_sort 한 번 + EntityID 정렬 순회**: 커맨드/엔티티를 한 번 정렬해 결정론과 캐시 친화 순회 동시 확보.
- **investigate-then-act**: 스냅샷은 현재 full. AOI/델타로 가기 전에 **먼저 측정**할 계획(추측 최적화 금지).

**계획 중(수치는 측정 예정)**:
- **델타 스냅샷 + AOI**(50m 그리드, 마스터플랜 §6.1): 풀 스냅샷 대역폭을 가시 엔티티 변경분만 보내 절감.
- **animId string→u16 추방**: 30Hz에 string 비용 제거(마스터플랜 §5.2 Codex P1, 이미 스키마는 u16).
- 정량 목표 수치는 정직성 지도에 없으므로 전부 **"측정 후 budget 설정"**.

---

## 6. 구현 예정 (Planned) — 동일한 깊이

### 6.1 승패 / 매치 종료 (가장 큰 구멍)

**무엇**: 넥서스 HP 0 → 해당 팀 패배 → PostGame 전이 → 결과 backend 전송 → 룸 정리.

**왜**: 지금은 챔피언이 죽고 살아나길 무한 반복할 뿐 "1판"이 없다. 게임이 게임이 되려면 종료 조건이 필수.

**어떻게(설계)**:
1. `eRoomPhase`에 `PostGame` 추가(`LobbyAuthority.h:15`). 넥서스 엔티티에 `NexusComponent{eTeam team}`.
2. 시스템 체인 끝에 `Phase_CheckWinCondition`: 넥서스 HP≤0인 팀 탐색 → `MatchResult{winnerTeam, durationTicks}` 생성.
3. `GameEnd` ReplicatedEvent 브로드캐스트(스키마에 enum 이미 있음, 마스터플랜 §5.3) → 클라 결과 화면.
4. backend(Go)에 `MatchCompleted` Kafka 이벤트로 전적 전파(도메인 14와 연결).
5. tick 루프 `IsInGamePhase()`가 PostGame에서 false → 시뮬 정지, 룸 teardown.

**Trade-off 예상**: 넥서스 파괴 = 비가역. 동시 양 넥서스 0(이론상) 시 tie-break 필요 → tickIndex+팀ID 결정론 우선순위로 해소. 항복/연결 끊김 시 종료도 같은 PostGame 경로 재사용해 분기 최소화.

**검증**: SimLab에 "넥서스 직접 데미지 → N tick 내 PostGame 전이" 골든 추가. exit code로 게이트.

### 6.2 랙 보정 적용 (인프라는 있음)

**무엇**: 히트스캔/타게팅을 *공격자가 본 과거 시점*에서 판정해 핑 높은 플레이어 불이익 제거.

**왜**: 지금 `RecordHistory`로 200ms(`kMaxRewindMs=200`, `LagCompensation.h:11`) 히스토리를 *쌓기만* 하고 명령 처리 시 `rewindTicks=0`(`GameRoomCommands.cpp:26`)으로 **항상 현재 위치** 판정. 핑 100ms면 적이 이미 이동한 위치로 판정돼 손해.

**어떻게**:
1. `Phase_DrainCommands`에서 `clientTimestampMs`와 `recvTimeMs` 차로 rewind 틱 계산(현재 `TraceCommandTiming`이 `observedClampedMs`까지 이미 계산, `CommandIngress.cpp:126`).
2. `cmd.rewindTicks = min(estimatedTicks, kMaxRewindTicks)`로 설정(현재 0 하드코딩 제거).
3. 캐스트/평타 사거리·히트 판정 시 대상 위치를 `LagCompensation`이 `rewindTicks` 전 스냅샷에서 조회.
4. **결정론 유지**: rewind는 읽기 전용 과거 조회만, 권위 상태는 현재 tick 기준으로 적용.

**Trade-off**: "쏜 사람 입장 공정" vs "맞은 사람이 '엄폐했는데 맞았다'" — 200ms cap으로 상한. clamp가 이미 코드에 있음.

**검증**: 인공 지연 주입 → rewind on/off A/B로 "같은 입력에 명중 판정이 바뀌는가" SimLab 비교.

### 6.3 클라 예측 + Reconciliation (Sim-5, 코드 0)

**무엇**: 클라가 입력 즉시 로컬에서 결과를 예측 표시하고, 서버 스냅샷 도착 시 차이를 rollback+replay로 보정.

**왜**: 풀 권위는 입력→화면 반영에 RTT만큼 지연. 예측이 없으면 클릭이 굼떠 보인다.

**어떻게(마스터플랜 §7)**:
1. `ClientInputBuffer`에 미확인 입력 보관(seq별).
2. `OnSnapshot`: NetEntityId→로컬 EntityID 매핑 갱신 → 서버 권위 상태로 World rewind → `m_localRng.SetState(snap.rngState)`로 RNG 동기화 → `lastAckedSeq` 이후 입력 전부 **같은 `Shared/GameSim` 코어로 재실행**(예측 재구성).
3. 이게 §2.2 "공용 시뮬 코어" 결정과 §1.2 결정론이 *없으면 불가능한* 이유. 스냅샷에 `rngState` 필드가 이미 있음(마스터플랜 §5.2).

**Trade-off**: 예측 빗나감(mispredict) 시 visual jitter. 보간으로 2프레임 내 흡수(검증 기준 §7.3). 예측은 *내 캐릭터*만, 타인은 보간만.

**검증**: 100ms RTT 주입 → "예측 즉시 반응 + 200ms 후 권위 확인", mispredict 시 rollback jitter < 2프레임. RNG state 동기 시 클라 예측/서버 권위 bit-equal.

### 6.4 델타 스냅샷 + AOI (대역폭)
풀→가시 엔티티 변경분 전송. 50m 그리드 visibleSet(마스터플랜 §6.1). 스키마 필드는 있고 항상 full인 상태에서 cutover. 측정 후 적용.

---

## 7. 면접 예상 질문 & 모범 답변

**Q1. (기본) 서버 권위 시뮬레이션이 뭐고 왜 쓰나요?**
클라가 위치·데미지 같은 *결과*를 계산해 서버에 통보하면 악성 클라가 거짓말할 수 있어 핵에 무너집니다. 그래서 클라는 이동/캐스트 *의도*만 보내고, 서버가 합법성을 검증한 뒤 *서버가* 상태를 계산합니다. 진실을 서버 한 곳에 두는 거죠. 제 엔진은 IOCP 위에 30Hz 고정-dt ECS tick 루프를 올려 위치·HP·쿨다운·투사체·미니언·타워를 전부 서버에서 판정합니다(`GameRoomTick.cpp:82`).

**Q2. (기본) 왜 고정 dt 30Hz인가요? 렌더 프레임률이랑 어떻게 다른가요?**
가변 dt면 프레임률에 따라 시뮬 결과가 갈라져 결정론이 깨집니다. 시뮬은 항상 1/30초로 전진하고, tick 스레드가 `sleep_until`로 실시간 페이싱만 맞춥니다(`GameRoomTick.cpp:72`). 렌더는 별도 프레임률로 돌며 받은 스냅샷을 보간해 그립니다. tick률과 렌더률을 분리하는 게 핵심입니다.

**Q3. (설계) 결정론을 어떻게 보장하나요?**
비결정성 3원천을 막습니다. 시간은 `std::chrono` 추방하고 `tickIndex×kFixedDt`만 사용(`DeterministicTime`). 난수는 seed 주입 xorshift `DeterministicRng`(`GameRoom.h:203`). 컨테이너는 시뮬 결과를 만드는 순회를 전부 `DeterministicEntityIterator<T>::CollectSorted`로 EntityID 정렬 후 순회(`GameRoom.cpp:172`). 시스템 실행 순서도 고정 DAG입니다. SimLab이 같은 seed에 per-tick FNV 해시 일치를 검사해 회귀 게이트로 강제합니다.

**Q4. (설계) 클라가 "나는 다른 플레이어다"라고 속이면요? anti-spoof는요?**
와이어 패킷에 issuerEntity 필드를 *아예 안 둡니다*(`GameCommandWire`). 서버가 `ResolveControlledEntity(sessionId)`로 lobby 권위에서만 제어 엔티티를 도출합니다(`SessionBinding.cpp:54`). 슬롯이 `bHuman && slot.sessionId==sessionId`일 때만 그 netId를 신뢰하죠. 그래서 클라가 남의 캐릭터로 명령을 위조할 수 없습니다. 거기에 수신 경계에서 시퀀스 anti-replay(`CommandIngress.cpp:31`)도 겁니다.

**Q5. (설계) 스킬 캐스트 검증은 어디서, 어떻게 하나요?**
모든 캐스트가 단일 게이트 `HandleCastSkill`(`CommandExecutor.cpp:1932`)을 통과합니다. SkillState 존재 → slot 범위 → 상태 차단(기절/침묵) → 대상 생존 → 타게팅 가능(은신/무적) → 학습 여부 → 쿨다운 → 사거리(거리² vs range²) 순으로 검사하고 거부는 전부 reason 코드로 로깅합니다. 쿨다운·사거리·학습을 *서버가 소유*하는 게 안티치트 1차 방어의 실체입니다.

**Q6. (adversarial) 게임이 1판 완결되나요? 넥서스 깨면 끝나요?**
**아니요, 거기까진 아직 안 했습니다.** `eRoomPhase`가 `SeatSelect/Loading/InGame`까지만이고 PostGame 전이가 없습니다(`LobbyAuthority.h:15`). 챔피언은 죽고 리스폰하지만 넥서스 파괴 승패 판정 코드는 0줄입니다. 다만 어떻게 채울지는 명확합니다: `NexusComponent` 추가 → 시스템 체인 끝에 `Phase_CheckWinCondition` → `GameEnd` 이벤트(스키마 enum은 이미 있음) → PostGame 전이로 tick 정지. 제가 권위 *루프*를 먼저 닫고 매치 *라이프사이클*은 다음 단계로 둔 건, 결정론·anti-spoof 같은 어려운 코어를 먼저 검증하려는 우선순위 선택이었습니다.

**Q7. (adversarial) 랙 보정 한다고 했는데, 실제로 적용되나요?**
**적용은 아직 안 됩니다 — 정직하게.** 인프라는 있습니다: 200ms 바운드로 매 tick 위치 히스토리를 기록하고(`LagCompensation.h:11`, `RecordHistory`), 커맨드 타이밍에서 `observedClampedMs`까지 계산합니다(`CommandIngress.cpp:126`). 그런데 명령 처리 시 `cmd.rewindTicks = 0`으로 하드코딩돼 있어(`GameRoomCommands.cpp:26`) 실제로는 항상 현재 위치로 판정합니다. 연결만 하면 됩니다 — 이미 계산하는 clamp된 ms를 틱으로 환산해 rewindTicks에 넣고, 히트 판정에서 과거 스냅샷을 조회하면 됩니다. 검증은 인공 지연 주입 후 on/off A/B로 명중 판정이 바뀌는지 SimLab 비교로 할 계획입니다.

**Q8. (adversarial) 클라 예측이랑 reconciliation은 구현됐나요?**
**아니요, 계획(Sim-5)만 있고 코드는 0입니다.** 현재는 권위 스냅샷을 클라가 직접 적용합니다. 다만 예측을 *할 수 있는 기반*은 의도적으로 깔아놨습니다: 시뮬 코어가 `Shared/GameSim` 공용이라 클라가 같은 코드를 돌릴 수 있고, 스냅샷에 `rngState` 필드가 있어 RNG까지 동기화 가능합니다(마스터플랜 §5.2). reconciliation은 `lastAckedSeq` 이후 입력을 그 공용 코어로 재실행하는 rollback+replay로 설계해뒀습니다(§7.2). 결정론을 먼저 박은 게 바로 이 예측을 가능하게 하려던 포석입니다.

**Q9. (심화) 풀 스냅샷이면 대역폭 안 터지나요? AOI/델타는요?**
지금은 항상 풀 스냅샷이라 엔티티 수에 비례합니다. 델타/AOI는 스키마 필드만 있고 미구현입니다 — 정직하게요. 다만 *추측으로* 먼저 안 만들었습니다. 제 원칙은 측정 후 최적화라, AOI 50m 그리드 visibleSet와 델타(`lastAckedCommandSeq` 기반 변경분)는 실제 대역폭을 캡처한 뒤 budget을 정해 들어갈 계획입니다(마스터플랜 §6.1). MOBA 한 룸 규모에선 풀 스냅샷이 당장 병목이 아니라는 판단도 있었습니다.

**Q10. (심화) tick 스레드랑 IOCP 네트워크 스레드가 같은 World를 건드리는데 레이스는요?**
`Tick()` 전체를 `std::lock_guard(m_stateMutex)`로 잡고(`GameRoomTick.cpp:84`), 네트워크 콜백의 커맨드는 즉시 World를 안 만지고 `CCommandIngress`의 락 보호 큐에 쌓습니다(`CommandIngress.cpp:64`). tick이 `DrainSorted`로 한 번에 빼와(`stable_sort`로 결정론 순서) 단일 스레드 컨텍스트에서 처리합니다. 즉 World 변경은 tick 스레드 단독, 네트워크 스레드는 큐 생산자만. `m_visibleTickIndex`만 atomic으로 노출합니다.

**Q11. (심화) 데미지를 왜 시스템마다 안 주고 큐로 모으나요?**
hook(챔프별 코드)이 hp를 직접 깎으면 데미지 공식(방어 관통·치명타·온힛·실드)이 16챔프에 흩어져 일관성·결정론이 깨집니다. 그래서 hook은 `DamageRequest`를 *만들기만* 하고, `CDamageQueueSystem`이 시스템 체인 끝 한 곳에서 처리합니다(`GameRoomTick.cpp:159`). 데미지 직후 Stat을 재계산하고 Death를 판정하는 순서도 고정입니다. "챔프 코드 = 의도, 공식 = 공통 시스템"이 원칙입니다.

**Q12. (adversarial) SimLab 결정론 테스트가 실게임이랑 같나요?**
**완전히 같진 않습니다.** SimLab은 navgrid 없는 `FlatWalkable` 평면에서 도는 GameSim-only 미러라, "서버 시뮬 *코어*의 결정론"을 검증합니다. 실맵 navgrid 기반 경로탐색까지 포함한 end-to-end 결정론은 아닙니다. 그래서 저는 "서버 시뮬 코어 결정론"으로만 주장하고, 거기에 더해 localhost 스모크(`--smoke-seconds=10` exit 0)로 실 네트워크 round-trip을 따로 확인합니다.

---

## 8. 30초 엘리베이터 피치

"제 서버권위 시뮬은 클라가 *결과*가 아니라 *의도*만 보내게 해서 핵 표면 자체를 없앤 30Hz 결정론 ECS tick 루프입니다. 시간·난수·컨테이너 순회의 비결정성을 다 막아 같은 seed면 같은 해시가 나오게 박았고, 그걸 SimLab 골든 게이트로 회귀 검증합니다. issuer는 세션→엔티티로 서버가 정해 스푸핑을 막고, 모든 스킬 캐스트가 쿨다운·사거리·학습을 검사하는 단일 게이트를 통과합니다. 솔직히 넥서스 승패 종료랑 클라 예측은 아직 계획 단계인데 — 결정론과 anti-spoof 같은 *어려운 코어*를 먼저 검증하려는 우선순위였고, 그 결정론을 박아둔 게 바로 예측·reconciliation을 가능하게 하려던 포석입니다. 저는 기능 목록보다 '진실을 한 곳에 두고 측정으로 검증하는 루프'를 보여드리고 싶습니다."
