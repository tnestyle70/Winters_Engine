Session - RHI capability truth contract 구현 결과

## 1. 예측 대 실측

- 예측: 공통 `IRHIDevice`가 backend enum으로 capability를 추정하지 않고, 실제 concrete backend가 자신의 runtime contract를 소유한다.
  - 실측: `IRHIDevice::GetCapabilities()`를 pure virtual로 바꾸고 `CDX11Device`와 `CDX12Device`가 각각 `m_Capabilities`를 초기화해 반환한다. enum 기반 `RHI_MakeDefaultCapabilities`와 이전 필드명 검색 결과는 0건이다.
- 예측: 하드웨어/API의 잠재 능력과 현재 Winters가 제품 경로에서 검증한 지원 범위를 분리한다.
  - 실측: `supports*`는 현재 runtime 지원만 뜻하고, `apiRequiresExplicitResourceStates`는 DX12 API가 구현에 요구하는 책임만 뜻한다. 검증되지 않은 compute, async compute, bindless, resource transition과 숫자 limit/alignment는 false 또는 0으로 유지한다.
- 예측: backend 선택 probe가 선택 성공뿐 아니라 capability 소유자와 세부 계약을 기계 판독 가능하게 증명한다.
  - 실측: report에 `capability_backend`, `feature_tier`, compute/async/bindless/transition 지원, explicit-state API 의무, frame resource slot 수를 추가했다. 8개 truth gate가 모두 예상값과 exit code를 통과했다.
- 예측: Engine 공개 헤더와 EngineSDK 사본이 같고, 일반 DX11 제품 경로가 유지된다.
  - 실측: `IRHIDevice.h`와 `RHICapabilities.h`의 source/SDK SHA-256이 각각 일치했다. repo root를 working directory로 실행한 `WintersGame.exe --rhi=dx11 --run-seconds=2`가 exit 0으로 종료했다.
- 빌드 실측: Engine Debug와 Client Debug를 순서대로 빌드해 성공했다. 기존 C4251/C4275 계열 DLL-interface 경고 외 이번 변경이 만든 컴파일·링크 오류는 없었다.
- 독립 비평 실측: 초기 P1 5건과 재비평 P1 1건을 모두 수용해 계약 의미, capability owner, SDK parity, probe/product smoke 구분, 구체 편집, 정상 실행 cwd를 수정했다. 최종 판정은 `PASS — P0 0, P1 0, P2 0`이다.

## 2. 판정

수정 반영 후 PASS. 이번 단계는 “DX11/DX12라는 API가 이 기능을 할 수 있는가”가 아니라 “현재 Winters의 선택된 backend가 공통 RHI를 통해 안전하게 제공한다고 증명한 기능은 무엇인가”를 보고한다. 따라서 현재 DX12는 `apiRequiresExplicitResourceStates=true`이면서 `supportsResourceTransitions=false`다. 전자는 API 구현 의무이고 후자는 아직 완성되지 않은 runtime 기능이므로 모순이 아니다.

현재 보고값은 다음과 같다.

| 선택 결과 | capability owner | feature tier | compute | async compute | bindless | resource transitions | API explicit states | frame resource slots |
|---|---|---|---:|---:|---:|---:|---:|---:|
| DX11 | DX11 | LegacyDX11 | no | no | no | no | no | 1 |
| DX12 | DX12 | ExplicitDesktop | no | no | no | no | yes | 2 |
| Vulkan 미등록 실패 | None | None | no | no | no | no | no | 0 |

## 3. 검증

- Engine Debug: PASS
- Client Debug: PASS
- source/EngineSDK 공개 헤더 SHA-256 parity: PASS 2/2
- `Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug`: PASS 8/8
- non-probe DX11 product smoke, repo root cwd: PASS, exit 0
- path-scoped `git diff --check`: 오류 없음. 기존 줄바꿈 정규화 경고만 관찰됨.
- stale helper/field 검색: `RHI_MakeDefaultCapabilities`, `requiresExplicitResourceStates`, `maxFramesInFlight` 0건

## 4. 갱신된 트레이드오프와 비주장

- DX12 `TransitionResource`와 state tracking은 아직 완성되지 않았다. explicit-state 의무를 보고하는 것은 구현 완료 주장이 아니다.
- DX11의 transition no-op이 의미적으로 안전한 경우와, DX12/Vulkan의 미구현 no-op처럼 위험한 경우를 아직 타입/검증 정책으로 분리하지 않았다.
- compute pipeline, dispatch, async queue, indirect draw, render-pass contract, bindless와 Vulkan backend/module은 이번 단계 범위가 아니다.
- `featureTier`는 backend 분류이며 제품 parity의 증거가 아니다. 제품 parity는 이후 동일 장면·동일 입력·동일 산출물 conformance로 별도 증명한다.
- `frameResourceSlotCount`는 backend가 실제 소유한 frame별 resource 슬롯 수일 뿐 swapchain latency, GPU queue depth 또는 최적 frame-in-flight 수를 뜻하지 않는다.

## 5. 다음 단계

Step 2B에서는 공통 RHI의 빈 구현을 세 종류로 분류한다.

1. 호출 의미가 backend에 흡수되어 안전한 semantic no-op
2. Begin/End 같은 lifecycle 안에서 다른 API 호출로 이미 충족되는 absorbed operation
3. 호출이 필요한데 비어 있는 unimplemented no-op

첫 구현 후보는 DX12 resource-state tracker와 실제 `ResourceBarrier` 발행이다. 새 dated PLAN에서 현재 resource wrapper의 상태 소유자, transition 호출 지점, swapchain backbuffer의 `PRESENT <-> RENDER_TARGET` 전이, upload/default resource 초기 상태와 검증 캡처를 먼저 고정하고 독립 비평을 통과한 뒤 수정한다.
