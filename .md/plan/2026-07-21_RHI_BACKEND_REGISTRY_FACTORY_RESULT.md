Session - RHI Backend Registry/Factory 구현 결과

## 1. 예측 대 실측

- 예측: `CEngineApp.cpp`의 concrete DX11/DX12 Device 생성 호출과 DX12 Device header가 제거되고, Registry 구현만 concrete Device를 생성한다.
  - 실측: `CEngineApp.cpp`에서 `CDX11Device::Create`, `CDX12Device::Create`, `CreateDX11DeviceForWindow`, `CreateDX12DeviceForWindow`, `RHI/DX12/DX12Device.h` 검색 결과가 0건이다. 새 `RHIBackendRegistry.cpp`에 DX11/DX12 생성 호출이 각각 1건 존재한다.
  - 계획 수정대로 legacy shader/cache bootstrap의 `dynamic_cast<CDX11Device*>` 2곳은 남아 있다.
- 예측: Auto/DX11/DX12는 등록 module과 실제 selected backend를 함께 증명하고 Vulkan은 module 미등록으로 실패한다.
  - 실측: 8개 truth gate가 모두 통과했다. 성공 case는 `module=DX11|DX12`, `reason=device_created`, `fallback=no`를 확인했다. Vulkan은 `module=None`, `selected=None`, `status=module_not_registered`, `reason=backend_module_not_registered`와 non-zero exit를 확인했다.
- 예측: Engine→Client Debug 빌드와 일반 DX11 제품 경로가 유지된다.
  - 실측: Engine Debug와 Client Debug가 순서대로 성공했고, 새 Engine DLL이 Client runtime 경로에 배포됐다. `WINTERS_RHI=dx11`, `--run-seconds=2` 제품 smoke가 exit 0으로 종료했다.
- 독립 비평 실측: 최초 P1 raw diagnostic pointer 수명 문제를 `std::string` 소유값과 fallback 전 복사로 수정한 뒤 최종 `PASS(P0 0, P1 0)`를 받았다.

## 2. 판정

수정 반영 후 PASS. `CEngineApp`의 backend 선택 정책과 concrete Device module/factory 책임이 분리됐고, 정적 Registry가 미등록·preflight 거절·생성 실패·backend identity 불일치를 서로 다른 상태로 표현한다. 이번 runtime에서 직접 증명한 상태는 성공과 module 미등록이며, 나머지 실패 분기는 제품 parity 근거로 주장하지 않는다.

## 3. 갱신된 트레이드오프

- 정적 Registry는 현재 DX11/DX12 두 module만 포함하며 동적 DLL module loader가 아니다. Windows Vulkan 구현 시 같은 module 계약을 추가하되 renderer를 복제하지 않는다.
- preflight는 Win32 window와 extent만 검사한다. 실제 adapter/driver/API 지원은 concrete Device 생성 결과에 맡기며, native capability 질의와 feature profile 판정은 Step 2에서 구현한다.
- `CEngineApp`에는 Device 생성 호출은 없어졌지만 legacy DX11 shader/cache를 위한 `CDX11Device` downcast 2곳이 남는다. 이 부채는 LoL DX11 제품 기능을 공통 RHI로 이관하는 Step 3의 삭제 기준이다.
- fallback 성공, ProbeRejected, DeviceCreationFailed, BackendIdentityMismatch를 강제로 재현하는 전용 테스트는 후속 RHI conformance suite에 남는다.
