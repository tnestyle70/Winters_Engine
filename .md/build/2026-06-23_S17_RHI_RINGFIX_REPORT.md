# 2026-06-23 S17 RHI Ringfix 반영 검증 보고서

## 기준 문서

- `.md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md`
- 상위 방향: `.md/plan/rhi/sessions/S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md`
- 경계 규칙: `.md/architecture/WINTERS_CODEBASE_COMPASS.md`

## 현재 결론

S17 전체 기준으로는 약 35% 반영 상태다.

- `PHASE=RINGFIX`: 핵심 구현과 빌드 검증, DX11/DX12 probe 생존 스모크까지 완료했다. 엄격히는 런타임 4종 전체 생존 확인과 시각 캡처 검증이 남아 있어 85~90% 완료로 본다.
- `PHASE=SNAPSHOT`: 미착수.
- `PHASE=LOLPORT`: 미착수.

즉, 현재는 S17의 선결 게이트인 descriptor ring 안정화가 들어간 상태이고, 공용 `RenderWorldSnapshot + CRHISceneRenderer` 단계는 아직 시작 전이다.

## 반영 내용

### 1. DX12 descriptor ring frame partition

대상 파일:

- `Engine/Private/RHI/DX12/DX12Device.cpp`

반영 내용:

- shader-visible SRV/Sampler ring heap을 frame-in-flight별 sub-range로 분리했다.
- `srvRingCapacity = kSrvHeapCapacity / kFrameCount`
- `samplerRingCapacity = kSamplerHeapCapacity / kFrameCount`
- `AllocFrameSrv()` / `AllocFrameSampler()`의 반환 base가 `frameIndex * perFrameCapacity + localNext`가 되도록 바꿨다.
- `BeginFrame()`에서 해당 frame의 ring cursor만 reset하고, `WaitForFrame(frameIndex)` 이후 재사용하도록 유지했다.

효과:

- frame 0과 frame 1이 같은 shader-visible descriptor heap index 0부터 덮어쓰던 cross-frame aliasing 위험을 제거했다.
- S16에서 이미 들어간 frame fence pipelining, dynamic buffer `frameResources[kFrameCount]` 로직은 재작성하지 않고 보존했다.

### 2. mid-frame initial upload guard

대상 파일:

- `Engine/Private/RHI/DX12/DX12Device.cpp`

반영 내용:

- `CreateTexture(desc, initialData, ...)`가 `m_bFrameRecording` 중 호출되면 debug log를 남기고 실패하도록 guard를 추가했다.
- frame recording 중 buffer 생성은 open frame command list reset을 피해야 한다는 계약 주석을 남겼다.

의도:

- 초기 데이터 업로드 경로가 내부 command list를 reset/execute/wait하는 구조라서, `BeginFrame~EndFrame` 중 호출될 경우 열려 있는 frame command list를 망가뜨릴 수 있다.
- 이번 단계에서는 별도 upload allocator/list를 새로 만들지 않고, S17 문서의 "최소 guard + 계약 명시" 방향으로 막았다.

### 3. RINGFIX runtime smoke 강화

대상 파일:

- `EldenRingClient/Public/EldenRingRHITestCubeRenderer.h`
- `EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp`

반영 내용:

- 기존 probe TestCube가 한 개의 texture/bind group만 쓰던 상태에서, 한 프레임에 두 개의 texture/bind group을 순서대로 바인딩해 좌우 큐브를 그리도록 확장했다.
- material A: 흑백 checker texture
- material B: 청색/금색 checker texture
- 각 material은 별도 constant buffer, texture, bind group을 가진다.

의도:

- S17 RINGFIX DoD인 "한 프레임에 서로 다른 텍스처를 가진 BindGroup 2개 이상을 그려도 섞이지 않음"을 기존 실행 파일 `WintersElden.exe --scene=probe` 경로에서 검증할 수 있게 했다.
- 새 `Smoke.exe`, `DX12.exe`, 별도 project는 만들지 않았다.

### 4. build hygiene 보정

대상 파일:

- `Client/Private/Scene/Scene_InGame.cpp`

반영 내용:

- `git diff --check` 실패 원인이던 EOF 빈 줄만 제거했다.
- 대규모 Scene 분해 diff는 기존 작업 상태이며 이번 RINGFIX 본문 변경이 아니다.

## 검증 결과

### CMake target build

명령:

```powershell
cmake --build out/build/msvc-ninja --config Debug --target WintersEngine WintersElden WintersEldenRingEditor
```

결과:

- 성공.
- 중간에 새로 추가된 다른 작업 파일들 때문에 CMake GLOB mismatch 재생성이 발생했지만, 재생성 후 `WintersEngine`, `WintersElden`, `WintersEldenRingEditor` 타깃 빌드는 통과했다.

### MSBuild full solution

명령:

```powershell
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal /clp:ErrorsOnly;Summary
```

결과:

- 성공.
- 경고 268개, 오류 0개.

참고:

- 한 차례 `ServerAICommandProducer` unresolved link가 있었지만, `Server.vcxproj` clean/build 후 기본 Debug intermediate에 object가 생성되면서 full solution 빌드가 통과했다. 소스 수정 없이 증분 산출물 상태를 정리한 케이스로 본다.

### diff hygiene

명령:

```powershell
git diff --check
```

결과:

- 성공.
- LF/CRLF 변환 경고만 남았다.

### boundary audit

명령:

```powershell
rg -n "ID3D11|ID3D12|d3d11.h|d3d12.h|DX11Shader|DX11Pipeline" Engine/Public Engine/Include Client/Public Shared EngineSDK/inc
```

결과:

- 기존 DX11 public 노출 hit가 남아 있다.
- 대표 잔존 위치:
  - `Engine/Public/Renderer/*`
  - `Engine/Public/Framework/CEngineApp.h`
  - `Engine/Include/GameInstance.h`
  - `Client/Public/GameObject/FX/FxSystem.h`
  - `Client/Public/GameObject/FX/FxBeamSystem.h`
  - `EngineSDK/inc/*`

판정:

- RINGFIX 단계에서는 신규 DX11 public 노출을 만들지 않았다.
- 잔존 hit 제거는 S17의 `PHASE=LOLPORT` DoD 범위다.

### runtime smoke

명령:

```powershell
EldenRingClient/Bin/Debug/WintersElden.exe --scene=probe --rhi=dx11
EldenRingClient/Bin/Debug/WintersElden.exe --scene=probe --rhi=dx12
```

결과:

- DX11: 8초 이상 생존, `CloseMainWindow()`로 정상 종료.
- DX12: 8초 이상 생존, `CloseMainWindow()`로 정상 종료.

판정:

- 기존 실행 파일과 backend 선택만 사용했다.
- 두 backend 모두 multi-bind-group probe 경로에서 즉시 크래시 없이 생존했다.
- 다만 실제 화면 픽셀 캡처나 사람이 본 시각 판정은 아직 수행하지 않았다.

## 기준 문서 대비 반영률

| 기준 문서 항목 | 현재 상태 | 반영률 | 비고 |
|---|---:|---:|---|
| S16 G1/G3/G4 보존 | 완료 | 100% | 기존 HARDEN 로직 재작성 없이 유지 |
| P0 RINGFIX descriptor ring partition | 완료 | 100% | frame별 SRV/Sampler ring sub-range 적용 |
| P1 mid-frame upload guard | 완료 | 90% | 최소 guard는 완료, 전용 upload command path는 후속 선택지 |
| RINGFIX 다중 BindGroup smoke | 부분 완료 | 80% | DX11/DX12 probe 생존 확인 완료, 시각/픽셀 캡처 미수행 |
| 공통 build 검증 | 완료 | 100% | CMake 타깃, full MSBuild, `git diff --check` 통과 |
| 런타임 4종 생존 | 부분 완료 | 50% | `WintersElden` DX11/DX12 probe 2종만 확인. Editor/LoL normal flow 미확인 |
| F2 SNAPSHOT | 미착수 | 0% | `RenderWorldSnapshot`, `CRHISceneRenderer` 아직 없음 |
| F3 LOLPORT | 미착수 | 0% | Client/Public DX11 hit 제거 전 |

요약 판정:

- `PHASE=RINGFIX`만 보면 85~90% 완료.
- S17 전체를 보면 약 35% 완료.
- 다음 착수 지점은 `PHASE=SNAPSHOT`이다.

## 남은 작업

1. RINGFIX 마감 검증 보강
   - `WintersEldenRingEditor.exe` 생존 확인.
   - `Client/Bin/Debug/WintersGame.exe` LoL DX11 normal flow 생존 확인.
   - 가능하면 DX11/DX12 probe 화면 캡처로 좌우 큐브 텍스처가 서로 다르게 보이는지 확인.

2. `PHASE=SNAPSHOT`
   - `RenderWorldSnapshot` public 구조 추가.
   - `CRHISceneRenderer` 골격 추가.
   - static mesh forward + sprite FX + debug 최소 pass 구성.
   - LoL map 또는 champion 1체가 snapshot renderer로 렌더되는지 DX11/DX12 양쪽 확인.

3. `PHASE=LOLPORT`
   - `Scene_InGame`이 legacy DX11 renderer 직접 호출 대신 snapshot을 작성하도록 점진 이관.
   - `GameInstance`의 DX11 concrete getter 의존 축소.
   - `Client/Public`의 `DX11Shader` / `DX11Pipeline` 노출 제거.
   - `EngineSDK/inc`는 public header 변경 이후 `UpdateLib.bat` 흐름으로 갱신.

## 현재 수정 파일 메모

이번 RINGFIX 관련 직접 수정:

- `Engine/Private/RHI/DX12/DX12Device.cpp`
- `EldenRingClient/Public/EldenRingRHITestCubeRenderer.h`
- `EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp`

검증 hygiene 보정:

- `Client/Private/Scene/Scene_InGame.cpp`

선행 HARDEN/기존 작업으로 이미 dirty였던 파일:

- `Engine/Private/RHI/DX12/DX12Device.h`
- `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj`

주의:

- 워킹트리에는 이 보고서 외에도 다른 사용자/선행 작업 변경과 untracked 파일이 많이 남아 있다.
- stage, commit, push는 수행하지 않았다.
