Session - RHI DX12 buffer state transition 결과

## 1. 예측 vs 실측

- 예측: DX11의 버퍼 전이는 API 장벽 없이도 호출 계약과 상태 범위를 검증하는 semantic no-op이어야 한다.
  - 실측: 열린 프레임, 유효한 버퍼 핸들, 허용된 버퍼 상태를 모두 확인한 뒤에만 성공한다. native barrier 진단값은 0이었다. 텍스처 전이는 구현되지 않은 상태이므로 명시적으로 실패한다.
- 예측: DX12 default-heap 버퍼는 wrapper가 현재 상태를 소유하고, 상태가 달라질 때만 `ID3D12GraphicsCommandList::ResourceBarrier`를 발행해야 한다.
  - 실측: probe가 `VERTEX_AND_CONSTANT_BUFFER -> COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER`를 기록했고 native barrier 진단값은 정확히 2였다. 같은 상태 전이는 장벽을 만들지 않으며, upload-heap 버퍼는 `GENERIC_READ` 호환 상태만 semantic no-op으로 허용한다.
- 예측: GPU가 probe 명령을 완료한 뒤에 디버그 계층 오류를 판정해야 한다.
  - 실측: DX12는 queue fence, DX11은 event query 기반 `WaitIdle()`을 통과한 뒤 diagnostics를 읽는다. DX12 Debug InfoQueue의 ERROR/CORRUPTION 수는 0이었다. fence의 device-removed sentinel인 `UINT64_MAX`도 완료값으로 인정하지 않는다.
- 예측: 공개 RHI 인터페이스 변경이 EngineSDK와 Client에 동일하게 전파되어야 한다.
  - 실측: Engine Debug와 Client Debug를 순서대로 빌드했고, `IRHIDevice.h`와 `IRHICommandList.h`의 source/EngineSDK SHA-256이 각각 일치했다.
- 독립 비평: 초기 P1 3건, 1차 재비평 P1 1건, 2차 재비평 P1 1건, 3차 재비평 P1 1건을 모두 반영했다. 최종 판정은 `PASS — P0 0, P1 0, P2 0`이다.

## 2. 판결

PASS. Step 2B의 범위인 일반 버퍼 상태 전이의 첫 수직 절편은 완료됐다.

| 백엔드 | 버퍼 전이 계약 | probe native barrier | 검증 | 텍스처 전이 | `supportsResourceTransitions` |
|---|---|---:|---|---|---|
| DX11 | 검증된 semantic no-op | 0 | not applicable | 미구현·실패 | false |
| DX12 | wrapper 상태 추적 + 실제 `ResourceBarrier` | 2 | Debug InfoQueue 오류 0 | 미구현·실패 | false |
| Vulkan | backend 미등록 | 0 | not run | 미구현 | false |

`supportsResourceTransitions`는 계속 false다. 버퍼 한 종류가 통과했을 뿐 텍스처, render target, depth, UAV와 render-pass lifecycle이 아직 완성되지 않았기 때문이다. 이번 결과는 DX12 LoL 제품 렌더링 완성이나 Vulkan 완성을 의미하지 않는다.

## 3. 검증

- Engine Debug build: PASS
- Client Debug build: PASS
- source/EngineSDK SHA-256 parity: PASS 2/2
  - `IRHIDevice.h`: `81273713603589924732D4FD259514F79D1ADD3ED6EDBA34371F70739EACC6B9`
  - `IRHICommandList.h`: `574B9DA7D4E570E3F3D994BAB48CD4E9EE5E7E57B5B9720A1B954D509EC9E456`
- `Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug`: PASS 8/8
  - DX11: `buffer_transition_probe=pass`, `barriers=0`, `validation=not_applicable`
  - DX12: `buffer_transition_probe=pass`, `barriers=2`, `validation=pass`
  - 미등록 Vulkan/None: `not_run`, `0`, `not_run`
- repo root cwd의 non-probe DX11 product smoke: PASS, exit 0
- path-scoped `git diff --check`: 오류 없음. 기존 줄바꿈 정규화 경고만 관찰됨.
- 구현 누락 검색: `IRHIDevice`의 DX11/DX12 구현에 `WaitIdle()`, 두 command-list 구현에 `GetDiagnostics()`와 두 `TransitionResource()` override가 존재함을 확인했다.

## 4. ⑤ 갱신 — 트레이드오프와 비주장

- `WaitIdle()`은 probe와 종료 같은 경계에서만 쓸 수 있는 비싼 전체 동기화다. 일반 프레임 렌더링의 상태 전이마다 호출하면 CPU/GPU 병렬성이 무너진다.
- DX11의 성공은 “아무것도 하지 않는다”가 아니라 핸들·상태·프레임 수명을 검증하고 API 장벽만 흡수한다는 뜻이다.
- DX12 upload heap은 `GENERIC_READ` 고정 제약 때문에 default heap과 같은 방식으로 상태를 바꾸지 않는다.
- 현재 진단 barrier count는 일반 RHI buffer transition이 직접 발행한 장벽만 센다. 기존 swapchain backbuffer의 `PRESENT <-> RENDER_TARGET` 장벽은 별도 수명 경로이므로 포함하지 않는다.
- Release에서 Debug InfoQueue가 없는 경로는 이번 Debug 진실 게이트와 별도다. 검증 가능 여부를 성공으로 위장하지 않는다.

## 5. 다음 단계

Step 2C에서 texture usage/flags와 초기 상태 계약을 고정하고, DX12 texture state tracking, render target/depth/UAV 전이, descriptor와 render-pass lifecycle을 한 수직 절편씩 구현한다. 그 단계가 통과하기 전에는 `supportsResourceTransitions=true`로 올리지 않는다.
