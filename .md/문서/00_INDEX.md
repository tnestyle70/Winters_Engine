# Winters AAA 엔진 — 챕터 deep-dive 인덱스

> 박제 일자: 2026-05-13
> 작성자: Claude (Opus 4.7 1M context)
> 위상: long-horizon 아키텍처 brief. S 시퀀스(S1~S10)와 직각.
> 현재 진행은 그대로: **S10 BotAIStage1 후속 — Death/TargetInvalid/Respawn 안정화.**

---

## 읽는 순서

### A. 처음 보는 경우
1. [WINTERS_AAA_SCALE_ENGINE_MASTER_BRIEF.md](WINTERS_AAA_SCALE_ENGINE_MASTER_BRIEF.md) — 마스터 brief (16 챕터 요약)
2. 본 [00_INDEX.md](00_INDEX.md) — 이 파일
3. 관심 챕터로 직진

### B. 챕터 의존 순서 (UE5 패턴 기반)
```text
Ch13 Tooling (UBT/UHT/Reflection)
    └→ Ch12 Editor (Stage B+)
        └→ Ch15 Data Pipeline (Editor 통합)

Ch1 RHI
    └→ Ch2 RenderGraph
        └→ Lighting / GI / VT / Nanite

Ch15 Data + Ch12 Editor
    └→ 모든 디자이너 워크플로

Ch4 Animation ─┐
Ch6 Audio     ─┼→ AnimNotify / GameplayCue 합류 → Ch8 GAS
Ch8 GAS       ─┘

Ch5 Physics
    └→ Ch9 AI Navigation + Ch4 Ragdoll

Ch3 World Partition (큰 월드만)
    └→ Ch4 Motion Matching (streaming 의존)

Ch7 Networking
    └→ Ch8 GAS prediction
    └→ Ch14 Services (게임 외부 통신)

Ch11 Cinematic
    └→ Ch4 + Ch6 + Ch8 + Ch10 + DynamicCamera 통합

Ch10 UI
    └→ Ch12 Editor UI 재사용

Ch16 Collaboration ← 모든 챕터를 가능하게 하는 사회적 인프라
```

### C. 현재 Winters 상태 기준 우선순위
1. **S10 안정화** (현재 진행) — 본 brief와 직각
2. **Ch7 Networking IOCP × Fiber** (memory `project_fiber_mastery_session_2026_05_11.md` 박제)
3. **Ch1 RHI** (DX12 Client visual parity)
4. **Ch8 GAS 1차** (Phase B-11d Ezreal 작업의 정형화)
5. **Ch4 Animation Stage 1~3** (castFrame 하드코드 탈피)
6. **Ch13 Tooling Stage 1** (.vcxproj 손 관리 탈피)
7. **Ch12 Editor Stage B** (디자이너 entry)

---

## 챕터 목록

| 챕터 | 주제 | 파일 | 우선순위 (Winters 현재) |
|------|------|------|------------------------|
| Ch1 | RHI (DX12/Vulkan/Metal/Console) | [01_Ch1_RHI.md](01_Ch1_RHI.md) | 높음 |
| Ch2 | RenderGraph / GI / VT / Nanite | [02_Ch2_RenderGraph_Lighting_GI.md](02_Ch2_RenderGraph_Lighting_GI.md) | 중 |
| Ch3 | World Partition / Streaming | [03_Ch3_WorldPartition_Streaming.md](03_Ch3_WorldPartition_Streaming.md) | 낮음 (LoL 한정 시) |
| Ch4 | Animation (StateMachine / Montage / IK) | [04_Ch4_Animation.md](04_Ch4_Animation.md) | 높음 |
| Ch5 | Physics (Rigid / Cloth / Destruction) | [05_Ch5_Physics.md](05_Ch5_Physics.md) | 낮음~중 |
| Ch6 | Audio (3D / DSP / Metasound) | [06_Ch6_Audio.md](06_Ch6_Audio.md) | 중 |
| Ch7 | Networking (IOCP / AOI / Replication) | [07_Ch7_Networking.md](07_Ch7_Networking.md) | **최우선** |
| Ch8 | GAS (Tags / Effects / Attributes) | [08_Ch8_GAS.md](08_Ch8_GAS.md) | **최우선** |
| Ch9 | AI (BehaviorTree / EQS / NavMesh) | [09_Ch9_AI.md](09_Ch9_AI.md) | 중 (S10 후속) |
| Ch10 | UI (UMG-tier / DataBinding) | [10_Ch10_UI.md](10_Ch10_UI.md) | 중 |
| Ch11 | Cinematics / Sequencer | [11_Ch11_Cinematics_Sequencer.md](11_Ch11_Cinematics_Sequencer.md) | 낮음 |
| Ch12 | Editor (ContentBrowser / DetailsPanel) | [12_Ch12_Editor.md](12_Ch12_Editor.md) | 중 |
| Ch13 | Tooling (UBT / UHT / DDC / Cooker) | [13_Ch13_Tooling.md](13_Ch13_Tooling.md) | 높음 |
| Ch14 | Services (Auth / Match / Telemetry) | [14_Ch14_Services.md](14_Ch14_Services.md) | 중 |
| Ch15 | Data Pipeline (DataTable / Asset) | [15_Ch15_Data_Pipeline.md](15_Ch15_Data_Pipeline.md) | 중 |
| Ch16 | Cross-discipline Collaboration | [16_Ch16_Collaboration.md](16_Ch16_Collaboration.md) | 항시 |

---

## 각 챕터 공통 구조

각 챕터는 동일 5섹션:

1. **기초 원리** — 왜 이 시스템이 필요한가
2. **핵심** — UE5 실제 코드 인용 (파일 경로 + 라인 + 실제 코드 발췌)
3. **심화** — production tier에서 신경 쓸 것들
4. **Winters 매핑** — 현재 상태 + 추가할 헤더/모듈 + 단계별 도입 + 게임별 적용
5. **검증 명령** — smoke test, 기대 로그
6. **다음 챕터로** — 의존 관계

---

## UE5 레퍼런스 위치

```text
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\
├── Engine\
│   ├── Source\
│   │   ├── Runtime\           188개 모듈
│   │   ├── Editor\            143개 모듈
│   │   ├── Developer\
│   │   ├── Programs\          UBT, UHT, Insights
│   │   └── ThirdParty\
│   ├── Plugins\Runtime\
│   │   ├── GameplayAbilities\       (Ch8 출처)
│   │   ├── Metasound\               (Ch6 출처)
│   │   └── ReplicationGraph\        (Ch7 출처)
│   └── Plugins\Online\OnlineSubsystem\  (Ch14 출처)
```

---

## Winters 실코드 anchor 위치

```text
C:\Users\user\Desktop\Winters\
├── Engine\Public\               엔진 SDK 헤더
│   ├── RHI\                     Ch1
│   ├── Renderer\                Ch2
│   ├── Resource\                Ch4 (Animation 1차)
│   ├── ECS\                     공통
│   ├── AI\                      Ch9
│   ├── Manager\                 Ch9 Navigation
│   └── Sound\                   Ch6 FMOD wrap
├── Engine\Private\              엔진 구현
├── Client\Public, Private\      클라
│   ├── Scene\                   씬 + bridge 계층
│   ├── GameObject\Champion\     챔프별 코드 (현재 분산, Ch8로 통합 대상)
│   ├── UI\                      Ch10
│   └── Network\                 Ch7 클라
├── Server\Public, Private\      서버
│   ├── Network\                 Ch7 IOCP × Fiber
│   └── Game\                    GameSim host
├── Shared\
│   ├── GameSim\                 Ch9 AI / Ch8 GAS / Ch7 server-side
│   ├── Network\                 Ch7 schema
│   ├── Replay\                  Ch7 R0~R3
│   └── Schemas\                 FlatBuffers
├── Services\                    Ch14 (Go monorepo)
└── Tools\                       Ch13
```

---

## 다음 단계 제안

1. 본 brief를 사용자가 1회 검토
2. 우선순위 챕터 1~2개 선정 (`/handoff` 또는 직접 지시)
3. 해당 챕터의 **1년 분 sub-plan** 작성 — 파일별/anchor별/검증 단위
4. sub-plan 기반으로 PR 단위 작업 시작

본 brief 자체는 **출력만**. 코드 0 변경. Rollback 불필요.
