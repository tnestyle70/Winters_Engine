Session - profiler.json 측정 근거로 엔진 전체(JobSystem/ECS/스케줄링/Fiber/인스턴싱/GPU Driven/드로우콜)를 Phase 0~7로 리팩터링하고, 각 Phase를 frame budget으로 게이트한다.

이 문서는 마스터 로드맵이다. 각 Phase의 개별 세션은 착수 시점에 `.md/계획서작성규칙.md` 형식(기존 코드/교체 코드 전문)으로 별도 계획서를 작성한 뒤 사용자 확인 후 반영한다.

---

## 0. 측정 근거 (profiler.json, 2026-06-11 캡처)

하드웨어: i5-13500HX (6P+8E, 14C/20T), RTX 4060 Laptop, Windows 11.
캡처 조건: F5 인게임, 60FPS 리미터 ON (`Frame::LimiterActive=1`).

| 항목 | 측정값 | 위치 |
|---|---|---|
| Frame | 9.54ms (EMA 12.95ms) | 전 스코프 threadId 11196 = 메인 스레드 단일 실행 |
| Update | 3.78ms (EMA 6.44ms) | `Scene_InGame::OnUpdate` |
| Render | 5.37ms (EMA 6.14ms) | `Scene::Render` 4.44ms + `Render::EndFrame` 0.62ms |
| Champion::AnimUpdate | 1.93ms | `Client/Private/Scene/Scene_InGame.cpp:2999` — 메인 스레드 CPU 애님 샘플링 |
| Vision::UpdateFow + TickVisibility | 1.44 + 0.86ms | `Engine/Private/ECS/Systems/VisionSystem.cpp:302, :137` |
| Map::DrawFrustumCulled | 1.19ms | `ModelRenderer::BuildClipVisibilityMask` 0.93ms — 1080 서브메시 매 프레임 전수 AABB 테스트 |
| Minimap::Icons | 0.75ms | 아이콘 56개, `Client/Private/UI/MinimapPanel.cpp:353` |
| UIOverlay::Render | 0.79ms | 체력바 1개당 쿼드 3~4개 개별 DrawImage |
| Scheduler | 0.63ms (EMA 1.67ms) | `Scheduler::ParallelBatches=0`, `MaxBatchSize=1` — 병렬 0% |
| SpatialIndex::Rebuild | 0.26ms | 매 프레임 full clear+rebuild, 108엔티티 |
| Render::EndFrame | 0.62ms | Present (VSync off) |
| 드로우콜 | 합계 ~94 (인덱스 ~47만) | GPU 부하 극소 — 완전 CPU 바운드 |
| BuildClipVisibilityMask 호출 | 109회/frame | 맵 1회 + 캐릭터별 매 프레임 재계산, 캐시 없음 |

병렬 인프라 현황:
- JobSystem 워커 = 코어-2 = 18개 (`Engine/Private/Core/JobSystem.cpp:49-50`), Chase-Lev work-stealing deque + 글로벌 mutex 큐. `WaitForCounter`(:306-360)는 help-steal + yield 스핀, 유휴 시 1ms CV 타임아웃. 어피니티/우선순위 미설정.
- Fiber: `Engine/Public/Core/Fiber/Fiber.h`, `FiberTypes.h`에 `eJobExecutionMode::FiberShell` 골격 존재. 잡마다 임시 CreateFiber/DeleteFiber — 풀 없음, 기본은 ThreadOnly.
- ECS: sparse-dense set(`Engine/Public/ECS/ComponentStore.h:63-65`) + `std::unordered_map<std::type_index, ...>`(`World.h:39`). ForEach는 `std::function` 경유.
- SystemScheduler(`Engine/Private/ECS/SystemScheduler.cpp`): phase별 access-conflict 배치. `kMinParallelBatchSize=2`인데 `MaxBatchSize=1` → 모든 배치가 1개 시스템이라 병렬 실행이 한 번도 안 일어남.
- 렌더: DX11 immediate 모드, 메인 스레드. `IRHICommandList`에 `DrawIndexedIndirect` 스텁만 존재. 인스턴싱 미사용. CPU 스키닝 + 드로우마다 본 팔레트 32KB CB 업로드(`Engine/Private/Renderer/ModelRenderer.cpp:425-465`).
- dt 클램프는 0.1s 상한으로 이미 재설계됨(`Engine/Private/Core/CTimer.cpp:14-38`) — 과거 16.7ms 하드클램프 이슈는 해소.

---

## 이론 FPS 분석 (현재 노트북 기준)

전제: 현재 씬 규모(챔피언 ~11, 미니언 60, 정글 12, 맵 1080 서브메시). GPU는 한계와 무관(RTX 4060에서 47만 인덱스/94드로우는 1ms 미만). 상한은 전적으로 CPU 크리티컬 패스가 결정한다.

| 단계 | 메인 스레드 크리티컬 패스 | 이론 FPS |
|---|---|---|
| 현재 (캡 해제 시) | 9.5~13ms | **77~105 FPS** |
| Phase 1 완료 (알고리즘/캐싱) | ~6.5ms | ~150 FPS |
| Phase 2 완료 (병렬화) | ~4.5ms | ~220 FPS |
| Phase 3~4 완료 (ECS 재설계 + 렌더 스레드 분리) | ~2.5~3.0ms | ~330~400 FPS |
| Phase 5~7 완료 (Fiber/GPU 스키닝/인스턴싱/캐싱) | ~1.5~2.0ms | **500~650 FPS (이론 상한)** |

이론 상한의 바닥: Present+드라이버+DWM 합성 ~0.8ms, 프레임 패킷 동기화 ~0.3ms, UI/ImGui 잔여 ~0.4ms. 창 모드 DWM이 추가 변동을 만들므로 650FPS 이상은 전체화면 독점/tearing 허용 없이는 무의미.

현실적 해석: 같은 씬에서 **지속 300~450 FPS**가 달성 가능한 목표이고, 이 리팩터링의 실제 가치는 "씬 10배(미니언 600+, FX 풀가동, 5v5 한타)에서 144~240 FPS 고정 + 1% low 안정화"다. 지금 9.5ms 프레임은 작은 씬을 비효율적으로 처리한 결과라서, 최적화 없이 씬을 키우면 선형 이상으로 무너진다.

---

## Phase 0 - 측정 인프라 정비 (모든 Phase의 게이트 기준)

목표: 캡 해제 베이스라인 + 신뢰 가능한 카운터 + 세션별 JSON 아카이브. 이거 없이는 모든 Phase의 성공/실패 판정이 불가능.

세션:
1. **프로파일러 카운터 중복 버그 수정** — `Engine/Private/Core/Profiler/CPUProfiler.cpp:114` 포인터 비교를 `SameProfilerName`(strcmp)으로 교체. 현 profiler.json에 `Mesh::DrawCalls`가 2번 찍히는 원인.
2. **rawEvents 트렁케이션 플래그** — `ProfilerTypes.h` 256 캡 도달 시 `"truncated": true`를 JSON에 기록.
3. **GPU 타임스탬프 쿼리 추가** — DX11 `ID3D11Query`(TIMESTAMP/DISJOINT)로 GPU 프레임 시간을 `GPU::Frame` 카운터로 노출. CPU 바운드 가설을 매 Phase 검증.
4. **세션별 캡처 아카이브** — F4 저장 경로를 `profiler.json` → `Profiles/profiler_YYYYMMDD_HHMMSS.json`으로 변경, 리미터 OFF 토글 키 추가(60FPS 캡 해제 측정용).
5. **멀티스레드 캡처 확인** — 워커 스레드 스코프가 rawEvents에 threadId별로 남는지 확인(Phase 2부터 필수).

검증 budget: 캡 해제 베이스라인 JSON 1개 확보 (`Frame` EMA, `GPU::Frame`, 중복 카운터 0건).
의존성: Engine 내부만. Client/Shared 영향 없음.

---

## Phase 1 - 메인 스레드 핫스팟 제거 (알고리즘·자료구조, 구조 변경 없음)

목표: Frame 9.5ms → 6.5ms. 아키텍처를 바꾸기 전에 중복/재계산 경로부터 제거한다(gotcha: 새 병렬 경로 전에 기존 중복 제거 우선).

세션:
1. **맵 컬링 캐시 + 청크화** — `Engine/Private/Resource/Model.cpp:587-650`. (a) 카메라 VP가 임계값 이상 변할 때만 마스크 재계산(탑다운 카메라라 프레임 간 변화 미세), (b) 1080 서브메시 전수 루프를 균등 그리드 청크(예: 16x16 셀)로 묶어 청크 AABB 선기각 후 서브메시 테스트. 예상 -0.9ms.
   - 측정: `Model::ClipVisibleSubmeshes`, 신규 `Map::CullChunkRejected`, `Map::CullMaskReused`.
2. **캐릭터별 BuildClipVisibilityMask 캐시** — 109회/frame 중 캐릭터 호출은 서브메시 수가 적으므로 bounds 1개로 통합 테스트 후 통과 시 마스크 전부 visible 처리. 예상 -0.15ms.
3. **SpatialIndex 증분 갱신** — `Engine/Private/ECS/SpatialIndex.cpp:18-58`. full clear+rebuild를 셀 변경 엔티티만 remove/insert로 교체, `unordered_map<i64,vector>`를 평탄 그리드 배열 + 프리리스트로 교체. 예상 -0.2ms.
   - 측정: 신규 `SpatialIndex::MovedEntities`.
4. **FOW dirty-region + 주기 하향** — `VisionSystem.cpp:302-401`. 64x64 텍스처 전체 갱신을 소스 이동 셀 주변 dirty rect로 한정, 갱신 주기를 10~15Hz로 내리고 프레임 분산(stagger). 예상 -1.0ms (이미 격프레임이면 절반).
   - 측정: 신규 `Fow::DirtyTexels`, `Vision::SourcesUpdated`.
5. **미니맵 아이콘 배칭** — `MinimapPanel.cpp:136-190, :353-360`. 아이콘 56개 x 2~4 드로우를 단일 아틀라스 + 쿼드 버퍼 1회 제출로 교체. 월드→미니맵 투영은 위치 변경 엔티티만 재계산. 예상 -0.6ms.
6. **체력바/오버레이 쿼드 배칭** — `Engine/Private/Manager/UI/UI_Manager.cpp:3505-3716`. 엔티티별 DrawImage 3~4회를 프레임당 동적 버텍스 버퍼 1개에 누적 후 1드로우. 예상 -0.5ms.
7. **ECS ForEach 디스패치 비용 제거** — `World.h:104-139`의 `std::function` 시그니처를 템플릿 람다 직접 호출로 교체(할당/간접호출 제거). Update 내 미계측 구간(~0.65ms)의 주요 후보. 예상 -0.3ms.

검증 budget: `Frame` EMA 6.5ms 이하, `Map::DrawFrustumCulled` 0.3ms 이하, `Minimap::Render` 0.35ms 이하, `UIOverlay::Render` 0.4ms 이하, FOW 시각 결함 없음.
의존성: 전부 Engine 내부 또는 Client 내부. LoL 전용 타입을 Engine public API로 올리지 않는다(맵 청크 컬링은 generic static mesh 경로로 구현).

---

## Phase 2 - JobSystem 정비 + 프레임 내 병렬화

목표: Frame 6.5ms → 4.5ms. 잡을 "낼 수 있는 구조"를 "실제로 내는 구조"로.

세션:
1. **워커 수/어피니티/대기 전략** — `JobSystem.cpp:49-50` 워커 18개를 게임플레이용 P-코어 5~6개 + 백그라운드용 E-코어 풀로 분리. `SetThreadPriority`/`SetThreadAffinityMask` 적용. 유휴 워커 yield 스핀을 이벤트 기반 블로킹(잡 제출 시 wake)으로 교체 — 외부 프로세스와의 코어 경쟁(스터터 원인 3) 제거.
   - 측정: 신규 `Job::WorkerWakeups`, `Job::StealAttempts`, OS 컨텍스트 스위치 비교.
2. **Champion/Minion AnimUpdate 병렬화** — `Scene_InGame.cpp:2999-3009`의 ForEach를 엔티티 슬라이스 parallel_for 잡으로 분할(애니메이터가 엔티티별 독립 상태임을 먼저 확인 — 확인 필요: `CAnimator::Update`의 공유 상태 유무). 1.93ms / 5워커 ≈ 0.4ms. 예상 -1.5ms.
3. **Vision 잡화** — TickVisibility(0.86ms)를 소스별 잡으로 분할, UpdateFow는 Phase 1의 dirty-region 결과를 잡에서 생성 후 메인에서 텍스처 업로드만. 예상 -0.5ms (메인 스레드 기준).
4. **Scheduler 병렬 배치 활성화** — `SystemScheduler.cpp`. `MaxBatchSize=1`의 원인 분해: 각 시스템 `DescribeAccess`의 Write 선언이 과대한지, phase 분리가 과한지 점검 후 read/write 선언을 정밀화해 동일 phase 내 무충돌 시스템(Status/Vision/스킬 Execute류)을 같은 배치로 묶는다.
   - 측정: `Scheduler::ParallelBatches` > 0, `Scheduler::MaxBatchSize` >= 3.
5. **Update/Render 내 독립 작업 선행 발사** — 프레임 시작 시 FOW/미니맵 투영/체력바 수집 잡을 먼저 제출하고 사용 직전 WaitForCounter로 합류(소프트웨어 파이프라이닝).

검증 budget: `Frame` EMA 4.5ms 이하, `Champion::AnimUpdate`(메인 스레드 합류 대기 기준) 0.5ms 이하, 워커 스코프가 rawEvents에 실제로 분산 기록, 외부 CPU 부하(에이전트 실행) 중 스터터 재현 테스트 통과.
의존성: Engine(JobSystem/Scheduler) + Client(AnimUpdate 호출부). Shared 영향 없음.

---

## Phase 3 - ECS 데이터 지향 재설계

목표: 엔티티 10배 스케일 대비. 현재 108엔티티에서는 절대 절감이 작지만(~0.5ms), 600+ 엔티티에서 선형 붕괴를 막는 구조 투자.

세션:
1. **컴포넌트 타입 레지스트리** — `World.h:39`의 `unordered_map<type_index>`를 정수 ComponentTypeId 테이블로 교체(조회 = 배열 인덱싱).
2. **핫 컴포넌트 SoA화** — TransformComponent의 position/rotation/scale/world matrix를 SoA 청크로 분리, `Transform::Execute`의 재귀 트리 갱신을 깊이별 평탄 배열 패스로 교체(부모가 항상 먼저 갱신되는 위상 정렬 순서 캐시).
3. **변경 감지(version counter)** — 컴포넌트 쓰기 시 버전 증가. Transform이 안 움직인 엔티티의 월드행렬 재계산/SpatialIndex 갱신/체력바 투영을 스킵.
4. **시스템 DAG 스케줄링** — phase 순차 실행을 read/write 선언 기반 의존성 그래프로 교체. 배치 단위가 아니라 시스템 단위로 의존성 충족 즉시 실행(Phase 5 Fiber와 결합 시 효과 극대).

검증 budget: 미니언 600 스폰 스트레스 씬에서 `Scheduler` + `Transform::Execute` + `SpatialHashSystem::Execute` 합계 2.0ms 이하. 기존 씬 회귀 없음.
의존성: Engine ECS 코어. Client 시스템들은 ForEach/GetComponent 시그니처 유지(어댑터로 흡수). Server의 Shared GameSim ECS와 구조를 맞출지는 별도 결정(확인 필요).

---

## Phase 4 - 렌더 분리 (프레임 패킷 + 렌더 스레드)

목표: Update와 Render를 겹쳐 크리티컬 패스를 max(Update, Render)로. 4.5ms → ~3.0ms.

세션:
1. **프레임 패킷 추출** — Scene::Render가 ECS를 직접 읽는 경로를 제거하고, Update 말미에 렌더에 필요한 데이터(가시 메시 리스트, 본 팔레트, UI 쿼드, FOW 텍스처 dirty rect)를 더블 버퍼 패킷으로 추출. 이 세션만으로도 렌더 중 ECS 접근 규칙이 정리됨.
2. **RHI 커맨드 기록 분리** — `IRHICommandList` 추상화를 DX11 deferred context 또는 자체 커맨드 버퍼로 구현, 렌더 스레드가 패킷 N을 제출하는 동안 메인이 Update N+1 실행.
3. **프레임 페이싱 재설계** — `CEngineApp.cpp:53-70` 리미터를 페이싱 인지형으로(타깃 시각 기준 제출, waitable swap chain 검토). 넷코드 55ms 고정 lerp 보간(`Scene_InGame.cpp:127`)을 서버 타임라인 기반 보간으로 교체하는 세션을 여기 묶는다(스터터 원인 2).

검증 budget: `Frame` EMA 3.0ms 이하(캡 해제), 프레임타임 표준편차/1% low를 카운터로 추가해 페이싱 개선 수치 확인, 입력 지연 체감 회귀 없음.
의존성: Engine RHI/Framework. 렌더 스레드 도입 후 Client 렌더 코드의 device 직접 접근 금지 규칙 필요 — 컴파스 문서에 규칙 추가.

---

## Phase 5 - Fiber 태스크 그래프 완성 (선택적, Phase 2/4 성과 후 판단)

목표: WaitForCounter의 스레드 블로킹을 fiber yield로 교체해 워커 점유율 극대화. 잡 안에서 잡을 기다리는 패턴(스케줄러 중첩, 렌더 패킷 생성)이 늘어난 뒤에야 효과가 있다.

세션:
1. **Fiber 풀** — 잡당 CreateFiber/DeleteFiber(`JobSystem.cpp:272-303`)를 사전 할당 fiber 풀(예: 128개, 64KB 스택)로 교체.
2. **WaitForCounter yield화** — 카운터 미충족 시 현재 fiber를 대기 리스트에 걸고 워커는 다음 잡 실행, 카운터 충족 시 fiber 재개. `FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`의 안전 규칙(TLS, 락 보유 중 yield 금지) 준수.
3. **프레임 전체 태스크 그래프** — Phase 3의 DAG + Phase 4의 패킷 생성을 fiber 잡 그래프로 통합. 메인 스레드는 그래프 시드 + Present만.

판단 게이트: Phase 2 완료 시점에 `Job::WorkerIdleRatio` 카운터가 30% 이상일 때만 진행. 아니면 보류(복잡도 대비 이득 없음 — Karpathy 단순성 원칙).
의존성: Engine Core 한정.

---

## Phase 6 - GPU 오프로드 (스키닝/인스턴싱/GPU Driven)

목표: CPU에 남은 렌더 준비 비용을 GPU로 이전. 노는 RTX 4060을 쓴다.

세션:
1. **GPU 스키닝** — CPU 본 팔레트 계산은 유지하되 스키닝 자체를 VS에서. 드로우당 32KB CB 업로드(`ModelRenderer.cpp:425-465`)를 프레임 단위 StructuredBuffer 1회 업로드 + 드로우별 오프셋으로 교체. 애님 샘플링(Phase 2에서 병렬화됨)과 분리.
2. **동일 메시 인스턴싱** — 미니언 60개(동일 wmesh 4종) / 터렛 / 정글 캠프를 `DrawIndexedInstanced`로. 인스턴스 버퍼(월드행렬+팀컬러+본 팔레트 오프셋)는 프레임 패킷에서 생성. 60드로우 → 4~8드로우.
   - 측정: `Mesh::DrawCalls` 94 → 40 이하.
3. **머티리얼 정렬 + 텍스처 아틀라스/배열** — `Model::MaterialBinds`(43+43)를 머티리얼 키 정렬 제출과 텍스처 배열로 절반 이하로.
4. **GPU 컬링 (DX11 한계 내)** — 맵 청크 컬링을 컴퓨트 셰이더 + `DrawInstancedIndirect`로. DX11은 multi-draw indirect가 없으므로 풀 GPU-driven(드로우 자체를 GPU가 생성)은 DX12 백엔드(`.md/plan/rhi/06_RHI_PHASE_5_DX12_BACKEND.md`) 완성 후로 명시 보류. 여기서는 인스턴스 카운트만 GPU가 결정하는 수준까지.

검증 budget: `GPU::Frame`이 CPU `Frame`보다 작게 유지(여전히 CPU 바운드 확인), `Mesh::DrawCalls` 40 이하, 스트레스 씬(미니언 600)에서 드로우콜이 엔티티 수에 비선형(인스턴싱 효과) 확인.
의존성: Engine RHI + 셰이더. 인스턴싱은 Engine generic 경로로 구현, Client는 인스턴스 데이터만 공급.

---

## Phase 7 - 메모리/캐싱 계층

목표: 프레임 내 할당 제로 + 중복 계산 공유. 1% low와 스케일 안정성.

세션:
1. **프레임 아레나 할당자** — 프레임 패킷/UI 쿼드/잡 페이로드의 per-frame `std::vector` 증식을 더블 버퍼 linear allocator로 교체.
2. **포즈 캐시 공유** — 동일 클립+동일 시간 버킷의 미니언 애니메이션은 팔레트를 1회만 샘플링해 공유(미니언 4종 x 상태 5종이면 프레임당 샘플링 ~20회로 상한). 미니언 600 씬에서 애님 비용을 엔티티 수와 분리.
3. **CB 링 버퍼** — 드로우별 cbuffer Update를 프레임 링 버퍼 + 오프셋 바인딩으로(DX11 11.1 `VSSetConstantBuffers1`).
4. **에셋 상주 캐시 정리** — 챔피언/미니언 스폰 히치(`08_MINION_SPAWN_HITCH_VISUAL_BINDING_PLAN.md` 연계) — 스폰 시 동기 로드/바인딩 비용을 사전 워밍.

검증 budget: 스트레스 씬 1% low가 평균 대비 70% 이상, 프레임 내 힙 할당 카운터(디버그 훅) 0 수렴.
의존성: Engine Core/Renderer. 게임플레이 로직 무변경.

---

## 실행 순서와 게이트 규칙

```text
Phase 0 → 1 → 2 → (3 ∥ 4) → 5(조건부) → 6 → 7
```

- 각 Phase는 budget 미달 시 다음 Phase로 넘어가지 않고 해당 병목 row를 재분해한다 (기존 100FPS 플랜 규칙 승계).
- 매 Phase 종료 시 캡 해제 F4 캡처를 `Profiles/`에 보관하고 budget 표와 비교한다.
- Phase 3과 4는 접점이 적어 병행 가능(3=ECS 코어, 4=RHI/Framework).
- Phase 5는 측정 게이트(`Job::WorkerIdleRatio` 30%+) 통과 시에만.
- 전 Phase 공통: 새 병렬 경로를 만들기 전에 기존 중복 렌더/update/cache 경로 제거가 선행. LoL 전용 타입을 Engine public API로 올리지 않는다.

## 미검증

- 빌드/런타임 미검증 (본 문서는 로드맵, 코드 미반영).
- `CAnimator::Update`의 스레드 안전성(공유 상태 유무) 확인 필요 — Phase 2 세션 2 착수 전 필수.
- Server Shared GameSim ECS와 Client ECS 구조 통일 여부 확인 필요 — Phase 3 착수 전 결정.
- EMA 6.44ms Update vs 캡처 프레임 3.78ms 간극(나쁜 프레임의 분포) — Phase 0 아카이브로 분해.
