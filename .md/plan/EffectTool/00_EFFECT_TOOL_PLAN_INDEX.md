# Phase G — 자체 Effect Tool (Niagara급 노드 그래프 이펙트 시스템) 구현 계획 (인덱스) — **v4**

> ⚠️ **v4 갱신 (2026-05-07, Niagara 소스 + Winters 코드베이스 실측 반영)**: **권위 마스터**는 [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md). `12_EFFECT_TOOL_NIAGARA_V2_MASTER.md` / `13_EFFECT_TOOL_V3_MASTER.md` / `14_NIAGARA_REFERENCE_DEEP_MAP.md` / `15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md` / `16_EFX_PROGRESS_AND_NEXT_ACTIONS.md` 는 17에 흡수된 참고 문서다. 본 인덱스 v1 의 Stage 1~7 sub-plan 은 연구/학습 자료로 보존한다.

## v4 진입 순서 (권장)

| Stage | 이름 | 우선순위 | 기간 |
|---|---|---|---|
| **EFX-0** | Legacy Bridge + 기존 preset 자산화 (현재 `Engine/Public/FX` + `Client/Public/GameObject/FX` v1 흡수) | ★★★ | 2~3 days |
| **EFX-1** | `.wfx` JSON v1 로더/세이버 + round-trip | ★★★ | 3~4 days |
| **EFX-2** | Runtime SoA + ParameterStore + FxTickSystem | ★★★ | 1 week |
| **EFX-3** | Renderer 6 종 (Sprite/Mesh/Ribbon/Beam/Light/Decal) | ★★★ | 1~2 weeks |
| **EFX-4** | EffectTool Editor MVP (Stack/Graph/Curve/Viewport/Parameter/ScratchPad/Toolbar) | ★★★ | 1~2 weeks |
| **EFX-5** | Graph Compiler + VM/HLSL translator | ★★ | 1 week |
| **EFX-6** | DataInterface 6 종 + Elden VFX 검증 | ★★ | 1 week |
| **EFX-7** | GPU Compute + Indirect Draw (Track 2 DX12 합격 후) | ★ | 보류 |
| **EFX-8** | Hot reload async pipeline | ★★ | 2~3 days |
| **EFX-9** | `.wfxbin` cooked binary | ★★ | 2~3 days |

세부 합격 기준과 구현 순서는 17번 마스터 §13을 권위로 따른다. GPU compute 는 Track 2 DX12 경로 안정 후 진입한다.

---

## v1 인덱스 (참고용 — 연구/학습 자료로 보존)


## 비전

상용 VFX 프레임워크 (Unreal **Niagara**, FromSoft **FXR**, Unity **VFX Graph**) 를 참조하되,
상용 의존 없이 **노드 그래프 기반 이펙트 시스템** 을 C++20 + HLSL 로 직접 구현.
LoL 스킬 이펙트부터 엘든링 보스 주문진까지 하나의 시스템으로 커버.

## 왜 그래프 기반인가 — 핵심 철학

기존 파티클 시스템 (UE3 Cascade, Unity Shuriken 초기) 은 **고정 파이프라인**.
`Spawn → Initialize → Update → Render` 가 하드코딩되고 아티스트는 정해진 모듈만 조합.

**Niagara / FXR 이 바꾼 것은 "데이터 흐름을 아티스트가 정의" 한다는 점**:

- **노드 = 순수 함수** (input → output)
- **엣지 = 데이터 의존성** (누가 누구의 결과를 소비하는가)
- **그래프 = DAG** (Directed Acyclic Graph) → 위상 정렬로 실행 순서 결정
- **컴파일 단계** 존재 → 그래프를 HLSL / 바이트코드로 변환

즉 이펙트 자체가 **작은 프로그램** 이 됨. 아티스트가 엔진을 수정하지 않고 새로운 시뮬레이션
(벡터필드, SDF 충돌, 플루이드) 을 합성 가능.

## 왜 자체 구현인가

- **포트폴리오 목적** — DAG 컴파일러 + SoA 파티클 시뮬 + GPU 코드 생성을 실제 이해하고 구현한 이력
- **Phase D / Phase E 와 수학 공유** — BVH, 몬테카를로, SDF 충돌이 물리 / Path Tracing 과 겹침
- **Phase B-5 연습모드 목표 부합** — "에디터 UI 가 곧 게임 모드" 철학에 정확히 부합. ImGui 노드 에디터로 아티스트-프로그래머 경계 제거
- **네트워크 결정성 제어** — Niagara 는 결정성 부족으로 경쟁 슈터들이 고생. 처음부터 "판정 FX" vs "시각 FX" 분리 설계

## Stage 로드맵 (1~7)

| Stage | 내용 | 난이도 | 문서 |
|---|---|---|---|
| 1 | 그래프 데이터 모델 + JSON 직렬화 + 위상 정렬 | 🟢 | [02_STAGE1_GRAPH_DATA_MODEL.md](02_STAGE1_GRAPH_DATA_MODEL.md) |
| 2 | ParticlePool (SoA) + swap-back kill + Attribute Registry | 🟢 | [03_STAGE2_PARTICLE_POOL_SOA.md](03_STAGE2_PARTICLE_POOL_SOA.md) |
| 3 | Executor — 표준 노드 (Spawn/Init/Update) + 컴파일 | 🟡 | [04_STAGE3_NODE_EXECUTOR.md](04_STAGE3_NODE_EXECUTOR.md) |
| 4 | Expression VM — 바이트코드 스택 머신 | 🟡 | [05_STAGE4_EXPRESSION_VM.md](05_STAGE4_EXPRESSION_VM.md) |
| 5 | DX11 Instancing Renderer — 빌보드 + 소팅 | 🟡 | [06_STAGE5_DX11_RENDERING.md](06_STAGE5_DX11_RENDERING.md) |
| 6 | ImGui 노드 에디터 + 실시간 프리뷰 뷰포트 | 🔴 | [07_STAGE6_NODE_EDITOR_IMGUI.md](07_STAGE6_NODE_EDITOR_IMGUI.md) |
| 7 | GPU Compute 백엔드 — HLSL 코드 생성 + Indirect Draw | 🔴 | [08_STAGE7_GPU_COMPUTE.md](08_STAGE7_GPU_COMPUTE.md) |

## 공통 시스템

| 주제 | 문서 |
|---|---|
| 아키텍처 + ECS 통합 + 디렉토리 | [01_ARCHITECTURE.md](01_ARCHITECTURE.md) |
| SkillHook / AnimationEvent / Sound / Network 통합 | [09_INTEGRATION.md](09_INTEGRATION.md) |
| FX Debugger + DebugDraw + Replay | [10_DEBUG_TOOLS.md](10_DEBUG_TOOLS.md) |

## 구현 순서 (의존성 기준)

```
Stage 1 Graph 모델          ← 모든 컴파일의 기본
     ↓
Stage 2 ParticlePool         ← SoA 없이는 성능 없음
     ↓
Stage 3 Executor             ← 실제 시뮬레이션 동작 (첫 불꽃 데모)
     ↓
Stage 5 Rendering            ← 화면에 뿌리기 (MVP 완성 지점)
     ↓ ──────────────────────── 여기까지 Phase 1 — LoL Q/W/E/R 전부 커버 가능
Stage 4 Expression VM         ← 수식 노드 표현력 폭발
     ↓
Stage 6 Node Editor           ← 아티스트 협업 필요 시점
     ↓
Stage 7 GPU Compute           ← 만 단위 파티클 (폭풍/블리자드)
```

**핵심 의사결정**: Stage 3 + 5 만으로 LoL 모작 이펙트 90% 를 커버할 수 있다.
Stage 6 (노드 에디터) 는 "혼자 개발" 이면 JSON 직접 편집으로 대체 가능 — **포트폴리오 가치는 Stage 7 GPU Compute 가 더 높음**.

## MOBA / 엘든링 사용 계획

| Stage | LoL (MOBA) | 엘든링 모작 (액션RPG) |
|---|---|---|
| 1 Graph 모델 | ✅ 스킬당 1 FxGraph | ✅ 주문 / 보스 기믹 |
| 2 SoA Pool | ✅ 필수 | ✅ |
| 3 Executor CPU | ✅ 1만 파티클까지 | 🟡 중간 |
| 4 Expression | ✅ Age-over-Life 색상 변화 | ✅ 주문 차지 비율 기반 커브 |
| 5 DX11 Rendering | ✅ 빌보드 + Add 블렌딩 | ✅ 트레일 / 리본 추가 필요 |
| 6 Node Editor | 🟡 선택 (JSON 편집 가능) | ✅ 아티스트 합류 시 필수 |
| 7 GPU Compute | ✅ 포탑 폭발 / 바론 AoE | ✅ 엘든비스트 마법진 / 대량 궤적 |

### 구체 시나리오

**LoL — 이렐리아 R (표지화의 춤)**: 검 8자루가 순차 투사 → 각 검이 히트 시 피 이펙트.
→ Emitter A (검 트레일, SpawnBurst×8 + Update CurlNoise 약간) + Emitter B (히트 이벤트 시 SpawnBurst×50 + Init Velocity Cone) 조합.

**LoL — 바론 궁 (3층 스웹)**: 원거리 AoE 마법진 3층 원.
→ Emitter A (지면 라인 링, SpawnRate + StaticMesh 파티클 shader) + Emitter B (먼지 버스트).

**엘든링 — 엘든비스트 별똥별**: 스킬 시전 1.5 초간 하늘에서 만 개 별 강하.
→ Phase 7 GPU Compute 필수. CPU 로는 불가능. SpawnRate 7000/s × Lifetime 2s = 14000 파티클 동시.

## Phase D / Phase E 와의 수학 공유

| 자료구조 / 수학 | Phase D Physics | Phase E Graphics | **Phase G FX** |
|---|---|---|---|
| SoA / Dense Array | PBD 파티클 시스템 | — | ParticlePool |
| Ray-AABB | Raycast | Path Tracing | GPU 충돌 (Phase 2 확장) |
| 난수 / PCG Hash | PBD 랜덤 초기화 | Monte Carlo 샘플러 | Init Position/Velocity 랜덤 |
| Stratified / Halton | — | Importance Sampling | Spawn 패턴 균일 분포 |
| Curl Noise | — | — | UpdateCurlNoise (핵심 시각 효과) |
| BVH | Broad Phase | 가속 구조 | Collision 노드 (Phase 2) |

## 참고 문헌

### 필수

- **Real-Time Rendering 4th Ed.** Ch.13 Volumetric and Translucency — 파티클 이론 기반
- **GPU Gems 1** Ch.23 "Hair Simulation" — SoA 시뮬 레퍼런스
- **GPU Gems 3** Ch.23 "High-Speed, Off-Screen Particles" — 소팅 / 블렌딩
- **Game Programming Gems 2** — Particle system design
- **Niagara Documentation** — docs.unrealengine.com/5.x/en-US/creating-visual-effects-with-niagara-for-unreal-engine/
- **Crisp, Mark — "VFX Graph Deep Dive"** (Unity GDC 2019)

### 논문 / 레퍼런스

- **Bridson, Hourihan, Nordenstam 2007** — "Curl-Noise for Procedural Fluid Flow" (SIGGRAPH)
- **Reeves 1983** — "Particle Systems — A Technique for Modeling a Class of Fuzzy Objects" (원조 논문)
- **Niagara Technical Whitepaper** — Epic Games GDC 2018
- **FromSoft FXR 포맷 분석** — SoulsModdingWiki (비공식)

### 코드 참고

- **NiagaraCore** 오픈 아키텍처 자료 (공식 소스는 UE 라이선스 필요)
- **Unity VFX Graph** 오픈 문서
- **ofxVFX** (openFrameworks VFX 애드온) — 간이 노드 그래프
- **Stingray Flow** (Autodesk, 폐기됨) — 아키텍처 공개 자료 있음

## 의존성

| 필요 | 상태 | 비고 |
|---|---|---|
| DX11 RHI | ✅ | 인스턴싱 draw 이미 가능 |
| ECS (Entity / ComponentStore / World) | ✅ Phase 1a | FxSystemComponent 얹을 자리 |
| ImGui 백엔드 | ✅ Phase B-5 | 노드 에디터 Stage 6 |
| JobSystem | 🔄 Phase 1b | Stage 3 이미터 병렬화 시 |
| Compute Shader | ✅ DX11 지원 | Stage 7 필수 |
| RenderGraph | ⏭️ Phase 2 | Stage 7 Indirect Draw 시 유리 |
| Phase D Physics BVH | ⏭️ | Collision 노드 추가 시 |
| 챔피언 AnimationEvent | ⏭️ Phase C-3 | Stage 9 통합 지점 |

## Phase B-6.6 FMOD 와 관계

사운드와 FX 는 **같은 이벤트 소스에서 동시에 트리거** 되는 게 이상적.
SkillHook (B-10) 에서 `FireHitEffect(skillId, worldPos)` 호출 시:
1. `CGameInstance::PlaySoundOn(...)` → FMOD 재생
2. `FxSpawnSystem::SpawnAt(fxAsset, worldPos)` → FxInstance 생성

→ 이 두 호출을 감싸는 **`FxAudioLink` 헬퍼 구조체** 를 FxAsset 에 넣어
  에디터에서 "FX 에셋에 사운드 드래그앤드롭 → 재생 시 자동 동기" 가능하게 설계.

## MOBA 특화 요구사항

LoL 같은 탑다운 MOBA 는 일반 VFX 엔진 대비 요구 다름:

| 요구 | 이유 |
|---|---|
| **결정적 시뮬** (판정 FX) | 서버 / 클라이언트 스킬 히트박스 일치 보장 |
| **시각 전용 FX 분리** | 클라에서만 돌리는 먼지 / 트레일은 비결정적이어도 무관 |
| **저 지연 스폰** | 스킬 Cast 시점부터 히트까지 지연 < 16ms |
| **Fog of War 통합** | 시야 밖 FX 는 컬링 |
| **AoE 시각화 일치** | 스킬 판정 범위 = FX 링 반지름 |
| **10 플레이어 동시 다중 FX** | 한타 씬 = 50+ 이펙트 동시 |

반면 Phase G Stage 7 (GPU Compute 만개) 는 MOBA 엔 오버스펙일 수 있음.
**Stage 7 은 엘든링 + 포트폴리오 용**. MOBA 에선 Stage 1~5 만 필수.

## 예상 소요

| Stage | 기간 | 비고 |
|---|---|---|
| 1 Graph 모델 | 3 일 | JSON + 위상 정렬 |
| 2 ParticlePool | 2 일 | SoA + swap-back |
| 3 Executor + 표준 노드 9종 | 1 주 | 첫 불꽃 데모 |
| 4 Expression VM | 4 일 | 바이트코드 + 파서 |
| 5 DX11 Rendering | 1 주 | 인스턴싱 + 소팅 + 블렌딩 |
| **Phase 1 (MVP)** | **3 주** | 여기까지 LoL 이펙트 가능 |
| 6 Node Editor | 2 주 | imgui-node-editor 통합 |
| 7 GPU Compute | 3 주 | HLSL 코드 생성 + Indirect |

## Phase 로드맵 배치

CLAUDE.md 의 엔진 Phase 로드맵에 삽입:

```
B-7a ModelRenderer 분해  →  B-7b Spawn System  →  ... →  Phase 4 Network
                                                              ↓
                                                          Phase G FX  ← 본 계획서
                                                              ↓
                                                          Phase D Physics (공유)
                                                              ↓
                                                          Phase E Graphics (공유)
```

즉 **Phase 4 (네트워크) 이후, Phase D 이전** 이 적기. 네트워크 프로토콜이 정해져야 결정적 FX 설계가 안정화됨.
