# 2026-07-17 SystemScheduler 병렬 배치 점화 리포트 (S042)

## 1. 결과 요약

리플레이 실측 시나리오(room1_tick1_1393.wrpl, Release+WINTERS_PROFILING)에서 스케줄러 병렬 배치가 **한 번도 발화하지 않던 상태(MaxBatchSize=1)를 실제 발화 상태로 전환**하고, 전/후를 각각 3런×6000프레임 실측했다.

| 카운터 (전 프레임 상수) | baseline ×3 | after ×3 |
|---|---|---|
| Scheduler::MaxBatchSize | 1 | **2** |
| Scheduler::ParallelBatches | 0 | **1** |
| Scheduler::SequentialBatches | 5 | 3 |
| Scheduler::SubmittedJobs | 0 | **2** |

워커 실행 실증(캡처 보존 rawEvents 64프레임, threadId 기준): `LocalStatus::Execute` 10~21회, `Vision::Execute` 16~24회, `Vision::TickVisibility` 1/1회가 비메인 스레드에서 실행. 나머지 프레임은 `WaitForCounter`의 help-execute 설계대로 메인 스레드가 잡을 직접 소화한 것 — 정상.

프레임 통계는 회귀 없음(baseline median 0.989~1.086ms vs after 0.808~1.325ms, 범위 겹침·드롭 0·Draw::Total 동등). **개선 %는 주장하지 않는다** — 원장 ±18% 변동 규칙 + 아래 §5 오염 캐비앗.

## 2. 원인 진단 (왜 MaxBatchSize=1이었나)

1. **Phase 파티셔닝이 1차 원인**: 배칭은 phase 내부에서만 일어나는데(SystemScheduler.cpp RebuildExecutionPlan), 리플레이/네트워크 모드 등록 시스템 5개가 전부 다른 phase(Transform=0, SpatialHash=1, LocalStatusEffect=4, Vision=5, YoneSoulSpawn=9)에 1개씩 — 선언과 무관하게 모든 배치가 크기 1. 오프라인 모드의 유일한 2-시스템 phase(2: LocalUnitAI+NavigationThrottle)도 `NavAgentComponent` Write가 진짜 충돌이라 분리됨.
2. **잠재 2차 원인**: `ISystem::DescribeAccess` 기본값이 `UnknownWritesAll()`(전충돌)이고 12개 등록 시스템 중 7개가 미오버라이드 — phase를 합쳐도 선언이 없으면 배치가 안 묶인다.
3. JobSystem은 무죄: Chase-Lev 레이스는 수정 완료(owner-only push + 전역 MPMC 큐, `PushToSomeDeque` 소멸), `Initialize(0)`=hw_concurrency−2 워커, 스케줄러 `m_pJobSystem` 실게임 non-null. 병렬 분기는 배치 크기만 채워지면 즉시 동작하는 상태였다.

## 3. 변경 내용 (4파일, 외과적)

- `Client/Public/GamePlay/Systems/LocalStatusEffectSystem.h`: GetPhase 4→5 (Vision과 co-phase) + DescribeAccess 선언.
- `Client/Private/GamePlay/System/LocalStatusEffectSystem.cpp`: `Write<Stun/Slow/Disarm>` 선언.
- `Engine/Public/ECS/Systems/VisionSystem.h` (+EngineSDK 자동 미러): DescribeAccess 선언.
- `Engine/Private/ECS/Systems/VisionSystem.cpp`: `Write<Visibility> + Read<Transform/SpatialAgent/VisionSource/VisionCone>` 선언.

phase 5 배치 = [LocalStatusEffect, Vision] — 접근 집합 disjoint → 스케줄러가 자동으로 한 배치로 묶어 잡 2개 제출 + WaitForCounter 배리어.

## 4. 안전 논증 (co-batch 성립 조건)

- **의미 보존**: Status의 소비자/생산자는 phase 4~5 사이에 없음(오프라인 AI는 phase 2, 스킬 CC 적용은 씬 코드). Vision과는 컴포넌트 집합이 disjoint라 상대 순서 자체가 무의미.
- **스레드 안전**: 두 시스템 모두 순수 CPU. Vision FoW는 시스템 소유 CPU 벡터(`m_vecFowTexture`)+dirty 플래그, D3D 호출 없음. SpatialIndex/ConcealmentIndex 조회는 읽기 전용.
- **스토어 레이스 없음**: `HasComponent`/`ForEach`는 find-only(엔티티 시그니처 마스크 없음 — 스토어별 sparse set). 위험 API는 `GetOrCreateStore` 경유 lazy-insert(`GetComponent` non-const, `RemoveComponent`)인데, 두 시스템 모두 ForEach/Has 가드 뒤에서만 호출 → 스토어가 이미 존재하는 find 경로만 탄다.
- **SimLab/서버 무관**: `CSystemSchedular`는 Client Scene_InGame 전용. Server는 시스템 Execute를 직접 호출, GameSim은 자체 스케줄러 없음 — 골든 해시 비노출.

## 5. 측정 방법과 오염 캐비앗

- 절차: `Tools/Profiler/run_profile_session.ps1 -SkipBuild -Label ...` ×3 (전) → 변경 → Engine+Client Release 재빌드 → ×3 (후). 캡처별 `Scheduler::*` 카운터는 `frames[].counters`에서 min/median/max 추출(6000프레임 전수).
- **오염 1**: 병행 Codex 세션이 baseline 빌드(20:47)와 after 빌드(21:0x) 사이에 Client 파일 6종+데이터 JSON을 수정 — 프레임 시간 비교는 "회귀 없음"까지만 유효, 개선 주장 불가. 구조 카운터는 스케줄러 실행 계획의 결정적 산물이라 오염 무관.
- **오염 2 (빌드 우회)**: Codex 진행 중 편집으로 `Minion_Manager.cpp`/`SnapshotApplier.cpp`의 `MinionCombatDef.h` include가 제거된 상태 — 소스 무수정 원칙을 지키기 위해 두 빌드 모두 `CL=/FI...MinionCombatDef.h` 환경변수 주입으로 컴파일. 그쪽 세션 수복 후에는 불필요.
- flatc 코드젠 1회 실패는 알려진 병렬 빌드 충돌(gotcha) — 재시도로 통과.

## 6. 남은 것 / 다음 슬라이스

- MaxBatchSize≥3은 리플레이 모드에서는 현 구조상 안전한 후보가 없음(Tx는 Transform Write로 전면 충돌, Yone은 entity 생성+D3D로 격리 필수). 후보를 늘리려면 (a) 오프라인 모드 phase 재설계(Nav/AI 데이터 플로우 재검증 필요), (b) 비-ECS 자원(SpatialIndex 등)의 태그 타입 선언 모델 도입 후 phase 통합.
- 이력서/면접 문서의 "MaxBatchSize=1 — 다음 최적화 대상" 서술은 이제 "co-phase+정직 선언으로 병렬 배치 실발화(MaxBatchSize=2, ParallelBatches=1/frame, 워커 실행 실측)"로 갱신 가능 — 문서 수정은 사용자 확인 후.
- 진짜 프레임 시간 병목은 여전히 로딩 G5(비동기 로드)와 GPU/드로우 쪽 — 이 변경은 "병렬 인프라가 실게임에서 실제로 돈다"는 구조 증명이 목적.
