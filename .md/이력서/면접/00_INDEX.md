# Winters 도메인 면접 대비 — 인덱스 & 메타 전략

작성일: 2026-06-26
구성: 17개 도메인 × (핵심개념·Trade-off·실제구현·검증·최적화·구현예정·Q&A·피치)
그라운드 트루스: [WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md](../WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md)
이력서/포트폴리오: [RESUME_DRAFT_AND_GUIDE_2026-06-26.md](../RESUME_DRAFT_AND_GUIDE_2026-06-26.md)

> 각 문서는 "이 도메인 면접을 이 문서 하나로 통과한다"를 목표로, **구현된 것과 구현 예정인 것을 같은 깊이로** 다룬다. 모든 주장은 정직성 지도의 레드플래그를 위반하지 않는다(과장=실패). 17개 에이전트가 코드를 직접 까서 작성했고, 정직성 지도와 코드의 일치를 교차검증했다.

## 문서 목록 (상태 + 한 줄)

| # | 문서 | 상태 | 본질 |
|---|---|---|---|
| 1 | [01_RHI_DX11_DX12](01_RHI_DX11_DX12.md) | working | DX12/VK 명시 모델 기준 핸들 추상화, DX11 production + DX12 parity |
| 2 | [02_RENDERER_GRAPHICS](02_RENDERER_GRAPHICS.md) | working | DX11 포워드 + GTAO SSAO + Cook-Torrance PBR + stylized + FoW |
| 3 | [03_ECS_DATA_DRIVEN](03_ECS_DATA_DRIVEN.md) | working | sparse-set ECS + 접근충돌 병렬 스케줄러 + JSON→codegen 불변 pack |
| 4 | [04_JOBSYSTEM_FIBER_PROFILER](04_JOBSYSTEM_FIBER_PROFILER.md) | working | Chase-Lev work-stealing JobSystem + CPU/GPU 프로파일러 |
| 5 | [05_NETWORK_IOCP_SNAPSHOT](05_NETWORK_IOCP_SNAPSHOT.md) | working | IOCP TCP 권위 서버 + FlatBuffers 스냅샷 복제 (UDP 설계) |
| 6 | [06_SERVER_AUTHORITY_SIM](06_SERVER_AUTHORITY_SIM.md) | working | 30Hz 결정론 ECS 권위 루프 Input→Snapshot→Visual |
| 7 | [07_CHAMPION_SKILL_GAS](07_CHAMPION_SKILL_GAS.md) | working | 함수포인터 훅 레지스트리 + 15챔피언 + 데이터주도 스킬 파라미터 |
| 8 | [08_AI_HFSM_UTILITY_BOT](08_AI_HFSM_UTILITY_BOT.md) | working | command-only 결정론 라인전 봇 (BT/MCTS/RL은 PoC) |
| 9 | [09_FX_EFFECT_TOOL](09_FX_EFFECT_TOOL.md) | working | WFX 데이터주도 큐 런타임 + 서버 cue 1회 재생 |
| 10 | [10_ASSET_PIPELINE_WFORMAT](10_ASSET_PIPELINE_WFORMAT.md) | working | .wmesh/.wskel/.wanim/.wmat end-to-end 쿡→로드→재생 |
| 11 | [11_ELDENRING_CLIENT_EDITOR](11_ELDENRING_CLIENT_EDITOR.md) | prototype | 원작 바이너리→Winters 바이너리 복원 + 쇼케이스/에디터 (게임플레이 0) |
| 12 | [12_PERFORMANCE_MEASUREMENT](12_PERFORMANCE_MEASUREMENT.md) ⭐ | working | 프로파일러+GPU타임스탬프+JSON캡처 (측정주도 개발의 척추) |
| 13 | [13_UI_PIPELINE](13_UI_PIPELINE.md) | working | DX11 배치 스프라이트 렌더러 + Atlas 매니페스트 + 데이터주도 HUD |
| 14 | [14_BACKEND_GO](14_BACKEND_GO.md) | working | 6개 Go 마이크로서비스 + WinHTTP C++ 실연동 |
| 15 | [15_SECURITY_ANTICHEAT](15_SECURITY_ANTICHEAT.md) | working | 서버권위 입력검증 1차방어 (유저모드/커널은 로드맵) |
| 16 | [16_COLLAB_TOOLING_BUILD](16_COLLAB_TOOLING_BUILD.md) | working | SimLab 결정론 골든 + 검증 하니스 + CMake/Ninja |
| 17 | [17_ANIMATION_PHYSICS_AUDIO](17_ANIMATION_PHYSICS_AUDIO.md) | working(약점) | 스켈레탈 애니 + SAT 충돌수학 + 2D FMOD 래퍼 |

## 학습/암기 우선순위 (엔진 제너럴리스트 기준)

자체엔진은 "다 안다"가 아니라 "내가 만든 것을 깊게 안다"가 핵심이다. 아래 순서로 굳혀라.

**Tier 1 — 어떤 면접에서도 무조건 (이 4개는 완벽 암기)**
1. **#12 측정 인프라** ⭐ — 모든 도메인의 검증 서사가 여기서 나온다. "측정 먼저, 그 측정이 내 한계를 드러냄"이 당신 서사의 척추.
2. **#1 RHI** — 엔진 프로그래머의 간판. explicit 모델 5개념·핸들·DLL 경계를 막힘없이.
3. **#6 서버권위 시뮬** — 게임서버/네트워크의 간판. Input→Command→Snapshot 루프·결정론.
4. **#3 ECS/데이터주도** — 시스템 설계 사고의 증거. sparse-set vs archetype·접근충돌 병렬.

**Tier 2 — 지원 직무에 따라 1~2개 추가**
- 서버/네트워크 지원 → #5 네트워크, #16 협업/검증(SimLab)
- 그래픽스 지원 → #2 렌더러, #10 에셋포맷
- 게임플레이 지원 → #7 챔피언스킬, #8 AI

**Tier 3 — 물어보면 정직하게 (깊이보다 경계가 중요)**
- #9 FX, #13 UI, #14 백엔드, #11 EldenRing, #15 보안, #17 애니/물리/오디오
- 이들은 "여기까지 했고 여기부터 계획"의 경계만 분명하면 충분. 약점 도메인일수록 정직한 경계가 신뢰를 만든다.

## 모든 도메인 공통 — 메타 면접 전략

### 1. 4박자로 답하라
어떤 질문이든 **문제 → 접근 → 검증 → 회고**로 답하면 "사고의 루프를 돈다"가 드러난다. "X를 구현했습니다"(기능 나열)가 아니라 "Y가 문제여서 Z로 접근하고 W로 검증했고, 그 검증이 V라는 한계를 드러냈습니다".

### 2. 정직성을 무기로
면접관은 과장을 잡으려 든다. 스스로 그어둔 선은 못 잡는다. 압박 질문("그거 실제로 돼요?")이 오면 **먼저 인정하고("솔직히 production은 X까지입니다") 그 다음 진짜 강점으로 전환**한다. 각 문서의 §7에 이 패턴의 모범답변이 있다.

### 3. 측정으로 말하라
"빨라졌다/최적화했다"는 약하다. "프로파일러 JSON으로 측정했고, F4로 지금 캡처해 보여드릴 수 있습니다"가 강하다. 정량 수치가 없으면 "측정 예정"이라고 정직하게. (수치를 외워 말하다 틀리면 더 위험.)

### 4. 도메인을 가로지르는 단골 질문 (cross-domain)
- **"왜 자체엔진을 만들었나?"** → 도피가 아니라 진단의 결과. 단일스레드/DX11 한계를 *프로파일러로 측정해 증명한 뒤* 만들었다(#12 + #1). 동료들은 엔진을 전제했고 나는 엔진을 의심했다.
- **"이 많은 걸 혼자 다 했나? 어디까지 진짜인가?"** → README 정직성 표와 도메인별 검증 게이트로 스스로 경계를 긋는다. 측정 인프라·SimLab 골든·검증 하니스가 "진짜 동작"의 증거(#12, #16).
- **"팀에서 협업되나?"** → Client/Engine/Shared 의존성 규칙을 빌드/하니스로 강제, ownership matrix·work packet으로 다중 작업자 전제 설계(#16). SR_Minecraft 팀 경험.
- **"결정론을 어떻게 보장/증명하나?"** → 고정 dt + DeterministicRng + 정렬 순회, SimLab이 same-seed 상태 해시 일치 + seed 민감도를 exit code로 게이트(#3, #6, #16). 단 크로스플랫폼 FP 재현성은 검증 범위 밖이라 솔직히 인정.
- **"가장 어려웠던 문제는?"** → 측정이 내 병렬 스케줄러가 실게임에서 한 번도 안 돈다는 걸(MaxBatchSize=1) 드러낸 일. 숨기지 않고 다음 작업으로 정의한 것이 가장 자랑스럽다(#4, #12).

### 5. 절대 쓰면 안 되는 표현 (정직성 지도에서 — 면접 즉사)
Vulkan/Metal/Console RHI · RenderGraph/GI/Nanite/PathTracing · Fiber 기반 잡시스템 · UDP 넷코드/AOI · 1판 완결/승패종료 · BT/MCTS/RL 봇 · 결제 시스템 · **Perforce 경험(코드 없음, P4=Phase4)** · 유저모드/커널 안티치트 · 물리 엔진 · 300~650 FPS 달성 · CMake 완전 전환. (→ 각 도메인 §7에 정직한 대체 답변 있음)

## 사용법
1. Tier 1 네 문서를 먼저 정독 → §7 Q&A를 입으로 답해본다(특히 압박 질문).
2. 지원 공고가 뜨면 직무에 맞춰 Tier 2를 추가, 이력서 도메인 순서도 재배치([RESUME_DRAFT §2-1 직무별 강조 스위치]).
3. 각 문서 §8 엘리베이터 피치를 30초 안에 막힘없이 말할 때까지.
4. 면접 전 F4로 프로파일러 JSON 1개 캡처해 두면 #12·#4의 "보여드릴 수 있다"가 실물이 된다.
