# RHI Session Index

작성일: 2026-05-25

목표: DX11 직접 호출 구조를 정리하고, WintersEngine.dll 하나가 DX11, DX12, Vulkan, Console backend를 선택할 수 있는 RHI 구조로 전환한다. 별도 `Smoke.exe`, `DX12.exe`, `Vulkan.exe` 같은 테스트 실행 프로젝트는 만들지 않는다.

## 원칙

- 단일 실행 파일 원칙: `WintersGame.exe`와 향후 `WintersLOL.exe`/`WintersElden.exe`만 유지한다.
- backend는 프로젝트가 아니라 `Engine/Private/RHI/<Backend>/` 구현이다.
- Smoke 검증은 별도 vcxproj가 아니라 `Client/Private/...Smoke.cpp`, unit/smoke 함수, 또는 runtime flag로 둔다.
- DX11은 폐기 대상이 아니라 첫 RHI backend다.
- Public/SDK 헤더에서 `d3d11.h`, `ID3D11*`, `RHI/DX11/*`가 사라지는 것이 1차 큰 합격선이다.
- RHI는 DX12/Vulkan/Console의 명시적 모델을 기준으로 잡고, DX11 backend가 immediate emulation을 담당한다.

## 세션 순서

| Session | 파일 | 목표 | 코드 변경 |
|---|---|---|---|
| S00 | `S00_SMOKE_PROJECT_REPRO.md` | 이전 데스크탑의 `Smoke.vcxproj` 상황 재현과 결론 박제 | 없음 |
| S01 | `S01_RH0_LEGACY_QUARANTINE.md` | DX11 getter와 public leak 위치 격리 | 작음 |
| S02 | `S02_RH1_CORE_TYPES_HANDLES.md` | RHI types, desc, handle table 1차 | 중간 |
| S03 | `S03_RH1_DX11_DEVICE_ADAPTER.md` | `CDX11Device`를 `IRHIDevice` 구현으로 세움 | 중간 |
| S04 | `S04_RH2_COMMANDLIST.md` | `IRHICommandList`와 DX11 immediate command list | 큼 |
| S05 | `S05_PUBLIC_DX11_PURGE.md` | Public/SDK DX11 헤더 제거 | 큼 |
| S06 | `S06_RENDERQUEUE_COUNTERS_CULLING.md` | RenderQueue, draw counter, CPU frustum culling | 큼 |
| S07 | `S07_RH3_PSO_RENDERPASS_BINDGROUP.md` | PSO, RenderPass, BindGroup 모델 | 큼 |
| S08 | `S08_RH4_RESOURCE_LIFETIME.md` | 64-bit handle, generation, lifetime policy | 중간 |
| S09 | `S09_DX12_BACKEND_NO_EXE.md` | DX12 backend 구현 계획, standalone exe 금지 | 큼 |
| S10 | `S10_VULKAN_CONSOLE_BACKENDS.md` | Vulkan/Console backend 확장 경계 | 큼 |
| S11 | `S11_NEXT_IMPLEMENTATION_HANDOFF.md` | 실제 main 기준 RHI 다음 구현 핸드오프 | 없음 |
| S12 | `S12_RHI_BOUNDARY_AUDIT.md` | RHI/DX11 경계 audit, Client private native DX11 제거 | 작음 |
| S13 | `S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md` | LoL을 첫 공용 RHI renderer 소비자로 만들고 Elden이 같은 pipeline 사용 | 큼 |
| S14 | `S14_PLAN_RULES_FRAME_LOOP_SHARED_ENTRY.md` | `/plan-rules`: `CEngineApp` frame loop를 backend-neutral scene entry로 여는 첫 적용 계획 | 작음 |
| S15 | `S15_ENGINE_RENDERING_FILTER_AUDIT.md` | Engine 렌더링/RHI 실사용 파일 표, 필터 재배치안, 삭제 후보를 코드 이동 전 고정 | 없음 |
| S16 | `S16_RHI_PRODUCTION_HARDENING_CODEX_HANDOFF.md` | DX11/DX12 device 완성 이후: 빌드위생(/FS), DX12 프레임 파이프라이닝+descriptor ring, 공용 `CRHISceneRenderer`+`RenderWorldSnapshot`. Codex 핸드오프 + 복붙 프롬프트 | 큼 |
| S17 | `S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md` | S16 G1~G4 검토(완료) + descriptor ring frame-in-flight 파티션 P0 게이트 + 공용 Scene Renderer(F2/F3, LoL 이관·Client/Public DX11 제거). Codex 핸드오프 | 큼 |
| S18 | `S18_2026-06-24_RHI_SCENE_ONLY_PARITY_GATE.md` | LoL normal F5 legacy draw를 유지하면서 `--rhi-scene-only` 명시 플래그에서 RHI scene snapshot parity를 검증하는 비교 게이트 | 적용 |

## 지금 바로 시작할 세션

다음 코드 작업은 S14부터 시작한다. S00은 현재 세션에서 재현 완료:

- `Tools/Smoke/Smoke.vcxproj`를 임시 생성했다.
- `git status`가 `?? Tools/Smoke/`만 표시했다.
- `git ls-files`에는 잡히지 않았다.
- `.gitignore`에도 잡히지 않았다.
- 결론: 이전 데스크탑에서 `Smoke.vcxproj`를 만들고 commit/push하지 않았다면 git clone에는 따라오지 않는다.

## 공통 검증 명령

```powershell
git status --short
rg -n "ID3D11|d3d11.h|RHI/DX11|CDX11Device|DX11Shader|DX11Pipeline" Engine/Include Engine/Public Client/Public Client/Private
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

## 절대 금지

- `Smoke.vcxproj`를 solution에 추가하지 않는다.
- `DX12.vcxproj`를 만들지 않는다.
- `DX12.exe`를 산출물 목표로 두지 않는다.
- backend 검증을 위해 별도 app project를 만들지 않는다.
- `Engine/Public` 또는 `EngineSDK/inc`에 backend native header를 노출하지 않는다.
> 2026-05-25 handoff: 다음 진행은 `S11_NEXT_IMPLEMENTATION_HANDOFF.md`를 기준으로 한다. DX12/Smoke standalone project를 다시 만들지 않고, 현재 `main`의 실제 상태에서 RHI 경계 축소와 backend-in-DLL 방향으로 이어간다.
