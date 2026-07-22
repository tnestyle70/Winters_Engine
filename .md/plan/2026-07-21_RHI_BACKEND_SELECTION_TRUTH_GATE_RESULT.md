Session - RHI backend selection truth gate 구현 결과

## 1. 예측 대 실측

- 예측: 기본값은 `Auto` 요청으로 들어가 DX11 Device를 선택하고, 명시적 DX11·DX12는 동일 backend를 선택하며, 미구현 Vulkan은 DX11로 fallback하지 않고 실패한다.
  - 실측: `default_auto`, `env_dx11`, `env_dx12`, `env_vulkan`이 모두 예상한 source/requested/selected/status와 exit code를 만족했다.
- 예측: CLI는 `WINTERS_RHI`보다 우선하고, 빈 CLI·중복 CLI·잘못된 환경값은 엔진 진입 전에 실패한다.
  - 실측: `cli_precedence`, `empty_cli`, `duplicate_cli`, `invalid_env`가 모두 통과했다.
- 예측: Client Debug 빌드와 일반 DX11 제품 경로가 유지된다.
  - 실측: Engine Debug와 Client Debug를 순서대로 빌드해 성공했고, 기존 C4251/C4275 경고 외 새 컴파일 오류는 없었다. probe 환경변수 없이 `WINTERS_RHI=dx11`, `--run-seconds=2` 제품 smoke가 exit 0으로 종료했다.
- 빌드 경로 보정: 계획 초안의 Visual Studio 18 경로는 이 머신에 없었고, `Winters.sln /t:Client`는 솔루션 target으로 유효하지 않았다. 설치된 VS 2022 MSBuild로 Engine/Client `.vcxproj`를 직접 빌드했다.
- 배포 순서 실측: Client는 Engine `ProjectReference`가 없어서 Client만 먼저 빌드하면 이전 `Client/Bin/Debug/WintersEngine.dll`이 남았다. Engine 빌드로 `EngineSDK`를 갱신한 뒤 Client를 다시 빌드하자 새 DLL이 배포됐고 8개 truth gate가 전부 통과했다.

## 2. 판정

PASS. LoL의 RHI 요청은 exact argv, `WINTERS_RHI`, 기본 Auto 순서로 결정되며 명시적 요청은 fail-closed로 동작한다. probe는 요청 출처, 요청 backend, 실제 Device backend, 상태를 기계 판독 가능한 report로 증명한다. S17의 근거 없는 `probe_dx12` 명칭도 `probe_default`로 교정했다.

## 3. 갱신된 트레이드오프

- 이번 probe는 실제 Device·SwapChain 생성과 backend identity만 증명한다. DX12의 제품 화면 parity나 Vulkan backend 구현을 증명하지 않는다.
- `WINTERS_INTERNAL_RHI_REQUEST_SOURCE`는 Client와 Engine 사이의 관찰용 process-local 전달 채널이다. 다음 Registry/selection contract 단계에서 구조화된 요청 정보로 대체할지 검토한다.
- 정확한 회귀를 위해 Debug 빌드는 `Engine -> Client` 순서를 지켜야 한다. 다음 단계에서는 이 순서를 전용 build gate 또는 solution dependency로 고정할 필요가 있다.
