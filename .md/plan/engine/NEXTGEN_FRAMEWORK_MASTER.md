# 차세대 게임 프레임워크 마스터 인덱스 — ECS v2 + Fiber + Render Graph + GPU Driven — **rev 2**

**작성일**: 2026-05-04
**rev 2 (2026-05-04, Codex 검토 반영)**: ① Fiber B 축을 v1 → **v2.1** ([FIBER_JOB_SYSTEM_v2.md](FIBER_JOB_SYSTEM_v2.md), 1655 lines, Codex 박제) 로 교체 ② 4 단계 (A→B→C→D) 를 **7 단계** (ECS → Worker-Safe CB → System Access Contract → Fiber → Render Graph → GPU Scene → GPU Driven) 로 확장 ③ 권위 마스터를 [`2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md`](2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md) (Codex) 로 명시 — 본 문서는 그 sub-plan 인덱스
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md) (P-1~P-19, 8 GATE)
**목표**: **엘든링 (오픈월드 + 보스 부위 파괴 + 멀티 던전) + LoL (5v5 MOBA + 멀티 GameRoom + 시야/타워/AI)** 결합 차세대 게임 부하 감당.
**부하 가정**: 엔티티 100K (오픈월드) / 드로우콜 10K+ / 5 GameRoom 동시 / 60 FPS @ 16.6ms 프레임 budget.

---

## §rev 2 Codex 권고 반영 매트릭스

| # | rev 1 (본 마스터 v1) | rev 2 정정 | 사유 |
|---|---|---|---|
| 1 | Fiber B 항목 = `FIBER_JOB_SYSTEM.md` v1 인용 | **`FIBER_JOB_SYSTEM_v2.md` v2.1 인용** | v2 가 이미 박제됨 (Codex 가 박제, 1655 lines) |
| 2 | 4 단계: ECS → Fiber → RG → GPU Driven | **7 단계**: ECS Foundation → Worker-Safe CommandBuffer → System Access Contract + Scheduler DAG → Fiber v2 → Render Graph over RHI → GPU Scene → GPU Driven Pipeline | Worker-Safe CB / System Access / GPU Scene 3 단계가 누락 — Fiber 깊이 연결 전에 ECS 의 race/lifetime 문제 해결 강제 |
| 3 | 권위 마스터로 본 문서 명시 | **권위 마스터를 [`2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md`](2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md) (Codex) 로 양도**. 본 문서는 sub-plan 인덱스 + 부하 가정 + GATE 매트릭스 보조 | Codex 마스터가 §0 진단 + §1~§8 까지 833 lines 본격 박제 — 권위 단일화 |
| 4 | "sync 완료" 표현 — 실제 git untracked | **git 상태 명시** — 신규 계획서들 `??` untracked. commit 별도 필요 | 사실과 표현 일치 |

---

## §0. 현재 코드베이스 결함 진단

### ECS — "반쪽짜리" (Generation 미관리)
| 결함 | 위치 | 위험 |
|---|---|---|
| **Generation/Version 비트 0** | [Entity.h:9](../../../Engine/Public/ECS/Entity.h:9) `using EntityID = uint32_t;` | id 재사용 시 stale handle = 새 엔티티 참조 → **silent use-after-free** |
| **Alive 추적 vector<bool>** | [Entity.h:67](../../../Engine/Public/ECS/Entity.h:67) `std::vector<bool> m_vecAlive` | bit-packed STL spec — `&m_vecAlive[i]` 불가, 캐시 비효율 |
| **Archetype 미지원** | [World.h:107-118](../../../Engine/Public/ECS/World.h:107) `ForEach<T1, T2>` 가 store1 순회 + store2 `Has` lookup | 컴포넌트 N 개 쿼리 시 O(N × HasLookup) — Archetype-based ECS (Bitsquid/EnTT) 보다 5~10× 느림 |
| **단일 World 가정** | [World.h:53](../../../Engine/Public/ECS/World.h:53) `class CWorld` | 서버 멀티 GameRoom (Sim-10 v2) / 엘든링 멀티 던전 시 인스턴스 N 개 깨짐 (Spatial v2 부분만 World owned 정정) |
| **DLL export 누수** | [World.h:21-24](../../../Engine/Public/ECS/World.h:21) `#pragma warning(disable: 4251)` | `unordered_map`/`unique_ptr` 멤버 export 시 ABI 불안 |

### JobSystem — Thread Pool MVP (Fiber 미구현)
| 결함 | 위치 | 위험 |
|---|---|---|
| **Worker 가 OS 스레드 블로킹** | [JobSystem.h:24-79](../../../Engine/Public/Core/JobSystem.h:24) Phase 5-A MVP | RenderSubmit Wait → OS 스레드 idle, 12코어 중 일부 유휴 |
| **Help-stealing busy-wait** | `WaitForCounter` 코멘트 "블로킹 아님" — 실제는 spin loop | CPU 낭비, 파워 효율 저하 |
| **Fiber 박제 0** | `Engine/Public/Core/Fiber*` 0 hit (Glob) | 의존성 그래프가 깊은 Phase 2+ 렌더링 진입 시 worker 직렬화 |

### Render Graph — 명령형 렌더 (선언형 X)
| 결함 | 위치 | 위험 |
|---|---|---|
| **명령형 BeginFrame/OnRender/EndFrame** | `Scene_InGame::OnRender`, `ModelRenderer::Render()` 직접 D3D11 호출 | 패스 의존성 수동 관리, Deferred/PostFX 진입 시 보일러플레이트 폭발 |
| **리소스 수명 ad-hoc** | 텍스처/RT 인스턴스 매니저 없음 | G-Buffer / SSAO / Bloom 진입 시 메모리 사용량 폭발 (transient pool 없음) |

### GPU Driven — CPU 가 100% 드로우콜 발행
| 결함 | 위치 | 위험 |
|---|---|---|
| **Per-entity Draw 호출** | `Scene_InGame::OnRender` 의 `m_World.ForEach<RenderComponent, TransformComponent>` → `rc.pRenderer->Render()` | 엔티티 1000+ 시 5~15ms CPU 비용 |
| **Indirect Draw 미사용** | DX11 `DrawIndexedInstancedIndirect()` 미호출 | GPU 컬링/LOD 미적용, CPU bottleneck |

---

## §1. Sub-Plan 인덱스 (★ rev 2 — 7 단계)

| # | 파일 | 범위 | 신규 클래스 | 효과 |
|---|---|---|---|---|
| **1** | [`ECS_V2_NEXTGEN.md`](ECS_V2_NEXTGEN.md) **(rev 2)** | **ECS v2 Foundation** — Generation (RHIHandles 패턴 재사용) + Archetype + Multi-World + Legacy Adapter | `EntityHandle = RHIHandle<EntityTag>`, `CArchetype`, `CECSWorld`, `CQueryBuilder` | use-after-free 0, ForEach 5~10× 가속 |
| **2** | [`2026-05-04_WEEK_4_DETAILED_BAKE.md`](2026-05-04_WEEK_4_DETAILED_BAKE.md) §"Worker-Safe CommandBuffer" | **Worker-Safe CommandBuffer** — 현재 [CCommandBuffer.h](../../../Engine/Public/ECS/CCommandBuffer.h) 단일 vector race 정정. SPSC 또는 per-worker 큐. | `CCommandBuffer<T>` worker-safe v2 | structural change race 0 |
| **3** | (Codex 마스터 §3-§4) | **System Access Contract + Scheduler DAG** — system 마다 read/write component declaration. Scheduler 가 의존 그래프 빌드. | `SystemAccessSpec`, `CECSScheduler` (DAG) | 같은 phase 병렬 race 자동 회피 |
| **4** | [`FIBER_JOB_SYSTEM_v2.md`](FIBER_JOB_SYSTEM_v2.md) **(v2.1, Codex 박제)** | **Fiber Job System v2.1** — Naughty Dog GDC 2015 모델, 1655 lines | (Codex 박제) | OS 스레드 블로킹 0, 의존성 그래프 자연 표현 |
| **5** | [`RENDER_GRAPH_v2.md`](RENDER_GRAPH_v2.md) **(rev 2)** | **Render Graph over RHI** — DAG + transient pool + RenderWorldSnapshot | `CRenderGraph`, `IRgPass`, `RenderWorldSnapshot`, `CRenderExtractionSystem` | G-Buffer/Lighting/PostFX 선언형. **렌더/시뮬 분리** |
| **6** | [`GPU_DRIVEN_PIPELINE_v2.md`](GPU_DRIVEN_PIPELINE_v2.md) **(rev 2)** §2-1~2-5 | **GPU Scene** — Mega Buffer + InstanceData layout (정적 인프라) | `CMegaBuffer`, `InstanceData (POD)` | 모든 메시 단일 풀 |
| **7** | [`GPU_DRIVEN_PIPELINE_v2.md`](GPU_DRIVEN_PIPELINE_v2.md) **(rev 2)** §2-6~2-7 | **GPU Driven Pipeline** — Compute Cull + IndirectDraw (frame 단위) | `CGPUCullPass`, `CIndirectDrawPass`, `IRHICommandList::DrawIndexedIndirect` 신규 | CPU 드로우 5~15ms → <1ms |

**총 LOC ~7500**. 본 마스터 + 4 sub-plan 모두 박제. 구현은 별도 cycle (B 기존 v1 활성화 → A 진입 → C → D 순서, 각 1~2주).

---

## §2. 의존 그래프 + 진입 순서 (★ rev 2 — 7 단계)

```
                ┌──────────────────────────────┐
                │  1. ECS v2 Foundation        │  (선행 0)
                │  EntityHandle = RHIHandle    │
                │  + Archetype + Legacy Adapter│
                └────────┬─────────────────────┘
                         │
                         ▼
                ┌──────────────────────────────┐
                │  2. Worker-Safe CommandBuffer│  (Fiber 깊은 연결 전 강제)
                │  SPSC / per-worker 큐        │
                └────────┬─────────────────────┘
                         │
                         ▼
                ┌──────────────────────────────┐
                │  3. System Access Contract   │  (read/write decl + DAG)
                │  + Scheduler DAG             │
                └────────┬─────────────────────┘
                         │
                         ▼
                ┌──────────────────────────────┐
                │  4. Fiber Job System v2.1    │  (FIBER_JOB_SYSTEM_v2.md)
                └────────┬─────────────────────┘
                         │
                         ▼
                ┌──────────────────────────────┐
                │  5. Render Graph over RHI    │  (RenderWorldSnapshot 분리)
                └────────┬─────────────────────┘
                         │
                         ▼
                ┌──────────────────────────────┐
                │  6. GPU Scene                │  (Mega Buffer + InstanceData)
                └────────┬─────────────────────┘
                         │
                         ▼
                ┌──────────────────────────────┐
                │  7. GPU Driven Pipeline      │  (Compute Cull + Indirect)
                └──────────────────────────────┘
```

**왜 1 → 2 → 3 가 Fiber (4) 전에?** Codex §0.1 의 핵심: "현재 ECS 는 component access declaration / structural change barrier / worker-safe CB 가 없다. 이 상태에서 Fiber `WaitForCounter` yield 를 깊게 붙이면, 버그가 JobSystem 문제가 아니라 **ECS lifetime/race 문제로 폭발**한다." Worker-Safe CB + Access Contract 가 Fiber 의 prerequisite.

### 진입 순서 — 왜 ECS v2 먼저?

1. **A (ECS v2)** — 모든 시스템이 EntityHandle/Component query 사용. Generation 안 박으면 Phase B-13/B-16 의 ECS request 패턴 (`TowerAggroNotifyComponent` 등) 이 stale handle 사고에 노출.
2. **B (Fiber)** — JobSystem 인터페이스 불변 (FIBER_JOB_SYSTEM.md §"public API 불변"). ECS v2 와 독립 — 병렬 진입 가능. 단 Render Graph 구현이 Fiber 의존이라 C 전에 마무리.
3. **C (Render Graph v2)** — DAG 패스 의존성 + Fiber 가 패스간 의존 자연 표현. ECS v2 의 Component Query 가 패스 입력 (예: `Query<TransformComponent, RenderComponent>` 결과를 Geometry pass 가 소비).
4. **D (GPU Driven v2)** — Render Graph 위에 Mega Buffer + Compute Cull. Render Graph 의 transient pool 이 Indirect args buffer 관리.

### 병렬 가능 매트릭스
| Sub-Plan | A | B | C | D |
|---|---|---|---|---|
| A ECS v2 | — | 병렬 | C 가 A 의존 | D 가 A 의존 |
| B Fiber | 병렬 | — | C 가 B 의존 | D 가 B 의존 |
| C RG v2 | A 후 | B 후 | — | D 가 C 의존 |
| D GPU Driven | A 후 | B 후 | C 후 | — |

**A/B 병렬 가능**, **C 는 A+B 후**, **D 는 C 후**. 1 인 작업: A → B → C → D 직렬 (8~10 주). 2 인 작업: (A∥B) → C → D (5~7 주).

---

## §3. 차세대 게임 부하 가정 (목표 측정값)

### 엘든링 + LoL 결합 시나리오

| 부하 | 수치 | 해결 sub-plan |
|---|---|---|
| 엔티티 수 (오픈월드 1 청크) | 100,000 | A (Archetype + bitset query) |
| 동시 멀티 World (서버) | 5 GameRoom | A (Multi-World) |
| 드로우콜 / frame | 10,000+ | D (Indirect Draw + GPU Cull) |
| 의존 패스 그래프 깊이 | 30+ (G-Buffer→SSAO→Lighting→PostFX→TAA→Tonemap) | C (Render Graph DAG) |
| Worker 활용률 | 95%+ (12코어) | B (Fiber, OS 스레드 블로킹 0) |
| 16.6ms frame budget 분배 | CPU 5ms / GPU 11ms | C+D 동시 |

### 개별 sub-plan KPI

**A (ECS v2)**:
- 100K 엔티티 ForEach<T1, T2, T3> < 200us (현재 추정 1~2ms)
- Use-after-free 0 (Generation mismatch 시 nullptr 반환)
- 멀티 World 인스턴스 5개 동시 생성 + 독립 Tick

**B (Fiber)**:
- WaitForCounter 시 OS 스레드 블로킹 0
- 1만 job stress test 통과 (FIBER_JOB_SYSTEM.md 의존)
- Worker 활용률 80%+ (현재 ~50%)

**C (Render Graph v2)**:
- 30+ 패스 DAG 컴파일 < 1ms
- Transient resource pool 메모리 -40% (vs 명시 alloc)
- Pass dependency 수동 관리 0

**D (GPU Driven v2)**:
- 10K 드로우콜 CPU 비용 < 1ms (현재 5~15ms)
- GPU frustum cull 90%+ 정확도
- Indirect Draw 호출 수 < 100 (현재 10K)

---

## §4. PITFALLS GATE 적용 (8 단계)

본 4 sub-plan 박제 시 PITFALLS §2 의 GATE A~H 8 단계 강제:
- **GATE A (사실 수집)**: 현재 [Entity.h](../../../Engine/Public/ECS/Entity.h), [World.h](../../../Engine/Public/ECS/World.h), [JobSystem.h](../../../Engine/Public/Core/JobSystem.h), 기존 [FIBER_JOB_SYSTEM.md](FIBER_JOB_SYSTEM.md), [RENDER_GRAPH_PLAN.md](RENDER_GRAPH_PLAN.md), [GPU_DRIVEN_PIPELINE.md](GPU_DRIVEN_PIPELINE.md) 모두 Read 후 인용.
- **GATE B (TODO 0)**: 각 sub-plan §1 Preflight 표 실측값 박제.
- **GATE C (호출 경로 grep)**: ECS v2 진입 시 모든 `m_World.AddComponent` / `HasComponent` / `GetComponent` / `ForEach` 호출자 grep + 마이그레이션 매트릭스 박제.
- **GATE D (ECS 책임 경계)**: Render Graph 의 Pass 가 Scene/CGameApp 직접 의존 금지 — Pass 입력은 Component Query, 출력은 RGTexture handle.
- **GATE E (향후 자료형)**: EntityHandle 64-bit (32-bit index + 24-bit generation + 8-bit world) — 엘든링 100K 엔티티 + 멀티 World 5개 + generation 16M 회 만족.
- **GATE F (Scheduler 동시성)**: Fiber JobSystem 의 Counter 가 atomic, ECS v2 의 Archetype iterator 가 thread-safe (write 충돌 0 보장 — JobSystem chunked 분할).
- **GATE G (Owner Scope)**: ECS v2 = `CECSWorld` per GameRoom/Scene. Render Graph = Scene-한정 (frame 단위 lifetime). GPU Driven Mega Buffer = `CEngineApp` 전역 (GPU 1개당 1개). Fiber = `CGameInstance` Tier-1 (프로세스 1개당 1개).
- **GATE H (인용 의미 + 행동 보존 + include)**: ECS v2 마이그레이션은 행동 보존 강제 — Generation 추가만으로 기존 게임 로직 동작 동일. Archetype 도입은 `ForEach` 결과 순서가 다를 수 있으나 "엔티티 순회 순서 의존 코드 0" 검증.

---

## §5. 다음 진입

본 마스터 박제 후 즉시 진입:

```
"차세대 프레임워크 마스터 박제 완료. 다음:

1. A (ECS v2) 박제 — .md/plan/engine/ECS_V2_NEXTGEN.md
   — EntityHandle 64-bit (index + generation + world)
   — Archetype 베이스 + bitset query
   — Multi-World (CECSWorld per GameRoom/Scene)
   — 마이그레이션 매트릭스 (모든 m_World.* 호출자 매핑)

2. C (Render Graph v2) 박제 — .md/plan/engine/RENDER_GRAPH_v2.md
   — 기존 RENDER_GRAPH_PLAN.md (v1) 검토 결과 반영
   — DX11 + RHI 추상화 (현재 IRHIDevice/RHITypes) 호환
   — Phase B-13 (FogOfWarRenderer / ModelRenderer 양 pass) 통합

3. D (GPU Driven v2) 박제 — .md/plan/engine/GPU_DRIVEN_PIPELINE_v2.md
   — 기존 GPU_DRIVEN_PIPELINE.md (v1) 검토 결과 반영
   — Mega Buffer + Compute Cull + Indirect Draw
   — Render Graph 의 transient pool 활용

4. B (Fiber) — 기존 FIBER_JOB_SYSTEM.md 활성화
   — Phase 5-B 진입 (현재 5-A MVP)
   — public API 불변 컨벤션

진입 전 PITFALLS GATE A~H 8 단계 의무 통과."
```

---

## §6. 마스터 BUDGET

| Sub-Plan | 박제 시간 | 구현 시간 | 검증 시간 | 합계 |
|---|---|---|---|---|
| A ECS v2 | 6h | 2 weeks | 3 days | ~3 weeks |
| B Fiber 활성화 | 0h (재사용) | 1 week | 2 days | ~1.5 weeks |
| C Render Graph v2 | 4h | 1.5 weeks | 2 days | ~2 weeks |
| D GPU Driven v2 | 6h | 2.5 weeks | 3 days | ~3.5 weeks |
| **총** | **16h 박제** | **~7 weeks** | **~10 days** | **~10 weeks** |

**1 인 작업 ~10 주, 2 인 병렬 ~6~7 주**. 차세대 PvP/MOBA/오픈월드 부하 감당 인프라 구축.

---

**END OF MASTER**
