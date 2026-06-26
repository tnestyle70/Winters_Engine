# 15. 보안 / 안티치트 — 면접 대비 세션

> 도메인 성숙도: **working** (서버권위 1차방어는 동작, 탐지형 안티치트는 설계만)
> 정직성 경계: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` §15
> 코드 근거: `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`, `Server/Private/Network/Session.cpp`, `Server/Private/Game/CommandIngress.cpp`, `Server/Private/Game/SessionBinding.cpp`, `Server/Public/Security/LagCompensation.h`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: "클라이언트를 신뢰하지 않는다"를 시스템 구조로 강제하는 것. 클라가 *결과(위치/데미지/쿨다운 소진)* 가 아니라 *의도(이동/캐스트 명령)* 만 보내게 만들고, 권위 서버가 그 의도를 매 명령 검증해 받아들이거나 거부한다.

**현재 상태(정직하게)**:
- **working**: 서버권위 명령 모델, 캐스트/평타 cooldown·range·target 검증, 세션-엔티티 바인딩, 입력 경계 anti-replay 시퀀스 윈도우 + flatbuffers 구조 검증, 200ms 바운드 랙 보상 히스토리 버퍼.
- **planned(코드 0줄)**: 탐지형 안티치트(유저모드 메모리 무결성, 디버거 탐지, 커널 드라이버, Vanguard 스타일)는 5단계 설계 문서만 존재.
- **핵심 정직성 선**: 이건 *탐지형(detection-based) 안티치트가 아니라 권위형(authority-based) 1차방어*다. "스피드핵을 탐지/차단했다"가 아니라 "클라가 위치를 주장할 수 없게 만들어 스피드핵의 공격 표면 자체를 제거했다"가 정확한 표현이다.

---

## 1. 핵심 개념 (본질)

### 1.1 왜 "서버 권위"가 1차 방어인가 — first principles

멀티플레이 치트의 근본 원인은 단 하나다: **신뢰 경계(trust boundary)가 잘못 그어져 있다.** 클라이언트는 공격자가 100% 통제하는 머신이다. 메모리, 실행 흐름, 네트워크 패킷 전부 조작 가능하다. 따라서 "클라가 계산한 결과를 서버가 그대로 받아 적용"하는 순간 게임은 끝난다 — 스피드핵은 "나 이 위치로 이동했다"를 빠르게 보내는 것이고, 데미지핵은 "내가 9999 데미지 줬다"를 보내는 것이다.

해법의 1차 원리: **권위(authority)를 서버로 옮긴다.** 클라가 보낼 수 있는 건 "의도(intent)"뿐이다 — "이 지점으로 가고 싶다", "이 슬롯 스킬을 이 타겟에게 쓰고 싶다". 결과(실제 위치, 실제 데미지, 쿨다운 소진 여부)는 **서버만** 계산한다. 그러면 클라가 거짓말할 표면이 사라진다. 스피드핵을 "막을" 필요가 없다. 애초에 클라가 위치를 주장할 수 없으니까.

### 1.2 권위 모델의 3대 구성요소

1. **Intent-only 입력**: 와이어 명령(`GameCommandWire`)에는 `kind`(Move/Cast/Attack…), `slot`, `targetNet`, `groundPos`, `direction`, `sequenceNum`만 담긴다. *실제 위치 결과 필드가 없다.* 이동은 서버가 그리드/navmesh 위에서 직접 계산한다.
2. **서버 소유 상태(server-owned state)**: 쿨다운, HP, 위치는 서버 컴포넌트가 진실이다. 클라는 예측(prediction)만 하고 서버 스냅샷으로 교정된다.
3. **명령 단위 검증(per-command validation)**: 모든 명령은 실행 전에 "이 엔티티가 이 명령을 지금 낼 자격이 있는가"를 검사받는다 — 살아있나, 타겟이 유효한가, 사거리 안인가, 쿨다운이 돌았나, 스킬을 배웠나.

### 1.3 결정론(determinism)이 안티치트의 토대인 이유

검증이 의미를 가지려면 **서버의 시뮬레이션이 권위적이고 재현 가능**해야 한다. Winters의 GameSim은 고정 dt(`DeterministicTime::kFixedDt`), 결정론적 RNG(`DeterministicRng`), 정렬된 엔티티 순회(`DeterministicEntityIterator::CollectSorted`)로 틱 단위 재현성을 보장한다. 이게 있어야 (a) 클라 예측과 서버 권위가 같은 규칙으로 수렴하고, (b) 랙 보상에서 과거 틱을 되감아 같은 결과를 재현할 수 있다.

### 1.4 입력 경계 보안 — anti-replay와 구조 검증

네트워크 경계는 별도의 공격면이다. 두 가지 근본 위협:
- **리플레이(replay)**: 공격자가 유효했던 패킷을 가로채 재전송 → 명령 중복 실행. 방어: **단조 증가 시퀀스 번호 + 수용 윈도우**. 이미 처리한 시퀀스 이하는 거부, 너무 앞선 시퀀스(점프)는 의심 플래그.
- **악성 페이로드(malformed buffer)**: 조작된 바이트로 파서를 터뜨리거나 OOB read 유발. 방어: 역직렬화 *전에* **flatbuffers Verifier로 구조 무결성 검증**. flatbuffers는 zero-copy라서 검증 없이 접근하면 신뢰 못 할 오프셋을 그대로 따라가게 된다.

### 1.5 랙 보상(lag compensation) / rewind — 왜 필요하고 왜 위험한가

플레이어는 자기 화면 기준으로 조준한다. 하지만 그 입력이 서버에 도착할 땐 RTT/2만큼 과거다. "내 화면엔 맞았는데 서버에선 빗나감"을 줄이려면 서버가 **명령 시점의 과거 월드 상태로 되감아(rewind)** 히트를 판정해야 한다. Winters는 엔티티별 위치/HP 히스토리를 **200ms로 바운드된 ring(deque)** 으로 보관한다(`CLagCompensation`). 바운드가 안티치트적으로 중요한 이유: rewind 한도를 못 박지 않으면 공격자가 큰 클라 타임스탬프를 보내 "임의 과거"로 되감게 만들 수 있다 — 한도가 곧 신뢰 경계다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 + Trade-off

### 2.1 권위형 vs 탐지형 안티치트

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **권위형(서버 검증)** | 치트 *종류*에 무관하게 전체 클래스를 무력화(스피드/텔레포트/쿨다운/데미지핵). OS 무관, 커널 권한 불필요, 오탐 0 | 월핵/맵핵 같은 *정보 노출*형은 FOW(전송 필터)까지 가야 막힘. 서버 CPU 비용 | **1순위 채택.** 신입 1인 범위에서 ROI 최강 |
| **탐지형(유저/커널)** | 메모리 조작·인젝션·하드웨어 핵까지 커버 | 커널 드라이버 = 안정성/BSOD/서명/오탐 지옥. 군비경쟁. 1인이 운영 불가 | **설계만(Phase 1~5 문서), 코드 0줄** |

**근본 트레이드오프**: 권위형은 "치트를 *탐지*하는 게 아니라 치트가 *작동할 수 없게* 만든다". 탐지형은 끝없는 군비경쟁이고 커널 드라이버는 BSOD/서명/유지보수 비용이 1인 프로젝트에서 비현실적이다. 그래서 권위형을 끝까지 밀고, 탐지형은 "이 분야를 이해한다"는 증거로 설계 문서만 남겼다.

### 2.2 검증 로직을 CommandExecutor에 흡수 vs 별도 Validator 클래스

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **CommandExecutor 내 인라인 검증** | 검증과 적용이 한 곳 → 검증 통과 후 상태가 변할 틈(TOCTOU)이 없음. 결정론 시뮬과 동일 경로 | "안티치트 모듈"이라는 독립 클래스가 안 보임 | **채택.** 권위 루프 안에서 검증이 곧 게임 규칙 |
| **별도 `CServerValidator`/`CSpeedChecker`** (Phase0 문서) | 모듈 경계 명확, 단위 테스트 쉬움 | 검증→적용 사이 상태 분리 위험, 중복 상태 | **파일 미생성.** 로직은 CommandExecutor에 흡수 |

**정직성 경계 (중요)**: `.md/plan/security/PLAN_SECURITY/Phase0_ServerAuthority.md`에 `CServerValidator`, `CSpeedChecker`, `CRangeChecker`, `CCooldownVerifier`, `CFOWManager` 클래스가 코드까지 적혀 있지만 **이 파일들은 실존하지 않는다.** 면접에서 "이 클래스 구현했냐"고 물으면 "그건 초기 설계안이고, 실제론 그 로직을 권위 루프인 CommandExecutor에 흡수했다 — 검증과 적용이 같은 경로여야 TOCTOU가 없기 때문"이라고 답해야 한다. "구현했다"고 하면 즉시 실패다.

### 2.3 TCP 권위 복제 vs UDP 넷코드

TCP 신뢰 채널 위에서 권위 복제를 구현했다(IOCP). UDP+델타+AOI는 헤더만 있고 .cpp가 없다. 안티치트 관점에선 **순서 보장 + 신뢰 전달**이 anti-replay 시퀀스 윈도우를 단순하게 만든다(재전송/순서 뒤바뀜을 TCP가 흡수). UDP로 가면 시퀀스 윈도우가 비트맵 기반으로 더 복잡해지는데, 그건 다음 단계로 미뤘다.

---

## 3. 실제 구현 (코드 근거)

### 3.1 Intent-only 와이어 명령

`ICommandExecutor.h:77-101` — `GameCommandWire`(클라→서버)와 `GameCommand`(서버 내부). 와이어에는 `kind`, `clientTick`, `sequenceNum`, `slot`, `targetNet`(NetEntityId), `groundPos`, `direction`, `itemId`만 있다. **실제 위치 결과 필드가 없다** — 클라가 "어디로 이동했다"를 주장할 통로 자체가 없다. `eCommandKind`(:62-75)는 Move/CastSkill/BasicAttack/LevelSkill/BuyItem/Flash 등 *의도*만 표현한다.

### 3.2 세션-엔티티 바인딩 (스푸핑 차단)

`SessionBinding.cpp:54-106` — `ResolveControlledEntity(sessionId, world, entityMap, pLobbyAuthority)`. 제어권은 **lobby authority의 슬롯에서만** 도출된다. 슬롯이 `bHuman && slot.sessionId == sessionId && netId != NULL`일 때만 그 netId→EntityID로 바인딩한다(:67-78). 즉 클라가 "나는 5번 엔티티를 조종한다"고 주장해도, 서버는 로비 권위가 그 세션에 배정한 슬롯에서만 제어 엔티티를 끌어온다. **다른 플레이어 엔티티에 명령을 꽂는 스푸핑이 구조적으로 불가능**하다. `BuildServerCommand`(`CommandExecutor.cpp:2757`)는 `controlledEntity`를 *서버가 결정한* issuerEntity로 박는다 — 와이어에 issuer 필드가 없다.

### 3.3 입력 경계 — flatbuffers 구조 검증 + anti-replay

**구조 검증** (`PacketDispatcher.cpp:73-79`): 역직렬화 전에
```cpp
flatbuffers::Verifier verifier(frame.payload.data(), frame.payload.size());
if (!Shared::Schema::VerifyCommandBatchBuffer(verifier)) {
    pSession->FlagSuspicious();  // 구조 깨진 버퍼 = 의심
    return;
}
```
검증 통과 후에만 `GetCommandBatch`로 접근(:81). 알 수 없는 패킷 타입도 `FlagSuspicious`(:64-65).

**anti-replay 시퀀스 윈도우** (`Session.cpp:24-40`):
```cpp
bool CSession::TryAcceptSequence(u32_t seq, bool& bSuspicious) {
    std::lock_guard lk(m_seqMutex);
    if (seq <= m_lastProcessedSeq) return false;           // 리플레이/중복 → 조용히 폐기
    if (seq > m_lastProcessedSeq + 60) { bSuspicious = true; return false; } // 비정상 점프 → 의심
    m_lastProcessedSeq = seq;
    return true;
}
```
`CommandIngress.cpp:29-36`이 배치 내 모든 명령에 대해 이를 호출하고, 거부되면 의심 플래그를 올린 뒤 그 명령을 건너뛴다. **Move 명령은 큐에서 coalesce**(`CommandIngress.cpp:74-85`) — 세션당 최신 Move 하나만 유지해 입력 스팸 표면을 줄인다.

### 3.4 명령 단위 검증 — Cast / BasicAttack / Flash

**CastSkill** (`CommandExecutor.cpp:1932-2059`)은 거부 사유 코드와 함께 순차 검증한다:
- `no-skill-state`(:1937), `invalid-slot`(slot>=5, :1942), `state-blocked`(`CanCast`, :1947), `dead-target`(:1953), `untargetable`(`CanBeTargetedBy`, :1959), `unlearned`(`IsSkillLearned` — 안 배운 스킬 캐스트 차단, :1978), `cooldown`(서버 소유 `slot.cooldownRemaining > 0`, :2055-2058).
- 사거리는 **거리제곱 vs (range + 양쪽 히트박스 반지름)²** 로 검사(`DistanceSqXZ > effectiveRange²`, 예: Annie Q :2081). sqrt 회피 + gameplay radius 보정.
- 쿨다운은 서버가 소유·세팅(`slot.cooldownRemaining = cooldown`, :2106). 클라는 쿨다운 소진을 *주장*할 수 없다.

**BasicAttack** (`:2296-2381`): `state-blocked`/`invalid-target`/`target-has-no-health`/`dead-target`/`untargetable`/`same-team`(아군 공격 차단, :2336)/`cooldown`(:2345)/`missing-transform` 검사 후, `DistanceSqXZ > rangeSq`면 공격 대신 추격(`StartAttackChase`)으로 전환(:2377). 모든 거부는 `LogBasicAttackReject(reason, …)`로 사유 로깅(:1489).

**Flash** (`:2608-2698`): `CanMove` 확인 → 실제 Flash 보유 확인(소환사 주문 슬롯 스캔, :2629-2644) → 서버 쿨다운 확인(:2651) → **서버가 range로 클램프**(`useLen = min(len, range)`, :2666) → walkable 위로 재해결(`TryResolveMoveTarget`, :2674). 클라가 "더 멀리 점멸"을 주장해도 서버가 사거리로 자른다.

### 3.5 랙 보상 히스토리

`LagCompensation.h:8-31` + `LagCompensation.cpp`:
- `kMaxRewindMs=200`, `kTickRate=30` → `kMaxRewindTicks = ceil(200*30/1000) = 6틱`(:11-13). **rewind 상한을 컴파일타임 상수로 못 박음** — 임의 과거 되감기 차단.
- `RecordHistory`(:8): 매 틱 살아있는 TransformComponent 엔티티를 정렬 순회(`DeterministicEntityIterator::CollectSorted`, :20)해 위치/HP/사망 상태를 `deque`에 push, `tickIndex > front + kMaxRewindTicks`면 pop_front로 200ms ring 유지(:40-41). **generation 불일치 시 히스토리 clear**(:36-37) — 엔티티 ID 재사용(handle 재활용)으로 인한 오염 방지.
- `TryGetHistoricalState`(:45): `rewindTicks > kMaxRewindTicks`면 거부(:48), 아니면 deque를 뒤에서 순회해 `tickIndex <= targetTick`인 가장 최근 프레임 반환.
- 클라 타임스탬프→rewind 변환은 `CommandIngress.cpp:120-127`에서 `min(|clockDelta|, kMaxRewindMs)`로 클램프 후 추적 로깅.

### 3.6 의심 집계 (정직성 경계)

`Session.h:35` — `void FlagSuspicious() { ++m_suspicionCount; }`. **집계 카운터일 뿐, kick/ban으로 연결돼 있지 않다.** Verifier 실패·시퀀스 점프·알 수 없는 패킷에서 호출되지만, 임계치 도달 시 세션을 끊거나 밴 큐에 넣는 로직은 아직 없다. 면접에서 과장 금지 — "현재는 신호 수집까지, 정책 집행(kick/ban wave)은 다음 단계"가 정확하다.

---

## 4. 검증 — 동작을 어떻게 증명했나

1. **거부 사유 로깅(reason code trace)**: 모든 검증 분기가 사유 문자열과 함께 `OutputDebugStringA` 계열로 로깅된다 — `LogCastSkill("reject", "cooldown", …)`(:2057), `LogBasicAttackReject("same-team", …)`(:2338). 인게임에서 치트 입력(사거리 밖 캐스트, 쿨다운 무시, 아군 공격)을 흉내 내면 해당 사유 코드가 정확히 찍히는지로 "검증이 실제 발화함"을 확인했다. CLAUDE.md의 "디버그 UI/바운드 OutputDebugString으로 권위 경로를 관찰" 원칙과 일치.
2. **SSLab 결정론 골든 테스트(인접 도메인 §16)**: 헤드리스 5v5 시뮬을 same-seed로 돌려 FNV 해시가 일치하는지로 *서버 시뮬 코어 결정론*을 회귀 게이트화. 권위 검증이 결정론을 깨지 않는다는 토대가 된다.
3. **랙 보상 타이밍 트레이스**: `CommandIngress.cpp:111-142`가 `clockDeltaMs`/`observedClampedMs`/`acceptedTick`/`execTick`을 찍어 클라-서버 클럭 오프셋이 200ms 한도로 클램프되는지 관찰.

**정직성**: 안티치트 *전용* 자동화 단위 테스트 스위트(Phase0 문서의 체크리스트 같은)는 아직 없다. "거부 사유 로깅 + 인게임 수동 재현 + 결정론 골든 테스트"가 현재의 검증 수단이다.

---

## 5. 최적화

**실제로 한 것**:
- **거리 검사 sqrt 회피**: 모든 사거리 비교를 `DistanceSqXZ vs range²`로(예 `CommandExecutor.cpp:2376-2377`). 매 명령마다 sqrt 한 번씩 아끼는 게 아니라 *비교를 제곱 공간에서* 해서 sqrt를 아예 제거.
- **Move 명령 coalesce**: 세션당 펜딩 Move를 최신 하나로 덮어씀(`CommandIngress.cpp:74-85`) — 이동 입력 스팸으로 인한 명령 큐 폭주(=DoS 표면)를 줄임.
- **랙 보상 ring 버퍼**: deque를 200ms로 바운드해 메모리/순회 비용 상한 고정.

**계획 중(측정 예정)**: 검증 핫패스의 컴포넌트 조회(`HasComponent`/`GetComponent` 반복)를 줄이는 것. Phase0 문서 원칙 "안티치트가 프레임당 1ms를 넘지 않는다"를 충족하는지 **아직 측정 안 함 — 측정 예정**. 정량 수치(예: 검증당 ns)는 현재 없다.

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

설계 문서: `.md/plan/security/00_SECURITY_INDEX.md`(Level 0~5 로드맵), `PART5_WINTERS_IMPLEMENTATION.md`, `PLAN_SECURITY/Phase0_ServerAuthority.md`. **이 섹션은 실제로 구현할 것이므로 동일 수준으로 설명한다.**

### 6.1 FOW(Fog of War) 전송 필터 — 월핵/맵핵 근본 차단

- **무엇/왜**: 권위 검증은 *행동*형 치트를 막지만 *정보 노출*형(월핵: 시야 밖 적 위치를 봄)은 못 막는다. 근본 원인은 "서버가 시야 밖 엔티티 정보를 클라에 전송"하는 것. **안 보내면 핵이 불가능하다.**
- **어떻게**: `CFOWManager`(Phase0 문서 :409-565 설계) — 엔티티별 위치/팀/시야범위 보관, `GetVisibleEntities(observerID)`가 같은 팀 + 적 중 아군 시야 내인 것만 반환. 스냅샷 빌더(`SnapshotBuilder`)가 이 필터를 거쳐 *플레이어별로* 가시 엔티티만 직렬화. 그리드 셀(200u) 기반으로 O(관측자×후보) → 셀 인접만 검사로 줄임.
- **Trade-off**: 플레이어별 스냅샷 = 브로드캐스트 단일 버퍼 재사용 불가, 서버 CPU/대역폭↑. 시야 경계 깜빡임(visibility flicker) 처리 필요. → MOBA는 시야가 핵심 메타라 이 비용은 정당.
- **검증**: 같은 팀=항상 가시, 적 팀 시야 내=가시, 적 팀 시야 밖=**패킷 미전송**을 단위 테스트(Phase0 :710-713 체크리스트). 패킷 캡처로 시야 밖 엔티티 좌표가 와이어에 안 실리는지 확인.

### 6.2 의심 집계 → 정책 집행 (kick / ban wave)

- **무엇/왜**: 현재 `FlagSuspicious`는 카운터만 증가. 신호는 모으지만 행동이 없다.
- **어떻게**: 임계치(`ESeverity` Low/Medium/High/Critical, Phase0 :50-56) 기반 정책 — Low=로그, High=즉시 kick, Critical=밴 큐. 즉시 차단보다 **데이터 수집 후 밴 웨이브**(00_INDEX 핵심원칙 :57)로 오탐 최소화 + 치터에게 탐지 시점 은폐. `ViolationRecord`(:67-77)를 외부 분석 파이프(Kafka detection-events 토픽, Part5 :18)로 흘려보냄.
- **Trade-off**: 즉시 kick = 즉효지만 오탐 시 정상 유저 차단(최악 시나리오). 밴 웨이브 = 치터가 당분간 활동하지만 누적 증거로 오탐↓. → 오탐 최소화 우선.
- **검증**: 위반 주입 시 severity별 분기(로그/kick/밴큐)가 맞게 타는지, 정상 플레이 100판에서 오탐 0인지.

### 6.3 유저모드 탐지 (Phase 1~2)

- **무엇/왜**: 권위형이 못 보는 클라 내부 조작(메모리 핵, DLL 인젝션, API 후킹).
- **어떻게**: 코드 섹션 해시 무결성 검증, `IsDebuggerPresent`/`NtQueryInformationProcess` 디버거 탐지, IAT/EAT 변조 탐지, 서명 안 된 모듈 탐지(00_INDEX Level 1~2). Heartbeat에 무결성 토큰 실어 서버 보고.
- **Trade-off**: 군비경쟁(우회 가능), 오탐(정상 오버레이/접근성 도구), 성능. → "추가 신호"로만 쓰고 권위 검증을 1차로 유지.
- **검증**: 알려진 패턴(Cheat Engine 어태치, 테스트 DLL 인젝션)에 탐지 발화, 정상 환경 오탐 0.

### 6.4 커널 안티치트 (Phase 3~5) — "이해는 하지만 안 만든다"

- **무엇/왜**: 유저모드보다 높은 권한의 핵(커널 드라이버 치트). 방어: `ObRegisterCallbacks`(프로세스 핸들 보호), `PsSetCreateProcessNotifyRoutine`, 미니필터, 하이퍼바이저(EPT)(00_INDEX Level 3~5, Part5 아키텍처).
- **정직한 입장**: **이건 설계 이해를 보여주는 문서일 뿐, 1인 프로젝트에서 만들 게 아니다.** 커널 드라이버는 BSOD/WHQL 서명/유지보수/오탐 비용이 팀+QA+법무를 요구한다. 면접에서 "왜 안 만들었나"엔 "범위와 ROI 판단 — 커널 안티치트는 1인이 책임질 수 있는 영역이 아니고, 권위형으로 막을 수 있는 치트 클래스를 먼저 100% 닫는 게 옳다"가 답.

---

## 7. 면접 예상 질문 & 모범 답변

**Q1 (기본). 안티치트를 만들었다는데 어떤 방식인가?**
A. 탐지형이 아니라 권위형 1차방어입니다. 핵심은 클라이언트가 *결과*가 아니라 *의도*만 보내게 명령 모델을 재설계한 겁니다. 와이어 명령(`GameCommandWire`)에는 위치 결과 필드가 아예 없고 "이동/캐스트 의도"만 담깁니다. 서버가 그 의도를 매 명령 검증(생존·타겟·사거리·쿨다운·학습 여부)하고 실제 결과는 서버만 계산합니다. 그래서 스피드핵·텔레포트·쿨다운핵의 공격 표면 자체가 사라집니다.

**Q2 (기본). 사거리 검증은 어떻게 하나?**
A. 거리제곱 비교입니다. `DistanceSqXZ(issuer, target) > effectiveRange²` (`CommandExecutor.cpp:2377`). sqrt를 피하려고 제곱 공간에서 비교하고, `effectiveRange`는 스킬 range에 양쪽 gameplay radius(히트박스 반지름)를 더해 보정합니다. 사거리 밖이면 거부가 아니라 자연스럽게 "추격 후 재시도"(StartAttackChase)로 전환합니다.

**Q3 (설계). 쿨다운을 클라가 아니라 서버가 소유한다는 게 무슨 의미인가?**
A. 쿨다운 잔여 시간(`slot.cooldownRemaining`)이 서버 컴포넌트에 있고, 캐스트가 수락될 때 서버가 직접 세팅합니다(:2106). 클라는 "내 쿨다운 다 돌았다"를 *주장*할 통로가 없습니다. 캐스트 명령이 와도 서버가 `cooldownRemaining > 0`이면 사유 `cooldown`으로 거부합니다(:2055). 클라 쿨다운 UI는 순수 예측일 뿐 권위가 아닙니다.

**Q4 (설계). 한 플레이어가 다른 플레이어 캐릭터에 명령을 보내는 건 어떻게 막나?**
A. 세션-엔티티 바인딩입니다. 와이어 명령에 "내가 누구를 조종한다"는 필드가 없습니다. 서버가 `ResolveControlledEntity(sessionId, …)`로 **lobby authority 슬롯에서만** 제어 엔티티를 도출합니다(`SessionBinding.cpp:54`). 슬롯의 sessionId가 일치하고 사람이 점유한 슬롯일 때만 바인딩되므로, 다른 엔티티에 명령을 꽂는 스푸핑이 구조적으로 불가능합니다.

**Q5 (설계). 리플레이 공격과 악성 패킷은?**
A. 두 경계입니다. (1) anti-replay: 단조 증가 시퀀스 윈도우(`Session.cpp:24`). 이미 처리한 시퀀스 이하는 폐기, +60 넘게 점프하면 의심 플래그. (2) 구조 검증: 역직렬화 *전에* flatbuffers Verifier(`PacketDispatcher.cpp:74`)로 버퍼 무결성을 확인합니다. flatbuffers는 zero-copy라 검증 없이 접근하면 조작된 오프셋을 그대로 따라가 OOB read가 나거든요.

**Q6 (심화). 랙 보상의 rewind를 200ms로 못 박은 이유는?**
A. 두 목적입니다. 플레이어는 자기 화면(과거) 기준으로 조준하니 서버가 명령 시점으로 되감아 히트를 판정해야 공정합니다. 그런데 rewind 한도를 안 정하면 공격자가 큰 클라 타임스탬프를 보내 임의 과거로 되감게 만들 수 있습니다. 그래서 `kMaxRewindTicks`(200ms=6틱)를 컴파일타임 상수로 박고(`LagCompensation.h:13`), 클라 타임스탬프도 `min(delta, 200ms)`로 클램프합니다. 한도가 곧 신뢰 경계입니다.

**Q7 (adversarial — 레드플래그). 솔직히 이거 "안티치트"라고 부르긴 좀 그런 거 아닌가요? 치트를 탐지하는 게 하나도 없잖아요.**
A. 정확한 지적이고, 저도 그 선을 분명히 긋습니다. 이건 *탐지형(detection-based)* 안티치트가 아니라 *권위형(authority-based)* 1차방어입니다. "스피드핵을 탐지해서 막았다"가 아니라 "클라가 위치를 주장할 수 없게 만들어 스피드핵이 *작동할 수 없게* 했다"가 정확한 표현입니다. 탐지형(유저모드 무결성·디버거 탐지·커널)은 5단계 설계 문서로 정리만 했고 코드는 0줄입니다. 제가 권위형을 먼저 끝까지 민 건, 치트의 근본 원인이 잘못된 신뢰 경계이고 그걸 고치면 치트 *클래스 전체*가 OS/군비경쟁과 무관하게 무력화되기 때문입니다.

**Q8 (adversarial — 레드플래그). 계획서엔 `CSpeedChecker`, `CServerValidator` 클래스가 코드까지 다 있던데, 이거 구현하신 거죠?**
A. 아닙니다 — 그건 Phase0 *설계안*이고 실제 그 파일들은 만들지 않았습니다. 처음엔 검증을 독립 Validator 클래스로 빼려 했는데, 검증과 적용이 분리되면 그 사이에 상태가 변하는 TOCTOU 위험이 있어서 로직을 권위 루프인 CommandExecutor 안에 흡수했습니다. 그래서 검증이 통과한 직후 같은 경로에서 상태가 바뀝니다. 즉 "별도 안티치트 모듈"이라는 클래스는 없지만, 그 책임은 명령 실행 핫패스 안에 사유 코드와 함께 살아 있습니다.

**Q9 (adversarial — 레드플래그). 의심 플래그를 올린다고 했는데, 그래서 치터를 밴하나요?**
A. 아직 안 합니다 — `FlagSuspicious`는 카운터 증가까지입니다(`Session.h:35`). Verifier 실패·시퀀스 점프·미지 패킷에서 신호를 모으지만, 임계치 도달 시 kick하거나 밴 웨이브에 넣는 정책 집행은 다음 단계입니다. 다만 이건 의도된 순서입니다 — 즉시 차단은 오탐 시 정상 유저를 끊는 최악의 시나리오를 만들기 때문에, 먼저 신호를 충분히 수집하는 인프라를 깔고 그 위에 severity 기반 정책과 밴 웨이브를 얹는 게 설계 원칙입니다.

**Q10 (adversarial). 월핵/맵핵은 이 구조로 못 막죠?**
A. 맞습니다, 현재 구조로는 못 막습니다. 권위 검증은 *행동*형(스피드/쿨다운/데미지)을 막지 정보 노출형은 못 막습니다. 근본 해법은 FOW 전송 필터입니다 — 시야 밖 엔티티를 *애초에 클라에 안 보내면* 월핵이 볼 게 없습니다. `CFOWManager` 설계가 있고(플레이어별 스냅샷에서 가시 엔티티만 직렬화), 이건 SnapshotBuilder에 필터를 끼우는 형태로 구현 예정입니다. 비용은 브로드캐스트 단일 버퍼를 못 쓰고 플레이어별 스냅샷을 만들어야 하는 건데, MOBA는 시야가 핵심 메타라 정당한 비용입니다.

**Q11 (심화). 검증이 결정론 시뮬레이션과 같은 경로라는 게 왜 중요한가?**
A. 검증이 의미를 가지려면 서버 시뮬이 권위적이고 재현 가능해야 합니다. GameSim은 고정 dt, 결정론 RNG, 정렬 순회로 틱 재현성을 보장하고, 이걸 헤드리스 5v5 골든 테스트(same-seed FNV 해시 일치)로 회귀 게이트화합니다. 덕분에 (1) 클라 예측과 서버 권위가 같은 규칙으로 수렴하고, (2) 랙 보상에서 과거 틱을 되감아도 같은 결과가 재현됩니다. 검증 로직을 시뮬 밖 별도 모듈에 두면 이 재현성이 깨집니다.

**Q12 (확장). 지금 TCP인데 UDP로 가면 anti-replay는 어떻게 바뀌나?**
A. 지금은 TCP가 순서·신뢰 전달을 보장해서 단일 `lastProcessedSeq + 윈도우`로 충분합니다. UDP로 가면 순서 뒤바뀜·손실이 생기니 단순 단조 증가로는 정상 패킷을 떨굽니다. 슬라이딩 윈도우 + 비트마스크(최근 N개 시퀀스 수신 여부를 비트로 추적)로 바꿔서, 윈도우 안에서 미수신 비트만 수락하고 재수신은 거부하는 형태가 됩니다. 이건 UDP 넷코드 자체가 아직 설계 단계라 같이 묶여 있습니다.

---

## 8. 30초 엘리베이터 피치

"제 안티치트의 핵심은 *치트를 탐지하는 게 아니라 치트가 작동할 수 없게 만드는* 겁니다. 치트의 근본 원인은 클라이언트를 신뢰한다는 잘못된 신뢰 경계라서, 클라가 위치 같은 *결과*가 아니라 이동·캐스트 *의도*만 보내게 명령 모델을 다시 짰습니다. 서버가 매 명령마다 생존·타겟·사거리·쿨다운·학습 여부를 검증하고 결과는 서버만 계산하니까, 스피드핵·텔레포트·쿨다운핵이 OS나 군비경쟁과 무관하게 통째로 무력화됩니다. 입력 경계엔 anti-replay 시퀀스 윈도우와 flatbuffers 구조 검증을, 히트 판정엔 200ms로 바운드된 랙 보상을 깔았습니다. 유저모드·커널 탐지형은 *제가 그 분야를 이해한다는 증거로 설계만* 남겼고 코드는 안 짰는데, 1인 프로젝트에서 커널 드라이버를 책임지는 것보다 권위형으로 막을 수 있는 치트 클래스를 먼저 100% 닫는 게 옳은 우선순위라고 판단했기 때문입니다."
