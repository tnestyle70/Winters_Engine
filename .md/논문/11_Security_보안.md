# 11. Security·안티치트(방어 관점) — 박사 연구 심화

> 전제 문서: [`00_PHD_Paper_Guide.md`](00_PHD_Paper_Guide.md). 본 문서는 가이드 §1(구현 vs 기여), §3(thesis statement), §4(구조), §7(평가: 탐지율·오탐율 FPR, ROC/AUC, 우회 비용, 오버헤드, 프라이버시), §10(연구 윤리·dual-use·responsible disclosure)을 **그대로 전제**한다. 모든 세부주제마다 "이건 구현 항목인가, 기여 후보인가?"를 가이드 §1로 되돌아가 묻는다.
>
> 독자: LoL 스타일 MOBA + 오픈월드 엔진 'Winters'를 만든 숙련 C++ 엔진 개발자. 서버는 server-authoritative GameSim에서 gameplay truth를 소유하며, lockstep/replay 결정론 권위를 지향한다.
>
> Top venue: **IEEE S&P (Oakland), USENIX Security, ACM CCS, NDSS** (저널 **IEEE TDSC, IEEE TIFS**). 산업 참고(비학술): DEF CON, Black Hat.
>
> **★ 윤리 전제 (가이드 §10, dual-use):** 본 문서는 **철저히 방어·탐지 연구(defensive / detection research)** 관점이다. 실제 치트 제작·우회의 실행 가능한 방법(specific bypass recipe)은 **쓰지 않는다.** 위협 모델은 **방어 설계를 정당화하는 수준에서 추상적으로만** 기술한다. 모든 기여는 **책임 있는 공개(responsible disclosure)** 와 **플레이어 프라이버시 윤리**를 명시적 제약으로 둔다. 이는 학술적 의무이자(IEEE S&P/USENIX의 ethics review 통과 요건), 본 testbed(자작 엔진 Winters)에서만 실험하므로 외부 시스템에 무해함을 보장한다.

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue; dual-use 윤리·responsible disclosure)

보안·안티치트는 "엔진 박사"가 가장 **위험하게 미끄러지는** 분야다. 두 방향의 미끄럼이 있다.

1. **구현 자랑으로 미끄러짐:** "나는 패킹/난독화/무결성 검사를 붙였다"는 **엔지니어링**이다. 안티치트 SDK(EAC/BattlEye/Vanguard)를 통합하는 것은 박사 기여가 아니다 — SDK를 **쓰는 것**과 **방어 메커니즘의 원리·한계를 새로 밝히는 것**은 다르다.
2. **윤리로 미끄러짐:** "치트를 더 잘 만드는 법"으로 흐르면 그 순간 박사 기여가 아니라 **무기 제작**이 된다. 보안 박사의 기여는 항상 **방어 가능성(defensibility)·탐지 가능성(detectability)·프라이버시 보존** 쪽에 있다. 이것이 가이드 §10이 안티치트를 "dual-use 특칙"으로 못 박은 이유다.

> **이 문서를 관통하는 연구 긴장(research tension) — 3중 trade-off:**
> 안티치트의 모든 설계는 **탐지력(detection power) ↔ 프라이버시·신뢰(privacy / trust) ↔ 강건성·우회비용(robustness / cost-to-evade)** 의 삼각 긴장 위에 있다.
> - 탐지력을 키우려 **클라이언트를 깊게 들여다보면**(커널/ring0) 프라이버시·안정성 논란이 폭발한다(Vanguard 논쟁).
> - 프라이버시를 지키려 **서버측만 보면**, 클라이언트 상태를 못 보니 탐지력이 떨어지고 적대적 회피(adversarial evasion)에 약하다.
> - ML로 탐지력을 키우면 **오탐(FPR)** 이 무고한 플레이어를 밴하고(제재 정당성 붕괴), 적대자가 탐지기를 **회피 학습**한다.
> 박사 기여는 이 삼각형의 **한 변을 정량적으로 밀어내는 것**이다 — "프라이버시를 지키면서 탐지력을 X만큼", "오탐 Y 이하에서 우회비용을 Z만큼". 단순히 "막았다"가 아니다.

### 0.1 구현 vs 기여 — 4개 세부주제 대조

| 세부주제 | 구현 (석사/산업) | 연구 기여 (박사) |
|---|---|---|
| 위협 모델·분류 | "치트 종류를 표로 정리했다" | "MOBA 위협을 **관측가능성(observability)·서버검증가능성** 축으로 정식화한 위협 모델과, 분야가 공유할 **벤치마크/데이터셋**을 제시(가이드 §5-6 기여 유형)" |
| 클라 무결성 | "안티치트 SDK를 붙였다 / 무결성 해시를 체크했다" | "**커널 권한 없이** 원격증명(remote attestation)으로 클라 무결성을 **프라이버시 침해 최소화**로 입증하는 프로토콜과 그 보안성 증명, 우회비용 정량화" |
| 서버측 탐지 | "이상한 입력을 서버에서 막았다" | "**적대적으로 강건한(adversarially robust)** aimbot 탐지기를, 라벨 부족 하에서 self/weak-supervision으로 학습해 ROC-AUC X·FPR Y@TPR로 달성하고, 제재 근거로 쓸 **설명가능성**을 제공" |
| 프라이버시 보존 검증 | "서버가 다 검증하니 프라이버시 안전" | "영지식 증명(ZKP)/TEE로 '치트 없음'을 **클라 데이터를 노출하지 않고** 검증하는 실용적 프로토콜과, 게임 틱 예산(16.6ms) 내 성능 달성" |

### 0.2 Top Venue 표 (보안)

| 구분 | Venue | 성격 | 이 분야 대표 채택 주제 |
|---|---|---|---|
| 시스템 보안 4대 | **IEEE S&P, USENIX Security, CCS, NDSS** | 공격·방어, 무결성, 탐지, 프로토콜 | 게임 치트 측정, 안티치트 메커니즘, 봇/공모 탐지 |
| 게임 보안 특화 | USENIX Security / CCS 내 게임 트랙, NDSS | 온라인 게임 부정행위 | bot detection, aimbot detection, server-side validation |
| 신뢰 컴퓨팅 | **IEEE S&P, CCS, USENIX Security** | TEE(SGX/TrustZone), remote attestation, ZKP 응용 | attestation 프로토콜, confidential computing |
| 머신러닝 보안 | **IEEE S&P, USENIX Security, CCS** + (NeurIPS/ICML 보안 워크숍) | adversarial ML, evasion/poisoning | 적대적 회피에 강건한 탐지 |
| 저널 | **IEEE TDSC** (Trans. Dependable & Secure Computing), **IEEE TIFS** (Trans. Information Forensics & Security) | 확장판/측정/포렌식 | 탐지 시스템 확장 평가, 포렌식 분석 |
| 산업(비학술, 인용용) | DEF CON, Black Hat | 사례·위협 근거 | 안티치트 우회 사례(방어 동기로만 인용) |

> **주의(가이드 §9):** DEF CON/Black Hat 발표는 **위협이 실재함**의 근거(motivation)로만 인용한다. 박사 기여의 1차 무대는 IEEE S&P/USENIX Security이며, 거기서는 **ethics statement·IRB·responsible disclosure**가 채택의 필수 요건이다(아래 §0.4).

### 0.3 Heilmeier 체크 (이 분야 적용)

가이드 §6의 Heilmeier Catechism을 박으면 좋은 질문이 걸러진다:
- **무엇을?** "온라인 MOBA에서 부정행위를, 플레이어 프라이버시를 해치지 않으면서, 무고한 사람을 밴하지 않고 탐지·억제한다."
- **지금 한계?** 강한 탐지는 커널 권한(프라이버시 논란)에 의존하거나 오탐이 높고 회피에 약하다. 프라이버시 보존 검증은 게임 실시간 예산에서 너무 느리다.
- **새로움?** 삼각 긴장(§0)의 한 변을 정량적으로 미는 메커니즘 — 예: 서버 권위 데이터만으로 적대적으로 강건한 탐지, 또는 커널 없이 검증 가능한 무결성.
- **누가 신경?** 모든 경쟁 온라인 게임(MOBA/FPS/RTS), e스포츠 무결성, 플랫폼(밴 정당성·법적 방어), 프라이버시 규제(GDPR) 하의 운영사.
- **어떻게 측정?** 탐지율(TPR)·오탐율(FPR)·ROC-AUC, 회피 비용(cost-to-evade), 런타임/대역폭 오버헤드, 프라이버시 누출(정보이론적·차등프라이버시 ε), 적대적 환경 하 성능 저하.

### 0.4 연구 윤리·responsible disclosure 운영 원칙 (가이드 §10 구체화)

이 분야 박사는 **방법론 장(章)에 별도 ethics 절**을 둔다. 본 문서 기여가 따르는 원칙:
- **방어 프레이밍 고정:** 모든 기여를 "탐지/방어 가능성"으로 진술한다. 우회 기법은 **위협 모델에서 추상적 능력(capability)으로만** 다루고 실행 레시피는 쓰지 않는다.
- **Testbed 격리:** 실험은 **자작 엔진 Winters에서만** 수행 → 외부 상용 안티치트·실제 플레이어에 무해. 상용 시스템(EAC/BattlEye/Vanguard)은 **접근방식 비교**로만 인용하고 역공학하지 않는다.
- **데이터 윤리(IRB):** 플레이어 행동 데이터(로그/리플레이)는 **동의·익명화·최소수집**. user study는 IRB 승인. 밴 의사결정 데이터는 차등프라이버시/집계 단위로만 공개.
- **Responsible disclosure:** 자작 시스템에서 발견한 방어 우회 취약점은 **수정 후·집계 형태로만** 보고하고, 일반화 가능한 *방어 교훈*만 공표한다(구체적 exploit PoC 비공개).
- **Dual-use 자기검열:** "이 결과가 방어보다 공격에 더 쓸모 있는가?"를 self-test. 그렇다면 진술 수위를 낮추거나 방어 대응책과 함께만 공개.

---

## 1. 위협 모델과 부정행위 분류 (Threat Model & Cheat Taxonomy) — 추상적·방어 목적

### 1.1 핵심 원리

안티치트 연구의 출발점은 **"무엇으로부터 방어하는가"를 형식화하는 것**이다. 방어 메커니즘은 위협 모델 없이는 평가 불가능하다 — 무엇을 막는지 정의해야 "막았다"를 측정한다. 부정행위는 **신뢰 경계(trust boundary)** 를 기준으로 본다: 게임은 **신뢰할 수 없는 클라이언트(untrusted client)** 와 **신뢰하는 서버(trusted server)** 로 나뉘고, 부정행위는 "클라이언트가 통제하는 영역에서 게임 규칙을 위반"하는 행위다.

방어 설계를 정당화하는 수준에서, 부정행위를 **공격자가 침해하는 보안 속성(security property)** 으로 추상 분류한다(구체적 구현법은 다루지 않음):

| 범주 | 침해하는 속성 | 방어가 기대야 할 곳 (추상) |
|---|---|---|
| **정보 노출 (information exposure)** — wallhack/maphack 류 | **기밀성(confidentiality)**: 클라가 보면 안 될 상태를 본다 | 서버가 **관련성(relevance) 밖 정보를 애초에 보내지 않음** (Area of Interest, fog-of-war culling → §10 Server) |
| **입력 자동화 (input automation)** — aimbot/botting 류 | **진정성(authenticity)**: 입력이 사람이 아니라 프로그램에서 나온다 | 행동/궤적의 **통계적 비인간성** 탐지 (§3), 입력 출처 증명(§2) |
| **상태 조작 (state manipulation)** — speedhack 류 | **무결성(integrity)**: 클라가 게임 상태/시간을 위조한다 | **서버 권위 검증** — 서버가 진실 상태를 소유, 불가능 전이 거부 (§3.1) |
| **프로토콜 악용 (protocol abuse)** | **무결성/가용성**: 잘못된/과도한 메시지로 서버·타 플레이어 교란 | 서버측 **불변식 검사·rate limiting·스키마 검증** (§3.1) |
| **공모·다중계정 (collusion / Sybil)** — win-trading, 봇 농장 | **공정성(fairness)**: 다수 계정이 결탁 | **그래프 기반** 관계 탐지 (§3.4) |

핵심 통찰: **위협의 대부분은 "클라이언트가 자기 영역에서 거짓말한다"로 환원**된다. 따라서 1차 방어선은 항상 **서버 권위(server authority)** — "클라이언트를 절대 믿지 말라(never trust the client)"가 분야의 제1원칙이다. 클라이언트 무결성(§2)은 서버가 볼 수 없는 영역(정보 노출, 입력 출처)을 보완하는 **2차 방어**다.

### 1.2 대표 기존 연구/시스템

- **Yan & Randell (2005), "A Systematic Classification of Cheating in Online Games"** — 온라인 게임 부정행위의 초기 체계적 분류. 위협 분류 연구의 출발점. (보안 속성 기반 taxonomy의 원형.)
- **Webb & Soh (2007), survey of network game cheats / Hoglund & McGraw (2007), *Exploiting Online Games*** — 위협의 실재성을 보인 초기 문헌(방어 동기로 인용; 본 문서는 공격 디테일을 다루지 않음).
- **Mönch et al., Feng et al. — MMO/네트워크 게임 트래픽 측정** — 위협 모델을 *측정 데이터*로 뒷받침하는 empirical 계열(가이드 §2-3 패턴 C).
- **상용 시스템(접근방식 비교로만 — 가이드 §9):**
  - **Easy Anti-Cheat (EAC), BattlEye** — 커널/유저모드 혼합 모니터링. 무결성 검사 중심.
  - **Riot Vanguard** — 부팅 시 로드되는 커널 드라이버(상시 상주) 접근. **프라이버시·안정성 논쟁의 대표 사례** — "탐지력 vs 신뢰"(§0 긴장)의 산업적 발현.
  - **VAC (Valve Anti-Cheat)** — 시그니처 + 서버측 통계 혼합, 지연 밴(deferred ban)으로 탐지 로직 노출 최소화.
- **Open problem(분야 공통):** **표준화된 위협 모델·벤치마크·공개 데이터셋의 부재.** 그래픽스(PSNR)·네트워킹(처리량)과 달리, 안티치트는 "탐지율을 무엇에 대해 쟀는가"의 **공통 기준선이 없다.** 각 논문이 자기 데이터로 자기 탐지기를 평가 → 비교 불가. 이 공백 자체가 강력한 박사 기여 영역(가이드 §5-6 "새 평가 방법·벤치마크").

### 1.3 메커니즘/알고리즘 (방어 관점 의사코드)

위협 모델을 **방어 정책으로 환원하는 결정 절차**. 각 입력/상태 변화를 "서버에서 검증 가능한가"로 라우팅한다(공격 코드가 아니라 **방어 라우팅 로직**):

```text
// 위협 모델 → 방어 배치 결정 (defensive routing). 공격 방법이 아니라 "어디서 막을지" 결정.
ClassifyAndDefend(observation):
    // observation = 클라가 보낸 입력/주장(claim) 또는 관측된 행동
    if observation.affects_authoritative_state:        // 위치/HP/쿨다운/시간 등
        // → 상태 조작·프로토콜 악용 범주: 1차 방어 = 서버 권위 검증
        return SERVER_AUTHORITATIVE_VALIDATION(observation)   // §3.1

    elif observation.is_input_stream:                  // 조준/이동/클릭 시퀀스
        // → 입력 자동화 범주: 서버는 결과만 보므로 통계·행동 탐지로 보완
        return BEHAVIORAL_ANOMALY_SCORING(observation)        // §3.2~3.3

    elif observation.requires_hidden_state:            // 클라가 못 봐야 할 정보 의존
        // → 정보 노출 범주: 탐지가 아니라 "애초에 안 보냄"이 정답
        return RELEVANCE_CULLING_AT_SERVER(observation)       // §10 Server(AoI/fog)

    else:
        // → 클라 내부 무결성: 서버가 직접 못 봄 → 원격증명으로 보완(2차 방어)
        return CLIENT_INTEGRITY_ATTESTATION(observation)      // §2

// 평가를 위한 위협 모델의 형식화 (벤치마크의 토대):
ThreatModel := {
    adversary_capability:  {can_modify_client_memory, can_read_client_state,
                            can_inject_input, can_forge_packets, can_collude},   // 추상 능력만
    defense_assumptions:   {server_is_trusted, network_may_be_observed,
                            client_is_fully_untrusted},
    success_metric_per_category: {TPR, FPR, cost_to_evade, overhead}             // §6
}
```

**왜 이 라우팅인가 (대학원 수준):** 방어 메커니즘의 효율은 위협을 **올바른 신뢰 경계에서 막느냐**에 달렸다. 상태 조작을 클라이언트에서 막으려 하면(클라 무결성) 끝없는 군비경쟁이지만, **서버가 진실 상태를 소유**하면 조작 자체가 무의미해진다(서버가 거부). 반대로 정보 노출은 서버가 데이터를 보냈다면 클라가 무엇을 하든 막을 수 없으니 — **탐지 문제가 아니라 정보 최소화(relevance) 설계 문제**다. 이 "범주 → 올바른 방어층" 매핑이 위협 모델의 실천적 가치다.

### 1.4 박사급 novel 각도 (open problems)

1. **표준 위협 모델·벤치마크·데이터셋(가장 강력한 공백).** 안티치트엔 ImageNet/SQuAD 같은 **공통 벤치마크가 없다.** 기여: MOBA/FPS 부정행위의 **형식적 위협 모델 + 공개 평가 데이터셋(합성+동의기반 실측)** + 표준 metric을 제시. 가이드 §5-6의 "새 평가 방법·벤치마크" — 과소평가되지만 분야를 통째로 움직이는 기여. (윤리: 데이터는 자작 testbed·동의·익명화.)
2. **위협 모델의 형식 검증.** "서버 권위 + AoI culling이 정보 노출 범주를 정말 닫는가"를 **형식적으로** 증명(어떤 hidden state도 클라에 도달 안 함). open: 게임의 풍부한 상태에서 relevance 정책의 완전성(completeness) 정의.
3. **위협의 정량적 위험도 모델.** 각 범주의 **게임플레이 영향(impact) × 탐지 난이도 × 발생 빈도**를 통합한 위험 점수로 방어 자원 배분을 최적화. open: impact를 게임 결과(승률 왜곡)로 측정하는 방법.

### 1.5 Thesis statement 예시

> "MOBA 부정행위를 **관측가능성·서버검증가능성** 두 축으로 형식화한 위협 모델과 그에 대응하는 공개 벤치마크를 구축하면, 서로 다른 탐지 기법을 **동일 기준(ROC-AUC, FPR@TPR, cost-to-evade)으로 비교**할 수 있고, 서버 권위 검증이 상태조작·프로토콜 범주를 닫음을 정량적으로 보여 클라이언트측 방어를 정보노출·입력자동화 범주로 한정할 근거를 제공한다."

(가이드 §3 형식: 새 평가 도구 X + 제약 C[방어·프라이버시] + 목표 Y[비교가능성] + 정량 Z + falsifiable: "두 탐지기가 같은 벤치마크에서 순위 역전이 재현되면 벤치마크가 유효".)

### 1.6 평가 방법

- **분류 타당성:** taxonomy가 **상호배타·전수(MECE)** 인가 — 실제 부정행위 사례를 범주에 매핑해 누락/중복 측정.
- **벤치마크 품질(기여라면):** 다양한 탐지기를 태워 **순위 안정성**, 합성 데이터와 실측의 분포 일치(domain gap), 난이도 스펙트럼.
- **위협 모델 적용성:** Winters 같은 실제 서버 권위 엔진에서 각 범주의 공격면(attack surface)이 닫히는지 case study.
- **Threats to validity:** taxonomy는 게임 장르 의존적(MOBA ≠ FPS) — 일반화 한계를 적시.

### 1.7 Winters 연결점

Winters는 **서버 권위 구조가 이미 가동**되어, §1.1의 "범주 → 방어층" 매핑이 자기 코드에서 실증된다.

- **서버 권위가 상태조작·프로토콜 범주를 닫는 1차 방어선:** [`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`](../../Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp)가 모든 클라 입력을 `GameCommand`로 받아 **서버에서만** gameplay truth를 변경한다. [`.md/TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md`](../TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md)의 합격 기준이 "client-only gameplay truth 변경 없이 server snapshot/event가 최종 상태를 만든다" — 즉 **상태조작 범주가 구조적으로 닫힘**.
- **정보 노출 범주의 정답이 탐지가 아니라 culling임을 Winters가 입증:** [`CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md`](../TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md) §1이 클라는 "snapshot 보간 / cue 재생 / UI"만 담당한다고 못 박음. fog/invisibility를 서버 relevance로 처리하면(→ §10 Server AoI) maphack류는 "보낼 데이터를 안 보냄"으로 닫힌다 — §1.1 매핑의 실물.
- **입력 자동화 범주만 서버측 탐지가 필요한 잔여 위협으로 남음:** 서버가 입력의 *결과*는 검증하지만 입력의 *출처(사람 vs 봇)* 는 못 본다 → §3의 행동 탐지가 Winters에 필요한 유일한 ML 영역. **이 범주 한정이 §1.4-(1) 위협 모델 기여의 testbed.**
- **벤치마크 데이터 소스:** 리플레이 시스템([`03_REPLAY_MVP...md`](../TODO/05-15/03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK.md), `.wrpl` = snapshot/event raw container)이 §1.4-(1) 공개 데이터셋의 **수집 인프라**(아래 §3.7에서 상술).

---

## 2. 클라이언트 무결성과 커널/유저모드 안티치트 (Client Integrity)

> **연구 범위 권고:** Winters testbed 맥락에서 **커널 안티치트(ring0 드라이버)는 연구 범위 밖을 권장**한다(프라이버시·안정성·법적 리스크, 자작 엔진에 과함). 본 절은 **메커니즘의 원리·트레이드오프·한계**를 박사 수준에서 정리하고, 박사 기여는 **"커널 권한 없이"** 강한 무결성을 얻는 방향(§2.4)에 둔다.

### 2.1 핵심 원리

클라이언트 무결성(client integrity)은 "플레이어 기기에서 도는 게임 클라이언트가 **변조되지 않았고 신뢰할 수 있는 환경에서 실행 중**임을 확인"하는 문제다. 서버가 못 보는 영역(클라 메모리, 입력 출처, 렌더링된 화면)을 보완하는 **2차 방어**다. 근본 난제: **방어자도 공격자도 같은 기기에서 돈다.** 플레이어가 자기 기기의 최고 권한을 가지므로, 방어 코드는 **공격자가 통제하는 환경에서 자신을 보호**해야 한다 — 이론적으로 불리한 비대칭 싸움(가이드 §0의 "탐지력 vs 신뢰" 긴장의 근원).

**모니터링 권한 수준과 트레이드오프:**

| 수준 | 보는 범위 | 강점 | 약점 (방어 관점에서 정직하게) |
|---|---|---|---|
| **유저모드(ring3)** | 자기 프로세스 메모리·모듈 | 배포 쉬움, 권한 낮아 안정·프라이버시 침해 작음 | 같은/높은 권한 공격자에게 **관측·우회 가능** — 방어가 공격자보다 권한이 높지 않음 |
| **커널모드(ring0)** | 시스템 전역(드라이버·프로세스·메모리) | 유저모드 위협을 위에서 관측 | **프라이버시·안정성 논란**(상시 상주·전체 접근), 커널 취약점 시 시스템 위험, 호환성 부담 |
| **하이퍼바이저(ring-1)** | OS 자체를 게스트로 관측 | 커널 루트킷도 위에서 봄 | 성능·복잡도, 가상화 충돌, 더 큰 신뢰 부담 |
| **하드웨어 신뢰근원(TPM/Secure Boot/TEE)** | 부팅·플랫폼 무결성 측정 | HW 기반 위변조 저항, 원격증명 가능 | HW 의존, 프라이버시(원격증명이 기기 식별 가능), 우회 표면 잔존 |

**무결성 측정(integrity measurement)의 원리:** 코드/환경을 **측정(해시)** 해 알려진 양품(known-good) 값과 비교. 핵심은 **신뢰 사슬(chain of trust)** — 신뢰 근원(root of trust, 예: TPM)이 다음 단계를 측정·서명하고, 그 단계가 또 다음을 측정. **원격증명(remote attestation)** 은 이 측정값을 서버가 **암호학적으로 검증**해 "이 클라가 변조 안 됐다"를 원격에서 확인하는 프로토콜. TPM의 PCR(Platform Configuration Register) + quote 서명이 고전적 메커니즘.

**왜 본질적으로 불완전한가(한계의 원리):** (1) **TOCTOU(검사-사용 시점 차)** — 측정 시점엔 깨끗하고 사용 시점엔 변조될 수 있다. (2) **측정의 완전성** — 무엇을 측정할지 빠뜨리면 그 틈이 공격면. (3) **신뢰 근원 가정** — HW 신뢰 근원이 침해되면 사슬 전체가 무너진다. 따라서 무결성은 "증명"이 아니라 **우회 비용을 높이는 확률적 방어**다.

### 2.2 대표 기존 연구/시스템

- **TPM / TCG 표준, Secure Boot, Measured Boot** — 플랫폼 무결성 측정·원격증명의 산업 표준(신뢰 근원·PCR·attestation).
- **Sailer et al. (2004), "Design and Implementation of a TCG-based Integrity Measurement Architecture (IMA)"** (USENIX Security) — Linux 무결성 측정의 고전. measured boot → attestation의 학술 원형.
- **Coker et al. (2011), "Principles of Remote Attestation"** (Int. J. Information Security) — 원격증명의 **원리·설계 원칙** 정리(무엇이 좋은 attestation인가). 본 절 §2.4의 이론적 토대.
- **Software-based attestation: Seshadri et al. (2004), "SWATT" / (2005) "Pioneer"** (IEEE S&P/SOSP) — **HW 신뢰 근원 없이** 타이밍 기반으로 코드 무결성을 증명하려는 시도. "커널/HW 없이 무결성"(§2.4) 연구 계보의 출발 — 그리고 그 **한계**(타이밍 가정의 취약성)도 함께 보여줌.
- **TEE: Intel SGX, ARM TrustZone** — 신뢰실행환경에서 코드·데이터를 OS로부터 격리(§4와 연결). SGX의 enclave attestation.
- **상용(접근방식 비교로만 — 가이드 §9, 역공학 안 함):** EAC/BattlEye(유저+커널 혼합), **Vanguard(부팅 커널 드라이버 상시 상주 → 프라이버시·안정성 논쟁의 교과서 사례)**. VAC(시그니처+지연 밴). **이들은 "어떤 권한 수준을 택했고 어떤 트레이드오프를 감수했나"의 비교 대상**이지 구현 모방 대상이 아니다.

### 2.3 메커니즘/알고리즘 (방어 관점 의사코드)

**원격증명 프로토콜의 방어 골격**(공격이 아니라 *검증* 로직). challenge-response로 재전송(replay)을 막는 표준 형태:

```text
// Remote attestation: 서버가 클라 무결성을 원격 검증. (HW root of trust 가정.)
// 핵심: nonce로 freshness, 서명으로 진정성, known-good로 무결성 판정.

Server → Client:  nonce ← random()                       // (1) freshness: 재전송 방지
Client:
    measurement ← Measure(code_pages, critical_state)     // (2) 측정(해시)
    quote       ← TPM.Quote(PCR ∥ measurement, nonce)     // (3) HW root가 서명
    send(quote, measurement)
Server:
    verify TPM signature over (measurement ∥ nonce)       // (4) 진정성+freshness 확인
    if measurement ∉ KnownGoodSet: flag_for_review        // (5) 무결성 판정 (즉시 밴 아님)
    // ★ 한계 인지: 이 판정은 '측정 시점'만 보증 — TOCTOU는 §2.4-(2)로 보완

// 프라이버시 보존 변형 (§2.4-(1)·§4와 연결): 측정 원본 대신 '준수 증명'만 전송
PrivacyPreservingAttest(measurement):
    proof ← ZK_Prove(statement = "measurement ∈ KnownGoodSet", witness = measurement)
    send(proof)            // 서버는 '양품임'만 알고 측정 원본(기기 지문)은 모름 → 프라이버시
    // → §4의 ZKP/TEE 검증으로 일반화
```

**설계 원리 주석(Coker et al. 원칙 반영):** 좋은 attestation은 (a) **fresh**(nonce로 재전송 차단), (b) **comprehensive**(측정 대상이 위협을 덮음), (c) **신뢰 근원에 명시적으로 의존**(무엇을 믿는지 분명), (d) **최소 노출**(검증에 필요한 것만 드러냄 — 프라이버시). 마지막 (d)가 §2.4·§4의 박사 각도로 직결된다.

### 2.4 박사급 novel 각도 (open problems)

1. **커널 권한 없이 강한 무결성(privacy-respecting integrity without ring0).** 핵심 open problem(가이드 §0 긴장의 정면 돌파). 기여: TPM/TEE **원격증명 + 프라이버시 보존 증명**으로, 커널 드라이버의 침습성 없이 "변조 안 된 환경" 입증. 보안성(우회 비용)과 프라이버시(기기 식별 불가)를 **동시에** 정량화. → IEEE S&P/USENIX Security.
2. **프라이버시 보존 무결성 증명(privacy-preserving attestation).** 원격증명은 본질적으로 **기기 지문(fingerprint)을 노출**해 추적 위험. 기여: ZKP로 "측정값이 양품 집합에 속함"만 증명하고 **측정 원본은 비공개**(§2.3 변형, §4와 융합). open: 게임 실시간 예산 내 ZKP 성능.
3. **TOCTOU에 강건한 연속 무결성(continuous attestation).** 단발 측정의 검사-사용 시점 차를 줄이는 **지속/임의시점 재측정**의 보안-오버헤드 trade-off 정식화. open: 재측정 빈도 ↔ 성능·배터리.
4. **무결성의 형식적 위협 모델·한계 증명.** "이 attestation이 막는 공격 능력의 정확한 경계"를 형식화 — 분야가 모호하게 두는 "무엇을 보장하는가"를 정리화. (방어의 **한계를 정직하게 증명**하는 것 자체가 기여 — 가이드 §7 threats to validity의 적극적 형태.)

### 2.5 Thesis statement 예시

> "TPM 기반 원격증명에 **영지식 준수 증명(ZK compliance proof)** 을 결합하면, 커널 권한과 기기 지문 노출 없이 클라이언트 실행 환경의 무결성을 입증할 수 있으며, 단발 attestation 대비 **TOCTOU 공격면을 W% 축소**하면서 검증 오버헤드를 게임 틱 예산(16.6ms)의 X% 이내, 프라이버시 누출을 정보이론적으로 ε 이하로 유지한다."

(가이드 §3: 새 프로토콜 X + 제약 C[no-ring0, 프라이버시] + 목표 Y[무결성] + 정량 Z[공격면·오버헤드·ε] + falsifiable.)

### 2.6 평가 방법

- **보안성(우회 비용):** 정의된 위협 모델 하에서 attestation을 우회하는 데 드는 **비용/난이도**를 정성·정량 분석(공격 *실행*이 아니라 비용 *추정*). 신뢰 근원 가정의 명시.
- **프라이버시:** 원격증명이 누출하는 정보량 — 기기 식별 가능성(linkability), 차등프라이버시 ε, 정보이론적 누출. **프라이버시 보존 변형이 baseline 대비 누출을 얼마나 줄이나.**
- **오버헤드:** attestation 지연(ms), 대역폭, CPU/배터리. 연속 attestation이면 빈도별 비용 곡선.
- **안정성:** false rejection(정상 환경을 변조로 오판) — 하드웨어/OS 다양성에서의 오탐. 이것이 곧 **플레이어 경험 비용**.
- **Baseline:** 커널 AC(상한 탐지력·하한 프라이버시), 유저모드 단독, attestation 없음. **삼각 긴장(§0)에서 자기 위치를 도표화.**
- **Threats to validity:** HW 신뢰 근원 침해 시나리오, TOCTOU 잔여 표면, 가정의 현실성.

### 2.7 Winters 연결점

- **연구 범위 경계가 Winters에 자연스럽게 그어짐:** Winters는 **서버 권위가 1차 방어선**이므로(§1.7), 클라 무결성은 *서버가 못 보는* 정보노출·입력자동화 범주에만 필요한 **보조**다. 커널 AC는 자작 MOBA testbed에 과하고 프라이버시·법적 리스크가 크다 — **§2.4-(1) "커널 없는 무결성"이 Winters에 맞는 유일한 현실적 방향**임을 testbed가 정당화.
- **무결성이 필요한 잔여 표면이 명확:** 클라가 담당하는 presentation 계층([`CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md`](../TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md) §1: snapshot 보간/FX cue/UI)은 gameplay truth가 아니므로 **변조돼도 서버 상태에 무영향** → 무결성 방어를 *정보노출 차단*과 *입력 출처 증명*으로 좁힐 수 있음(공격면 최소화의 실증).
- **솔직한 한계:** Winters는 TPM/attestation 인프라가 없다 → §2는 본 레퍼런스 세트에서 **가장 "구현보다 원리·한계 정리"에 무게**를 두는 절. 박사 기여로 가려면 attestation 프로토타입을 별도 구축해야 하며, 그조차 §2.4-(1)/(2)의 프라이버시 보존 방향으로만 권장(가이드 §10 윤리).

---

## 3. 서버측 권위 검증과 통계·ML 기반 이상탐지 (Server-Side Detection)

> **★ 본 문서의 심장.** Winters가 가장 강한 testbed를 제공하는 절. 서버 권위(→ [`10_Server`](00_PHD_Paper_Guide.md) 네트워크 문서)가 1차 방어선이고, 그 위에 통계·ML 탐지가 2차로 얹힌다.

### 3.1 핵심 원리 — 서버 권위 검증 (불가능 입력 거부)

서버측 방어의 제1층은 **권위 검증(authoritative validation)**: 서버가 진실 상태를 소유하고, 클라가 보낸 모든 입력/주장을 **물리·게임 규칙으로 검증**해 **불가능한 것을 거부**한다. 이는 ML이 아니라 **결정론적 규칙(deterministic invariants)** 이며, 상태조작·프로토콜 범주(§1.1)를 **0%/100%로** 닫는다(애매함 없음). 핵심 검증 종류:

- **유효성(validity):** 값이 유한·범위 내인가(NaN/Inf/거대값 거부) — 프로토콜 악용 1차 차단.
- **물리 가능성(physical plausibility):** 이동 거리 ≤ (최대속도 × Δt)인가(speedhack 거부), 경로가 통과 가능한가.
- **권한·자원(authority/resource):** 쿨다운·마나·사거리·대상 유효성·생존 상태 — "지금 이 행동이 규칙상 가능한가".
- **불변식(invariant):** 상태 전이가 게임 규칙이 허용하는 전이 집합에 속하는가.

```text
// 서버 권위 검증: 불가능 입력 거부 (deterministic, ML 아님). 상태조작/프로토콜 범주를 닫음.
ValidateCommand(world /*=truth*/, cmd):
    e ← cmd.issuerEntity
    if not Alive(world, e):              return REJECT("dead-state")     // 죽은 채 행동 불가
    if not IsFinite(cmd.payload) or |cmd.payload| > MAX_ABS:
                                          return REJECT("invalid-pos")    // NaN/Inf/거대값
    switch cmd.kind:
      MOVE:
        if not PathReachable(world, e, cmd.target):
                                          return REJECT("no-grid-path")   // 통과 불가 경로
        // 속도 위반은 매 tick 위치 적분으로 검사: |Δpos| ≤ maxSpeed·Δt (speedhack)
      BASIC_ATTACK / CAST_SKILL:
        if DistanceSq(world,e,cmd.target) > Range(e,cmd.slot)^2:
                                          return REJECT("out-of-range")
        if OnCooldown(world,e,cmd.slot):  return REJECT("cooldown")
        if Mana(world,e) < Cost(e,cmd.slot): return REJECT("no-mana")
    return ACCEPT    // 서버만 여기서 gameplay truth 변경

// 거부는 로그로 남겨 사후 탐지(§3.2)·라벨링의 신호로 재사용 (rate 제한).
```

**왜 이것이 1차 방어선인가:** 상태조작·speedhack류는 "클라가 거짓 상태를 주장"하는 것인데, 서버가 진실을 소유하면 **거짓 주장이 그냥 거부**된다 — 군비경쟁이 아니라 구조적 종결. 그러나 한계: 서버는 **입력의 출처(사람/봇)와 클라의 인지(wallhack으로 본 정보)** 를 직접 못 본다 → **입력자동화·정보노출 범주는 권위 검증으로 안 닫힘** → §3.2~3.4의 통계·ML 탐지가 필요해진다.

### 3.2 핵심 원리 — 통계·행동 기반 이상탐지

권위 검증을 통과하는 "규칙상 가능하지만 비인간적인" 행동(aimbot의 완벽 조준, 봇의 기계적 패턴)은 **이상탐지(anomaly detection)** 로 잡는다. 원리: **인간 플레이의 통계적 분포**를 모델링하고, 그로부터의 **이탈(deviation)** 을 점수화. 부정행위를 직접 정의하지 않고 "정상에서 얼마나 먼가"로 본다(라벨 부족 대응).

- **Aimbot 탐지:** 조준 궤적의 **운동학적 특징** — 조준점이 표적에 도달하는 시간·곡률·overshoot·정지 분포. 인간은 생물학적 떨림·반응지연·비최적 경로를, aimbot은 **부자연스럽게 매끄럽거나 즉각적인** 분포를 보인다(직접 메커니즘 아니라 **통계적 서명**으로만 진술).
- **Botting 탐지:** 행동 시퀀스의 **반복성·엔트로피** — 봇은 의사결정 다양성이 낮다(낮은 행동 엔트로피, 주기성).
- **방법론:** 지도학습(라벨 있으면), **이상탐지/단일클래스(one-class SVM, isolation forest, autoencoder 재구성 오차)**, 시퀀스 모델(HMM/RNN/Transformer로 궤적·행동열).

### 3.3 메커니즘/알고리즘 (방어 관점 의사코드)

**적대적으로 강건한, 설명 가능한 행동 탐지 파이프라인**(탐지 *판정* 로직; 회피 기법은 다루지 않음):

```text
// 서버측 행동 이상탐지. 입력: 권위 검증을 통과한 행동 스트림(리플레이/스냅샷에서 추출).
// 출력: 위험 점수 + 설명(제재 근거). 즉시 밴이 아니라 review로 라우팅.

ExtractFeatures(trajectory, actions):
    f_aim  ← {time-to-target, curvature, overshoot, post-fire settle, snap-frequency}
    f_bot  ← {action-entropy, periodicity(FFT peak), reaction-time distribution}
    return normalize(f_aim ∥ f_bot)        // 인간 분포 기준 정규화

Score(features):
    s ← AnomalyModel.score(features)        // one-class / autoencoder recon-error 등
    // ★ 적대적 강건성: 결정경계 근처 민감도를 낮추고(robust training),
    //   회피로 쉽게 흔들리는 특징에 의존하지 않도록 feature 견고화
    return s

Decide(s, history):
    if s < τ_low:           return CLEAN
    if s > τ_high:          return FLAG_HIGH_CONFIDENCE       // 그래도 즉시밴 아님
    return ESCALATE_TO_REVIEW(evidence = TopContributingFeatures(features))
    // ★ 설명가능성: '왜 의심되는가'를 사람이 검토 가능한 근거로 (제재 정당성)

// 핵심 운영 원칙(FPR 비용):
//  - 단일 점수로 밴하지 않는다. 누적 증거 + 사람 검토 + 지연 제재(deferred ban).
//  - FPR(무고한 밴)의 비용이 FNR(놓친 치터)보다 훨씬 크다 → 임계값을 보수적으로.
```

**왜 FPR이 결정적인가(이 분야 고유 원리):** 일반 분류는 정확도를 보지만, 안티치트에서 **거짓양성(무고한 플레이어 밴)은 치명적**이다 — 신뢰 붕괴·법적 분쟁·환불. 따라서 평가는 **고정 FPR에서의 TPR(TPR@FPR=0.1%)** 로 본다. 또한 적대자는 **탐지기를 회피하도록 학습(adversarial evasion)** 하므로, 정적 정확도가 아니라 **회피 비용(cost-to-evade)** 과 **적대적 환경 하 성능 저하**가 진짜 metric이다.

### 3.4 박사급 novel 각도 (open problems)

1. **적대적으로 강건한 탐지(adversarially robust detection) — 핵심 open problem.** 탐지기가 공개되면 적대자가 회피 학습한다(군비경쟁). 기여: 회피에 대해 **증명 가능하게 강건한** 탐지(robustness 하한 보증), 또는 회피 비용을 정량적으로 높이는 feature/모델 설계. → IEEE S&P/USENIX Security. (방어: 회피 *방법*이 아니라 회피 *비용 상승*을 연구.)
2. **라벨 부족 대응(weak/self-supervised cheat detection).** 확정 치터 라벨은 희소·노이즈(밴 기록이 ground truth가 아님). 기여: self-supervised로 인간 행동 표현을 학습하고 소수 라벨로 미세조정, weak-label(신고/통계) 노이즈에 강건한 학습. open: 라벨 노이즈 모델링.
3. **설명가능한 제재 근거(explainable detection for sanction).** 밴은 **정당화 가능**해야 한다(법적·신뢰). 기여: 블랙박스 점수가 아니라 **사람이 검토 가능한 증거**(어떤 행동이 왜 비인간적인지)를 내는 탐지. open: 설명의 충실성(faithfulness) vs 단순성.
4. **그래프 기반 공모·봇농장 탐지(collusion / Sybil).** win-trading·다중계정은 개인 행동이 아니라 **관계**에서 드러난다. 기여: 플레이어-매치 이분그래프/상호작용 그래프에서 GNN/커뮤니티 탐지로 결탁 클러스터 발견. open: 정상 친구관계 vs 결탁 구분(또 FPR 문제).
5. **궤적의 운동학적 인간성 모델(kinematic humanness model).** 인간 조준의 생물역학(Fitts의 법칙, 떨림·saccade)을 **생성 모델**로 세우고, 그 우도(likelihood)로 aimbot을 탐지. open: 입력 장치·실력 다양성을 흡수하는 일반화.

### 3.5 Thesis statement 예시

> "서버 권위 리플레이에서 추출한 운동학적·행동 특징에 대해 **self-supervised 표현 학습 + 적대적 강건 훈련**을 적용하면, 확정 라벨이 희소한 조건에서도 aimbot/botting을 **FPR 0.1%에서 TPR X%(ROC-AUC Y)** 로 탐지하면서, 회피 학습에 대한 성능 저하를 Z%p 이내로 억제하고, 각 판정에 대해 사람이 검토 가능한 제재 근거를 제공한다."

(가이드 §3: 새 학습기법 X + 제약 C[라벨부족·적대환경·낮은 FPR] + 목표 Y[탐지] + 정량 Z + falsifiable: "회피 모델이 성능을 Z%p 초과로 떨어뜨리면 반증".)

### 3.6 평가 방법

- **탐지 성능(이 분야 핵심 — 가이드 §7):** **ROC 곡선·AUC**, 그리고 **고정 저FPR에서의 TPR**(TPR@FPR=0.1%/1%). 평균 정확도 단독 금지 — 클래스 불균형·FPR 비용 때문.
- **적대적 강건성:** 회피 시도(적대적 perturbation, distribution shift) 하에서 AUC/TPR 저하 곡선, **cost-to-evade**(회피에 드는 비용/제약).
- **라벨 품질:** ground truth가 밴 기록인가 합의 라벨인가 — 라벨 노이즈가 결과에 미치는 영향(가이드 §7 threats).
- **설명가능성:** 제재 근거의 충실성(사람 평가), case study.
- **오버헤드:** 서버측 추론 비용(per-player ms, 틱 예산 영향), 대역폭(특징 추출이 추가 데이터를 요구하나).
- **Baseline:** 규칙 기반(권위 검증 only), 단순 임계값, 기존 탐지기(one-class SVM 등), naive 지도학습. **ablation:** 특징군 분리, robust training on/off, self-supervision on/off.
- **Threats to validity:** 실력 편향(고수를 치터로 오판), 입력 장치 다양성, 도메인 갭(합성↔실측), 라벨 누출.

### 3.7 Winters 연결점 — ★ 가장 강력한 testbed

Winters는 **서버 권위 검증(§3.1)이 이미 코드로 가동**되고, **리플레이가 탐지 데이터 소스**가 되어, §3 전체의 거의 이상적 testbed다.

- **§3.1 권위 검증이 1:1로 실재(의사코드 ↔ 코드):** [`CommandExecutor.cpp`](../../Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp)에 §3.1 의사코드의 모든 검증이 박혀 있다 —
  - **유효성:** `IsValidPos`가 `std::isfinite(x/y/z) && |x/y/z| ≤ kMaxMoveCommandAbs(=10000.f)`로 NaN/Inf/거대값 거부(L383-389, 상수 L52). **프로토콜 악용 1차 차단의 실물.**
  - **생존 상태:** `!health.bIsDead && fCurrent > 0`로 죽은 채 행동 거부(L379) → 거부 reason `"dead-state"`.
  - **사거리:** `DistanceSqXZ > range*range`로 out-of-range 거부(L924), reject reason `"out-of-range"`(L1246).
  - **쿨다운/마나/경로:** cooldown 해석(L517-530), move의 `"invalid-pos"`/`"no-grid-path"` 거부(L1415, L1456).
  - **거부 로그가 사후 신호:** reject가 `[Command] ... reject reason=...`로 남고 **rate 제한**(`s_rejectLogCount` L1240-1252, L1410-1419) → **§3.2 이상탐지·라벨링의 신호원으로 그대로 재사용 가능.**
  - **즉, "불가능 입력 거부"가 추상론이 아니라 자기 서버에서 상태조작·프로토콜 범주를 닫고 있음.**
- **권위의 단일 진실 보장:** [`01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md`](../TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md)의 GameRoom phase loop(`Phase_DrainCommands → Phase_ExecuteCommands → ... → Phase_BroadcastSnapshot`)와 합격 기준("client-only gameplay truth 변경 없이 server snapshot/event가 최종 상태")이 **서버가 truth를 소유**함을 보장 → §3.1의 전제(server = truth)가 충족.
- **리플레이 = 탐지 데이터셋 인프라(§3.4-(2) 라벨 부족·§1.4-(1) 벤치마크의 핵심):** [`03_REPLAY_MVP...md`](../TODO/05-15/03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK.md) + [`Server/Private/Game/ReplayRecorder.cpp`](../../Server/Private/Game/ReplayRecorder.cpp)가 `Phase_BroadcastSnapshot`/`Phase_BroadcastEvents`와 **동일 bytes**를 `.wrpl`(WRPL container)로 기록. 즉 **모든 매치의 권위적 상태·이벤트 스트림이 사후 분석용으로 보존** → §3.3 특징 추출(궤적/행동열)의 원천, 라벨링·벤치마크 데이터 소스. [`05-09/Replay/`](../TODO/05-09/Replay/) 문서군이 server recorder 계획.
- **결정론 서버가 입력 검증의 토대(→ [`04_JobSystem`](04_JobSystem_병렬처리.md) §2, Server 03):** Winters 서버가 lockstep/replay 결정론을 지향 → **같은 입력 = 같은 권위 상태**이므로 (a) 리플레이가 비트단위 재현 가능한 ground truth가 되고, (b) "관측된 결과가 입력에서 결정론적으로 도출 가능한가"를 검증할 수 있다(speedhack/상태조작의 결정론적 반증). 결정론은 §3.1 권위 검증의 **신뢰 기반**.
- **입력자동화만 남는 잔여 위협(§1.7 재확인):** 권위 검증이 상태조작/프로토콜을, AoI/fog가 정보노출(→§10)을 닫으면 — **aimbot/botting(입력 출처) 탐지가 Winters에서 ML이 필요한 유일한 영역** → §3.2~3.5 ML 탐지의 명확한 적용 대상. 리플레이가 그 학습 데이터를 공급.
- **측정 인프라(가이드 §7):** CLAUDE.md의 "inspectable debug UI/overlay + bounded `OutputDebugString` trace" 문화(이미 reject 로그가 그 형태) + `maxJitter`류 로깅이 탐지기 오버헤드·FPR 측정대.

---

## 4. 프라이버시 보존·검증 가능 안티치트 (연구 프론티어)

### 4.1 핵심 원리

가장 미래지향적 절. §0 삼각 긴장의 **궁극적 해소 시도**: "치트하지 않았음"을 **플레이어 프라이버시를 침해하지 않으면서 암호학적으로 증명**한다. 두 축:

- **영지식 증명(Zero-Knowledge Proof, ZKP):** 증명자(클라)가 검증자(서버)에게 **"명제가 참"임을 명제 외 정보를 일절 노출하지 않고** 확신시킨다. 안티치트 응용: "내 클라이언트는 양품이고 / 내 입력은 허용된 범위에서 나왔고 / 내가 본 정보는 권한 내였다"를 **클라 메모리·기기 지문을 노출하지 않고** 증명. ZK의 세 성질 — 완전성(참이면 받아들임), 건전성(거짓이면 거의 못 속임), 영지식성(증명이 비밀을 누출 안 함) — 이 마지막이 프라이버시의 핵심.
- **신뢰실행환경(TEE) + 원격증명:** 게임 로직(또는 그 무결성 검사)을 **하드웨어 격리 enclave(SGX/TrustZone)** 에서 실행해, OS·플레이어조차 내부를 볼 수 없게 하고, enclave가 "정상 실행 중"임을 원격증명. **검증 가능 연산(verifiable computation)** — 결과가 변조 없이 계산됐음을 증명.

**왜 프론티어인가(원리적 매력과 한계):** 이 접근은 삼각 긴장을 **동시에** 푼다 — 탐지력(증명으로 보장)·프라이버시(영지식성)·신뢰(암호학적). 그러나 **실용성 장벽**: ZKP 생성은 무겁고(게임 틱 16.6ms엔 보통 너무 느림), TEE는 HW 의존·부채널(side-channel) 취약·신뢰 부담. 따라서 이 절의 박사 기여는 거의 전부 **"실용적 성능 달성"** 에 있다.

### 4.2 대표 기존 연구/시스템

- **Goldwasser, Micali & Rackoff (1985), "The Knowledge Complexity of Interactive Proof Systems"** — ZKP의 창시 논문. 이 분야 모든 것의 출발.
- **zk-SNARK/zk-STARK 계보:** Ben-Sasson et al. ("Pinocchio" Parno et al. 2013, IEEE S&P; zk-STARKs 2018) — **간결 비대화 증명**으로 ZKP를 실용 영역에 근접시킨 계보. 검증 가능 연산의 토대.
- **Verifiable computation: Gennaro, Gentry & Parno (2010); "Pinocchio" (2013, IEEE S&P)** — 위임 연산의 정확성을 효율적으로 검증.
- **TEE: Intel SGX(Costan & Devadas 2016, SGX Explained), ARM TrustZone** + **부채널 공격 문헌(예: 캐시 기반)** — TEE의 능력**과 한계**를 함께 보는 균형(방어 연구는 한계도 인용).
- **게임 적용은 거의 미개척:** ZKP/TEE를 게임 안티치트에 실제 적용한 동료심사 연구는 희소 — **그래서 open problem이자 박사 기회**(가이드 §2-3, "풀리지 않던 문제"). 블록체인 게임의 검증 가능 상태 정도가 인접 사례.

### 4.3 메커니즘/알고리즘 (방어 관점 의사코드)

**검증 가능 안티치트의 골격**(프라이버시 보존 *검증* 프로토콜):

```text
// 목표: 클라가 '규칙을 지켰음'을 비밀(메모리/입력 원본/기기 지문) 노출 없이 증명.

// (A) ZKP 기반 — 입력 합법성의 영지식 증명
Client(secret = {raw_input, local_state}):
    statement ← "exists input s.t. input ∈ AllowedInputSpace(game_rules)
                 AND observed_actions = Simulate(input)"
    proof ← ZK_Prove(statement, witness = secret)        // 비밀은 안 나감
    send(proof)
Server:
    accept ⟺ ZK_Verify(statement, proof)                 // '지켰다'만 확신, 원본 모름
    // 완전성·건전성·영지식성으로 탐지력+프라이버시 동시 확보

// (B) TEE 기반 — 무결성 검사를 enclave에서 + 원격증명
Enclave (HW 격리, 플레이어/OS도 못 봄):
    verdict ← RunIntegrityChecks(client)                 // 검사 로직 자체가 보호됨
    return Attest(verdict, nonce)                          // enclave 서명
Server: verify enclave attestation → trust verdict

// ★ 실용성 게이트(이 절의 박사 난제): 위 증명/검증이 게임 실시간 예산에 들어오는가?
//   - 무엇을 증명할지 최소화(전체가 아니라 보안 임계 명제만)
//   - 사전계산/증분 증명(incremental)으로 per-tick 비용 분산
//   - 비대화/간결 증명(SNARK/STARK)으로 검증 비용 ↓
```

**설계 원리:** "무엇을 증명할지"의 **최소 명제 선택**이 성능을 좌우한다 — 전체 게임 상태가 아니라 *보안상 임계인 작은 명제*("이 입력이 허용 공간에 있다")만 증명하면 ZKP 회로가 작아진다. 이 **명제 최소화 + 증분 증명**이 §4.4 기여의 핵심.

### 4.4 박사급 novel 각도 (open problems)

1. **게임 실시간 예산 내 ZKP(practical real-time ZKP for games) — 핵심 난제.** ZKP를 16.6ms 틱(또는 비동기 사후 검증)에 맞추는 것. 기여: **증명 명제 최소화 + 증분/배치 증명 + GPU 가속**으로 게임 적용 가능 성능 달성, 보안-성능 trade-off 정식화. → IEEE S&P/CCS.
2. **무엇을 증명해야 충분한가(soundness of partial proofs).** 게임 전체를 증명할 수 없으니 일부만 증명 → "어떤 명제 집합을 증명하면 어떤 위협 범주가 닫히는가"를 형식화. open: 부분 증명의 보안 보장 경계(§2.4-(4)·§1.4-(2)와 연결).
3. **TEE의 게임 적용과 부채널 강건성.** 게임 로직 일부를 enclave로 옮길 때 성능·부채널 trade-off. 기여: 안티치트 임계 경로만 TEE로 격리하는 최소 TCB 설계와 부채널 완화. open: 게임의 높은 처리량에서 TEE 오버헤드.
4. **프라이버시-탐지력 Pareto front의 형식화.** §0 삼각 긴장을 **정량적 Pareto front**로 — "이 프라이버시 수준(ε)에서 달성 가능한 최대 탐지력"의 경계를 이론적으로 규명. (가이드 §5 기여 유형 4 "새 이론/모델".)

### 4.5 Thesis statement 예시

> "게임 입력 합법성의 **보안 임계 명제만을 선택**해 증분 zk-SNARK로 증명하면, 플레이어의 입력 원본·클라 상태를 노출하지 않고도(영지식성) 상태조작·입력범위 위반을 서버가 검증할 수 있으며, 전체 상태 증명 대비 **증명 생성 비용을 K배 절감**해 비동기 사후 검증 예산 내에 들이면서, 프라이버시-탐지력 Pareto front 상에서 기존 서버측 탐지를 지배한다."

(가이드 §3: 새 프로토콜+이론 X + 제약 C[프라이버시·실시간] + 목표 Y[검증 가능 안티치트] + 정량 Z[비용 절감·Pareto] + falsifiable.)

### 4.6 평가 방법

- **성능(실용성 게이트):** 증명 생성/검증 시간(ms), 증명 크기(bytes), per-tick/per-match 오버헤드 — **게임 예산 대비**. TEE면 enclave 진입·메모리 오버헤드.
- **프라이버시:** 영지식성의 형식 보장 + 실제 누출 측정(무엇이 새는가), 차등프라이버시 ε, linkability.
- **보안성:** 건전성(soundness) 보장, 부분 증명이 닫는 위협 범주의 형식적 경계, TEE 부채널 잔여 위험.
- **Pareto 분석:** 프라이버시(ε) vs 탐지력(TPR@FPR) vs 비용의 3D trade-off에서 자기 위치 — §0 삼각 긴장의 정량화.
- **Baseline:** 평문 서버측 탐지(상한 탐지력·하한 프라이버시 = §3), 전체 상태 ZKP(상한 프라이버시·하한 성능), TEE 없음.
- **Threats to validity:** 암호 가정(SNARK setup), HW 신뢰(TEE), 명제 선택의 완전성, 실측 가능성(프로토타입 규모).

### 4.7 Winters 연결점

- **검증 가능 연산의 토대가 이미 있음 — 결정론 + 리플레이:** Winters의 결정론 서버(같은 입력 → 같은 상태)는 "관측된 결과가 합법 입력에서 도출됨"을 증명하려는 §4.3-(A)의 **자연스러운 검증 대상**이다 — 결정론이 곧 `observed = Simulate(input)` 명제의 성립 기반. 리플레이([`03_REPLAY...md`](../TODO/05-15/03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK.md))는 그 명제를 **비동기 사후 검증**(틱 예산 밖)으로 돌릴 수 있는 데이터·실행 트레이스를 제공 → §4.4-(1) "비동기 사후 검증 예산"의 현실적 출구.
- **명제 최소화의 적용처:** §3.7의 `CommandExecutor` 검증식들(사거리·쿨다운·유한성)이 곧 §4.3 "보안 임계 명제"의 후보 — 전체 게임이 아니라 *이 작은 규칙들*만 ZK로 증명하면 회로가 작아진다(§4.4-(2)). Winters의 명시적 validation 규칙이 "무엇을 증명할지"를 이미 추려 둔 셈.
- **솔직한 한계(가이드 §7):** Winters엔 ZKP/TEE 인프라가 전혀 없다 → §4는 본 세트에서 **가장 미래지향·가장 testbed 거리가 먼** 절. 박사 기여로 가려면 별도 암호 프로토타입이 필요하고, 게임 실시간 성능 장벽이 크다. 따라서 §4는 Winters에서 **"결정론·리플레이를 검증 가능 연산의 입력으로 삼는 비동기 사후 검증"** 정도가 현실적 진입점이며, 실시간 ZKP는 장기 프론티어로 둔다.

---

## 종합. 통합 학위논문 구조 예시 (three-papers → 하나의 명제)

가이드 §4.1 "Three Papers Make a Thesis" 모델로, **이 분야의 인접 3문제**를 하나의 학위논문으로 묶는다. 묶는 명제는 §0의 삼각 긴장이다. **Winters의 강점(서버 권위·리플레이·결정론)에 맞춰 서버측 탐지를 중심에 두고, 커널 AC는 범위 밖으로 둔다.**

> **통합 Thesis statement:**
> "서버 권위 게임 시뮬레이션을 기반으로, **프라이버시를 보존하면서 적대적으로 강건한** 부정행위 탐지·검증 체계를 구축하면, 무고한 플레이어를 밴하지 않고(낮은 FPR) 회피에 강건하게(높은 cost-to-evade) 부정행위를 억제하며, 그 정당성을 설명·암호학적으로 보장할 수 있다."

```text
제목(가제): Privacy-Preserving, Adversarially-Robust Cheat Detection
             for Server-Authoritative Online Games

Ch 1. 서론
   1.3 Thesis: 위 통합 명제
   1.4 기여:
       (C1) MOBA 부정행위의 형식적 위협 모델 + 공개 벤치마크/데이터셋 (서버 권위가
            닫는 범주를 정량화, 잔여 위협을 입력자동화로 한정)
       (C2) 서버 권위 리플레이 기반, 라벨 부족·적대 환경에 강건한 설명 가능 행동 탐지
            (aimbot/botting/공모)
       (C3) 결정론·리플레이를 검증 가능 연산으로 삼는 프라이버시 보존 사후 검증
            (입력 합법성의 비동기 증명)
       (보조) 커널 없는 프라이버시 보존 무결성(attestation)의 원리·한계 정리 (범위 경계)

Ch 2. 배경/관련 연구
   - 위협 분류 (Yan&Randell 2005), 게임 보안 측정
   - 서버 권위 검증 (→ 10_Server netcode)
   - 이상탐지·적대적 ML (one-class, robust learning)
   - ZKP/TEE (GMR 1985, Pinocchio 2013, SGX), remote attestation (IMA, Coker 원칙)
   - gap: "강한 탐지 = 침습/고FPR/회피취약" — 프라이버시·강건·정당성을 동시 만족하는
     서버측 체계의 부재; 표준 벤치마크 부재

Ch 3 (논문1, USENIX Security): C1 — Threat Model & Benchmark
   - Winters 서버 권위/리플레이로 범주별 공격면이 닫히는지 case study
   - 공개 벤치마크(자작 testbed·동의·익명화) + 표준 metric(ROC-AUC, FPR@TPR, cost-to-evade)
   - 평가: taxonomy MECE, 벤치마크 순위 안정성

Ch 4 (논문2, IEEE S&P): C2 — Robust, Explainable Behavioral Detection
   - 리플레이 특징(운동학/행동 엔트로피) + self/weak-supervision + 적대적 강건 훈련
   - 설명 가능 제재 근거, 그래프 기반 공모 탐지
   - 평가: TPR@FPR=0.1%, AUC, cost-to-evade, 적대 환경 저하, 오버헤드

Ch 5 (논문3, CCS/NDSS): C3 — Privacy-Preserving Verifiable Anti-Cheat
   - 결정론+리플레이를 입력으로 한 입력 합법성 비동기 ZK 검증, 명제 최소화
   - 프라이버시-탐지력 Pareto front 형식화
   - 평가: 증명 비용, ε 누출, soundness 경계, Pareto 지배

Ch 6. 종합 평가 — Winters MOBA 서버에서 C1+C2+C3 통합(권위 검증→탐지→검증 파이프라인)
Ch 7. 논의 — threats(실력 편향, 라벨 노이즈, 도메인 갭, 암호/HW 가정), 일반화(FPS/RTS),
       ★ ethics & responsible disclosure 절(가이드 §10)
Ch 8. 결론 — 통합 명제 증명 정리, future work(실시간 ZKP, 커널 없는 강한 무결성)
```

핵심: 3편이 각각 **독립 venue 논문**(USENIX/S&P/CCS)이면서, 서론·결론이 "프라이버시 보존 + 적대적 강건 + 정당성"이라는 **단일 명제**로 묶는다(가이드 §4.1). Winters는 C1(서버 권위가 닫는 범주의 실증), C2(권위 검증 코드 + 리플레이 데이터), C3(결정론·리플레이를 검증 입력으로) **세 기여 모두에 작동하는 testbed**를 이미 보유 — 가이드 §2 "엔진은 검증 플랫폼" 그 자체이며, **커널 AC를 범위 밖에 둠으로써 윤리·범위가 깔끔**하다(가이드 §10).

---

## 참고문헌

**위협 모델·게임 보안 측정**
- Yan, J., & Randell, B. (2005). A Systematic Classification of Cheating in Online Games. *NetGames*.
- Webb, S. D., & Soh, S. (2007). Cheating in Networked Computer Games: A Review. *DIMEA*.
- Hoglund, G., & McGraw, G. (2007). *Exploiting Online Games*. Addison-Wesley. (위협 실재성의 근거로만 인용; 공격 디테일 비채택.)
- (상용 시스템 EAC/BattlEye/Riot Vanguard/VAC — 접근방식 비교로만, 동료심사 학술 출판 아님; 가이드 §9.)

**클라이언트 무결성·원격증명·신뢰 컴퓨팅**
- Sailer, R., Zhang, X., Jaeger, T., & van Doorn, L. (2004). Design and Implementation of a TCG-based Integrity Measurement Architecture. *USENIX Security*.
- Coker, G., et al. (2011). Principles of Remote Attestation. *International Journal of Information Security*, 10(2).
- Seshadri, A., Perrig, A., van Doorn, L., & Khosla, P. (2004). SWATT: SoftWare-based ATTestation for Embedded Devices. *IEEE S&P*.
- Seshadri, A., et al. (2005). Pioneer: Verifying Code Integrity and Enforcing Untampered Code Execution on Legacy Systems. *SOSP*.
- (TPM/TCG, Secure Boot, Intel SGX, ARM TrustZone — 표준·플랫폼 문서.)

**암호학적 검증·프라이버시 보존 (ZKP/TEE/Verifiable Computation)**
- Goldwasser, S., Micali, S., & Rackoff, C. (1985). The Knowledge Complexity of Interactive Proof Systems. *STOC* / *SIAM J. Computing* (1989).
- Gennaro, R., Gentry, C., & Parno, B. (2010). Non-Interactive Verifiable Computing. *CRYPTO*.
- Parno, B., Howell, J., Gentry, C., & Raykova, M. (2013). Pinocchio: Nearly Practical Verifiable Computation. *IEEE S&P*.
- Ben-Sasson, E., et al. (2018). Scalable, Transparent, and Post-Quantum Secure Computational Integrity (zk-STARKs). *ePrint*.
- Costan, V., & Devadas, S. (2016). Intel SGX Explained. *IACR ePrint*.

**이상탐지·적대적 ML (탐지 기법 토대)**
- (One-class SVM: Schölkopf et al. 2001; Isolation Forest: Liu et al. 2008 — 이상탐지 토대.)
- (Adversarial robustness: Goodfellow et al. 2015; Madry et al. 2018 — 적대적 강건 학습 토대.)
- (Graph-based Sybil/collusion: SybilGuard/SybilRank 계열 — 그래프 기반 공모 탐지.)

> 인용 원칙(가이드 §10): 위 중 **확실한 정전(canonical)** 문헌만 정확 인용했고, 분야 토대(이상탐지·적대 ML·그래프 탐지)는 계열로만 표기했다. 게임 안티치트에 ZKP/TEE를 적용한 동료심사 연구는 희소하므로 **인접 분야 정전 + open problem 프레이밍**으로 다뤘다(가이드 §2-3 "풀리지 않던 문제"). 모든 위협 진술은 방어 정당화 수준의 추상이며 실행 가능한 공격 기법을 담지 않는다.

**Winters testbed (1차 출처)**
- [`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`](../../Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp) — 서버 권위 입력 검증(유한성·사거리·쿨다운·생존·경로 거부; reject 로그)
- [`Server/Private/Game/GameRoom.cpp`](../../Server/Private/Game/GameRoom.cpp), [`Server/Private/Game/ReplayRecorder.cpp`](../../Server/Private/Game/ReplayRecorder.cpp) — phase loop, 권위 snapshot/event 기록
- [`.md/TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md`](../TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md) — 서버 권위 완성 (1차 방어선)
- [`.md/TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md`](../TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md) — 클라=presentation, 서버=truth 규칙
- [`.md/TODO/05-15/03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK.md`](../TODO/05-15/03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK.md), [`.md/TODO/05-09/Replay/`](../TODO/05-09/Replay/) — 리플레이(.wrpl) = 사후 탐지·라벨링·검증 데이터 소스
- 결정론 서버·입력 검증 토대: [`04_JobSystem_병렬처리.md`](04_JobSystem_병렬처리.md) §2, [`.md/TODO/05-07/Server/03_SERVER_PARALLEL_PHASES.md`](../TODO/05-07/Server/03_SERVER_PARALLEL_PHASES.md)
