Session - ShaderResource texture transition 첫 수직 절편 결과

## 1. 예측 vs 실측

- 적중 — 기존 제품 `CreateTexture` 호출자 6곳은 모두 ShaderResource 전용이어서 Engine Debug와 Client Debug 빌드가 유지됐다.
- 적중 — probe는 기존 제품과 같은 initial-data upload 경로로 2x2 texture를 생성했다. DX12 upload가 끝난 뒤 wrapper 상태는 ShaderResource였고, `ShaderResource -> CopyDest -> ShaderResource` 전이에서 일반 RHI texture barrier가 정확히 2회 발행됐다.
- 적중 — DX11은 동일 전이를 frame·handle·usage·state 검증 후 semantic no-op으로 흡수했고 native barrier count는 0이었다.
- 적중 — RenderTarget, DepthStencil, UnorderedAccess, ShaderResource|RenderTarget usage와 `depth=2`는 생성에 실패했다. ShaderResource/CopyDest 외 9개 resource state도 모두 실패했고 barrier count를 늘리지 않았다.
- 적중 — DX12 queue fence 완료 뒤 Debug InfoQueue ERROR/CORRUPTION은 0이었다. DX11은 validation not applicable로 보고했다.
- 적중 — `Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug`는 8/8 PASS였다. DX11 texture 결과는 `pass/0/not_applicable`, DX12는 `pass/2/pass`, 미등록 Vulkan/None은 `not_run/0/not_run`이었다.
- 적중 — repo root cwd의 non-probe `WintersGame.exe --rhi=dx11 --run-seconds=2`는 exit 0이었다.
- 적중 — path-scoped `git diff --check`는 오류 없이 통과했고 기존 LF→CRLF 경고만 관찰됐다.
- 계획 비평 — 초기 `FAIL — P0 0, P1 2, P2 1`의 report/harness 불완전 지시와 음성 probe 부족을 모두 수용해 수정했다. 델타 재비평은 `PASS — P0 0, P1 0, P2 0`이었다.

## 2. 판결

수정 반영 후 PASS. ShaderResource 전용 2D texture의 생성 계약과 `ShaderResource <-> CopyDest` 상태 전이는 DX11/DX12에서 검증됐다. 다만 전체 texture transition 기능은 아직 완성되지 않았으므로 두 backend의 `supportsResourceTransitions`는 계속 false다.

## 3. ⑤ 갱신

- exact-usage fail-closed는 아직 view/descriptor 수명이 없는 RenderTarget·DepthStencil·UAV를 성공처럼 보이지 않게 한다. 2C-2/2C-3에서 각 usage를 native view와 render-pass/UAV probe까지 함께 추가할 때만 허용 범위를 넓힌다.
- DX11의 semantic no-op은 무검증 no-op이 아니다. 허용 state가 늘어날 때마다 native bind/copy hazard와 실제 사용 가능성을 먼저 증명해야 한다.
- DX12의 texture wrapper가 상태를 소유하므로 upload 경로와 frame transition 경로가 같은 `state`를 갱신해야 한다. 별도 upload queue나 subresource별 전이가 도입되면 단일 all-subresources state는 틀리게 된다.
- 현재 probe는 모든 mip/subresource를 한 상태로 취급한다. mip별 streaming이나 split barrier가 필요해지는 시점에는 subresource state tracker로 교체해야 한다.
- color/depth attachment view, render-pass load/store, UAV descriptor/barrier, DX12 LoL 제품 parity와 Vulkan backend는 이번 결과의 비주장 범위다.
