# Effect Tool v1 — Codex 검토 결과 + v2 정정 매트릭스

**작성일**: 2026-05-04
**상태**: ✅ 검토 완료. v2 박제는 [`12_EFFECT_TOOL_NIAGARA_V2_MASTER.md`](12_EFFECT_TOOL_NIAGARA_V2_MASTER.md) 참조.
**가이드**: PLAN_AUTHORING_PITFALLS.md (P-1~P-19, 8 GATE)

---

## §0. 검토 결론

기존 [`00_EFFECT_TOOL_PLAN_INDEX.md`](00_EFFECT_TOOL_PLAN_INDEX.md) 와 Stage 1~7 sub-plan 은 **방향은 좋고, 연구 계획서로서 가치 높음**. 다만 **현재 코드베이스 기준으로 바로 반영하기에는 greenfield 계획이 너무 강함**. 엘든링 작업용 Niagara급 Tool 로 올리려면 **Stage 순서 변경 + 6 결함 정정** 후 진입.

**Fiber 활용 보류 권장**: ParticlePool 시뮬을 Fiber JobSystem 안에 박제하는 건 NEXTGEN_FRAMEWORK_MASTER §1 의 4 (Fiber M3 yield + wait list) 가 안정된 이후. 현재 ThreadOnly CPU 시뮬 + DX11 instancing 으로 충분히 Niagara급 가능.

---

## §1. Codex 6 결함 (실측 인용)

### 결함 1 — Stage 0 Legacy Bridge 누락

**현 코드**:
- [`Client/Public/GameObject/FX/FxSystem.h:20`](../../../Client/Public/GameObject/FX/FxSystem.h:20) — 기존 시스템 동작 중
- [`Client/Public/GameObject/FX/FxMeshSystem.h:13`](../../../Client/Public/GameObject/FX/FxMeshSystem.h:13) — Mesh effect 시스템
- [`Client/Private/UI/EffectTuner.cpp:36`](../../../Client/Private/UI/EffectTuner.cpp:36) — Irelia preset 7 종 hardcode

**v1 박제 문제**: `Engine/Public/FX/` 신규 시스템을 처음부터 박제 → 기존 IreliaFx/YasuoFx/AnnieFx preset 과 **이중화**.

**v2 정정**: **EFX-0 Legacy Bridge** 신설 — `LegacyFxAdapter` + `FxAssetFromPreset`. 기존 preset 22 종 (Irelia 7 + Yasuo 5 + Annie 5 + Yone 5) 자동 자산화. 기존 game code 0 변경.

### 결함 2 — Niagara 핵심 ParameterMap 누락

**v1 박제 문제**: 단순 DAG 노드만 박제. Niagara 의 **ParameterMap 5 namespace** 없음 — 결국 hardcoded 노드 모음 = 유연성 X.

**Niagara 핵심 5 namespace**:
- `System.*` — 매 frame 갱신, 모든 emitter 공유 (DeltaTime, WorldTime)
- `Emitter.*` — emitter 단위 (SpawnRate, Position)
- `Particle.*` — 개별 particle SoA 컬럼 (Position, Velocity)
- `User.*` — gameplay 코드 set (BossPhase, PlayerHP)
- `Event.*` — 이벤트 payload (HitNormal, HitDamage)

**v2 정정**: **EFX-1 ParameterMap 박제** — 5 namespace + `CFxParameterMap` API. 노드가 namespace 양방향 흐름.

### 결함 3 — Stage 5 Rendering 이 billboard 중심 (Elden VFX 부족)

**v1 박제 문제**: [`06_STAGE5_DX11_RENDERING.md`](06_STAGE5_DX11_RENDERING.md) 가 빌보드만 박제. Elden 작업용 Tool 인데 **검기 / 보스 장판 / 회피 잔상 / 마법진** 미지원.

**필요 6 타입**:
1. Billboard (기존)
2. **Ribbon / Trail** — 검기 / 회피 잔상
3. **Beam** — 마법 광선 / 락온
4. **Ground Decal** — 보스 장판 / 마법진
5. **Mesh Particle** — 검 파편 / 돌가루
6. **Shockwave Ring** — 도약 충격파

**v2 정정**: **EFX-3 Multi-Render Type** — 6 타입 동시 박제 (4 weeks). v1 의 Stage 5 → Stage 3 로 앞당김.

### 결함 4 — Editor 진입 너무 늦음

**v1 박제 문제**: [`07_STAGE6_NODE_EDITOR_IMGUI.md`](07_STAGE6_NODE_EDITOR_IMGUI.md) — Stage 6. 그 전 5 stage 동안 JSON 손편집. **엘든링 VFX 작업 불가능**.

**현 EffectTuner 한계**:
- [EffectTuner.cpp:62](../../../Client/Private/UI/EffectTuner.cpp:62) `kPresetNames[] = {"BA Slash", "Q Trail", ..., "R Pulse"}` Irelia 7 hardcode
- `ImGui::Begin("Effect Tuner — Irelia")` 제품 종속
- Yasuo / Annie / Elden 보스 / Class & Servant servant 효과 모두 적용 불가

**v2 정정**: **EFX-4 Scene_EffectTool 앞당김** — Stage 1 직후 (EFX-3 후) 진입. 제품 독립 + 4 패널 (Asset Browser / Preview / Inspector / Timeline) + Hot Reload.

### 결함 5 — ECS API 미스매치

**v1 박제 문제**: 일부 코드 박제가 현재 ECS API 와 안 맞음.

**문제 예시**:
- `world.GetStore<T>()` — 현재 [World.h:89](../../../Engine/Public/ECS/World.h:89) 에 없음. 실제 API 는 `ForEach<T>(...)`, `GetComponent<T>(id)`, `HasComponent<T>(id)`.
- `ENGINE_DLL` — 현재 매크로 `WINTERS_ENGINE`.
- `#include "Entity.h"` flat — AGENTS.md L437 의 "subdir 보존" 룰 위반 (PITFALLS P-8). 정확히는 `"ECS/Entity.h"`.

**v2 정정**: 모든 v2 박제는 `WINTERS_ENGINE` + `"ECS/Entity.h"` + `"ECS/World.h"` + `world.ForEach<T>(...)` / `GetComponent` / `HasComponent` 사용.

### 결함 6 — Raw path pointer 구조 (asset tool 부적합)

**v1 박제 문제**: 기존 컴포넌트가 raw 포인터 사용:
- [FxBillboardComponent.h:18](../../../Client/Public/GameObject/FX/FxBillboardComponent.h:18) `const wchar_t* texturePath = nullptr;`
- [FxMeshComponent.h:24-26](../../../Client/Public/GameObject/FX/FxMeshComponent.h:24) `const char* modelPath = nullptr;` + `const wchar_t* texturePath = nullptr;`

**문제**:
- raw 포인터 = 정적 리터럴만 가능. Tool 에서 동적 로드한 텍스처 경로는 string lifetime 관리 필요.
- 핫리로드 시 path 변경 → 기존 component 가 dangling pointer
- 자산 핸들 일관성 (`RHITextureHandle` 등) 과 다름

**v2 정정**: **EFX-1 핸들 전환**:
- `FxAssetHandle` (RHIHandle<FxAssetTag>)
- `RHITextureHandle` 사용 (이미 RHI 측 박제)
- string table — 자산 이름은 `CFxAssetRegistry::GetAssetPath(handle)` 호출

---

## §2. v1 → v2 Stage 매트릭스

| v1 Stage | v2 Stage | 변경 |
|---|---|---|
| (없음) | **EFX-0** Legacy Bridge | **신설** |
| Stage 1 그래프 데이터 | EFX-1 FxAsset + ParameterMap | **+ ParameterMap 5 namespace** |
| Stage 2 ParticlePool SoA | EFX-2 ParticlePool + Deterministic + CommandBuffer | **+ deterministic + worker-safe** |
| Stage 5 DX11 Rendering | **EFX-3** Multi-Render Type 6 종 | **앞당김 + 6 타입 (Ribbon/Beam/Decal/Mesh/Shockwave 추가)** |
| Stage 6 Node Editor | **EFX-4** Scene_EffectTool | **앞당김 + 제품 독립 + Hot Reload** |
| Stage 3 Executor + Stage 4 VM | EFX-5 Node Executor + Expression VM | (변경 없음) |
| (없음) | **EFX-6** Elden VFX Pack | **신설 — 검증 작품** |
| Stage 7 GPU Compute | **EFX-7** GPU (보류) | **RHI/RG 안정 후** |

---

## §3. v1 박제의 사용 가치 (참고용 보존)

기존 v1 sub-plan (Stage 1~7) 의 **방향과 디자인 의도** 는 v2 가 그대로 차용. 단 코드 박제 부분만 정정 필요. v1 문서들 deprecated 로 표시하지 않음 — 연구/학습 자료로 가치 유지.

| v1 문서 | v2 활용 |
|---|---|
| 01_ARCHITECTURE.md | EFX-1 + EFX-2 의 ECS 통합 부분 차용 |
| 02_STAGE1_GRAPH_DATA_MODEL.md | EFX-1 의 노드 / 엣지 / DAG 박제 |
| 03_STAGE2_PARTICLE_POOL_SOA.md | EFX-2 의 SoA 박제 |
| 04_STAGE3_NODE_EXECUTOR.md | EFX-5 의 Executor 박제 |
| 05_STAGE4_EXPRESSION_VM.md | EFX-5 의 VM 박제 |
| 06_STAGE5_DX11_RENDERING.md | EFX-3 의 Billboard 부분만 차용 (5 타입 신규 추가) |
| 07_STAGE6_NODE_EDITOR_IMGUI.md | EFX-4 의 Inspector 노드 그래프 패널 부분 차용 |
| 08_STAGE7_GPU_COMPUTE.md | EFX-7 진입 시 차용 (보류) |

---

## §4. PITFALLS 새 사례 — 본 검토에서 발견

### 결함 5 → P-13 재발

`world.GetStore<T>()` 같은 미존재 API 호출 박제 = PITFALLS **P-13 (미존재 API 호출)** 재발 사례. 박제 진입 전 `Engine/Public/ECS/World.h` grep + 실재 API 확인 필수.

### 결함 6 → 신규 사례 추가 권고

**P-20 신규 권고**: Tool / Editor 시스템에서 raw `const char*` / `const wchar_t*` path 멤버 사용 금지. 자산 (asset) 시스템 진입 시 **항상 핸들 (`RHIHandle<TTag>`) + string table** 패턴.

이건 PITFALLS.md 에 추가 박제 권고.

---

## §5. 다음 진입

본 검토 박제 후 [`12_EFFECT_TOOL_NIAGARA_V2_MASTER.md`](12_EFFECT_TOOL_NIAGARA_V2_MASTER.md) 진입. v2 마스터가 EFX-0 ~ EFX-7 의 모든 박제 + 시간표 + Multi-Game 통합 + ECS 통합 + GATE 매트릭스 보유.

진입 전 PITFALLS GATE A~H 8 단계 의무 통과.

---

**END OF V1 REVIEW**
