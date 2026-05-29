# 5월 세션 로그 (2026-05-01 ~ 2026-05-06)

> CLAUDE.md 가 운영 브리프로 압축된 이후 5월 누적 세션. 4월 로그 (`SESSION_LOG_2026-04.md`) 의 후속.
> 마지막 갱신: 2026-05-06.

---

## 5/1 — Twin Track 통합 + Codex 14 패치

- `.md/plan/engine/2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md` 박제 완료 (codex 14 패치 반영)
- CLAUDE.md 압축 검토 계획서 (codex): `.md/plan/engine/CLAUDE_COMPRESSION_REVIEW_PLAN_2026_05_01.md`
- 압축 직전 백업: `.md/archive/CLAUDE_MD_2026-05-01_PRE_COMPRESSION.md`

## 5/2 — 운영 브리프 압축 + Week2~17 박제 + Fiber Track 신설

- CLAUDE.md 신규 운영 브리프로 압축 (1285 lines → 720 lines).
- Week2~6 박제 (5 파일): `2026-05-{02,03,04,05,06}_WEEK_*_DETAILED_BAKE.md`
- Week7-9 / W10-13 / W14-17 박제 완료, codex 진입 대기.
- Fiber JobSystem (Phase 5-B) 트랙 신설: `.md/plan/engine/FIBER_JOB_SYSTEM.md`
- Track 1/2/3 분리 + W1~W6 진행률 기재.

## 5/3 ~ 5/4 — PLAN_AUTHORING_PITFALLS + Champion B-13 박제

- `.md/process/PLAN_AUTHORING_PITFALLS.md` 신규 (P-1~P-15 + 5단계 GATE) — 13_YONE_v1 P1×2+P2×2 사고 계기.
- 이후 P-16~P-19 추가 + GATE 6→8 단계로 확장.
- B-13 마스터 + sub 02/03/04/05 박제 (마스터 v2 + sub v2 통합본). v1 결함 P1×5+P2×5 정정 매트릭스 박제.
- 챔프 5체 신규 폴더 박제: Annie / Ashe / Fiora / Jax / Yone (`Client/Private/GameObject/Champion/`). 표준 3-file (FxPresets/Skills/Registration). Yone 만 메쉬 그룹 가시성 패턴.

## 5/5 — DX12 Scaffold + Fiber JobSystem v2

- `Engine/Private/RHI/DX12/` 28 파일 박제: Device/Queue/SwapChain/CommandList/PipelineState/RenderPass/BindGroup/BindGroupLayout/RootSignature/DescriptorHeap/MemoryAllocator/ResourceBarrier/Sampler/Shader/Texture/Buffer.
- `Winters.sln` 에 `Debug-DX12|x64`, `Release-DX12|x64` config 추가. **단 Engine 만 진짜 DX12 config**, Server/Client/Tools 는 `Debug|x64` / `Release|x64` 매핑 (sln L34, L42, L50).
- `Engine/Include/Engine.vcxproj` `Debug-DX12|x64` config: OutDir `Bin\Debug-DX12\`, AdditionalDependencies `d3d12.lib;d3d11.lib;dxgi.lib;d3dcompiler.lib;dxguid.lib`.
- ThirdPartyLib 에 `D3D12MA` 추가 (`Engine/ThirdPartyLib/D3D12MA/Src/D3D12MemAlloc.cpp`).
- Fiber JobSystem v2.1: 헤더 3개 (`Engine/Public/Core/Fiber/{FiberTypes,Fiber,FiberPool}.h`) + `JobSystem.h:13` 가 FiberTypes include + `eJobExecutionMode { ThreadOnly, FiberShell }` + `SetExecutionMode/GetExecutionMode/m_eExecutionMode` 박제.
- `JobSystem.cpp` Fiber 동작 본체 박제: `WorkerLoop` 가 FiberShell 모드면 `ConvertThreadToFiber` (L160-163) + `TryExecuteItemOnFiber` (L253) 가 매 job `CreateFiber + SwitchToFiber + DeleteFiber` (L266-272) + `FiberShellEntry` (L275). 단 **CFiberPool 미사용 (매 job 생성/삭제)**, **WaitForCounter Yield 없음 (busy-wait L287-)**.

## 5/6 — 동기화 검증 + 의존성 분석 + CLAUDE.md 갱신 (본 세션)

- GitHub `tnestyle70/Winters_Engine` ↔ 메인 레포 100% 동기화 확인 (HEAD `8756ca7` = origin/main, ahead/behind 0).
- `.md` 145개 마크다운 인덱스화 + 트랙별 마스터 문서 매핑.
- Engine 내부 의존성 매트릭스 추출 + mermaid 그래프 박제 (`DEPENDENCY_GRAPH_2026-05-06.md`).
- CLAUDE.md ↔ 실제 코드 갭 검증: 챔프 7체→12체, IRHI 보조 인터페이스 4개→7개, 셰이더 `space0` 0건, Fiber "박제 0%" 부정확 (FiberShell mode 1차 박제 완료) 등.
- 의존성 위반 발견: AI ↔ ECS 진성 순환, Renderer→ECS / Resource→ECS 역참조, RHI ↔ Renderer 양방향, GameInstance.h DLL 경계 4건 (Sound/EntityBlueprint/ProfilerOverlay/CPUProfiler).
- codex 검토 반영: "caller 마이그 진행 중" → "IRHI facade 통과 시작, DX11 native bridge 잔존", "8 셰이더 space0" → "Shaders/* + Shaders/SSAO/* + PBR 셰이더 전수 11개", "DLL 경계 4건 풀 누설" → "public coupling 4건, 제거 난이도 상이 (포인터=forward 가능, EntityBlueprint=값=정의 필요)".
- CLAUDE.md 본 갱신 (`PRE_UPDATE` 백업 후) — 마지막 갱신일 → 2026-05-06.
- TODO.md 재작성 (Phase Stabilization-0 / Fiber 안정화 / DX12 bootstrap).
- 신규 계획서 2 박제: `.md/plan/engine/FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`, `.md/plan/rhi/DX12_BOOTSTRAP_PLAN.md`.

---

## 핵심 미결 (5/6 시점, 우선순위 순)

### Stabilization-0 (codex "다음 한 방", 일회성 게이트)
1. Engine/External/imgui `imconfig.h` 로컬 수정 정리 — 빌드 필수면 vendor 분기 / 아니면 reset
2. `Debug|x64` 4 프로젝트 전체 빌드 검증 (Engine → Client → Server → AssetConverter)
3. EngineSDK/inc · /lib 동기화 정상 여부
4. `Debug-DX12` Engine compile-only 검증 (Client/Server/Tools 는 자동 fallback)
5. CLAUDE.md / TODO.md 현재 코드 기준 갱신 (= 본 세션 산출)

### Track A — Fiber JobSystem 안정화 (`FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`)
1. `CFiberPool::Acquire/Release` 박제 — 매 job CreateFiber/DeleteFiber 제거
2. `WaitForCounter` Yield-aware — Fiber 컨텍스트에서 다른 fiber 로 switch
3. `Get_WorkerSlot()` 안정화 — Fiber resume 가능 시 잘못된 thread-local 인덱스 방어
4. `Scene_InGame.cpp:564` `pNav->Set_JobSystem(pJS)` 주석 복구 검증

### Track B — RHI/DX12 Bootstrap (`DX12_BOOTSTRAP_PLAN.md`)
1. `Debug-DX12` config 빌드 통과 (Engine 단독)
2. `CDX12Device::Create` 정상 호출 (`HWND` + `width/height`)
3. SwapChain/Queue/CommandList 초기화 (clear color present)
4. `Alt+F4` 정상 종료 (WaitIdle)
5. **시각 동일성은 W10-13 진입 (보류)**

### 보류 (Stabilization-0 후)
- AI ↔ ECS 진성 순환 (EntityID alias 분리)
- GameInstance.h DLL 경계 4건 (Hygiene follow-up)
- Shared → Engine.ECS 역의존 (deterministic 가드)
- Client → DX11 누설 3 hit (W7 진입 전 caller 마이그)
- 셰이더 `register(... space0)` 명시 (DX12 SM 5.1+ 진입 전)
- 6 챔프 metallic/roughness 차별화 (Track 1 W6 후속)
- B-13 회귀 검증 (SpatialHash↔MinionAI 행동 보존)

---

## 산출물 인덱스

- 계획서: `FIBER_JOB_SYSTEM_v2.md`, `2026-05-{07,10,14}_WEEK_*_DETAILED_BAKE.md`, `B13/00_INDEX_MASTER.md` v2 + sub v2
- 신규 코드: Fiber 헤더 3개, RHI/DX12 28 파일, IRHI 8 인터페이스 (Device + 7 보조), Champion 5 신규 폴더
- 신규 셰이더: `Mesh3D_PBR.hlsl`, `Skinned3D_PBR.hlsl`, `BRDF/BRDF_GGX.hlsli`, `SSAO/GTAO_CS.hlsl`, `SSAO/GTAO_Blur_CS.hlsl`, `NormalOnly.hlsl`, `SkinnedNormalOnly.hlsl`
- 신규 Renderer: `CMaterialPBR`, `NormalPass`, `SSAOPass`, `FogOfWarRenderer`
- 본 세션 산출: `DEPENDENCY_GRAPH_2026-05-06.md`, `CLAUDE_MD_2026-05-06_PRE_UPDATE.md`, `FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`, `DX12_BOOTSTRAP_PLAN.md`
