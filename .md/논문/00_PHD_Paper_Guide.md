# 00. 박사 논문 작성 가이드 — 게임 엔진 연구 (PhD Paper Guide)

> 이 문서는 "게임 엔진을 주제로 박사 논문을 쓴다"가 실제로 무엇을 의미하는지,
> 박사 논문의 형태·핵심 원리·구조·평가 기준·작성 프로세스를 정리한 키스톤 문서다.
> `01_` ~ `11_` 도메인 문서는 모두 이 문서의 개념 틀(특히 §1 "구현 vs 연구 기여")을 전제로 한다.

## MobaZero / Game AI 집중 코퍼스

- [MobaZero 논문 정복 인덱스](MobaZero/00_INDEX.md)

---

## 1. 가장 중요한 구분: "구현"이 아니라 "기여(Contribution)"다

가장 흔한 오해부터 깬다.

> ❌ "Forward+, PBR, GI, ECS, Netcode, Anticheat를 다 잘라서 구현하면 박사 논문이다."

이것은 박사 논문이 **아니다**. 이것은 **엔지니어링 포트폴리오** 또는 **산업 결과물**이다.
박사 학위논문(doctoral dissertation)의 본질은 단 하나다:

> **"인류 지식에 대한 독창적 기여(an original contribution to knowledge)"**

| 구분 | 구현 (Implementation) | 연구 기여 (Research Contribution) |
|------|----------------------|-----------------------------------|
| 질문 | "어떻게 만들지?" (How to build) | "무엇이 새로운가? 왜 더 나은가?" (What is new / why better) |
| 산출물 | 동작하는 시스템 | 검증된 명제(thesis) + 시스템 |
| 성공 기준 | 돌아간다 | 기존 최첨단(SOTA)을 정량적으로 능가하거나, 풀리지 않던 문제를 푼다 |
| 비교 대상 | 없음 (만들면 끝) | 반드시 baseline과 비교 (ablation, SOTA 대비) |
| 평가 | 데모 | 재현 가능한 실험 + 통계 + 동료심사(peer review) |
| 예시 | "나는 실시간 GI를 구현했다" | "동적 장면에서 light leaking 없이 1ms 이내 diffuse GI를 갱신하는 probe 배치 알고리즘을 제안하고, DDGI 대비 메모리 40%↓·품질 동등을 보였다" |

**핵심 명제:** 당신이 나열한 주제 목록(RL/MCTS, GI/PBR/Forward+, PBD/FFT, ECS/Fiber, World Partition, Netcode, Anticheat…)은 **박사 논문의 목차가 아니다.** 그것은 **연구를 펼칠 수 있는 "분야(area)"의 목록**이다. 박사는 이 중 **하나의 분야**에서 **하나의 좁은 문제(narrow problem)**를 골라 **하나의 새 기여**를 만든다.

---

## 2. 그래서 게임 엔진으로 박사를 한다는 것은?

게임 엔진은 그 자체로 박사 주제가 아니다. 게임 엔진은 **연구를 검증하는 플랫폼(research platform / testbed)**이다. 박사는 다음 셋 중 하나의 형태를 띤다.

### 2-1. 패턴 A — "시스템 안의 한 알고리즘"을 혁신
가장 흔한 컴퓨터그래픽스/시스템 박사 형태.
- 예: "실시간 렌더링 파이프라인"이 플랫폼이고, 기여는 "새로운 light culling 자료구조".
- 게임 엔진은 기여를 **실제 워크로드로 평가**하는 환경이 된다.

### 2-2. 패턴 B — "시스템 그 자체"가 기여 (Systems 논문)
SOSP/OSDI/EuroSys 계열. 시스템 전체의 새로운 아키텍처가 기여.
- 예: "결정론적 lockstep과 client prediction을 동시에 만족시키는 새 동기화 아키텍처"
- 단, "잘 만든 엔진"이 아니라 **"이 설계가 기존 설계로는 불가능한 무엇을 가능케 하는가"**가 핵심.

### 2-3. 패턴 C — "측정·이해"가 기여 (Empirical / Measurement 논문)
- 예: "상용 MOBA 5종의 netcode를 측정해 lag compensation의 실제 한계를 정량화하고 새 모델을 제시"
- 새 코드보다 **새로운 측정·통찰·일반화 가능한 법칙**이 기여.

> **결론:** "엔진 전체를 자른다"가 아니라, **하나의 분야 → 하나의 미해결 문제(open problem) → 하나의 새 기여 → 엄밀한 검증**. Winters 엔진은 그 검증의 testbed로 쓴다.

---

## 3. Thesis Statement (학위논문 명제) — 논문의 심장

박사 논문 전체는 **단 한 문장**으로 압축되는 명제(thesis statement)를 **증명**하는 글이다. 이 문장이 없으면 박사가 아니다.

좋은 thesis statement의 형식:

```text
"[새로운 기법/구조 X]를 사용하면 [제약 조건 C] 하에서 [목표 Y]를
 [기존 방법 대비 정량적 개선 Z]로 달성할 수 있다."
```

예시:
- "Probe 가시성을 런타임에 점진적으로 학습하는 DDGI 변형은, 동적 파괴 장면에서 light leak 없이 60fps GI를 메모리 동급에서 달성한다."
- "Fiber 기반 work-stealing 스케줄러에 결정론적 재현 레이어를 추가하면, lockstep 시뮬레이션을 멀티코어에서 cross-platform 비트 단위로 재현 가능하다."

검증 가능성(falsifiability)이 핵심이다. **"X는 좋다"는 명제가 아니다.** "X는 C 하에서 Z만큼 Y를 개선한다"가 명제다.

---

## 4. 박사 논문의 표준 구조 (Form & Structure)

분야마다 변형은 있으나, 골격은 거의 보편적이다 (IMRaD의 확장).

```text
┌─ 표제부 (Front Matter)
│   Title Page / 초록(Abstract) / 감사의 글 / 목차 / 표·그림 목록 / 용어집
│
├─ Ch 1. 서론 (Introduction)               ← 논문의 "왜"
│     1.1 동기 (Motivation)
│     1.2 문제 정의 (Problem Statement)
│     1.3 Thesis Statement (한 문장 명제)
│     1.4 기여 목록 (Contributions, bullet 3~5개)
│     1.5 논문 구성 (Outline)
│
├─ Ch 2. 배경 및 관련 연구 (Background & Related Work)  ← 논문의 "지금까지"
│     - 기초 이론 (읽는 이가 따라올 수 있게)
│     - 기존 연구 분류·비판적 분석 (단순 나열 ❌)
│     - "gap": 기존 방법이 못 푸는 지점 명시 → 내 기여의 정당화
│
├─ Ch 3~N. 기여 챕터 (Core / Contribution Chapters)     ← 논문의 "내가 한 것"
│     보통 2~4개. 각 챕터 = 보통 학회 논문 1편.
│     각 챕터 내부: 접근법 → 설계/알고리즘 → 구현 → 그 챕터의 평가
│
├─ Ch N+1. 종합 평가 (Evaluation / Results)             ← 논문의 "증명"
│     - 실험 설계, baseline, metric, ablation, 통계
│     - 한계(limitations)도 정직하게
│
├─ Ch N+2. 논의 (Discussion)
│     - 결과 해석, 일반화 가능성, 위협 요인(threats to validity)
│
├─ Ch N+3. 결론 및 향후 연구 (Conclusion & Future Work)
│     - 기여 재진술, thesis statement가 증명되었음을 정리
│
└─ 참고문헌 (References) / 부록 (Appendices)
```

### "Three Papers Make a Thesis" 모델
현대 공학 박사의 가장 흔한 형태. 각 기여 챕터(Ch 3~N)가 **이미 동료심사를 통과한 학회/저널 논문 1편**에 대응한다. 즉 박사는 "큰 글 하나"가 아니라 **"연결된 논문 3편 + 그것을 묶는 서론·결론"**이다. 이 모델이면 §1의 "분야 목록"에서 **한 분야 안의 인접한 3개 문제**를 푸는 것이 한 학위논문이 된다.

> 예) 렌더링 박사 = [논문1: 새 probe 배치] + [논문2: probe의 시간적 안정화] + [논문3: 저사양 GPU 이식] → "동적 장면 실시간 GI" 학위논문.

---

## 5. 기여의 유형 (Taxonomy of Contributions)

심사위원은 "이 논문의 기여가 어떤 종류인가"를 본다. 게임 엔진 연구에서 가능한 기여 유형:

1. **새 알고리즘** — 더 빠르거나, 더 정확하거나, 더 적은 자원. (대부분의 그래픽스/물리 논문)
2. **새 자료구조** — 더 나은 cache 지역성, 동시성, 메모리. (시스템/병렬)
3. **새 시스템/아키텍처** — 기존 설계로 불가능하던 조합을 가능케. (Systems)
4. **새 이론/모델/증명** — 수렴성, 안정성, 정확도 한계 증명. (이론)
5. **새 경험적 발견** — 측정으로 드러난 일반화 가능한 법칙. (Empirical)
6. **새 평가 방법·벤치마크·데이터셋** — 분야가 공유할 측정 도구. (과소평가되지만 강력)

게임 엔진 박사는 보통 1·2·3의 조합이며, 4(증명)나 5(측정)가 섞이면 강해진다.

---

## 6. 좋은 연구 질문(Research Question)을 찾는 법

박사의 진짜 어려움은 "구현"이 아니라 **"좋은 질문을 찾는 것"**이다.

- **Heilmeier Catechism** (질문 점검 체크리스트):
  1. 무엇을 하려는가? (전문용어 없이)
  2. 지금은 어떻게 하고, 한계는 무엇인가?
  3. 무엇이 새로운가? 왜 성공할 것 같은가?
  4. 성공하면 누가 신경 쓰는가? 어떤 차이가 생기는가?
  5. 위험과 비용은? 얼마나 걸리나? 성공을 **어떻게 측정**하나?

- 좋은 질문의 신호: **"실현 가능 + 새로움 + 측정 가능 + 중요"** 4박자.
- 나쁜 질문의 신호: 너무 큼("게임 AI를 풀겠다"), 측정 불가("더 재미있게"), 이미 풀림, 아무도 신경 안 씀.

---

## 7. 평가 방법론 (Rigor) — 게임 엔진 특화

박사를 "구현"과 가르는 결정적 선. 게임 엔진 연구의 metric:

| 축 | 대표 지표 |
|----|-----------|
| **성능(Performance)** | frame time(ms), throughput(M particles/s), latency(ms), memory(MB), bandwidth(KB/tick) |
| **확장성(Scalability)** | core 수·CCU·엔티티 수 대비 곡선, 약/강 scaling |
| **품질(Quality)** | PSNR, SSIM, FLIP, ΔE; perceptual user study |
| **정확도(Accuracy)** | 수치해와의 오차, ground truth(오프라인 path tracer) 대비 |
| **안정성(Stability)** | 시간적 일관성(temporal flicker), 수렴, 결정론 재현률 |
| **정성(Qualitative)** | 사용자 연구, 전문가 평가, 사례 연구 |

엄밀성의 필수 요소:
- **Baseline 비교**: 최소 1개의 SOTA, 그리고 naive 기준.
- **Ablation study**: 내 기법의 각 구성요소를 하나씩 제거해 "무엇이 효과의 원천인지" 증명.
- **통계적 유의성**: 평균만 ❌. 분산·신뢰구간·여러 시드/장면.
- **재현성(Reproducibility)**: 하드웨어·세팅·데이터 명시, 가능하면 코드·데이터 공개.
- **Threats to validity**: 결과를 못 믿게 만들 수 있는 요인을 스스로 적시.

> Winters 엔진의 강점: §CLAUDE.md의 "inspectable debug UI/overlay + bounded trace" 문화가 곧 **측정 인프라**다. 박사 평가의 절반은 측정 인프라에서 나온다.

---

## 8. 작성·연구 프로세스와 타임라인 (전형적 4~6년)

```text
1년차  : 분야 탐색 → 광범위 문헌조사 → 도구/엔진 testbed 구축
        → 자격시험(Qualifying Exam) / 연구계획서(Proposal)
2년차  : 첫 기여 → 첫 학회 논문 투고 (≒ 기여 챕터 1)
3~4년차: 기여 2·3 → 추가 논문 (≒ 챕터 2·3)
5년차  : 학위논문 집필(서론·결론으로 3편을 하나의 명제로 묶기) → Defense(공개 발표·심사)
```

핵심 원칙:
- **글쓰기는 마지막이 아니다.** 매 기여마다 논문을 써서 동료심사를 통과시킨다(이미 검증된 챕터가 쌓임).
- **논문이 먼저, 학위논문은 나중에 "엮기".** 그래서 thesis statement가 처음부터 명확해야 인접 문제를 고를 수 있다.

---

## 9. 발표 무대 (게임 엔진 분야별 Top Venue)

기여를 "어디에 내느냐"가 분야 정체성을 정한다. 도메인 문서마다 해당 venue를 명시한다.

| 분야 | 학술 Top Venue | 저널 | 산업(비학술, 참고용) |
|------|----------------|------|----------------------|
| 그래픽스/렌더링 | SIGGRAPH, SIGGRAPH Asia, EGSR, HPG, I3D | ACM TOG, IEEE TVCG | GDC, Digital Dragons |
| 물리/시뮬레이션 | SIGGRAPH, SCA(Symp. on Computer Animation), Eurographics | ACM TOG | GDC |
| 게임 AI | AAAI, IJCAI, **IEEE CoG**, **AIIDE**, NeurIPS | IEEE ToG, TCIAIG | GDC AI Summit |
| 병렬/시스템 | **PPoPP**, SOSP, OSDI, EuroSys, ASPLOS | TOPC, TOCS | CppCon, GDC |
| 네트워킹 | SIGCOMM, NSDI, NetGames | ToN | GDC Networking |
| 보안/안티치트 | **IEEE S&P**, USENIX Security, CCS, NDSS | TDSC, TIFS | DEF CON, Black Hat |
| HCI/플레이어 | CHI, CHI PLAY, DiS | — | GDC |

> **주의:** GDC는 권위 있지만 **동료심사 학술 출판이 아니다.** 박사 기여는 학술 venue를 1차로 한다(GDC는 사례·영향력 근거로 인용).

---

## 10. 연구 윤리·인용·재현성

- **표절·자기표절 금지**: 이전 자기 논문도 인용 없이 재사용 불가.
- **인용 규범**: 모든 주장은 (a) 인용 (b) 내 실험 (c) 자명함 중 하나로 뒷받침.
- **데이터·코드 공개**: 재현 가능성은 현대 평가의 핵심. artifact evaluation(예: PPoPP/OSDI badge).
- **인간 대상 연구(user study)**: IRB 승인. 게임 플레이어 연구는 동의·익명화 필수.
- **보안 연구 특칙**: 안티치트 같은 dual-use 주제는 **방어/탐지 관점**으로 프레이밍하고, 책임 있는 공개(responsible disclosure) 원칙을 따른다. (→ `11_Security` 문서 참고)

---

## 11. 흔한 실패와 글쓰기 원칙

흔한 박사 실패 패턴:
- **"구현 자랑"**: 비교·측정 없이 "내가 만든 것" 나열. → 석사 프로젝트 수준.
- **scope 폭주**: §1의 목록을 다 하려다 아무것도 깊지 못함. → 하나만 깊게.
- **약한 baseline**: 일부러 약한 비교군. → 심사에서 즉사.
- **thesis statement 부재**: "이 논문이 증명하는 한 문장"을 못 말함.

글쓰기 원칙:
- 능동태·현재시제, 주장→근거 순서, "왜 이게 중요한가"를 먼저.
- 그림·표가 본문보다 먼저 이해되게. (그래픽스 논문은 figure가 절반의 설득)
- 모든 챕터는 "이 챕터의 기여 한 줄"로 시작·끝맺기.

---

## 12. 심사위원이 보는 4가지 (평가 루브릭)

1. **Novelty (독창성)**: 진짜 새로운가? 기존과 무엇이 다른가?
2. **Rigor (엄밀성)**: 증명/실험이 결론을 지지하는가? 재현 가능한가?
3. **Significance (중요성)**: 누가 왜 신경 쓰는가? 분야를 움직이는가?
4. **Clarity (명료성)**: 읽고 이해·재현할 수 있게 썼는가?

> 한 줄 자가진단: **"내 thesis statement는 무엇이고, 그것을 증명하는 실험은 무엇이며, 무엇과 비교했는가?"** 이 셋을 즉답 못 하면 아직 박사 단계가 아니다.

---

## 13. 도메인 문서 사용법

`01_` ~ `11_` 문서는 각 분야별로 다음을 담는다(공통 템플릿):

```text
0. 이 분야를 박사로 본다는 것 (구현 vs 기여, top venue)
N. 세부 주제마다:
   - 핵심 원리(이론·수학)
   - 대표 기존 연구(SOTA, 핵심 논문)
   - 자료구조/알고리즘(의사코드)
   - 박사급 novel 기여가 될 각도(open problems)
   - thesis statement 예시
   - 평가 방법
   - Winters 엔진과의 연결점(testbed)
말미. 이 분야 통합 학위논문 구조 예시 + 참고문헌
```

각 문서를 읽을 때 항상 §1로 돌아와라: **"이건 구현 항목인가, 기여 후보인가?"**

### 문서 인덱스
- `01_AI_인공지능.md` — Reinforcement Learning, MCTS, Genetic/Evolutionary
- `02_Graphics_그래픽스렌더링.md` — Global Illumination, Deferred, PBR, Forward+, Volumetric Fog, Ray Tracing
- `03_Physics_물리시뮬레이션.md` — IK Solver, Spherical Harmonics, Position Based Dynamics, FFT Ocean, Gerstner Waves
- `04_JobSystem_병렬처리.md` — Chase-Lev Deque, Fiber, ECS, Data-Oriented Design
- `05_Profiler_프로파일러.md` — Sampling/Instrumentation, GPU timing, 자동 병목 탐지
- `06_Editor_에디터_애셋로더.md` — Asset Pipeline, Hot Reload, Streaming, Undo/협업
- `07_FX_이펙트시스템.md` — GPU Particles, Niagara식 데이터 주도 시뮬, 결정론 FX
- `08_WorldPartition_월드파티션.md` — Spatial Partitioning, Streaming, HLOD, Relevance
- `09_Backend_백엔드.md` — Economy, Inventory, Shop, Trade, 일관성·부정방지
- `10_Server_네트워크.md` — Client Prediction, Lag Compensation, Lockstep, Server Authority, AoI
- `11_Security_보안.md` — Kernel/Usermode Anticheat, 서버측 탐지, ML 부정탐지(방어 관점)
```
