# FX Manual Session Recovery - 2026-05-08

## 현재 대화 규칙

- 사용자가 직접 코드베이스에 한 줄씩 반영하면서 이해하는 것이 목표다.
- Codex는 기본적으로 코드 파일을 직접 수정하지 않는다.
- 진행 방식은 `코드 출력 -> 사용자가 수동 반영 -> Codex 검토 -> 다음 코드 출력`이다.
- 예외: 사용자가 명시적으로 `직접 반영`을 요청한 경우에만 코드베이스를 수정한다.
- EngineSDK는 원칙적으로 수동 편집 대상이 아니다. `Engine/Public`이 원본이고 `UpdateLib.bat` 또는 빌드 이벤트가 `EngineSDK/inc`로 복사한다.

## 큰 목표

- 근본 목표는 LoL 스타일 FX 구현이다.
- 기준 문서: `.md/TODO/05-07/FX개념!/AAA_VFX_GRAYSCALE_AND_SHADER.md`
- DX12 마이그레이션은 목적이 아니라, FBX/mesh FX를 안정적으로 얹기 위한 수단이다.
- 현재 수동 반영 트랙은 먼저 DX11 레거시 FX를 안정화하고, 이후 RHI/DX12로 올릴 수 있는 중립 FX ABI를 만드는 흐름이다.

## 현재 수동 반영 단계

### Step 1 완료 검토

- 새 헤더: `Engine/Public/FX/FxMaterialDesc.h`
- 프로젝트 등록: `Engine/Include/Engine.vcxproj` 등록 확인.
- 필터 등록: `Engine/Include/Engine.vcxproj.filters`의 `03. Renderer\05. FX` 등록 확인.
- SDK 복사본: `EngineSDK/inc/FX/FxMaterialDesc.h`도 Public 헤더와 동일함을 확인.

### Step 2 완료 검토

- 새 헤더: `Engine/Public/FX/FxDepthMode.h`
- SDK 복사본: `EngineSDK/inc/FX/FxDepthMode.h`도 Public 헤더와 동일함을 확인.
- 프로젝트 등록: `Engine/Include/Engine.vcxproj` 등록 확인.
- 필터 등록: `Engine/Include/Engine.vcxproj.filters`의 `03. Renderer\05. FX` 등록 확인.
- `git diff --check` 기준 해당 파일들 공백 오류 없음.

### SDK 배포 메모

- `UpdateLib.bat`는 `Engine/Public`을 `EngineSDK/inc`로 재귀 복사한다.
- 따라서 SDK가 재귀 복사를 못 하는 구조는 아니다.
- 단, SDK 반영은 `UpdateLib.bat` 실행 또는 Client/Engine 일부 구성의 PreBuild/PostBuild 실행 시점에 일어난다.
- DX12 Engine 구성에는 현재 Engine 자체 PostBuild SDK 배포가 빠져 있다. Client DX12 구성은 PreBuild에서 `UpdateLib.bat`를 호출한다.
- 이후 수동 코드 출력은 `Engine/Public` 원본 기준으로 안내하고, SDK는 빌드 후 stale 여부만 검토한다.

### 다음 단계

1. `Engine/Public/FX/FxAsset.h`에 `FxMaterialDesc.h`, `FxDepthMode.h`를 include하고, `FxEmitterDesc`에 `material/depthMode`를 추가한 상태다.
2. `Engine/Private/FX/FxAsset.cpp`의 asset parser에서 legacy 필드 파싱 후 `FxEmitterSetMaterialFromLegacyFields(emitter)`를 호출하는 코드가 반영됐다.
3. `ParseWfxJson`의 moved emitter duplicate push는 제거됐다. fallback emitter push는 정상이다.
4. `Client/Public/GameObject/FX/FxMeshComponent.h`에 `FxMaterialDesc material`, `eFxDepthMode depthMode`, `SetMaterialFromDesc`, `RefreshMaterialFromLegacyFields`가 반영됐다.
5. `Client/Private/GameObject/FX/FxMeshSystem.cpp`와 `Client/Private/GameObject/FX/LegacyFxAdapter.cpp`에서 asset material/depthMode를 실제 mesh component로 전달하는 변경이 직접 반영됐다.
6. `FxMeshSystem::Render`에서 렌더 파라미터를 `FxMeshComponent::material/depthMode` 기준으로 구성하도록 직접 반영됐다.
7. `FxMeshComponent`에는 `bMaterialReady`가 추가됐고, 코드형 legacy 프리셋은 `Spawn`/`Render`에서 `RefreshMaterialFromLegacyFields()`로 material 기준값을 채운다.
8. depth mode를 renderer까지 명시적으로 전달해 `OverlayNoDepth` 같은 의도를 실제 DX11 depth state에 반영하는 변경이 반영됐다.
9. `LegacyFxAdapter`, `FxLegacyAssetDumper`, `EffectTuner`에 depth mode round-trip이 직접 반영됐다.
10. 다음은 `.wfx` 로더가 dumper가 쓰는 `scale`, `rotation`, `uv_scroll`, `fade_in/out`, offset/velocity 같은 필드를 다시 읽도록 보강한다.

## 방금 확인한 핵심 상태

- `Engine/Public/FX/FxMaterialDesc.h` 내용은 정상이다.
- enum 값은 현재 `LOLBrushRim`으로 들어가 있다. 이후 코드 출력 시 이 이름에 맞춰 진행하거나, 사용자가 원하면 `LoLBrushRim`으로 정리한다.
- `FxDepthMode`는 아직 실제 렌더 경로에 연결되지 않은 중립 의도 표현이다.
