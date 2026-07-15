# 10. Server·네트워크(Netcode) — 박사 연구 심화

> **코드 상태 동기화 (2026-07-11)**: 현재 구현은 IOCP TCP server-authority이며 lobby/Command/Snapshot/Event가 모두 TCP를 탄다. UDP는 header/fragment/reliability/client API 선언만 있고 송수신 구현·호출자, delta/AOI runtime은 없다. 아래 TCP-control/UDP-gameplay 및 AOI·reliability 서술은 연구/목표 architecture로 읽고, 구현 사실과 최신 전체-session UDP + Server Fiber 설계는 [2026-07-11 통합 감사](../plan/2026-07-11_FULL_UDP_AND_SERVER_FIBER_INTEGRATION_AUDIT.md)를 우선한다.

> 전제 문서: [`00_PHD_Paper_Guide.md`](00_PHD_Paper_Guide.md). 본 문서는 가이드 §1(구현 vs 기여), §3(thesis statement), §4(구조), §7(평가: 지연 ms, 대역폭 KB/tick, CCU 확장성, 예측 정확도, 부정 저항성)을 **그대로 전제**한다. 모든 세부주제마다 "이건 구현 항목인가, 기여 후보인가?"를 가이드 §1로 되돌아가 묻는다.
>
> 독자: LoL 스타일 MOBA + 오픈월드 엔진 'Winters'를 만든 숙련 C++ 개발자. 현재 **TCP(제어면, BanPick/Backend) + UDP(데이터면, InGame gameplay)** 분리 서버 권위(server-authoritative) 아키텍처를 구현 중이다. 서버는 fixed-tick·stable-sort·`/fp:precise`로 **결정론 기반**을 깔고, lag compensation·delta snapshot·AOI를 단계적으로 얹는다.
>
> Top venue: **SIGCOMM, NSDI, NetGames, MMSys** (저널 IEEE/ACM ToN). 산업 참고(비학술): **GDC Networking**.

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue)

네트워크 게임 코드(netcode)는 가이드 §1의 함정이 **가장 가혹하게** 작동하는 분야다. "나는 client prediction과 server reconciliation, lag compensation, delta snapshot, AOI를 다 구현했다"는 문장은 **산업 결과물**이지 박사 기여가 아니다. 이 네 가지는 전부 **2001년 이전에 산업에서 작동하던 기법**이다 — Bernier(Valve, 2001), Bettner & Terrano(Ensemble, 2001), Carmack의 Quake3 `.plan`(1996~1998). 코드로 작동시키는 것은 엔지니어링이다.

박사가 살아나는 지점은 가이드 §5의 기여 유형 중 **4(새 이론/모델: 공정성·예측오차의 정량 모델), 5(새 경험적 발견: 상용 netcode 측정으로 드러난 법칙), 6(새 평가 방법·벤치마크)** 이며, 이 분야에서는 **5와 6이 특히 비어 있다.** 이것이 본 문서를 관통하는 전략이다.

> **이 분야를 박사로 만드는 결정적 사실 — 학술 출판의 공백:**
> netcode는 산업이 학계를 **20년 이상 앞서는** 희귀한 분야다. client-side prediction의 1차 출처는 동료심사 논문이 아니라 Valve 위키 문서와 GDC 발표이고, rollback netcode의 정전(canon)은 Tony Cannon의 EVO 포럼 글과 GGPO 코드다. 이는 박사 기회의 **공백(gap)**을 뜻한다(가이드 §2-3 측정·이해 논문). 산업은 "동작하는 휴리스틱"을 가졌지만, **(a) 왜 그 파라미터인가, (b) 공정성·반응성의 정량 한계는 어디인가, (c) CCU·RTT 분포에 따라 어떻게 일반화되는가**에 대한 검증 가능한 모델이 거의 없다. 측정 인프라를 갖춘 엔진(Winters의 `OutputDebugString` + inspectable overlay 문화, 가이드 §7)은 바로 이 공백을 메우는 testbed다.

### 0.1 구현 vs 기여 — 5개 세부주제 대조

| 세부주제 | 구현 (석사/산업) | 연구 기여 (박사) |
|---|---|---|
| Client Prediction & Reconciliation (§1) | "입력을 예측 적용하고 server snapshot이 오면 rewind & replay로 맞춘다" | "예측 오차의 **지각적(perceptual) 비용**을 모델링해, 비결정 물리에서도 misprediction-induced visual pop을 **최소화하는 reconciliation 정책**이 naive snap 대비 사용자 인지 오류율을 Z% 낮춤을 user study로 증명" |
| Lag Compensation (§2) | "서버가 200ms history를 들고 발사 시점으로 rewind해 hit 판정한다" | "rewind 기반 보상이 유발하는 **'엄폐 뒤 피격(shot-behind-cover)' 불공정성**을 가변 RTT 분포에서 **정량 모델**로 표현하고, 공정성-반응성 Pareto front 상에서 기존 고정-window 대비 우월한 적응형 window를 제시" |
| Deterministic Lockstep (§3) | "입력만 전송하고 모두 같은 sim을 돌린 뒤 checksum으로 desync를 잡는다" | "**cross-platform 부동소수점 결정론**을 보장하면서 lockstep sim을 **멀티코어 병렬**로 돌리는 실행 모델(→04 JobSystem)이 단일스레드 대비 X배 scaling을 결정론 손실 0으로 달성함을 증명" |
| Server Authority (§4) | "클라는 입력만 보내고 모든 판정은 서버가 한다" | "권위 시뮬레이션의 **반응성-권위 trade-off**를 정량화하고, CCU N에서 권위 비용을 **sub-linear**로 유지하는 입력 검증·재시뮬 분할 아키텍처가 기존 full re-sim 대비 CPU Y% 절감을 보임" |
| AOI / Delta Compression (§5) | "grid로 관심 영역을 자르고 baseline 대비 delta만 보낸다" | "CCU 확장에서 **대역폭-정확도(state staleness) Pareto 최적**을 푸는 relevance·priority 할당이, fixed-radius grid 대비 동일 대역폭에서 인지 가능한 state error를 W% 낮춤 + 그 relevance가 08 World Partition 가시성과 **동형(isomorphic)**임을 형식화" |

### 0.2 Top Venue 표 (네트워킹/멀티미디어 시스템)

| 구분 | Venue | 성격 | 이 분야 대표 채택 주제 |
|---|---|---|---|
| 네트워크 시스템·측정 | **SIGCOMM** | 프로토콜·측정·트래픽 모델 | latency/loss 측정, congestion, 트래픽 특성화 |
| 네트워크 시스템 설계 | **NSDI** | 분산 시스템·실측 설계 | 분산 게임 서버, edge, replication |
| 게임 네트워크 전문 | **NetGames** (ACM/IEEE) | netcode·MMO·지연 영향 | lag compensation, interest management, cheat |
| 멀티미디어 시스템 | **MMSys / NOSSDAV** | 실시간 미디어·게임 스트리밍 | cloud gaming, latency QoE, scalability |
| 저널 | **IEEE/ACM ToN** | 확장판/모델·증명 | 트래픽 모델, 큐잉, 공정성 분석 |
| 산업(비학술, 인용용) | **GDC Networking** | 사례·영향력 근거 | Bernier(2001), Aldridge(Halo/GDC 2011), Glenn Fiedler |

> **주의(가이드 §9):** Bernier(Valve), Bettner & Terrano(GDC 2001), Carmack(.plan), Cannon(GGPO), Fiedler(Gaffer On Games)는 **권위 있으나 동료심사 학술 출판이 아니다.** 박사 기여의 1차 무대는 NetGames/MMSys/ToN이고, 이들 산업 출처는 "이 문제가 실재하고 산업 표준이 이렇다"는 motivation·baseline 근거로 인용한다. 산업 표준 사례와 학술 인용을 본문에서 명시적으로 구분한다.

### 0.3 Heilmeier 체크 (이 분야 적용)

가이드 §6의 Heilmeier Catechism을 박으면 좋은 질문이 걸러진다:
- **무엇을?** "높은·가변적 RTT에서도 공정하고 반응성 있게 느껴지는 서버 권위 MOBA netcode."
- **지금 한계?** 산업 휴리스틱(고정 200ms rewind, 고정 grid AOI)은 동작하지만 **왜 그 값인지, 어떤 RTT 분포에서 무너지는지** 검증된 모델이 없다.
- **새로움?** 공정성/예측오차/대역폭을 **측정 가능한 목적함수**로 놓고, 적응형 정책을 그 위에서 최적화 + 한계를 증명.
- **누가 신경?** MOBA/FPS/격투 게임 서버, e스포츠 공정성, cloud gaming, 안티치트(→11).
- **어떻게 측정?** 지연(ms), 대역폭(KB/tick), CCU 대비 scaling 곡선, 예측 정확도(reconciliation correction 크기·빈도), 공정성 지표(피격자 RTT 대비 사망 편향), 부정 저항성.

---

## 1. 클라이언트 예측과 서버 조정 (Client-Side Prediction & Reconciliation)

### 1.1 핵심 원리

**문제:** 서버가 권위(authority)를 가지면, 클라가 입력을 보내고 → 서버가 처리하고 → 결과 snapshot이 돌아올 때까지 **최소 1 RTT**가 걸린다. 100ms RTT면 키를 눌러도 100ms 뒤에 캐릭터가 움직인다. 이는 조작감을 파괴한다.

**예측(prediction)의 답:** 클라는 서버 응답을 기다리지 않고, **같은 시뮬레이션 코드**로 입력을 **즉시 로컬 적용**한다(키 누름 → 즉시 이동). 동시에 그 입력을 서버로 보낸다. 클라는 "서버가 내 입력을 받으면 이렇게 될 것"을 **선반영**한다.

**조정(reconciliation)의 답:** 서버는 권위적 결과를 snapshot으로 돌려준다. 이 snapshot은 **과거의 입력까지만 반영**된 상태다(network delay만큼 과거). 클라는 그동안 더 많은 입력을 예측 적용했다. 따라서:

```text
1. 클라는 모든 미확인(unacked) 입력을 sequence 번호와 함께 버퍼에 보관한다.
2. 서버 snapshot이 "input seq=N까지 반영" 정보를 달고 도착한다.
3. 클라는 자신의 상태를 server snapshot 상태로 되감는다(rewind).
4. seq > N인 버퍼의 입력들을 그 위에 다시 적용한다(replay).
5. 결과가 현재 예측 상태와 같으면 → 예측 성공, 시각적 변화 없음.
   다르면 → server가 진실, 차이만큼 보정(correction). 이때 pop/rubber-banding이 보인다.
```

이 **rewind & replay** 가 client prediction의 심장이다. 핵심 통찰: **예측이 맞으면 latency가 0처럼 느껴지고, 틀려야만 보정 비용(시각적 불연속)을 낸다.** 따라서 연구의 본질은 "예측을 더 자주 맞히고, 틀렸을 때 덜 아프게" 다.

**결정론 의존성:** rewind & replay가 작동하려면 클라의 예측 sim과 서버의 권위 sim이 **같은 입력 → 같은 결과**여야 한다(최소한 예측 대상 subset에서). 비결정적 sim이면 "예측이 맞았는지" 판정 자체가 불가능하다. 이것이 §3 결정론과 §1을 묶는 지점이고, Winters가 prediction을 sim-only component subset으로 제한하는 이유다.

### 1.2 대표 기존 연구/사례

- **Bernier, Y. W. (2001), "Latency Compensating Methods in Client/Server In-game Protocol Design and Optimization"** (GDC; Valve). **이 분야의 산업 정전(canon).** prediction + lag compensation의 결합을 Half-Life/Counter-Strike 맥락에서 정식화. 동료심사 아님 → motivation·baseline으로 인용.
- **Gabriel Gambetta, "Fast-Paced Multiplayer" 시리즈** (웹 튜토리얼). client-side prediction → server reconciliation → entity interpolation → lag compensation을 4부로 교육적으로 정리. 산업/교육 자료, 비학술이나 용어 표준화에 기여.
- **Quake3 / Source 모델** (Carmack `.plan` 1998, Valve Source SDK 문서). snapshot + prediction + delta의 산업 원형.
- **Aldridge, D. (2011), "I Shot You First: Networking the Gameplay of Halo: Reach"** (GDC). 서버 권위·예측·통계적 hit 판정의 산업 사례. 비학술.
- **학술 인접:** **Mauve, M. et al. (2004), "Local-Lag and Timewarp: Providing Consistency for Replicated Continuous Applications"** (IEEE Trans. Multimedia). **prediction/reconciliation의 드문 정식 학술 모델** — local lag(입력을 일부러 지연)과 timewarp(되감기 재실행)의 일관성-반응성 trade-off를 형식화. 본 절에서 가장 인용 가치 높은 동료심사 논문.
- **Savery & Graham (2013), "Timelines: Simplifying the Programming of Lag Compensation for the Next Generation of Networked Games"** (Multimedia Systems). 보상 기법의 추상화·프로그래밍 모델.

> **open problem (가이드 §5-4,5):** (a) **예측 오차의 지각적 최소화** — 어떤 correction이 사람 눈에 덜 띄는가는 거의 측정되지 않았다. (b) **비결정 물리에서의 조정** — rigid body·연쇄 충돌처럼 비결정/카오스적 sim에서 rewind & replay는 발산한다. 부분 예측·관성 보정·서버 hint의 trade-off가 미개척.

### 1.3 알고리즘/프로토콜 (의사코드)

**클라이언트 예측 루프 (sequence 기반 reconciliation):**

```text
// 클라 상태: pendingInputs[] (seq별 입력 보관), localState (예측된 현재 상태)
// 서버 snapshot: { authoritativeState, lastProcessedInputSeq }

on every input frame:
    input = sample_input()
    input.seq = nextSeq++
    send_to_server(input)                 // UDP, reliable-ordered 채널
    pendingInputs.push(input)
    localState = simulate(localState, input)   // 즉시 예측 적용 (지연 0 체감)
    render(localState)

on server snapshot arrives (auth, lastProcessedInputSeq):
    localState = auth                     // (1) 권위 상태로 rewind
    pendingInputs.drop_until(lastProcessedInputSeq)   // 서버가 이미 본 입력 폐기
    for inp in pendingInputs:             // (2) 미확인 입력만 replay
        localState = simulate(localState, inp)
    // 이제 localState = "서버 권위 + 내가 그 뒤 친 입력" = 최신 예측
    // auth와 직전 예측의 차이가 misprediction → 여기서 보정 pop이 발생
```

**서버 권위 처리 (Winters GameRoom 흐름과 정합):**

```text
on command batch arrives (sessionId, inputs[]):
    for inp in inputs:
        if validate(inp, sessionId):       // cooldown/range/mana/dead-state 검증 (§4)
            enqueue(sessionId, inp, acceptedTick = currentTick)

on fixed tick:
    cmds = drain_all_pending()
    stable_sort(cmds, key=(acceptedTick, sessionId, sequenceNum))  // 결정론 (§3)
    for c in cmds: apply_to_world(c)
    step_simulation(dt_fixed)
    for each session s:
        snap = build_snapshot(s, visible_set(s))   // AOI (§5)
        snap.lastProcessedInputSeq[s] = last_acked_seq(s)
        send(s, snap)                               // unreliable-sequenced 채널
```

**Reconciliation의 핵심 불변식:** `simulate`는 클라·서버가 **비트 동일**해야 하며(또는 예측 subset에서 동일), `pendingInputs`는 서버 ack로만 비워진다 — 즉 입력은 **ack 받을 때까지 책임이 클라에 남는다.**

### 1.4 박사급 novel 각도 (open problems)

1. **지각 기반 reconciliation (perceptual reconciliation).** 모든 misprediction을 똑같이 snap으로 갚지 않는다. correction의 **크기·방향·맥락**(시야 내/외, 정지/이동, 적 근처 여부)에 따라 **언제 즉시 snap, 언제 smooth blend** 할지를 결정하는 정책을 학습/최적화하고, 그것이 인지 오류율을 줄임을 **user study(가이드 §7, IRB)** 로 증명. 기여 유형 4+5.
2. **비결정 물리에서의 부분 예측(partial prediction under non-determinism).** 캐릭터 locomotion은 예측, 연쇄 물리(넉백 충돌)는 서버 hint로만 보간 — 어떤 상태를 예측 대상에 넣고 뺄지의 **자동 분할**과 발산 한계 분석. (→03 Physics, →04 결정론)
3. **예측 신뢰도(prediction confidence) 모델.** 입력·맥락에서 "이 예측이 빗나갈 확률"을 추정해, 고위험 구간만 보수적으로(예측 약화·local-lag 추가) 처리. local-lag(Mauve 2004)의 적응형 일반화.
4. **reconciliation 비용의 정량 모델.** correction 빈도·크기를 RTT 분포·tick rate·입력 패턴의 함수로 모델링 → "어떤 조건에서 prediction이 이득/손해"의 **닫힌 형태 경계**. 기여 유형 4.

### 1.5 Thesis statement 예시

> "Server reconciliation의 보정(correction)을 일률적 snap이 아니라 **오차의 지각적 비용 모델**로 스케줄링하면, 가변 RTT(50~250ms) 하의 서버 권위 MOBA에서 **사용자가 인지하는 위치 불연속(visual pop)을 naive snap 대비 Z% 줄이면서** 권위 정합성(server 상태와의 최종 수렴)을 100% 유지한다."

### 1.6 평가 방법

- **예측 정확도:** misprediction rate(snapshot당 보정 발생 비율), correction 크기 분포(위치 Δ, 각도 Δ), replay 깊이(미확인 입력 수).
- **반응성:** 입력→화면 반영 지연(예측 on/off 대비), motion-to-photon.
- **지각 품질:** user study — 동일 시나리오에서 정책별 "끊김·순간이동을 느꼈는가" 응답률, 2AFC 선호도. (가이드 §7 perceptual, IRB)
- **Baseline:** (하한) 예측 없음(full RTT 지연), (중간) naive snap reconciliation(Bernier 식), (SOTA) Mauve timewarp/local-lag.
- **부하 조건:** RTT {50,100,150,200,250}ms × loss {0,2,5}% × jitter, 여러 시드. 통계적 유의성(가이드 §7).

### 1.7 Winters 연결점

- **예측 입력 버퍼가 이미 존재:** [`Client/Private/Network/Client/ClientInputBuffer.cpp`](../../Client/Private/Network/Client/ClientInputBuffer.cpp) — `Push`/`DropAcked(ackedSeq)`/`ForEachAfter(ackedSeq, fn)`가 **정확히 §1.3의 pendingInputs 링버퍼**다. `DropAcked`는 서버가 ack한 seq 이하를 폐기, `ForEachAfter`는 미확인 입력 replay 순회 — reconciliation의 (1)(2)가 코드에 반쯤 들어와 있다. **여기에 rewind & replay sim 호출을 붙이는 것이 §1 구현, 보정 정책을 최적화하는 것이 §1.4 기여.**
- **권위 sim 단일 경로:** [`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`](../../Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp) — client/server가 **공유**하는 command 실행기. 클라 예측과 서버 권위가 같은 코드를 부르므로 reconciliation의 "같은 입력 → 같은 결과" 전제를 만족시킬 자리.
- **로드맵상 M5가 prediction:** [`02_UDP_GAMEPLAY_TRANSPORT_MIGRATION.md`](../TODO/05-07/02_UDP_GAMEPLAY_TRANSPORT_MIGRATION.md) §10 — "M5: client prediction, local command buffer, reconciliation, interpolation buffer, **sim-only component subset**". sim-only subset 제한이 §1.4-(2) 부분 예측의 출발점.
- **권위 규칙 명문화:** [`CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md`](../TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md) §1 — "최종 위치/데미지/쿨타임/stage/hit/death는 GameSim/server 소유, 클라는 입력 수집·snapshot 보간·cue 재생만". 이는 reconciliation에서 **무엇을 예측하고 무엇을 서버에 맡길지**의 경계를 이미 그어 둔 것 → §1.4-(2)의 분할 정책 testbed.

---

## 2. 지연 보상 (Lag Compensation)

### 2.1 핵심 원리

**문제:** 클라는 **과거**를 본다. 서버 snapshot이 도착하는 데 ½RTT가 걸리고, entity interpolation을 위해 클라는 추가로 보간 버퍼(보통 100ms)만큼 더 과거를 렌더한다. 그래서 화면에서 적을 정확히 조준해 쏴도, 그 클릭이 서버에 도착할 때(½RTT 뒤) 적은 이미 다른 곳에 있다. 보상이 없으면 **고핑 플레이어는 절대 못 맞힌다.**

**되감기(rewind)의 답:** 서버는 모든 hit 가능 entity의 **최근 위치 history**(시간별 스냅샷)를 보관한다. 클라의 발사 명령이 도착하면, 서버는 그 명령에 실린 **클라가 본 시점**(또는 `serverTime − ½RTT − interpBuffer`)으로 entity 위치를 **되감아** 그 과거 상태에서 hit 판정을 한다. 즉 **"쏜 사람이 본 세계가 맞다"** 를 권위로 인정한다.

```text
실제 시간선:
  t0: 슈터 화면에 적이 (x=10)에 보임  ← 슈터는 여기를 쏨
  t0+½RTT: 명령이 서버 도착. 이때 적의 현재 위치는 (x=14)
  서버: rewind window에서 t0 시점 적 위치(x=10)를 복원 → 그 위치로 hit 판정
```

**핵심 trade-off — 공정성의 이전:** 이 기법은 슈터에게 공정하지만 **피격자에게 불공정**할 수 있다. 피격자가 엄폐물 뒤로 숨은 **후에도**, 서버가 과거로 되감으면 "아직 노출돼 있던 과거"에서 맞을 수 있다 → **"엄폐 뒤 피격(shot behind cover / shot around corner)"**. lag compensation은 불공정을 없애는 게 아니라 **슈터↔피격자 사이에서 이전**한다. 누가 핑 페널티를 지느냐의 정책 문제다.

### 2.2 대표 기존 연구/사례

- **Bernier (2001, GDC, Valve)** — §1.2와 동일. lag compensation을 "서버가 명령에 실린 클라 시각으로 entity를 backtrack"으로 정식화. **산업 표준의 원전.** rewind window 상한(약 1초)·"엄폐 뒤 피격" 부작용을 명시. 비학술.
- **Lee & Chang / NetGames 계열의 fairness 연구** — 보상이 RTT 차이에 따라 승률에 미치는 편향을 측정. 동료심사.
- **Claypool, M. & Claypool, K. (2006~), latency-and-player-performance 시리즈** — 장르별로 latency가 명중/완수에 미치는 영향을 정량 측정(예: FPS는 precision-deadline 모델). 공정성 모델의 학술 토대.
- **산업 사례:** Halo: Reach(Aldridge 2011), Overwatch(Ford, GDC 2017 "favor the shooter" 토론) — "shooter favor vs victim favor"를 명시적 설계 선택으로 다룸. 비학술.

> **open problem (가이드 §5-4,5):** (a) **공정성의 정량 모델 부재** — "엄폐 뒤 피격"이 RTT 분포에 따라 얼마나 자주, 누구에게 발생하는지의 닫힌 모델이 없다. (b) **가변 RTT에서의 편향** — 고정 rewind window는 저핑·고핑을 다르게 대우한다. 적응형 보상의 공정성-반응성 곡선이 미개척.

### 2.3 알고리즘/프로토콜 (의사코드)

**서버 측 rewind 히트 판정 (Winters `CLagCompensation`과 정합, 30Hz·200ms window):**

```text
// 서버: entity별 위치/콜라이더 history를 tick 단위 circular buffer로 보관
struct HistoryFrame { tick; generation; LagCompensatedEntityState state; }
history: map<EntityID, deque<HistoryFrame>>   // 최근 kMaxRewindTicks 만큼만 유지

on every server tick(tickIndex):
    for each hittable e:
        history[e].push_back({tickIndex, e.generation, snapshot_collider(e)})
        history[e].pop_old(tickIndex - kMaxRewindTicks)   // 200ms = 6 tick @30Hz

on fire command(shooter, ray, clientRenderTime):
    // 1. 슈터가 본 시점 계산 (명령에 timestamp 또는 ack된 snapshot tick 동봉)
    rewindTicks = clamp(currentTick - tick_of(clientRenderTime), 0, kMaxRewindTicks)
    // 2. 모든 타깃을 그 과거 위치로 복원해 판정
    for each candidate target t:
        if history[t].TryGetHistoricalState(rewindTicks, &past):
            if ray_hits(ray, past.collider):
                apply_damage(shooter, t)    // 권위적 데미지는 현재 tick에 적용
    // 3. window 밖이면 보상 없음 → 현재 위치로만 판정 (저항성: 무한 rewind 금지)
```

**공정성 가드(부정·악용 저항, →11):**

```text
// clientRenderTime을 클라가 보냄 → 조작 가능 → 검증 필수
validate(clientRenderTime):
    expected = serverTime - estimatedHalfRTT(shooter) - maxInterpBuffer
    if |clientRenderTime - expected| > tolerance: reject or clamp  // rewind 악용 차단
    if rewindTicks > kMaxRewindTicks: clamp to window              // 무한 과거 금지
```

### 2.4 박사급 novel 각도 (open problems)

1. **공정성의 정량 모델 + 측정.** "shot-behind-cover"를 **확률 사건**으로 모델링: P(피격자가 이미 엄폐했는데 피격) = f(슈터 RTT, 피격자 RTT, rewind window, 엄폐 진입 타이밍). 이를 시뮬·실측으로 검증하고 **공정성 지표**(RTT 대비 사망 편향, Gini-like 불평등)를 정의. 기여 유형 5+6 — 이 분야가 가장 비어 있는 곳.
2. **적응형 rewind window.** 고정 200ms 대신 RTT·jitter·게임 맥락(원거리 저격 vs 근접 난전)에 따라 window를 조절해 **공정성-반응성 Pareto front**를 개선. 고정 window가 그 front의 한 점일 뿐임을 보이고 우월한 정책을 제시.
3. **양방향 공정성(victim-aware compensation).** 피격자의 "방금 엄폐" 신호를 보상 결정에 반영(엄폐 후 grace window). shooter-favor↔victim-favor를 연속 파라미터로 보고 최적점을 user study로 탐색.
4. **결정론적 rewind와 재현.** rewind 판정을 replay·anti-cheat 검증에서 **비트 동일 재현** 가능하게 만드는 history 직렬화·복원(→§3, →05-09 Replay). e스포츠 분쟁 판정에 직결.

### 2.5 Thesis statement 예시

> "Lag compensation의 rewind window를 슈터·피격자 RTT 분포에 적응시키면, '엄폐 뒤 피격' 불공정 사건 발생률을 **고정 200ms window 대비 Z% 낮추면서** 슈터 체감 명중률 저하를 W% 이내로 억제하는, 공정성-반응성 Pareto 상의 우월점을 달성한다."

### 2.6 평가 방법

- **공정성:** shot-behind-cover 발생률(엄폐 진입 후 피격 / 전체 피격), RTT 분위수별 kill/death 편향, "favor the shooter" 강도.
- **반응성/정확도:** 슈터 명중률(보상 on/off·window별), 판정과 클라 시각의 정합(클라가 본 hit ↔ 서버 hit 일치율).
- **저항성(부정):** 조작된 clientRenderTime·과대 rewind 요청에 대한 reject율, 악용 시 얻는 이득 상한.
- **Baseline:** (하한) 보상 없음, (산업 표준) 고정 200ms rewind(Bernier), (SOTA) 적응형 제안.
- **조건:** RTT 분포(대칭/비대칭 매치업: 저핑 vs 고핑), 엄폐 타이밍 분포, tick rate. 통계·신뢰구간.

### 2.7 Winters 연결점

- **실제 lag compensation 구현 존재:** [`Server/Public/Security/LagCompensation.h`](../../Server/Public/Security/LagCompensation.h) / [`Server/Private/Security/LagCompensation.cpp`](../../Server/Private/Security/LagCompensation.cpp) — `CLagCompensation`이 `kMaxRewindMs = 200`, `kTickRate = 30`, `kMaxRewindTicks` 상수와 entity별 `std::deque<HistoryFrame>` circular history, `TryGetHistoricalState(entity, rewindTicks, out)`를 이미 갖췄다. **§2.3 의사코드가 거의 그대로 코드로 존재.** 200ms/30Hz는 **고정 파라미터** — 이것이 §2.4-(2) "왜 200ms인가, 적응형이 더 나은가"를 묻는 박사 질문의 직접 baseline이다.
- **보안 폴더에 위치 = 권위·저항성과 결합:** `LagCompensation`이 `Server/.../Security/`에 있고 `ICommandExecutor`/`ILagCompensationQuery` 인터페이스로 검증 경로에 물린다 → clientRenderTime 조작 검증(§2.3 가드, →11 Security)이 자연스럽게 같은 모듈.
- **결정론 history → replay 재현:** `HistoryFrame`에 `tickIndex`/`generation`이 있어 rewind를 tick 기준으로 재현 가능 → [`05-09/Replay/`](../TODO/05-09/Replay/00_REPLAY_INDEX.md)의 서버 권위 replay와 결합하면 §2.4-(4) "분쟁 판정용 비트 동일 rewind 재현"의 testbed.
- **공정성 측정 인프라:** CLAUDE.md의 inspectable overlay + bounded trace 문화로 "어느 tick에 어떤 rewind로 누가 맞았는가"를 로깅하면 §2.4-(1) 공정성 정량 모델의 데이터 수집대가 된다.

---

## 3. 결정론적 락스텝 (Deterministic Lockstep)

### 3.1 핵심 원리

**문제:** RTS는 수천 유닛을 동기화해야 한다. 모든 유닛의 위치/체력을 매 틱 전송하면 대역폭이 폭발한다.

**락스텝의 답:** **상태를 보내지 않고 입력(명령)만 보낸다.** 모든 클라가 **완전히 동일한 결정론적 시뮬레이션**을 돌린다. 같은 초기 상태 + 같은 입력 시퀀스 → 같은 결과. 1500 유닛이든 1유닛이든 전송량은 **플레이어 명령 수**에만 비례한다(유닛 수와 무관).

**전제 — 비트 단위 결정론(bit-exact determinism):** 모든 머신이 같은 결과를 내려면 sim이 **완벽히 결정론적**이어야 한다. 단 1비트라도 갈리면 즉시 **desync**(상태 분기)가 누적돼 게임이 갈라진다. 결정론을 깨는 주범:
- **부동소수점:** 같은 IEEE-754라도 컴파일러·CPU·FMA·x87 80bit·SSE·`-ffast-math`·초월함수(sin/sqrt) 구현 차이로 결과가 갈린다. → cross-platform이 가장 어렵다.
- **비결정적 순회:** `unordered_map` 순회 순서, 포인터 주소 정렬.
- **비동기·멀티스레드:** 실행 순서 의존(→04 JobSystem과의 긴장).
- **RNG:** 시드·소비 순서 불일치.

**입력 지연(input delay)의 답:** 명령을 즉시 실행하지 않고 **N틱 뒤**에 실행하기로 약속한다(예: 현재 100틱이면 명령은 102틱에 실행). 그 사이 네트워크로 명령이 모두 도착할 시간을 번다. 모든 클라가 "이번 틱에 실행할 모든 명령"을 받기 전엔 틱을 진행하지 않는다(가장 느린 클라가 전체를 묶음 — lockstep의 비용).

**desync 탐지:** 매 틱(또는 주기적으로) sim 상태의 **checksum**을 교환한다. 불일치하면 desync — 어느 틱에서 누가 갈렸는지 추적해 디버그.

### 3.2 대표 기존 연구/사례

- **Bettner, P. & Terrano, M. (2001), "1500 Archers on a 28.8: Network Programming in Age of Empires and Beyond"** (GDC). **결정론적 락스텝의 산업 정전.** 입력만 전송, 입력 지연(turn 기반), out-of-sync 탐지, 28.8k 모뎀 제약 하 수천 유닛 동기화를 정식화. 비학술이나 이 분야 필수 인용.
- **롤백 넷코드 — Cannon, T. (GGPO, ~2006)** (격투 게임). lockstep의 입력 지연 대신 **예측 + 롤백**: 상대 입력을 예측해 즉시 진행하다가, 실제 입력이 도착하면 그 시점으로 **롤백 후 재시뮬**(§1의 rewind & replay를 P2P 결정론 sim에 적용). 격투 게임의 사실상 표준. GGPO·"Fightin' Words"(Cannon) — 비학술이나 정전. **§1·§3을 잇는 핵심 사례.**
- **GGPO 계열 산업화:** Skullgirls, Street Fighter 6의 rollback. 비학술.
- **학술 인접:** **deterministic execution / record-replay**(→04 참고문헌: DMP·Kendo·CoreDet, ASPLOS) — 멀티스레드 결정론을 OS/런타임 차원에서 보장하는 시스템 연구. lockstep의 "결정론적 sim" 요구와 직접 연결. **Terano & Bettner가 못 푼 멀티코어 결정론을 학술이 푸는 다리.**

> **open problem (가이드 §5-3,4):** (a) **cross-platform 부동소수점 결정론** — soft-float·fixed-point·엄격 FP 제약의 성능-이식성 trade-off가 미해결. (b) **멀티코어 병렬 결정론**(→04 JobSystem) — work-stealing의 비결정적 스케줄과 lockstep의 비트 결정론을 동시 달성. (c) **가변 지연** — 가장 느린 peer가 전체를 묶는 lockstep을 입력 지연 적응·rollback 혼합으로 완화.

### 3.3 알고리즘/프로토콜 (의사코드)

**락스텝 turn 루프 (입력 지연 + checksum):**

```text
// 모든 peer가 동일: turn 기반, INPUT_DELAY 틱 뒤 실행
INPUT_DELAY = 2
on every fixed tick T:
    myCmd = sample_input()
    broadcast(Command{ executeTick = T + INPUT_DELAY, peerId, myCmd, seq })

    if not all_commands_received_for(T):    // 모든 peer의 T용 명령 도착?
        STALL                               // lockstep: 못 받으면 진행 불가
        return

    cmds = gather_all(T)
    deterministic_sort(cmds, key=(executeTick, peerId, seq))  // 순서 고정
    for c in cmds: apply(c)
    step_simulation_fixed(dt)               // 비트 결정론 sim

    if T % CHECKSUM_INTERVAL == 0:
        broadcast(Checksum{ tick=T, hash=hash_world_state() })
        // 다른 peer hash와 불일치 → DESYNC 보고 + 분기 tick 로깅
```

**롤백 변형 (GGPO식 — §1 reconciliation의 P2P 결정론판):**

```text
on every frame:
    localInput = sample()
    predictedRemote = predict(lastKnownRemoteInput)   // 보통 "직전과 동일" 가정
    advance_simulation(localInput, predictedRemote)   // 기다리지 않고 즉시 진행
    save_state(frame)                                 // 롤백 대비 상태 저장

on real remote input arrives for frame F:
    if predictedRemote[F] != realRemote[F]:           // 예측 실패
        rollback_to(F)                                // F 시점 상태 복원
        for f in F..current:                          // 올바른 입력으로 재시뮬
            advance_simulation(localInput[f], realRemote[f])
        // 화면은 current로 — 롤백은 1프레임 내 보이지 않게 처리
```

**결정론 보존 규칙(Winters와 정합):** fixed timestep · `deterministic_sort`(stable, key 명시) · sim 경로 `unordered` 순회 금지 · `/fp:precise` · render/fx/audio를 sim에서 분리.

### 3.4 박사급 novel 각도 (open problems)

1. **Cross-platform 비트 결정론의 비용-이식성 분석.** soft-float vs fixed-point vs 엄격 IEEE(`/fp:precise`+초월함수 통일 라이브러리)의 성능·정확도·이식성을 정량 비교하고, MOBA/RTS sim에서 "충분한 결정론"의 최소 비용을 도출. 기여 유형 4+5.
2. **병렬 결정론(deterministic parallel lockstep) — 04 JobSystem 직결.** work-stealing의 비결정적 실행 순서와 lockstep의 비트 결정론을 **분리**: 실행 순서는 자유, 관찰 가능 결과는 고정(출력 격리 + 결정론적 reduction). lockstep sim을 멀티코어로 X배 scaling하면서 재현률 100% 증명. (04 통합 명제와 공유)
3. **하이브리드 지연(adaptive delay + rollback).** RTS의 입력 지연과 격투의 rollback을 **연속 스펙트럼**으로 보고, RTT·sim 비용·롤백 가능 깊이에 따라 자동 선택. 가변 지연에서 lockstep stall을 최소화.
4. **결정론 위반 자동 탐지·국소화.** checksum desync가 발생했을 때 **어느 코드 경로·어느 변수**가 갈렸는지 자동 추적(state diff bisection). 분야 공유 디버그 도구(기여 유형 6).

### 3.5 Thesis statement 예시

> "Work-stealing의 비결정적 스케줄과 lockstep의 비트 결정론을 **출력 격리·결정론적 reduction 계약**으로 분리하면, MOBA 서버 sim을 멀티코어에서 single-thread 대비 X배 scaling하면서 1000회 실행 상태 해시 일치율 **100%**(cross-platform 포함)를 유지한다."

### 3.6 평가 방법

- **결정론 재현률:** N회 실행(같은 입력)의 상태 해시 일치 비율, cross-platform(x86↔ARM, MSVC↔Clang) 일치율, desync 발생 tick 분포.
- **대역폭:** 락스텝 명령 트래픽(유닛 수 무관성 검증) vs state replication(유닛 수 비례) — 동일 시나리오 KB/tick 대조.
- **확장성:** 유닛 수(1500+) 대비 sim 비용, 병렬 결정론은 core 수 대비 strong/weak scaling.
- **지연 강건성:** RTT·jitter에서 lockstep stall 빈도, rollback 깊이 분포.
- **Baseline:** single-thread lockstep(결정론 상한·성능 하한), state replication(§5와 대조), GGPO rollback(격투 SOTA).

### 3.7 Winters 연결점

- **결정론 가드가 이미 정책으로 박제:** [`00_TCP_UDP_MIGRATION_INDEX.md`](../TODO/05-07/00_TCP_UDP_MIGRATION_INDEX.md) §3 및 [`02_UDP..._RELIABILITY_DELTA_AOI.md`](../TODO/05-15/02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md) — "stable sort, `/fp:precise`, sim 경로 unordered iteration 금지, render/editor/fx/audio를 prediction subset에서 제외, `unordered_map` 순회 grep guard". **§3.3 결정론 규칙이 빌드·정책에 강제되어 있음.**
- **MOBA는 순수 lockstep이 아니라 server-authoritative + state replication:** Winters는 RTS식 P2P lockstep이 아니라 **권위 서버 + snapshot**이다(§4·§5). 따라서 §3의 직접 적용은 **(a) 서버 sim 자체의 결정론**(replay 재현·anti-cheat 재검증)과 **(b) §1 client prediction의 비트 정합**이다. lockstep은 RTS 비교 baseline·롤백 개념 출처로 작동.
- **병렬 결정론 testbed(→04):** Server 문서군([`05-07/Server/03_SERVER_PARALLEL_PHASES.md`](../TODO/05-07/Server/03_SERVER_PARALLEL_PHASES.md))이 phase를 read-heavy(병렬)/write-heavy(직렬)로 나눠 결정론을 보존 중 → §3.4-(2) "병렬+결정론 동시 달성"이 04 JobSystem 챕터와 공유하는 **단일 통합 명제**의 한 축.
- **결정론 검증 = replay:** [`05-09/Replay/`](../TODO/05-09/Replay/00_REPLAY_INDEX.md)의 [`ReplayRecorder.cpp`](../../Server/Private/Game/ReplayRecorder.cpp)가 snapshot/event를 녹화 → 같은 입력 재생 시 비트 동일 재현 여부가 §3.6 재현률 측정대. 로드맵 M6("deterministic replay, cross-process sim validation, static/rg determinism guard", 02 문서 §10)이 정확히 이 평가다.

---

## 4. 서버 권위 (Server Authoritative) 시뮬레이션

### 4.1 핵심 원리

**문제:** 클라를 믿으면 부정행위(cheat)가 자명하다 — "내가 맞혔다", "내 체력은 무한", "나는 순간이동했다"를 클라가 주장하면 끝이다. P2P·client-authoritative는 신뢰 경계(trust boundary)가 없다.

**서버 권위의 답:** **서버만이 게임 상태(gameplay truth)를 변경한다.** 클라는 **입력(의도)만** 보내고, 결과는 서버가 판정해 snapshot/event로 돌려준다. 클라는 그 결과를 **표현(presentation)**만 한다 — 렌더, 애니메이션, FX, 사운드, UI. 신뢰 경계는 클라↔서버 사이에 명확히 그어지고, **클라가 보낸 어떤 것도 검증 없이 상태에 반영되지 않는다.** 이것이 안티치트의 **토대**다(→11 Security): 권위가 없으면 탐지할 진실 자체가 없다.

**핵심 trade-off — 권위 vs 반응성:** 모든 것을 서버가 판정하면 §1의 latency 문제가 생긴다. 그래서 client prediction(§1)으로 반응성을 빌려오되, **권위는 절대 클라에 넘기지 않는다.** prediction은 "서버가 이렇게 판정할 것"의 추측일 뿐, 틀리면 서버가 이긴다. 즉 **반응성은 예측으로, 정합성은 권위로** — 둘의 분리가 설계의 핵심.

**핵심 trade-off — 권위 비용의 확장:** 서버가 모든 클라의 sim을 권위적으로 돌리면 CPU가 CCU에 비례(혹은 그 이상으로) 증가한다. 입력 검증·재시뮬·AOI(§5)로 이 비용을 sub-linear로 누르는 것이 확장성 연구다.

### 4.2 대표 기존 연구/사례

- **Bernier (2001), Aldridge (2011), Ford "Overwatch Gameplay Architecture and Netcode" (GDC 2017)** — 서버 권위 + 예측의 산업 표준. Overwatch는 "서버가 권위, 클라는 예측, 불일치 시 서버 우선"을 명시. 비학술.
- **Baughman & Levine (2001), "Cheat-proof playout for centralized and distributed online games"** (INFOCOM). **서버 권위·반치팅을 다룬 드문 동료심사 논문** — 명령 공개 순서·신뢰 경계의 형식화. ToN 확장판 존재.
- **GauthierDickey et al. (2004), "Low latency and cheat-proof event ordering for peer-to-peer games"** (NOSSDAV) — 권위 분산 시 순서·부정 방지.
- **MMO 서버 아키텍처:** EVE Online(단일 샤드)·대규모 권위 시뮬의 산업 사례(GDC). 비학술.

> **open problem (가이드 §5-3,4):** (a) **권위와 반응성의 정량적 균형** — 무엇을 예측 허용하고 무엇을 서버 전용으로 둘지의 원칙적 분할. (b) **권위 시뮬 비용의 확장** — CCU·entity 증가 시 권위 재시뮬 비용을 sub-linear로 유지하는 분할(AOI·관심 기반 시뮬 LOD).

### 4.3 알고리즘/프로토콜 (의사코드)

**서버 권위 tick 파이프라인 (Winters GameRoom phase와 정합):**

```text
on fixed tick:
    Phase_DrainCommands:                  // 모든 transport(TCP/UDP)에서 입력 수집
        cmds = drain_all_sessions()
    Phase_ExecuteCommands:                // 신뢰 경계: 여기서만 검증·실행
        stable_sort(cmds, key=(acceptedTick, sessionId, seq))
        for c in cmds:
            if validate(c): apply_authoritative(c)   // 아래 검증
            else: log_reject(c.reason)
    Phase_ServerBotAI:                    // 봇도 command executor를 통해서만 mutate
    Phase_SimulationSystems:              // 이동/투사체/충돌/버프 — 서버 권위 sim
    Phase_ServerDeathAndRespawn:
    Phase_BroadcastEvents:                // damage/cast/death/fx cue (snapshot과 분리)
    Phase_BroadcastSnapshot:              // AOI 적용 상태 복제 (§5)

validate(cmd):                            // 신뢰 경계의 핵심 — 클라 주장 불신
    if issuer is dead: reject "dead-state"
    if skill on cooldown: reject "cooldown"
    if target out of range: reject "out-of-range"   // 거리/실린더 검증
    if not enough mana: reject "mana"
    if stage2 without stage1 window: reject "stage2-window"
    return ok
```

**권위 원칙(불변식):** "최종 위치·데미지·쿨타임·hit·death·objective state는 서버 소유"; "클라가 보낸 결과(데미지값·hit 주장)는 절대 신뢰하지 않고, 클라가 보낸 의도(이동·스킬·타깃)만 받아 서버가 재계산"; "FX/애니/사운드는 server event/cue로 한 번만 발생 — local prediction과 double-spawn 금지".

### 4.4 박사급 novel 각도 (open problems)

1. **반응성-권위 분할의 원칙화.** "예측해도 안전한 상태"(자기 locomotion)와 "서버 전용 상태"(데미지·hit·자원)를 **형식적 기준**으로 분리하고, 그 분할이 부정 표면(cheat surface)을 키우지 않으면서 반응성을 극대화함을 증명. (→§1.4-(2), →11)
2. **권위 비용의 sub-linear 확장.** 권위 재시뮬을 **관심 기반 시뮬 LOD**(먼 곳은 저빈도/근사 권위)로 분할하고, 그것이 CCU N에서 CPU를 sub-linear로 유지하면서 게임플레이 정합을 깨지 않는 경계를 정량화. (→§5 AOI, →08 World Partition)
3. **검증 비용 vs 재시뮬 비용 trade-off.** 모든 command를 full 검증 재시뮬 vs 가벼운 검증 + 사후 감사(audit)·롤백의 비용-저항성 곡선. (→11 사후 탐지)
4. **권위 일관성 모델.** 분산/멀티 인스턴스 권위(샤딩)에서 경계를 넘는 상호작용의 일관성(linearizability vs eventual)과 게임플레이 체감의 관계.

### 4.5 Thesis statement 예시

> "서버 권위 재시뮬을 **관심 기반 시뮬 LOD**로 분할하면, 부정 저항성(클라 신뢰 0)을 유지한 채 CCU N에서 서버 CPU를 **full re-sim 대비 sub-linear(Y% 절감)**로 유지하면서 게임플레이 정합 오류를 인지 불가 수준으로 억제한다."

### 4.6 평가 방법

- **확장성:** CCU·entity 수 대비 서버 tick time(ms)·CPU·메모리 곡선, full re-sim 대비 절감률.
- **반응성:** 입력→권위 반영 지연, prediction hit율(§1.6과 공유).
- **부정 저항성:** 조작 입력(speed/teleport/무한자원/거짓 hit)에 대한 reject율, 통과 시 이득 상한(→11).
- **정합성:** 권위 분할(시뮬 LOD) on/off 시 게임플레이 결과 차이(인지 가능 여부).
- **Baseline:** (상한 정합) 전체 full 권위 re-sim, (하한 저항성) client-authoritative, (산업) Overwatch식 예측+권위.

### 4.7 Winters 연결점

- **권위 파이프라인이 phase로 구현됨:** [`Server/Private/Game/GameRoom.cpp`](../../Server/Private/Game/GameRoom.cpp) + [`GameRoomTick.cpp`](../../Server/Private/Game/GameRoomTick.cpp) + [`GameRoomCommands.cpp`](../../Server/Private/Game/GameRoomCommands.cpp) — `Phase_DrainCommands → ExecuteCommands → ServerBotAI → SimulationSystems → ServerDeathAndRespawn → BroadcastEvents → BroadcastSnapshot` 순서가 [`01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md`](../TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md) §1-5에 명문. **§4.3 의사코드가 실제 코드 구조.**
- **신뢰 경계 = CommandExecutor:** [`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`](../../Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp)가 cooldown/range/mana/target/dead-state 검증을 **단일 acceptance 흐름**으로 통일(01 문서 §1-1). "client-only hook이 HP/position/cooldown/damage를 영구 변경하지 못하게" — 신뢰 경계 위반 회귀를 금지(11 권위 규칙 §16).
- **권위-표현 분리 명문:** [`CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md`](../TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md) §1·§8 — "클라는 입력/snapshot 보간/cue 재생만; FX는 server EffectTrigger 한 곳에서만; local prediction과 server FX double-spawn 금지". §4.4-(1) 반응성-권위 분할의 실측 사례.
- **봇도 권위 경유:** 01 문서 §1-5 — "bot AI는 component 직접 mutate가 아니라 `m_pendingExecCommands`로 command executor에 흘림". AI(→01)조차 권위 경계를 우회 못 함 → §4.4-(1)의 분할 일관성 testbed.
- **안티치트 토대(→11):** lag compensation이 `Server/.../Security/`에 있고 command validation이 reject 사유를 로깅 → 권위가 곧 부정 탐지의 ground truth.

---

## 5. 관심영역 관리 (Area of Interest / Interest Management)와 델타 압축

### 5.1 핵심 원리

**문제 1 (관련성):** 모든 클라에게 전체 월드 상태를 보내면 대역폭이 CCU×entity로 폭발한다. 하지만 플레이어는 **자기 주변·자기 시야**만 알면 된다. 멀리 있는 entity는 보낼 필요가 없다(MOBA의 fog of war처럼).

**AOI(Interest Management)의 답:** 각 플레이어에게 **관련 있는(relevant) entity 부분집합**만 복제한다. 관련성 판정 방식:
- **Grid/Cell 기반:** 월드를 격자로 나눠 플레이어가 속한 셀 + 인접 셀의 entity만. 단순·빠름.
- **Aura/Radius 기반:** 플레이어 중심 반경 내. 시야·사거리와 정합.
- **Region/Zone 기반:** 의미적 영역(레인·정글).

이는 **08 World Partition의 가시성·streaming relevance와 동형(isomorphic)** 이다: "이 관찰자에게 어떤 객체가 관련 있는가"라는 같은 질문을, World Partition은 렌더/로딩으로, netcode는 복제로 답한다. 같은 공간 자료구조(spatial hash/grid/BVH)를 공유할 수 있다.

**문제 2 (중복):** 관련 entity라도 매 틱 전체 상태(위치·체력·버프…)를 보내면 변화 없는 필드까지 반복 전송된다.

**델타 압축(delta compression)의 답 — Quake3 모델:** 서버는 클라가 **마지막으로 ack한 snapshot(baseline)**을 기억한다. 새 snapshot은 baseline 대비 **바뀐 필드만(delta)** 보낸다. 클라는 baseline에 delta를 적용해 복원. ack된 baseline이 있는 한 대역폭은 **변화량에만 비례**한다. Carmack의 Quake3 networking(`.plan`)이 이 모델의 원형: "the server sends the difference between the client's last acknowledged state and the current state."

**문제 3 (대역폭 예산):** delta 후에도 대역폭이 한도를 넘으면, **우선순위(priority)**로 중요한 entity(가까운·전투 중·플레이어가 보는)를 먼저 보내고 나머지는 다음 틱으로 미룬다(priority accumulator).

### 5.2 대표 기존 연구/사례

- **Carmack, J. (Quake3 .plan, 1998) / Quake3 networking 모델** — snapshot + delta + ack 기반 baseline의 산업 원형. 비학술이나 델타 압축의 정전.
- **Bharambe et al. (2008), "Donnybrook: Enabling Large-Scale, High-Speed, Peer-to-Peer Games"** (SIGCOMM). **대규모 게임 확장의 대표 동료심사 논문** — interest management(누구의 상태를 누구에게)로 FPS를 수백 명으로 확장. SIGCOMM 채택 — 이 분야 박사 1차 무대 사례.
- **Bharambe, Pang & Seshan (2006), "Colyseus: A Distributed Architecture for Online Multiplayer Games"** (NSDI). 분산 게임 상태·관심 관리의 시스템 논문.
- **Boulanger, Kienzle & Verbrugge (2006), "Comparing interest management algorithms for massively multiplayer games"** (NetGames). **AOI 알고리즘(grid vs aura vs ...) 직접 비교** — §5의 baseline 비교 방법론 출처.
- **Steed & Oliveira (2009), *Networked Graphics*** (책). interest management·dead reckoning·일관성의 학술적 정리.
- **Fiedler, G. (Gaffer On Games), "Snapshot Compression" / "State Synchronization"** — delta·quantization·priority의 산업 실무 정리. 비학술.

> **open problem (가이드 §5-1,4,6):** (a) **CCU 확장 AOI** — 수천 CCU에서 관련성 계산 자체가 병목. (b) **대역폭-정확도(state staleness) 최적** — 같은 대역폭에서 어떤 entity를 얼마나 자주 갱신해야 인지 오류가 최소인가. (c) **relevance가 08 World Partition 가시성과 동형** — 통합 공간 질의로 양쪽을 동시에 푸는 자료구조.

### 5.3 알고리즘/프로토콜 (의사코드)

**AOI + delta + priority snapshot 빌드 (Winters SnapshotBuilder와 정합):**

```text
// 서버: 플레이어별 관심 집합 → delta → 우선순위 예산 할당
on build_snapshot(player p, currentTick):
    // 1. 관심 집합 (AOI) — spatial query (grid/radius). 08 World Partition과 공유 가능
    visible = spatial_query(p.position, p.viewRadius)   // + fog/vision 규칙

    // 2. baseline: p가 마지막 ack한 snapshot
    baseline = baseline_cache[p]            // 없으면 full snapshot

    // 3. delta 구성: added / changed / removed
    added   = visible - baseline.entities
    removed = baseline.entities - visible   // AOI 밖으로 나간 것도 명시 제거
    changed = { e in (visible ∩ baseline) : state(e) != baseline.state(e) }

    // 4. 우선순위 예산: 대역폭 한도 내에서 중요한 것부터
    candidates = added ∪ changed
    for e in candidates: e.priority = f(distance, in_combat, last_update_age, is_player)
    sort_desc(candidates, by priority)
    packet = {}
    budget = MTU_or_rate_budget
    for e in candidates:
        if size(packet) + size(delta(e)) > budget: break   // 남은 건 다음 틱
        packet.add(delta(e, baseline))
        e.priority_accumulator = 0          // 보낸 것은 우선순위 리셋
    packet.removed = removed
    packet.kind = (baseline exists) ? Delta : Full
    packet.baselineTick = baseline.tick
    send(p, packet)

on client recv snapshot:
    if kind == Full: state = packet.entities
    elif packet.baselineTick == my_last_acked_tick:
        apply added/changed/removed to local baseline
    else: request_full_resync()            // baseline 불일치 → full로 복구
    ack(packet.tick)                        // ack-only heartbeat 가능
```

**핵심 불변식:** delta는 **양쪽이 합의한 baseline**에서만 유효 — baseline mismatch 시 즉시 full resync(stale state 누적 금지). removed를 명시해야 AOI 밖 entity가 클라에 유령으로 남지 않는다.

### 5.4 박사급 novel 각도 (open problems)

1. **대역폭-정확도 Pareto 최적 스케줄링.** entity별 갱신 빈도를 "인지 가능한 state error 기여도"로 가중해 **고정 대역폭 예산 하에서 총 인지 오류 최소화** 문제로 형식화하고 최적/근사 해를 제시. priority 휴리스틱(Fiedler)이 그 해의 근사임을 보이고 능가. 기여 유형 4+6.
2. **CCU 확장 관심 관리.** 관련성 계산을 O(CCU²) → sub-quadratic으로(공간 자료구조·증분 갱신) 만들고, 수천 CCU에서 관심 계산 자체의 scaling을 측정. (→08, →04 병렬)
3. **08 World Partition과의 통합 relevance 질의.** 렌더 가시성·streaming·네트워크 복제를 **단일 spatial relevance 추상**으로 통합 — 한 번의 공간 질의가 세 소비자를 모두 답한다는 설계가 중복 계산을 제거함을 증명. 기여 유형 3.
4. **지각 기반 quantization.** 위치·각도를 거리·중요도에 따라 적응적으로 양자화(먼 entity는 거친 비트)해 대역폭-인지품질 trade-off를 최적화. (delta와 결합)

### 5.5 Thesis statement 예시

> "Entity 갱신을 **인지 가능 state-error 기여도로 가중한 우선순위 예산 문제**로 정식화하면, 고정 대역폭(KB/tick) 하에서 fixed-radius grid AOI + naive delta 대비 플레이어가 인지하는 상태 오차를 **W% 낮추면서** CCU 증가에 sub-quadratic으로 확장한다."

### 5.6 평가 방법

- **대역폭:** 플레이어당 KB/tick, full vs delta 절감률, AOI on/off 절감률, CCU 대비 총 대역폭 곡선.
- **정확도(staleness):** 클라가 본 상태와 서버 권위 상태의 오차(위치 Δ·갱신 지연), 인지 가능 error 비율.
- **확장성:** CCU·entity 대비 관심 계산 시간·서버 CPU, sub-quadratic 검증.
- **복구:** baseline mismatch 시 full resync 빈도·비용, packet loss 하 staleness.
- **Baseline:** (하한) full broadcast, (산업 표준) Quake3 grid+delta, (학술) Donnybrook/Colyseus interest management, Boulanger의 알고리즘 비교.

### 5.7 Winters 연결점

- **AOI·SnapshotBuilder가 이미 존재:** [`Server/Private/Game/AOI.cpp`](../../Server/Private/Game/AOI.cpp) + [`SnapshotBuilder.cpp`](../../Server/Private/Game/SnapshotBuilder.cpp) + [`GameRoomReplication.cpp`](../../Server/Private/Game/GameRoomReplication.cpp). [`02_..._RELIABILITY_DELTA_AOI.md`](../TODO/05-15/02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md) §1-6 — "per-session baseline cache, AOI 적용 시 entity removal도 delta에 포함해 stale 정리, player별 visible set을 snapshot build 입력으로". **§5.3 의사코드가 코드 구조와 정합.**
- **delta/baseline/full-resync 프로토콜 설계됨:** 02 문서 §1-2(`SnapshotEnvelope`, `SnapshotKind{Full,Delta}`, `baselineTick`, added/changed/removed)와 §1-8(`SnapshotApplier`: full overwrite vs delta 순차 적용, baseline mismatch 시 full resync). §5.3 클라 적용부와 동일.
- **transport 신뢰성 채널이 AOI/delta 전제:** [`Shared/Network/UdpReliabilityChannel.h`](../../Shared/Network/UdpReliabilityChannel.h) / [`UdpFragmentHeader.h`](../../Shared/Network/UdpFragmentHeader.h) / [`UdpPacketHeader.h`](../../Shared/Network/UdpPacketHeader.h) / [`SeqMath.h`](../../Shared/Network/SeqMath.h) — `UnreliableSequenced`(snapshot), `ReliableOrdered`(command), ack/ackBitfield, MTU 1200·fragment. snapshot의 unreliable-sequenced + baseline ack가 §5.3 ack 모델의 실제 구현 토대.
- **08 World Partition 동형성:** MOBA fog of war([`05-13/05_FOG_OF_WAR_VISION_REVEAL_PLAN.md`](../TODO/05-13/05_FOG_OF_WAR_VISION_REVEAL_PLAN.md))의 vision 판정이 곧 AOI 관련성 — §5.4-(3) "렌더 가시성=네트워크 relevance 통합 질의"의 직접 testbed. ECS `VisionSystem`/`SpatialHashSystem`(→04 §3.7)이 공유 spatial query 후보.
- **대역폭 측정 인프라:** 02 문서가 "snapshot payload size를 매 전송 로그에 남긴다"(MTU 감시) — CLAUDE.md overlay/trace 문화와 합쳐 §5.6 KB/tick 측정대.

---

## 종합. 통합 학위논문 구조 예시 (three-papers → 하나의 명제)

가이드 §4.1 "Three Papers Make a Thesis" 모델로, **이 분야의 인접 3문제**를 하나의 학위논문으로 묶는다. netcode는 학술 출판이 드물므로(§0), 묶는 명제는 **측정·모델 기여**를 전면에 둔다 — "구현했다"가 아니라 "측정·정량화·모델로 분야의 공백을 메운다".

> **통합 Thesis statement:**
> "가변 RTT·CCU 하의 서버 권위 MOBA netcode에서 **공정성·예측오차·대역폭을 측정 가능한 목적함수로 정식화**하고 그 위에서 적응형 정책을 최적화하면, 산업 표준 고정-파라미터 휴리스틱이 그 목적함수의 특수해일 뿐임을 보이고, 공정성-반응성-확장성의 Pareto front를 정량적으로 전진시킬 수 있다."

```text
제목(가제): Measuring and Optimizing Fairness, Prediction, and Scalability
             in Server-Authoritative MOBA Netcode

Ch 1. 서론
   1.3 Thesis: 위 통합 명제
   1.4 기여:
       (C1) 지각 기반 reconciliation — 예측오차의 지각적 비용 모델 + user study (§1)
       (C2) lag compensation 공정성의 정량 모델 + 적응형 rewind window (§2)
       (C3) 대역폭-정확도 Pareto 최적 AOI/delta 스케줄링 + CCU 확장 (§5)
       (보조) 서버 권위 분할(시뮬 LOD)의 비용-저항성 분석 (§4), 결정론 재현 기반 (§3,→04)

Ch 2. 배경/관련 연구
   - 산업 표준(명시 구분): Bernier 2001, Bettner&Terrano 2001, Carmack .plan,
     Cannon GGPO, Aldridge/Ford GDC, Fiedler
   - 동료심사: Mauve 2004(timewarp/local-lag), Baughman&Levine 2001(cheat-proof),
     Bharambe 2008(Donnybrook), Bharambe 2006(Colyseus), Boulanger 2006(IM 비교),
     Claypool(latency-performance)
   - gap: 산업이 학계를 20년 앞섬 → "왜 그 파라미터, 어떤 한계, 어떻게 일반화"의
     검증 가능한 모델·측정이 비어 있음 (가이드 §2-3 측정 논문의 기회)

Ch 3 (논문1, NetGames/CHI PLAY): C1 — Perceptual Reconciliation (§1)
   - Winters ClientInputBuffer + 공유 CommandExecutor로 rewind&replay 구현
   - 보정 정책별 user study(IRB) — visual pop 인지율, 2AFC
   - 평가: misprediction rate, correction 분포, 지각 오류율; baseline Mauve/Bernier

Ch 4 (논문2, NetGames/ToN): C2 — Fairness of Lag Compensation (§2)
   - Winters CLagCompensation(200ms/30Hz)을 baseline으로, 적응형 window 제안
   - shot-behind-cover 확률 모델 + 가변 RTT 측정
   - 평가: 공정성 지표, 명중률, 부정 저항성; Pareto front

Ch 5 (논문3, SIGCOMM/NSDI/MMSys): C3 — Scalable Optimal Interest Management (§5)
   - Winters AOI/SnapshotBuilder + delta/baseline을 baseline으로
   - 인지 state-error 가중 우선순위 예산 최적화 + 08 World Partition 통합 질의
   - 평가: KB/tick, staleness, CCU sub-quadratic; baseline Quake3/Donnybrook

Ch 6. 종합 평가 — Winters 5~N client MOBA에서 C1+C2+C3 통합 측정
        (지연 ms, 대역폭 KB/tick, CCU scaling, 예측 정확도, 공정성, 부정 저항성)
Ch 7. 논의 — threats(RTT 분포 현실성, user study 일반화, FP 결정론, sim-only subset 경계),
        일반화(FPS/격투/cloud gaming)
Ch 8. 결론 — 통합 명제 증명 정리, future work(분산 권위 샤딩, edge, ML 기반 정책)
```

핵심: 3편이 각각 **독립 venue 논문**(NetGames/ToN/SIGCOMM)이면서, 서론·결론이 "산업 휴리스틱을 측정 가능한 목적함수로 환원하고 능가"라는 **단일 명제**로 묶는다(가이드 §4.1). Winters는 C1(ClientInputBuffer + 공유 CommandExecutor), C2(실재 CLagCompensation 200ms/30Hz), C3(AOI/SnapshotBuilder/delta)에 **모두 작동하는 testbed**를 이미 보유 — 게다가 UDP transport(CUdpCore)는 아직 계획 단계라 **구현 전 측정 설계**를 처음부터 박아 넣을 수 있다(가이드 §7 측정 인프라 우선). 이는 가이드 §2의 "엔진은 검증 플랫폼" 그 자체다.

> **구현 vs 기여 최종 환기(가이드 §1):** "TCP/UDP 분리·prediction·lag comp·delta·AOI를 구현했다"는 **산업 결과물**이다. 본 분야의 박사 기여는 그 위에서 **(a) 공정성·예측오차·대역폭을 측정 가능하게 정의하고, (b) 산업 고정 파라미터가 어떤 모델의 특수해인지 밝히고, (c) 적응형으로 Pareto front를 전진시킴을, (d) Winters라는 실 워크로드에서 baseline 대비 정량적으로 증명**하는 것이다. netcode의 학술 공백(§0)이 이 측정·모델 기여를 특히 가치 있게 만든다.

---

## 참고문헌

**Client Prediction / Reconciliation / Lag Compensation (산업 표준 — 비학술, 명시 구분)**
- Bernier, Y. W. (2001). Latency Compensating Methods in Client/Server In-game Protocol Design and Optimization. *GDC* (Valve; 산업, 비학술).
- Gambetta, G. Fast-Paced Multiplayer (Parts I–IV). *gabrielgambetta.com* (교육, 비학술).
- Carmack, J. (1996–1998). *.plan files* (id Software; Quake/Quake3 networking, 비학술).
- Aldridge, D. (2011). I Shot You First: Networking the Gameplay of Halo: Reach. *GDC* (산업, 비학술).
- Ford, T. (2017). Overwatch Gameplay Architecture and Netcode. *GDC* (산업, 비학술).
- Fiedler, G. Networked Physics / Snapshot Compression / State Synchronization. *gafferongames.com* (산업, 비학술).

**Deterministic Lockstep / Rollback (산업 표준 — 비학술)**
- Bettner, P., & Terrano, M. (2001). 1500 Archers on a 28.8: Network Programming in Age of Empires and Beyond. *GDC* (산업, 비학술).
- Cannon, T. (~2006). GGPO Rollback Networking / "Fightin' Words" (격투 게임, 비학술).

**Netcode / Interest Management / Fairness (동료심사 학술)**
- Mauve, M., Vogel, J., Hilt, V., & Effelsberg, W. (2004). Local-Lag and Timewarp: Providing Consistency for Replicated Continuous Applications. *IEEE Transactions on Multimedia*, 6(1).
- Savery, C., & Graham, T. C. N. (2013). Timelines: Simplifying the Programming of Lag Compensation for the Next Generation of Networked Games. *Multimedia Systems*.
- Baughman, N. E., & Levine, B. N. (2001). Cheat-proof Playout for Centralized and Distributed Online Games. *IEEE INFOCOM* (저널 확장: *IEEE/ACM ToN*).
- GauthierDickey, C., Zappala, D., Lo, V., & Marr, J. (2004). Low Latency and Cheat-proof Event Ordering for Peer-to-Peer Games. *NOSSDAV*.
- Bharambe, A., et al. (2008). Donnybrook: Enabling Large-Scale, High-Speed, Peer-to-Peer Games. *SIGCOMM*.
- Bharambe, A., Pang, J., & Seshan, S. (2006). Colyseus: A Distributed Architecture for Online Multiplayer Games. *NSDI*.
- Boulanger, J.-S., Kienzle, J., & Verbrugge, C. (2006). Comparing Interest Management Algorithms for Massively Multiplayer Games. *NetGames*.
- Claypool, M., & Claypool, K. (2006). Latency and Player Actions in Online Games. *Communications of the ACM* / 후속 *NetGames* 시리즈.
- Steed, A., & Oliveira, M. F. (2009). *Networked Graphics: Building Networked Games and Virtual Environments*. Morgan Kaufmann.

**결정론적 실행 (→04 JobSystem과 공유)**
- Devietti, J., et al. (2009). DMP: Deterministic Shared Memory Multiprocessing. *ASPLOS*.
- Olszewski, M., Ansel, J., & Amarasinghe, S. (2009). Kendo: Efficient Deterministic Multithreading in Software. *ASPLOS*.

**Winters testbed (1차 출처)**
- 전송·신뢰성: [`Shared/Network/UdpPacketHeader.h`](../../Shared/Network/UdpPacketHeader.h), [`UdpFragmentHeader.h`](../../Shared/Network/UdpFragmentHeader.h), [`UdpReliabilityChannel.h`](../../Shared/Network/UdpReliabilityChannel.h), [`SeqMath.h`](../../Shared/Network/SeqMath.h), [`PacketEnvelope.h`](../../Shared/Network/PacketEnvelope.h)
- 서버 권위·AOI·delta·replay: [`Server/Private/Game/`](../../Server/Private/Game/) — `GameRoom.cpp`, `GameRoomTick.cpp`, `GameRoomCommands.cpp`, `GameRoomReplication.cpp`, `SnapshotBuilder.cpp`, `AOI.cpp`, `ReplayRecorder.cpp`
- 신뢰 경계: [`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`](../../Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp)
- lag compensation: [`Server/Public/Security/LagCompensation.h`](../../Server/Public/Security/LagCompensation.h), [`Server/Private/Security/LagCompensation.cpp`](../../Server/Private/Security/LagCompensation.cpp)
- client 예측·적용: [`Client/Private/Network/Client/`](../../Client/Private/Network/Client/) — `ClientInputBuffer.cpp`, `CommandSerializer.cpp`, `SnapshotApplier.cpp`, `EventApplier.cpp`
- 계획·정책: [`.md/TODO/05-07/00_TCP_UDP_MIGRATION_INDEX.md`](../TODO/05-07/00_TCP_UDP_MIGRATION_INDEX.md), [`02_UDP_GAMEPLAY_TRANSPORT_MIGRATION.md`](../TODO/05-07/02_UDP_GAMEPLAY_TRANSPORT_MIGRATION.md), [`.md/TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md`](../TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md), [`02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md`](../TODO/05-15/02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md), [`.md/TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md`](../TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md), [`.md/TODO/05-09/Replay/`](../TODO/05-09/Replay/00_REPLAY_INDEX.md)
