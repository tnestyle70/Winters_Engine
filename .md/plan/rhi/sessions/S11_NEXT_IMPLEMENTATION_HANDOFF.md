# S11. Next Implementation Handoff

작성일: 2026-05-25

목표: DX12 SmokeHost / standalone 테스트 프로젝트를 다시 만들지 않고, 현재 `main`의 실제 RHI 상태를 기준으로 다음 구현 세션을 이어간다.

## 현재 실제 상태

- Branch: `main`
- Remote: `origin/main`과 동기화됨
- 작업트리 주의: `.md/EldenRing/00_ELDENRING_INDEX.md`, `.md/EldenRing/10_ASSET_PIPELINE_TOOLING.md`, `Tools/EldenAssetPipeline/` 변경은 RHI 작업 범위 밖의 기존 작업으로 간주한다.
- `DX12.exe`, `DX12.vcxproj`, `Smoke.vcxproj`, `Tools/DX12SmokeHost`는 현재 tracked tree에 없다.
- `Tools/DX12SmokeHost`는 삭제되었고 `Winters.sln` 등록도 제거되었다.
- `Client/Bin/Data`, `Client/Bin/Resource`, ThirdParty lib/bin, `Tools/Bin/flatc.exe`는 clone 재현성을 위해 LFS/track 상태로 올라가 있다.
- Engine / Server / Client Debug, Release 빌드가 통과한 상태에서 push 완료되었다.

## 현재 RHI 코드 상태

- `CGameInstance::Get_RHIDevice()`는 현재 `IRHIDevice*`를 반환한다.
- `Get_MeshShader()`, `Get_MeshPipeline()`, `Get_BlendStateCache()`는 아직 DX11 concrete shim이다.
- 이전 계획의 `*_LegacyDX11` 이름은 rebase 이후 최종 `main`에는 남아 있지 않다.
- 따라서 다음 목표는 이름만 바꾸는 것이 아니라, remaining DX11 shim 호출부를 audit하고 `IRHIDevice` 기반 경계로 좁히는 것이다.

## /plan-rules

1. Standalone backend exe/project 금지
   - 만들지 않는다: `DX12.exe`, `Vulkan.exe`, `Smoke.exe`
   - 만들지 않는다: `DX12.vcxproj`, `Vulkan.vcxproj`, `Smoke.vcxproj`, `*SmokeHost.vcxproj`
   - backend 검증은 Engine/Client config, runtime flag, 또는 기존 project 내부 smoke 함수로만 한다.

2. Public/SDK native leak 증가 금지
   - `Engine/Include`, `Engine/Public`, `EngineSDK/inc`에 새 `d3d11.h`, `ID3D11*`, `RHI/DX11/*` 노출을 추가하지 않는다.
   - 기존 DX11 concrete type은 즉시 삭제하지 말고 shim 목록에 올린 뒤 하나씩 줄인다.

3. 코드 이동보다 경계 축소 우선
   - 먼저 호출부를 audit한다.
   - 다음에 facade를 정한다.
   - 그 다음 renderer/resource 단위로 치환한다.

4. 검증 게이트
   - 최소: `git diff --check`
   - 최소: Engine Debug build
   - Client 호출부를 건드렸으면 Client Debug build
   - RHI public contract를 건드렸으면 Server Debug build도 확인한다.

5. 문서 갱신 규칙
   - 세션 완료 시 이 파일 또는 해당 Sxx 문서에 `Actual Result`를 남긴다.
   - 계획과 실제 코드가 달라지면 실제 코드를 우선하고 문서를 정정한다.

## 다음 구현 세션

### Session 00. Frame Loop Shared Entry

기준 계획:
- `S14_PLAN_RULES_FRAME_LOOP_SHARED_ENTRY.md`

목표:
- `CEngineApp`의 update/render loop가 DX11 여부와 무관하게 `SceneManager` entry를 호출하게 한다.
- DX11 전용 ImGui/UI cursor/profiler overlay는 DX11 gate 안에 남긴다.
- LoL과 Elden이 같은 SceneManager render entry 위에서 이후 `CRHISceneRenderer`로 이관될 수 있게 한다.

완료 기준:
- DX11 기존 scene flow 유지
- DX12에서도 scene update/render entry 호출
- standalone smoke project 없음

### Session 01. RHI Boundary Audit

산출물:
- DX11 direct call 목록
- DX11 shim 목록
- RHI-ready 목록

대상:
- `Engine/Include`
- `Engine/Public`
- `Engine/Private/RHI`
- `Engine/Private/Renderer`
- `Engine/Private/Framework`
- `Client/Public`
- `Client/Private`

검색 기준:

```powershell
rg -n "d3d11.h|ID3D11|ID3D11Device|ID3D11DeviceContext|CDX11Device|DX11Shader|DX11Pipeline|CBlendStateCache|RHI/DX11" Engine Client
```

완료 기준:
- 새 standalone smoke project 없음
- audit 결과가 `S12_RHI_BOUNDARY_AUDIT.md`에 정리됨
- Client에서 새 DX11 direct dependency 증가 없음

### Session 02. GameInstance RHI Facade

목표:
- `CGameInstance`의 RHI 진입점과 legacy render shim을 명확히 분리한다.
- `Get_RHIDevice()`는 `IRHIDevice*` 공식 통로로 유지한다.
- DX11-only shader/pipeline getter는 다음 치환 대상임을 문서와 주석으로 고정한다.

완료 기준:
- `Get_RHIDevice()` 호출부가 RHI 방향으로 쓰이는지 확인
- shader/pipeline/blend getter 호출부가 audit 목록에 올라감
- 이름 변경만으로 빌드 리스크를 만들지 않음

### Session 03. DX11 Adapter Expansion

목표:
- `CDX11Device`가 `IRHIDevice` 구현으로 담당할 영역을 넓힌다.
- buffer/texture/shader/sampler/native handle API를 현재 public RHI contract와 맞춘다.

완료 기준:
- DX11 backend가 기존 렌더 동작을 깨지 않음
- DX12 backend와 같은 interface vocabulary를 사용
- public header에 DX11 native type 추가 노출 없음

## 금지된 우회로

- `Tools/DX12SmokeHost` 재생성
- `Tools/Smoke` 재생성
- `Winters.sln`에 backend smoke project 재등록
- build 통과를 위해 RHI public type을 DX11 concrete로 되돌리기
- `Client`에서 새 `ID3D11*` 의존성 추가
