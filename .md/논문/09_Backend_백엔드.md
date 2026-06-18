# 09. 게임 백엔드(경제·인벤토리·거래) — 박사 연구 심화

> 전제 문서: [`00_PHD_Paper_Guide.md`](00_PHD_Paper_Guide.md). 본 문서는 가이드 §1(구현 vs 기여), §3(thesis statement), §4(구조), §7(평가)을 **그대로 전제**한다. 다만 본 분야의 평가축은 그래픽스의 frame time이 아니라 **처리량(TPS, transactions/s), 지연(p50/p99 latency), 일관성·정합성(consistency/invariant), 확장성(scalability under shard/CCU), 부정거래율(fraud/dupe rate)**이다. 모든 세부주제마다 "이건 구현 항목인가, 기여 후보인가?"를 가이드 §1로 되돌아가 묻는다.
>
> 독자: LoL 스타일 MOBA + 오픈월드 엔진 'Winters'를 만든 숙련 C++ 개발자. 백엔드는 이미 Go 마이크로서비스(`Services/`)로 분리되어 있고(auth/profile/shop/payment/matchmaking/leaderboard), PostgreSQL·Redis·Kafka 인프라가 있으며, 인게임 경제는 `Shared/GameSim`의 ECS command 흐름(`BuyItem`)으로 별도 존재한다.
>
> Top venue: **SOSP, OSDI, VLDB, SIGMOD, NSDI, EuroSys** (분산시스템/DB 성격). 산업 참고(비학술): **GDC Online**(구 Austin), 배포 사례 블로그(EVE Online/Riot/Valve), 정부 보고서(가상경제).

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue)

게임 백엔드는 "엔진 박사"가 **"나는 상점 서비스를 만들었다"로 끝나기 가장 쉬운** 분야다. JWT 인증, REST API, PostgreSQL 트랜잭션, Redis 캐시, Kafka 이벤트 — 이것들은 전부 **이미 산업 표준으로 존재하는 기법**이고, Winters의 `Services/` 디렉터리(아래 §0.4)는 그것을 충실히 구현한 **잘 만든 사내 서비스**다. 잘 만든 사내 서비스는 학위논문이 아니다. 그것은 **엔지니어링 결과물**이다.

이 분야가 박사가 되는 지점은 가이드 §5의 기여 유형 중 **3(새 시스템/아키텍처), 4(새 이론/증명: 불변식·정합성의 형식 검증), 5(새 경험적 발견: 대규모 경제·부정거래의 측정 법칙), 6(새 벤치마크/데이터셋)**이다. "게임 경제·거래"는 분산 데이터베이스 연구(VLDB/SIGMOD)와 시스템 연구(SOSP/OSDI)의 한 응용 도메인이지만, 다음 세 가지가 **게임을 일반 OLTP와 다르게 만드는 특수성**이며 이것이 박사 기여의 표면이다.

> **이 문서를 관통하는 연구 긴장(research tension):**
> 게임 백엔드는 **세 가지 모순을 동시에** 안고 있다.
> (1) **두 개의 경제가 다른 일관성 모델로 공존한다.** 인게임 경제(분당 골드, 아이템)는 *전투 틱의 일부*라 강한 일관성·저지연이 필수이고(틱 내 권위 결정), 메타 경제(계정 재화 RP/코인, 스킨)는 *결제·법적 정합성*이 필수다(돈이 걸려 ACID·감사). 두 경제는 같은 "아이템·재화" 개념을 공유하지만 **정합성 요구가 정반대**다.
> (2) **부분 실패(partial failure)가 곧 경제 붕괴다.** 거래 중 한쪽만 커밋되면 아이템 **복제(dupe)** 또는 소실이 생기고, 이는 그래픽스의 1프레임 깜빡임과 달리 **영구적·전파적**이다(복제된 골드는 시장 전체를 인플레시킨다).
> (3) **공정한 적대자(adversary)가 상주한다.** RMT(real-money trading) 봇, 사기 거래, 매크로는 경제를 **공격 표면**으로 만든다(→ `11_Security` 연결). 일반 은행 OLTP에는 "재미있게 경제를 망가뜨리려는 수백만 동시 적대자"가 없다.
> **"강일관성 + 확장성 + dupe-free + 부정탐지를 게임 워크로드 특성 위에서 동시에"** — 이 사중 제약의 게임 특화 최적점이 본 문서의 핵심 박사 각도다.

### 0.1 구현 vs 기여 — 4개 세부주제 대조

| 세부주제 | 구현 (석사/산업) | 연구 기여 (박사) |
|---|---|---|
| 가상 경제 | "골드 소스/싱크를 넣고 인플레를 막았다" | "라이브 텔레메트리로 **재화 흐름을 추정하고 source/sink 파라미터를 자동 조정**해, 수동 패치 대비 목표 인플레율 편차를 X% 줄임을 다중 시즌 시뮬레이션으로 증명" |
| 인벤토리 | "아이템 template/instance를 나누고 직렬화·버전 마이그레이션을 했다" | "수백만 항목 인벤토리의 **버전 마이그레이션을 무중단·증분으로 수행**하면서 p99 읽기 지연을 Y ms 이내로 유지하는 lazy schema-on-read 모델 + 정합성 증명" |
| 거래/트랜잭션 | "2PC/Saga로 거래를 구현하고 idempotency key를 넣었다" | "분산 거래의 **exactly-once와 dupe-freedom을 불변식으로 형식 명세하고, 거래 경로를 model checking(TLA+)으로 검증**해 경쟁·부분실패 하에서도 위반이 없음을 증명" |
| 확장성/부정 | "샤딩하고 봇을 룰 기반으로 차단했다" | "거래 그래프에서 **RMT/사기 네트워크를 그래프 임베딩으로 탐지**해 룰 기반 대비 precision/recall을 z만큼 개선하고, 강일관성↔확장성 트레이드오프의 게임 특화 최적점을 정량화" |

### 0.2 Top Venue 표 (게임 백엔드 = 분산 DB/시스템 응용)

| 구분 | Venue | 성격 | 이 분야 대표 채택 주제 |
|---|---|---|---|
| 데이터베이스 | **VLDB, SIGMOD** | 트랜잭션·일관성·분산 저장 | serializability, distributed transactions, isolation |
| 시스템 | **SOSP, OSDI** | 분산 합의·내결함성·일관성 | consensus(Paxos/Raft), exactly-once, state machine replication |
| 네트워크 시스템 | **NSDI** | 측정·대규모 분산 운영 | datacenter consistency, large-scale measurement |
| 유럽 시스템 | **EuroSys** | 런타임·트랜잭션·재현성 | transactional systems, fault injection |
| 저널 | **ACM TODS, ACM TOCS, VLDB Journal** | 확장판/증명 | — |
| 산업(비학술, 인용용) | GDC Online, 배포 사례 | 사례·영향력·문제 실재 근거 | EVE Online 단일 샤드, Diablo III RMAH, Path of Exile 경제 |

> **주의(가이드 §9):** GDC Online 발표나 운영 블로그(예: CCP의 EVE 경제 보고서, Riot 엔지니어링 블로그)는 **권위 있으나 동료심사 학술 출판이 아니다.** 박사 기여의 1차 무대는 VLDB/SOSP이고, 산업 사례는 "이 문제가 수백만 유저 규모로 실재한다"는 motivation 근거로 인용한다.

### 0.3 Heilmeier 체크 (이 분야 적용)

가이드 §6의 Heilmeier Catechism을 본 분야에 박으면:
- **무엇을?** "수백만 명이 동시에 거래·구매하는 게임 경제에서, 아이템이 복제되거나 사라지지 않으면서도 빠르고, 봇/사기를 잡아내는 백엔드."
- **지금 한계?** 강일관성(2PC, 단일 DB)은 안 확장되고, 확장 가능한 것(eventual consistency, 샤딩)은 dupe/소실 창을 연다. 부정탐지는 룰 기반이라 우회된다.
- **새로움?** 거래 불변식을 **형식 명세 + 기계검증**으로 보장(코드 리뷰·테스트 너머), 경제 균형을 **데이터 기반 자동조정**, 사기를 **거래 그래프 구조**로 탐지.
- **누가 신경?** MMO/MOBA/마켓플레이스 운영사, 결제·규제 준수, 그리고 일반 분산 트랜잭션·이상탐지 커뮤니티.
- **어떻게 측정?** 처리량(TPS), p99 지연, **dupe 사건 수(불변식 위반 횟수)**, 경제 인플레율 편차, 부정탐지 precision/recall, 샤드 수 대비 strong/weak scaling.

### 0.4 Winters 백엔드의 현 위치 — 정직한 자기 평가

Winters의 실제 백엔드는 이미 산업 수준에 가깝다. 그래서 **"무엇이 구현이고 무엇이 미해결 연구인가"의 경계가 특히 날카롭다.**

```text
Services/cmd/{auth,leaderboard,matchmaking,profile,payment,shop}  : 포트 8081~8086 분리 마이크로서비스
Services/pkg/{auth(jwt),cache(redis),database(postgres),messaging(kafka),middleware,response}

메타 경제(계정 재화) — 이미 ACID로 구현됨:
  Services/internal/payment/repository.go  ProcessCharge: BEGIN → INSERT payment_transactions
                                            → UPDATE wallets ... RETURNING balance
                                            → INSERT coin_transactions(ledger) → COMMIT
  Services/internal/payment/service.go     Charge: FindByIdempotencyKey 선조회(멱등),
                                            gateway.VerifyReceipt(결제 검증) 후 처리
  Services/internal/shop/repository.go     Purchase: BEGIN
                                            → SELECT price ... WHERE is_active=true
                                            → SELECT balance ... FOR UPDATE (비관적 락)
                                            → UPDATE wallets balance-price RETURNING
                                            → INSERT inventory ... ON CONFLICT DO UPDATE quantity+1 (upsert)
                                            → INSERT coin_transactions → COMMIT

인게임 경제(전투 재화) — ECS 틱 안, 다른 일관성 모델:
  Shared/GameSim/Definitions/ItemDef.h     CItemRegistry: 하드코딩 item template(template/instance 미분리)
  Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp:2101 HandleBuyItem:
      서버 권위 검증(issuer alive, item 존재, gold>=price, slot 여유) 후 즉시 mutate,
      감사로그/멱등키/롤백 없음 (틱 결정론에 의존)
```

이 그림에서 **이미 구현된 것(기여 아님)**: JWT, REST, 마이크로서비스 분리, 단일 DB 트랜잭션, `FOR UPDATE` 비관적 락, idempotency-key 선조회, Kafka 이벤트 발행, ledger 테이블(`coin_transactions`). 이건 "잘 만든 사내 서비스"다.

**아직 미해결(기여 후보)**: ① 인게임 경제와 메타 경제 **사이의 거래**(예: 인게임 보상→계정 재화 환원)가 두 일관성 모델·두 저장소를 건너야 할 때의 exactly-once. ② payment의 idempotency-key가 **경쟁(동시 중복 요청)** 하에서도 dupe를 막는지의 **형식 증명**(현재는 "선조회 후 INSERT"라 TOCTOU 창이 있을 수 있다 — §3.4). ③ 샤딩 후 wallet/inventory가 여러 노드에 흩어졌을 때의 정합성. ④ 경제 파라미터 자동조정. ⑤ 거래 그래프 기반 RMT 탐지. 이 다섯이 본 문서가 다루는 박사 표면이다.

> **핵심 자가진단:** Winters 백엔드를 "더 잘 구현"하는 것은 박사가 아니다. Winters 백엔드를 **testbed로 삼아, 위 ①~⑤ 중 하나의 미해결 문제에 검증된 명제를 만드는 것**이 박사다.

---

## 1. 가상 경제·재화(Virtual Economy) 설계와 시뮬레이션

### 1.1 핵심 원리

가상 경제의 근본은 **재화 보존이 인위적이라는 것**이다. 실물 경제와 달리 게임 재화는 **무에서 생성(source)되고 무로 소멸(sink)**한다. 경제 건강은 이 둘의 균형으로 결정된다.

- **Source(수도꼭지, faucet):** 재화가 시스템에 들어오는 경로. MOBA 인게임: 미니언 처치 골드, 시간당 패시브 골드, 챔피언 킬. 메타: 일일 보상, 결제(RP 충전 → Winters `payment.Charge`).
- **Sink(배수구, drain):** 재화가 시스템에서 빠지는 경로. 인게임: 아이템 구매(`HandleBuyItem`에서 `gold.amount -= price`), 부활 대기 비용. 메타: 스킨 구매(`shop.Purchase`), 수수료.

**Faucet-Drain 모델의 핵심 명제:** 통화 공급량의 시간 변화는 source 유입률에서 sink 유출률을 뺀 것이다.

```text
dM/dt = Σ(faucet_i) - Σ(drain_j)

M       : 유통 통화량 (money supply)
faucet  : 단위 시간당 생성 (mob gold rate, daily reward, RP purchase)
drain   : 단위 시간당 소멸 (item cost, repair, fee, tax)

인플레이션: dM/dt > 0 이 지속되고 재화당 효용이 고정이면,
            가격(아이템 가치 대비 골드)이 상승 → 신규 유저 진입 장벽 상승.
```

MOBA의 **인게임 경제는 closed·ephemeral**이다(게임당 30~40분, 경제가 게임 종료 시 리셋). 그래서 인플레는 "게임 내 시간에 따른 파워 곡선"으로 의도적이다 — 분당 골드는 *밸런스 노브*다. 반면 **메타 경제는 open·persistent**다(계정 재화는 영구). 여기서 누적 인플레·dupe·RMT가 진짜 문제가 된다. **이 두 경제의 시간 척도 차이(분 vs 년)가 일관성 요구를 가른다.**

경제학의 **교환방정식(MV = PQ, Fisher)**을 게임에 적용하면: 통화량 M과 유통속도 V의 곱이 가격수준 P와 거래량 Q의 곱과 같다. 게임 운영자는 M(source/sink), V(거래 마찰·바인딩), Q(콘텐츠 소비)를 *직접 조절 가능한 정책 변수*로 쥐고 있다는 점이 실물 경제와 결정적으로 다르다.

### 1.2 대표 기존 연구·시스템

- **Castronova, E. (2001), "Virtual Worlds: A First-Hand Account of Market and Society on the Cyberian Frontier."** 가상세계 경제를 실증 경제학 대상으로 본 효시. EverQuest의 시급·환율을 측정. 가상경제가 실물 경제 법칙(공급/수요)을 따른다는 것을 보임.
- **Lehdonvirta, V. & Castronova, E. (2014), "Virtual Economies: Design and Analysis"** (MIT Press). 가상경제 설계의 표준 교과서. faucet/sink, money supply, 가격 안정화 메커니즘 체계화.
- **EVE Online (CCP Games)** — 가장 깊이 측정·공개된 라이브 경제. **사내 경제학자(real economists)**를 고용하고 월간 경제 보고서(Monthly Economic Report)에서 통화량·생산·파괴(sink)·지역별 물가지수를 공개. 산업 사례지만 "라이브 경제를 데이터로 운영"의 정점. (학술 아님, motivation 인용.)
- **Diablo III Real-Money Auction House (2012, 폐지 2014)** — *실패 사례로서 강력하다.* 실물 거래소가 게임 보상 루프를 붕괴시키고 봇·사기를 유발해 폐지. "경제 설계 실패가 게임을 죽인다"의 교과서적 증거.
- **Agent-Based Modeling(ABM)** — Bonabeau (2002), "Agent-based modeling: Methods and techniques for simulating human systems" (PNAS). 개별 에이전트(플레이어)의 미시 행동에서 거시 경제 창발을 시뮬레이션. 경제 패치를 라이브 배포 전 검증하는 도구.

### 1.3 자료구조·알고리즘 (의사코드)

핵심 연구 도구는 **경제 시뮬레이터**다: 라이브 배포 없이 정책(source/sink 파라미터) 변경의 거시 효과를 예측한다. ABM이 표준 접근이다.

```text
# Agent-Based 경제 시뮬레이션 (정책 변경 사전 검증)
struct Agent:
    gold, inventory[]
    archetype          # casual / hardcore / trader / bot
    policy             # 행동 정책: 언제 벌고/쓰고/거래하는가 (텔레메트리에서 추정)

function SIMULATE_ECONOMY(agents[], faucet_params, sink_params, T_days):
    M_history = []
    price_history = []                       # 시장 가격(거래 체결가) 추적
    for day in 1..T_days:
        for agent in agents:                 # 미시: 각 에이전트 행동
            earned  = SAMPLE_FAUCET(agent, faucet_params)   # 사냥/보상/결제
            agent.gold += earned
            action = agent.policy.decide(agent, market_state)
            if action == BUY:  EXECUTE_TRADE(agent, market, sink_params)  # drain 포함
            if action == SELL: LIST_ON_MARKET(agent, market)
        M = Σ(agent.gold for agent in agents) # 거시: 통화량 집계
        M_history.append(M)
        price_history.append(MEDIAN(market.recent_fills))
    return analyze(M_history, price_history)  # 인플레율, 지니계수, 가격 안정성

# OPEN PROBLEM: faucet/sink_params를 라이브 텔레메트리로 추정하고
#   목표 인플레율을 만족하도록 자동 역산(control loop)하는 것 (§1.4)
```

**데이터 기반 자동 조정**의 골격(미해결 영역):

```text
# 경제 균형 자동 조정 (control-theoretic auto-balancing)
target_inflation = 0.02                      # 월 2% 등 정책 목표
loop each balancing_period:
    M_t        = MEASURE_MONEY_SUPPLY()      # 라이브 텔레메트리
    inflation  = ESTIMATE_INFLATION(price_index_history)
    error      = inflation - target_inflation
    # 제어기(PID 등)로 sink 강도 조절 — 단, 플레이어 체감 급변 방지 제약
    Δsink      = CONTROLLER(error, constraints={max_player_perceptible_change})
    sink_params = sink_params + Δsink
    # 반사실 검증: 적용 전에 ABM으로 시뮬레이션해 안정성 확인 (배포 게이트)
    if SIMULATE_ECONOMY(sampled_agents, faucet, sink_params).is_stable():
        DEPLOY(sink_params)
```

### 1.4 박사급 novel 각도 (open problems)

1. **데이터 기반 경제 균형 자동조정(closed-loop economy control).** 현재 게임 경제는 디자이너가 수동으로 source/sink를 패치한다(느리고 사후적). 박사 각도: 라이브 텔레메트리에서 경제 상태를 **추정**하고, **제어이론(control theory)**으로 sink/faucet을 자동 조정하되 *플레이어 체감 급변을 제약*하는 안정적 제어기. 기여 = control loop + 안정성(수렴) 증명 + ABM 반사실 검증.
2. **ABM 시뮬레이터의 라이브 정합성(sim-to-live calibration).** ABM이 라이브와 얼마나 일치하는가는 늘 의심받는다. 박사 각도: 라이브 거래 로그로 에이전트 정책을 **역강화학습(IRL)**으로 추정해, 시뮬레이터의 예측 오차를 정량화하고 보정. 기여 = "경제 시뮬레이터의 신뢰구간"이라는 측정 방법론.
3. **봇/RMT의 경제적 식별.** 봇은 **경제 행동 패턴**(파밍 일관성, 비인간적 거래 그래프)으로 드러난다. §4와 연결되지만, 여기서는 "경제 균형 관점"에서 봇이 통화량에 미치는 영향을 모델링하고 탐지 임계를 경제 손실로 정당화.

### 1.5 Thesis statement 예시

```text
"라이브 거래 텔레메트리에서 에이전트 정책을 추정한 agent-based 경제 시뮬레이터와
 제어이론 기반 sink 자동조정 루프를 결합하면, MOBA 메타 경제에서
 디자이너 수동 패치(평균 주기 N주) 대비 목표 인플레율 편차를 X% 줄이면서
 플레이어 체감 가격 급변(주당 변화율)을 임계 이하로 유지할 수 있다."
```

검증: baseline = 고정 파라미터 + 주기적 수동 패치. SOTA = rule-based 동적 조정. metric = 인플레율 편차, 가격 변동성, 지니계수(부의 집중), 시뮬레이터-라이브 예측오차.

### 1.6 평가 방법

- **시뮬레이션 기반:** 다중 시즌(예: 12개월) ABM 실행, 여러 시드. 인플레율·가격지수·부의 분포(지니).
- **반사실(counterfactual):** 같은 초기 상태에서 정책 A vs B를 시뮬레이션 비교.
- **Sim-to-live 검증:** 시뮬레이터 예측과 실제 라이브 지표의 오차(가능하면 과거 패치를 retrodict).
- **안정성:** 제어 루프가 외란(이벤트성 재화 폭주, 봇 유입) 후 수렴하는가.
- **Threats to validity:** 에이전트 정책 추정의 편향, 라이브 행동의 비정상성(메타 변화), 시뮬레이터 단순화.

### 1.7 Winters 연결점

- 인게임 경제 testbed: `Shared/GameSim`의 분당 골드·미니언 골드(faucet)와 `HandleBuyItem`(sink, `CommandExecutor.cpp:2127` `gold.amount -= pItem->price`). MOBA 인게임 경제는 **closed·짧은 시간척도**라 "단일 게임 내 파워 곡선" 밸런싱의 깨끗한 testbed.
- 메타 경제 testbed: `payment.Charge`(RP 충전 faucet) + `shop.Purchase`(스킨 sink) + `coin_transactions` ledger. **open·영속**이라 누적 인플레·자동조정의 testbed.
- 측정 인프라: Kafka `TopicPlayerEvents`/`TopicPaymentEvents`(`messaging/kafka.go`)가 이미 경제 이벤트 스트림을 발행한다 — ABM 캘리브레이션·텔레메트리의 천연 데이터 소스. **이것이 가이드 §7가 강조한 "측정 인프라"의 실재 기반.**

---

## 2. 인벤토리·아이템 시스템 (데이터 모델)

### 2.1 핵심 원리

인벤토리의 근본 설계 결정은 **template(definition) vs instance(ownership) 분리**다.

- **Template(아이템 정의):** 불변·공유. "Long Sword는 가격 350, +10 AD." 모든 'Long Sword'가 공유. Winters: `ItemDef`(`ItemDef.h:26`)와 `CItemRegistry`의 하드코딩 테이블.
- **Instance(보유 인스턴스):** 가변·유저별. "유저 U가 Long Sword 1개를 슬롯 2에 보유, 강화 +3, 인챈트 X." Winters 인게임: `InventoryComponent.itemIds[]`(현재 itemId만 — 인스턴스 상태 거의 없음). Winters 메타: PostgreSQL `inventory(user_id, item_id, quantity)`(`shop/repository.go`).

**왜 분리하는가:** (1) 저장 효율 — 100만 유저가 같은 검을 가져도 정의는 하나. (2) 밸런스 패치 — 정의를 바꾸면 모든 인스턴스에 즉시 반영. (3) 인스턴스별 상태(내구도·소켓·랜덤 옵션)는 인스턴스에만. 이 분리가 **스키마 진화의 단위**를 정한다(template 변경 vs instance 마이그레이션).

**스택(stacking)과 슬롯(slot) 모델:**

```text
모델 A — 슬롯 배열(slot array):  inventory[slot] = {itemId, count}
   장점: 위치 고정(UI 드래그·정렬), 빠른 슬롯 접근. MOBA 인게임에 적합.
   Winters 인게임: InventoryComponent.itemIds[kMaxSlots] — 정확히 이 모델(LoL 6칸).

모델 B — 항목 집합(item bag):   rows of (user_id, item_id, quantity, instance_data)
   장점: 가변 크기, 관계형 질의·집계 용이. 메타 인벤토리(스킨 수천 종)에 적합.
   Winters 메타: shop/repository.go inventory 테이블 — 정확히 이 모델.
```

**직렬화·버전 마이그레이션:** 인벤토리는 영속 상태라 **스키마가 시간에 따라 진화**한다(필드 추가, 의미 변경). 핵심 명제: *저장된 데이터에는 버전이 박혀 있어야 하고, 코드는 모든 과거 버전을 읽을 수 있어야 한다.*

```text
저장 레코드:  { schema_version: u16, payload: bytes }

읽기 경로(중요):
   v_data = read(record)
   while v_data.version < CURRENT_VERSION:
       v_data = MIGRATE[v_data.version](v_data)   # v_n → v_{n+1} 단계 마이그레이션
   return deserialize(v_data)

전략 비교:
   eager(즉시 일괄 마이그레이션): 단순하나 대규모에서 다운타임·부하 폭증.
   lazy(schema-on-read, 읽을 때 변환): 무중단이나 읽기 경로에 변환 비용·복잡도.
```

### 2.2 대표 기존 연구·시스템

- **Schema evolution 문헌(DB):** Curino et al. (2008), "Schema Evolution in Wikipedia: Toward a Web Information System Benchmark." 스키마가 실제로 얼마나 자주·어떻게 변하는지 실증. 마이그레이션의 비용·패턴 분류.
- **Protocol Buffers / FlatBuffers / Cap'n Proto** — 진화 가능한 직렬화의 산업 표준. **앞으로·뒤로 호환(forward/backward compatibility)** 규칙(필드 번호 불변, 신규 필드 optional). Winters는 `Shared/Schemas/*.fbs`(FlatBuffers)를 네트워크에 이미 사용 — 인벤토리 영속에도 동일 원리 적용 가능.
- **Event Sourcing / CQRS** — Fowler, M. (2005). 상태를 직접 저장하지 않고 **이벤트 로그**로 보관, 현재 상태는 재생(replay)으로 도출. 인벤토리에 적용하면 "획득/소비/거래" 이벤트가 진실, 잔액은 파생. 감사·시점복구·dupe 추적에 강함(§3과 연결).
- **MMO 인벤토리 사례:** WoW의 item GUID(전역 유일 인스턴스 ID), Path of Exile의 stash·거래 시스템. 산업 사례.

### 2.3 자료구조·알고리즘 (의사코드)

**대규모 저지연 일관 갱신**(open problem)의 핵심 — 인벤토리는 읽기 압도적(HUD·상점 표시)에 쓰기 정합성 필수(구매·거래). 읽기-쓰기 비대칭을 다루는 lazy-migration + 캐시 일관성:

```text
# Schema-on-read + 캐시 일관 인벤토리 읽기
function READ_INVENTORY(user_id):
    cached = redis.GET("inv:" + user_id)          # Winters: pkg/cache/redis.go
    if cached and cached.version == CURRENT_VERSION:
        return cached                              # fast path (대부분의 읽기)
    rows = db.QUERY("SELECT ... FROM inventory WHERE user_id=$1", user_id)
    inv  = []
    for row in rows:
        item = MIGRATE_TO_CURRENT(row)             # lazy: 필요한 것만 변환
        if row.version < CURRENT_VERSION:
            db.WRITE_BACK(user_id, item)           # 선택: 점진적 영속(write-on-read)
        inv.append(item)
    redis.SET("inv:" + user_id, inv, ttl)
    return inv

# 쓰기 시 캐시 무효화(일관성): 쓰기는 DB 트랜잭션 후 캐시 invalidate
function WRITE_INVENTORY_TX(user_id, mutation):
    db.TRANSACTION:                                # Winters shop.Purchase 패턴
        apply(mutation)                            # INSERT/UPDATE ... ON CONFLICT
    redis.DEL("inv:" + user_id)                    # invalidate (write-through 대신 invalidate)
    publish_event(InventoryChanged{user_id})       # Kafka: 다른 노드/세션에 전파
```

### 2.4 박사급 novel 각도 (open problems)

1. **대규모 인벤토리의 무중단 증분 마이그레이션.** 수억 항목을 다운타임 없이, p99 읽기 지연을 유지하며 마이그레이션. 박사 각도: lazy schema-on-read + 백그라운드 점진 마이그레이션 + **마이그레이션 중 일관성 불변식 증명**(과거/현재 버전 혼재 상태에서도 읽기가 정확). 기여 = 알고리즘 + 정합성 증명 + 지연 SLA 유지 실증.
2. **읽기-지배 인벤토리의 캐시 일관성.** 인벤토리는 여러 세션(게임 클라, 웹, 모바일)에서 동시 읽힌다. 캐시 무효화의 정확성(stale read로 인한 dupe UI)이 문제. 박사 각도: 게임 세션 특화 캐시 일관성 프로토콜(읽기 다수, 쓰기 희소, 단기 stale 허용 vs 거래 시 강일관 필요의 혼합).
3. **Template-instance 분리의 형식 모델.** 정의 변경(밸런스 패치)과 인스턴스 보존의 정합성을 타입 시스템·불변식으로 명세. 특히 "정의가 사라진 아이템(deprecated item)"을 보유한 인스턴스의 안전한 처리.

### 2.5 Thesis statement 예시

```text
"보유 인스턴스에 스키마 버전을 박고 schema-on-read 변환과 백그라운드 증분 마이그레이션을
 결합한 인벤토리 저장 모델은, 수억 항목 규모에서 무중단으로 스키마를 진화시키면서
 혼재 상태에서의 읽기 정합성을 보장하고(불변식 I로 명세·검증) p99 읽기 지연을 Y ms 이내로 유지한다."
```

검증: baseline = eager 일괄 마이그레이션(다운타임). metric = 마이그레이션 중 p99 읽기 지연, 처리량 저하율, 불변식 위반 0건(형식 검증 + 부하 테스트), 마이그레이션 완료 시간.

### 2.6 평가 방법

- **부하 하 지연:** 읽기-지배 워크로드(예: 95% 읽기) 동안 마이그레이션 실행, p50/p99 읽기 지연 추적.
- **정합성:** 혼재 버전 상태에서 무작위 읽기 정확성, 불변식 위반 카운트(형식 검증 + 런타임 assert).
- **확장성:** 항목 수·유저 수 대비 마이그레이션 시간·메모리 곡선.
- **Threats to validity:** 워크로드 편향(읽기/쓰기 비율), 특정 스키마 변경 종류에의 과적합.

### 2.7 Winters 연결점

- **두 모델이 공존하는 testbed:** 인게임 `InventoryComponent.itemIds[kMaxSlots]`(슬롯 배열, 인스턴스 상태 거의 없음)와 메타 `inventory` 테이블(항목 집합, 관계형). 두 모델의 동시 운영·변환은 그 자체로 연구 표면.
- **template/instance 현황:** 인게임은 `CItemDef` 하드코딩(`ItemDef.h`)이라 *instance가 거의 비어 있음* — 강화·소켓 같은 인스턴스 상태를 도입하면 마이그레이션 문제가 비로소 발생(연구 진입점).
- **마이그레이션 인프라:** FlatBuffers(`Shared/Schemas`)가 네트워크 진화에 이미 쓰이므로, 영속 인벤토리 직렬화에 같은 forward/backward 호환 규칙을 testbed로 재사용 가능.
- **캐시:** `pkg/cache/redis.go`가 이미 존재 — 읽기-지배 인벤토리 캐시 일관성 실험의 기반.

---

## 3. 거래/교환과 트랜잭션 정합성 (Exactly-once·아이템 복제 버그)

이 절이 본 문서의 **심장**이다. 아이템 복제(dupe) 버그는 게임 백엔드 연구가 일반 OLTP와 가장 날카롭게 만나는 지점이다.

### 3.1 핵심 원리

**거래(trade)의 정의:** 두 당사자 A, B 사이에서 *원자적으로* 자산이 교환되어야 한다. A가 아이템 X를 잃고 B가 X를 얻는 일은 **전부 일어나거나 전부 일어나지 않아야** 한다(ACID의 Atomicity). 부분 실패 = A는 잃었는데 B는 못 얻음(소실) 또는 A는 안 잃었는데 B가 얻음(**복제, dupe**).

**ACID를 게임 거래에 박으면:**
- **Atomicity:** 거래는 불가분. 중간 크래시 시 전부 롤백.
- **Consistency:** 거래 전후로 **불변식**이 유지 — 가장 중요한 것이 **"전체 아이템 수 보존"**(거래는 소유자만 바꿀 뿐 총량 불변). dupe = 이 불변식 위반.
- **Isolation:** 동시 거래가 서로 간섭하지 않음. dupe의 단골 원인은 격리 부족(같은 아이템을 두 거래가 동시에 읽고 둘 다 성공).
- **Durability:** 커밋된 거래는 크래시에도 살아남음.

**아이템 복제(dupe)의 두 근본 원인:**

```text
원인 1 — 경쟁(race / 격리 부족):
   거래 T1, T2가 동시에 아이템 X(소유자 A)를 읽는다.
   T1: A가 X를 B에게 → OK (A still "has" X in T2's snapshot)
   T2: A가 X를 C에게 → OK (격리 안 됐으면 둘 다 검증 통과)
   결과: X가 B와 C 둘 다에게 존재 → 1개가 2개로 복제.
   방어: 직렬화 가능 격리(serializable) 또는 비관적 락(SELECT ... FOR UPDATE).
        Winters shop.Purchase는 wallet에 FOR UPDATE를 건다(잔액 race 방지) — 올바른 패턴.

원인 2 — 부분 실패(partial failure / 비원자성):
   분산 거래: A의 인벤토리(노드1)에서 빼고, B의 인벤토리(노드2)에 더한다.
   노드1 커밋 성공 → 노드2 커밋 전 크래시 → A는 잃고 B는 못 얻음(소실).
   또는 재시도가 멱등하지 않으면: 빼기는 1번, 더하기 재시도로 2번 → 복제.
   방어: 분산 원자성(2PC) 또는 Saga + 멱등 + exactly-once.
```

**Exactly-once의 진실:** 네트워크에서 "정확히 한 번 전달"은 **불가능**하다(FLP/Two Generals). 가능한 것은 **at-least-once 전달 + 멱등 처리 = exactly-once 효과(effectively-once)**다. 즉 메시지는 여러 번 와도, **처리는 멱등키(idempotency key)로 중복 제거**해 결과가 한 번 적용된 것과 같게 만든다. Winters `payment.Charge`가 `FindByIdempotencyKey`로 선조회하는 것이 바로 이 패턴이다(`payment/service.go:42`).

### 3.2 대표 기존 연구·시스템

- **Gray, J. (1981), "The Transaction Concept: Virtues and Limitations"** (VLDB). 트랜잭션·ACID 개념의 정초. 모든 거래 정합성 논의의 출발점.
- **Garcia-Molina, H. & Salem, K. (1987), "Sagas"** (SIGMOD). **장기 실행 트랜잭션을 보상 트랜잭션(compensating transaction) 시퀀스로 분해.** 분산 거래에서 2PC의 블로킹·가용성 문제를 피하는 표준. 게임 거래(다단계·다서비스)에 직접 적합. *이 분야 필수 인용.*
- **Two-Phase Commit (2PC)** — Gray (1978), Lampson & Sturgis. 분산 원자 커밋의 고전. 강하지만 coordinator 장애 시 블로킹 → 고가용 게임 백엔드에서 기피되는 이유.
- **Bernstein, Hadzilacos & Goodman (1987), "Concurrency Control and Recovery in Database Systems."** 동시성 제어(2PL, MVCC)·복구의 표준 교과서. dupe의 격리 원인을 이해하는 토대.
- **Event Sourcing + Audit Log** — Young, G. / Fowler. 모든 거래를 불변 이벤트 로그로 → **dupe 사후 추적·롤백·감사**의 기반. 게임 운영에서 "어떻게 복제됐나"를 추적하는 유일한 실용적 방법.
- **형식 검증 사례:** Newcombe et al. (2015), "How Amazon Web Services Uses Formal Methods" (CACM). **TLA+로 분산 프로토콜의 미묘한 버그를 사전 발견.** 게임 거래 불변식 검증의 직접 방법론적 선례. *§3.4의 핵심 근거.*

### 3.3 자료구조·알고리즘·프로토콜 (의사코드)

**Saga 기반 분산 거래(2PC 회피, 게임에 적합):**

```text
# 거래 Saga: A의 아이템 X ↔ B의 골드 G (서로 다른 샤드/서비스 가능)
# 각 단계는 멱등(idempotency_key=trade_id 사용), 실패 시 보상으로 롤백
function EXECUTE_TRADE_SAGA(trade_id, A, B, item_X, gold_G):
    log(trade_id, "STARTED")                         # 감사로그(이벤트 소싱)
    try:
        s1 = ESCROW_ITEM(trade_id, A, item_X)        # 1. A의 X를 에스크로(잠금)로 이동
        s2 = ESCROW_GOLD(trade_id, B, gold_G)        # 2. B의 G를 에스크로로 이동
        s3 = GIVE_ITEM(trade_id, B, item_X)          # 3. 에스크로 X → B
        s4 = GIVE_GOLD(trade_id, A, gold_G)          # 4. 에스크로 G → A
        log(trade_id, "COMMITTED")
    catch failure at step k:
        # 보상 트랜잭션을 역순으로 — 각 보상도 멱등
        COMPENSATE(steps[k-1 .. 1])                  # 예: 에스크로 환원
        log(trade_id, "ABORTED", reason)

# 멱등 단계의 핵심: 같은 (trade_id, step)이 두 번 와도 한 번만 적용
function ESCROW_ITEM(trade_id, owner, item):
    db.TRANSACTION:
        if EXISTS(saga_steps WHERE trade_id=trade_id AND step='ESCROW_ITEM'):
            return                                   # 이미 적용됨 → no-op (exactly-once 효과)
        ASSERT(owner still owns item)                # 격리: FOR UPDATE로 잠금
        MOVE item: owner → escrow(trade_id)
        INSERT saga_steps(trade_id, 'ESCROW_ITEM', applied_at)   # 멱등 마커
```

**불변식 명세(형식 검증 대상):**

```text
# 거래 시스템이 절대 위반하면 안 되는 안전성 불변식(safety invariants)
INVARIANT NoDupe:
    ∀ item_instance i: COUNT(owners of i across all locations incl. escrow) == 1
    # 어떤 아이템 인스턴스도 동시에 두 곳(또는 두 소유자)에 존재하지 않는다.

INVARIANT Conservation:
    ∀ trade T: Σ(assets before T) == Σ(assets after T)
    # 거래는 소유자만 바꾼다. 총량은 보존된다(생성/소멸은 faucet/sink만).

INVARIANT NoLostAsset:
    ∀ asset a in escrow(trade_id): eventually a ∈ {A's inv, B's inv}  (liveness)
    # 에스크로에 들어간 자산은 결국 어느 한쪽에 귀속된다(영구 소실 없음).

# OPEN PROBLEM: 위 불변식을 TLA+ 등으로 명세하고, Saga + 멱등 + 격리 프로토콜이
#   모든 인터리빙·부분실패에서 NoDupe ∧ Conservation을 보존함을 model checking으로 증명.
```

### 3.4 박사급 novel 각도 (open problems)

1. **분산 환경 exactly-once 거래의 검증된 프로토콜.** 샤딩된 게임 백엔드에서 두 유저가 다른 샤드에 있을 때의 거래. 박사 각도: Saga + 멱등 + 게임 특화 격리를 결합한 프로토콜을 설계하고, **NoDupe·Conservation을 TLA+로 명세·model checking으로 증명**(AWS의 Newcombe 2015 방법론을 게임 거래에 적용). 기여 = 프로토콜 + 형식 증명 + 실측 처리량/지연.
2. **자동 dupe 불변식 검증(형식·런타임 혼합).** 코드 변경마다 거래 경로의 불변식 위반을 자동 탐지. 박사 각도: (a) 정적 — 거래 코드에서 불변식 위반 가능 경로를 model checking으로, (b) 동적 — 라이브에서 NoDupe를 연속 감사(audit)하는 저비용 불변식 모니터(아이템 인스턴스 카운트의 분산 집계). 기여 = "게임 경제 불변식의 형식+런타임 검증 프레임워크."
3. **TOCTOU 멱등의 정확성.** Winters `payment.Charge`는 `FindByIdempotencyKey`(선조회) → 없으면 `ProcessCharge`(INSERT). 두 동시 요청이 같은 키로 동시에 선조회하면 **둘 다 "없음"을 보고 둘 다 INSERT를 시도**할 수 있다(TOCTOU 창). 안전성은 결국 `payment_transactions.idempotency_key`의 **UNIQUE 제약**(DB가 두 번째 INSERT를 거부)에 의존한다 — 그런데 `ErrIdempotencyConflict`(`payment/handler.go:75`)가 그 충돌을 다루는 것을 보면 설계자도 인지하고 있다. 박사 각도: "선조회+제약" 패턴의 정확성을 **형식적으로** 보이고, 충돌 시 재조회로 *최초 결과를 반환*하는 것이 멱등 의미론을 보존함을 증명. (이것이 구현 디테일을 연구 명제로 끌어올리는 정확한 예다.)

### 3.5 Thesis statement 예시

```text
"샤딩된 게임 백엔드의 유저 간 거래를 Saga와 멱등 단계로 구성하고, 거래 불변식
 NoDupe(모든 아이템 인스턴스의 전역 소유자 수 = 1)와 Conservation(거래 전후 자산 총량 보존)을
 TLA+로 명세해 model checking으로 검증하면, 2PC 대비 가용성을 유지하면서도
 모든 부분실패·동시성 인터리빙에서 dupe-freedom을 보장하고 처리량을 T TPS로 달성할 수 있다."
```

검증: baseline = 2PC(가용성 비교), naive(격리 없는 다단계). metric = **dupe 사건 0건(형식 검증 + fault-injection 부하 테스트)**, 처리량(TPS), p99 거래 지연, coordinator 장애 시 가용성.

### 3.6 평가 방법

- **형식 검증:** TLA+/Apalache로 불변식을 모든 상태에서 검사(유한 모델). 위반 카운트 = 0 목표.
- **Fault injection:** 거래 각 단계 사이에 크래시·메시지 중복·재정렬·지연을 주입(Jepsen 스타일)하고 NoDupe·Conservation을 사후 감사.
- **부하/처리량:** 동시 거래 수 대비 TPS, p50/p99 지연, abort율.
- **재현성:** 정확한 인터리빙 시드·장애 스케줄 공개(아티팩트).
- **Threats to validity:** 형식 모델과 구현의 괴리(model-code gap), 유한 모델의 상태 폭발로 인한 부분 검증.

### 3.7 Winters 연결점

- **이미 올바른 패턴들(기여 아님, 그러나 testbed로 강력):** `shop.Purchase`의 `SELECT balance ... FOR UPDATE`(격리 — dupe 원인1 방어), `INSERT ... ON CONFLICT DO UPDATE`(upsert 멱등성), `payment.Charge`의 `FindByIdempotencyKey`(exactly-once 효과), `coin_transactions` 테이블(감사 ledger — 사후 추적). 이건 단일 DB 단일 트랜잭션이라 ACID가 *공짜로* 성립한다.
- **연구가 시작되는 경계:** 위 보장은 **단일 PostgreSQL 안에서만** 성립한다. (a) 인게임 경제(`HandleBuyItem`, ECS 틱, 멱등키·감사로그 없음 — 틱 결정론에 의존)와 (b) 메타 경제(Go/SQL)를 **건너는 거래**, 또는 (c) **샤딩** 후 거래는 전부 분산 exactly-once 문제로 진입한다. 이 경계가 §3.4의 박사 표면.
- **인게임 거래의 부재가 기회:** Winters 인게임에는 아직 *유저 간 아이템 거래*가 없다(`HandleBuyItem`은 NPC 상점 구매뿐). 유저 간 거래를 도입하는 순간 dupe 문제가 발생 — 연구를 *처음부터 형식 명세 위에* 세울 수 있는 깨끗한 testbed.
- **감사 인프라:** Kafka `TopicPaymentEvents`(`payment/service.go`)와 `coin_transactions`가 event-sourcing 스타일 감사의 기반 — §3.4의 런타임 불변식 모니터를 여기에 얹을 수 있다.

---

## 4. 확장성·일관성·부정탐지 (분산 시스템)

### 4.1 핵심 원리

게임 백엔드가 수백만 동시접속(CCU)으로 가면 단일 DB로는 불가능하다 → **샤딩(sharding)**. 샤딩은 즉시 **CAP 정리**와 충돌한다.

**CAP 정리(Brewer):** 네트워크 분할(Partition)이 있을 때, 시스템은 **일관성(Consistency)**과 **가용성(Availability)** 중 하나만 택할 수 있다. 게임 백엔드의 핵심 통찰은 **데이터마다 다른 선택을 한다는 것**:

```text
강일관성(CP) 필요 — 돈·소유권이 걸린 곳:
   wallet 잔액, 아이템 소유권, 거래.  → dupe/소실은 치명적이라 일관성 우선.
   Winters: shop.Purchase(FOR UPDATE), payment(idempotency) — CP 성향.

가용성·결과적 일관성(AP) 허용 — 표시·통계:
   리더보드 랭킹, 친구 온라인 상태, 프로필 통계.  → 잠깐 stale해도 무방.
   Winters: leaderboard(Kafka consumer로 비동기 갱신 — leaderboard/consumer.go) — AP 성향.
```

**샤딩 키 선택**이 거래 정합성을 좌우한다. user_id로 샤딩하면 *같은 유저의* wallet+inventory는 한 샤드(거래 내 단일 트랜잭션 가능). 그러나 *서로 다른 유저 간* 거래는 **샤드를 건넌다**(→ §3의 분산 거래 문제). "거래 당사자를 같은 샤드에 두는가"는 일관성↔확장성 트레이드오프의 핵심 노브.

**멱등 재시도(idempotent retry):** 분산 시스템에서 네트워크 타임아웃 시 재시도는 필수인데, 멱등하지 않으면 dupe를 만든다. 모든 상태 변경 연산에 멱등키를 박는 것이 분산 정합성의 기본기(§3.1과 동일 원리).

**부정탐지(fraud/bot detection):** 게임 경제의 적대자는 (1) **봇/매크로**(자동 파밍 → 인플레), (2) **RMT**(현금 거래 → 경제·약관 위반), (3) **사기**(거래 사기, 결제 사기/차지백). 룰 기반("시간당 X회 이상 거래 차단")은 단순하나 우회되고 오탐이 많다. **그래프 기반·ML 기반**이 SOTA 방향: 거래·계정 관계를 그래프로 보면 RMT/사기 네트워크가 **구조적 이상**(밀집 부분그래프, 자금 세탁 경로, 동기화된 행동)으로 드러난다.

### 4.2 대표 기존 연구·시스템

- **Brewer, E. (2000), CAP conjecture** + **Gilbert & Lynch (2002)** 형식 증명. 분산 일관성↔가용성 트레이드오프의 정초.
- **DeCandia et al. (2007), "Dynamo: Amazon's Highly Available Key-value Store"** (SOSP). eventual consistency·해시 샤딩·hinted handoff의 산업·학술 정초. 게임 백엔드 NoSQL 설계의 원류.
- **Corbett et al. (2012), "Spanner: Google's Globally-Distributed Database"** (OSDI). TrueTime으로 **전역 강일관성(external consistency)**을 확장 가능하게. "강일관성 vs 확장성은 양자택일이 아닐 수 있다"의 반례 — 게임 거래에 직접 시사.
- **Akkio (Annamalai et al., 2018, OSDI)** — 데이터 지역성(locality)을 동적으로 이동. 거래 당사자/핫 데이터를 같은 샤드로 모으는 아이디어의 시스템적 선례.
- **그래프 기반 사기탐지:** Akoglu et al. (2015), "Graph-based Anomaly Detection and Description: A Survey." 그래프 이상탐지 종합. **Hooi et al. (2016), "FRAUDAR: Bounding Graph Fraud in the Face of Camouflage"** (KDD) — 위장에도 견고한 밀집 부분그래프 탐지. 게임 RMT 네트워크 탐지에 직접 적용 가능.
- **게임 봇 탐지 학술:** Chen et al., "Identifying MMORPG Bots: A Traffic Analysis Approach" / 거래 네트워크·행동 시퀀스 기반 봇 탐지 다수. (→ `11_Security`와 강하게 연결.)

### 4.3 자료구조·알고리즘·프로토콜 (의사코드)

**그래프 기반 RMT/사기 네트워크 탐지**(open problem 중 가장 ML·그래프 색채):

```text
# 거래 그래프에서 RMT/사기 네트워크 탐지
# 노드 = 계정, 엣지 = 거래/재화 이동(가중치=금액, 시각), 특징 = 행동 프로파일
function DETECT_FRAUD_NETWORK(transaction_log, window):
    G = BUILD_GRAPH(transaction_log within window)    # 계정-계정 거래 그래프
    # 신호 1: 구조 — 한쪽으로만 흐르는 밀집 부분그래프(자금 집중 = RMT 환금 패턴)
    dense = FRAUDAR(G)                                 # 위장에 견고한 밀집 블록 탐지
    # 신호 2: 행동 — 비인간적 일관성(파밍 봇), 동기화된 타이밍(봇넷)
    embeddings = NODE2VEC(G) ⊕ behavior_features(account)   # 구조+행동 결합
    suspects   = CLASSIFIER(embeddings)                # 라벨(과거 제재)로 지도학습
    # 신호 3: 흐름 — source(봇 파밍) → sink(환금 계정) 경로 추적
    flows = TRACE_VALUE_FLOW(G, sources=farming_accounts)
    return RANK(dense ∪ suspects, by=economic_loss)    # 경제적 손실 기준 우선순위

# OPEN PROBLEM:
#  - 위장(camouflage: 정상 거래 섞기)·적응형 적대자에 견고한 탐지
#  - 라벨 희소(제재 데이터 적음) 하에서의 준지도/이상탐지
#  - 실시간성(거래 즉시 vs 배치)과 탐지 정확도의 트레이드오프
```

**일관성↔확장성 트레이드오프의 게임 특화 측정 골격:**

```text
# 같은 워크로드를 일관성 수준별로 돌려 게임 특화 최적점 탐색
for consistency_level in [serializable, snapshot, causal, eventual]:
    for shard_strategy in [user_hash, locality_aware(거래 당사자 동거)]:
        deploy(consistency_level, shard_strategy)
        measure:
            throughput_TPS, p99_latency
            dupe_incidents          # 약한 일관성에서 증가하는가
            cross_shard_trade_ratio # locality 전략이 줄이는가
        plot(scalability_curve vs CCU)
# 기여: "게임 거래 워크로드에서 dupe-free를 유지하는 최약 일관성 수준"의 정량적 경계
```

### 4.4 박사급 novel 각도 (open problems)

1. **강일관성↔확장성의 게임 특화 최적점.** 일반 DB 연구는 "serializable은 비싸다"를 안다. 박사 각도: **게임 거래 워크로드의 특성**(읽기 압도적, 거래는 희소하지만 dupe-free 필수, 대부분 같은 유저 내 연산)을 활용해, "dupe-freedom을 보장하는 **최약(weakest)** 일관성 수준"을 정량적으로 규명하고, 그 수준에서 locality-aware 샤딩으로 확장성을 회복. 기여 = 워크로드 특화 일관성 경계 + 실증.
2. **그래프 기반 적응형 RMT/사기 탐지.** 박사 각도: 위장·적응형 적대자에 견고하고, 라벨이 희소한 게임 환경에서 동작하는 거래 그래프 탐지. 핵심은 **경제적 손실로 임계를 정당화**(precision/recall을 운영 비용·인플레 손실로 환산)하고, 적대자 적응을 게임이론적으로 모델링. 기여 = 탐지 방법 + 경제 손실 기반 평가 프레임.
3. **부정탐지와 경제 모델의 결합.** §1의 경제 시뮬레이터와 §4의 탐지를 결합 — 봇이 경제에 미치는 영향을 시뮬레이션해 탐지의 *경제적 ROI*를 정량화. 기여 = "탐지 정확도"를 넘어 "탐지가 경제 건강에 주는 효과"라는 새 평가축.

### 4.5 Thesis statement 예시

```text
"게임 거래 워크로드(읽기 지배·희소한 거래·유저 내 지역성)에서는 serializable 전체를 강제하지 않고
 거래 경로에만 인과적 일관성+거래 당사자 동거 샤딩(locality-aware sharding)을 적용해도
 dupe-freedom을 보존할 수 있으며, 이 구성은 serializable 단일 DB 대비 CCU 확장성을 S배 높이면서
 p99 거래 지연을 유지한다."
```

또는 부정탐지 버전:

```text
"계정-거래 그래프의 구조적 임베딩과 행동 특징을 결합한 탐지는, 정상 거래로 위장한
 RMT 네트워크에 대해 룰 기반 대비 precision/recall을 z만큼 개선하며, 탐지 임계를
 경제적 손실로 정당화할 때 인플레 손실을 W% 저감한다."
```

검증: baseline = serializable 단일 DB / 룰 기반 탐지. metric = TPS·p99·dupe 사건 / precision·recall·F1·경제 손실 저감·적대자 적응 후 견고성.

### 4.6 평가 방법

- **확장성:** CCU·샤드 수 대비 TPS·지연 곡선(weak/strong scaling). cross-shard 거래 비율.
- **일관성:** 약한 일관성 수준에서 dupe 사건 수(Jepsen-style 분할 주입 하).
- **부정탐지:** 라벨된 제재 데이터로 precision/recall/F1, ROC-AUC. **적대자 적응 실험**(위장 거래 주입 후 성능 저하). 경제 손실 환산.
- **재현성:** 거래 그래프 데이터셋·샤딩 토폴로지·장애 스케줄 공개.
- **Threats to validity:** 라벨 편향(제재된 것만 양성), 워크로드 일반화, 적대자 모델의 현실성, 합성 데이터 vs 라이브.

### 4.7 Winters 연결점

- **CP/AP 분리가 이미 코드에 있다:** 거래·잔액은 PostgreSQL 트랜잭션(CP, `shop`/`payment`), 리더보드는 Kafka consumer 비동기 갱신(AP, `leaderboard/consumer.go`). 이건 §4.1의 "데이터마다 다른 CAP 선택"의 실재 예 — 측정 testbed로 쓸 수 있다.
- **샤딩 부재가 연구 진입점:** Winters는 아직 단일 DB(`pkg/database/postgres.go`)다. 샤딩을 도입하는 순간 §4.4의 일관성↔확장성·cross-shard 거래 문제가 생긴다 — 처음부터 측정 설계 위에 세울 기회.
- **탐지 데이터 소스:** Kafka `TopicPlayerEvents`/`TopicPaymentEvents`와 `coin_transactions` ledger가 **거래 그래프 구축의 원천 데이터**. `matchmaking`/`profile`의 계정 관계까지 합치면 RMT 그래프 탐지 testbed가 된다.
- **인게임↔메타 적대자 모델:** MOBA는 인게임(매크로·스크립트로 분당 골드 조작 시도)과 메타(RMT·결제 사기) 양쪽에 적대자가 있어, 두 시간척도의 부정탐지를 한 testbed에서 다룰 수 있다(→ `11_Security` 보안과 직결).

---

## 종합. 통합 학위논문 구조 예시

"Three Papers Make a Thesis"(가이드 §4) 모델로, **하나의 명제** 아래 인접한 세 기여를 묶는다.

> **통합 Thesis Statement:**
> "게임 경제의 거래 정합성은 거래 불변식(NoDupe·Conservation)의 **형식 명세**를 중심에 두고,
> 그 위에 (1) dupe-free를 보장하는 **최약 일관성·locality 샤딩**, (2) 불변식을 **형식+런타임으로 검증**하는
> 거래 프로토콜, (3) 불변식을 우회하는 적대자를 **거래 그래프로 탐지**하는 방어를 쌓으면,
> 강일관성 단일 DB 대비 확장성을 높이면서도 검증된 dupe-freedom과 경제 건강을 동시에 달성할 수 있다."

```text
Ch 1. 서론
   - 동기: 게임 경제의 dupe·인플레·RMT는 영구적·전파적 피해 (그래픽스 버그와 다름)
   - 문제: 강일관성·확장성·dupe-free·부정탐지의 사중 제약을 게임 워크로드에서 동시 달성
   - Thesis Statement(위)
   - 기여 3+1 (불변식 형식화 / 검증된 거래 프로토콜 / 그래프 탐지)

Ch 2. 배경·관련연구
   - ACID·격리·CAP(Gray 1981, Gilbert-Lynch 2002)
   - Saga(Garcia-Molina & Salem 1987), 2PC, Dynamo/Spanner
   - 형식검증(Newcombe 2015 TLA+), 그래프 사기탐지(FRAUDAR 2016)
   - gap: 위 기법들은 일반 OLTP용. 게임 워크로드 특성(읽기지배·희소거래·적대자 상주)에
          맞춘 "검증된 dupe-free 확장 거래 + 경제 건강"은 미해결.

Ch 3. [기여 1 / VLDB·SIGMOD급] 거래 불변식의 형식 명세와 최약 일관성
   - NoDupe·Conservation을 TLA+로 명세
   - 게임 워크로드에서 dupe-free를 보존하는 최약 일관성 수준 규명 + locality 샤딩
   - 평가: TPS·p99·dupe사건·CCU 확장성 (baseline: serializable 단일 DB)

Ch 4. [기여 2 / SOSP·OSDI급] 검증된 exactly-once 거래 프로토콜
   - Saga + 멱등 + 게임 격리, model checking으로 NoDupe∧Conservation 증명
   - 런타임 불변식 모니터(저비용 연속 감사)
   - 평가: fault injection(Jepsen) 하 dupe 0건, 2PC 대비 가용성

Ch 5. [기여 3 / KDD·NSDI급] 거래 그래프 기반 적응형 부정탐지
   - 구조 임베딩+행동 특징, 위장·라벨희소에 견고, 경제손실 기반 임계
   - 평가: precision/recall, 적대자 적응 견고성, 인플레 손실 저감

Ch 6. 종합평가 / Ch 7. 논의(threats) / Ch 8. 결론·향후연구
부록: TLA+ 명세 전문, 거래 그래프 데이터셋, fault-injection 스케줄(재현성)
```

> **자가진단(가이드 §12):** 이 학위논문의 thesis는 한 문장으로 말할 수 있고(위), 그것을 증명하는 실험은 명확하며(형식검증+fault injection+탐지 평가), 무엇과 비교하는지(serializable 단일 DB, 2PC, 룰 기반)도 분명하다. → 박사 단계의 요건을 만족.

---

## 참고문헌

**트랜잭션·일관성·분산 DB**
- Gray, J. (1981). *The Transaction Concept: Virtues and Limitations.* VLDB.
- Garcia-Molina, H., & Salem, K. (1987). *Sagas.* SIGMOD.
- Bernstein, P., Hadzilacos, V., & Goodman, N. (1987). *Concurrency Control and Recovery in Database Systems.* Addison-Wesley.
- Gilbert, S., & Lynch, N. (2002). *Brewer's Conjecture and the Feasibility of Consistent, Available, Partition-Tolerant Web Services.* ACM SIGACT News (CAP 형식 증명).
- DeCandia, G., et al. (2007). *Dynamo: Amazon's Highly Available Key-value Store.* SOSP.
- Corbett, J., et al. (2012). *Spanner: Google's Globally-Distributed Database.* OSDI.
- Annamalai, M., et al. (2018). *Sharding the Shards: Managing Datastore Locality at Scale with Akkio.* OSDI.

**형식 검증·정합성**
- Newcombe, C., et al. (2015). *How Amazon Web Services Uses Formal Methods.* Communications of the ACM (TLA+).
- Fowler, M. (2005). *Event Sourcing* / CQRS (industry, methodological reference).

**스키마 진화·직렬화**
- Curino, C., et al. (2008). *Schema Evolution in Wikipedia: Toward a Web Information System Benchmark.*
- Protocol Buffers / FlatBuffers / Cap'n Proto (forward/backward compatibility, industry standards).

**가상 경제**
- Castronova, E. (2001). *Virtual Worlds: A First-Hand Account of Market and Society on the Cyberian Frontier.* CESifo Working Paper.
- Lehdonvirta, V., & Castronova, E. (2014). *Virtual Economies: Design and Analysis.* MIT Press.
- Bonabeau, E. (2002). *Agent-based Modeling: Methods and Techniques for Simulating Human Systems.* PNAS.
- CCP Games, *EVE Online Monthly Economic Report* (industry, motivation only).

**부정탐지·그래프 이상탐지**
- Akoglu, L., Tong, H., & Koutra, D. (2015). *Graph-based Anomaly Detection and Description: A Survey.* Data Mining and Knowledge Discovery.
- Hooi, B., et al. (2016). *FRAUDAR: Bounding Graph Fraud in the Face of Camouflage.* KDD.
- Grover, A., & Leskovec, J. (2016). *node2vec: Scalable Feature Learning for Networks.* KDD.

**테스트·내결함성 방법론**
- Kingsbury, K. *Jepsen* (분산 시스템 일관성 fault-injection 테스트, industry/방법론).

> **인용 원칙(가이드 §10):** 위 문헌 중 연도·저자가 확실한 학술 출판만 정식 인용했다. EVE/Diablo III/Path of Exile, Jepsen, Fowler 등은 **산업·방법론 참고**로 명시 표기하며 학술 기여 근거로는 쓰지 않는다. 박사 본문에서 모든 정량 주장은 (a) 위 인용 (b) 본인 실험 (c) 자명함 중 하나로 뒷받침해야 한다.
