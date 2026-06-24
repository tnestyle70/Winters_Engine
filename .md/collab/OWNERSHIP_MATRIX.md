# Ownership Matrix

목적: 두 장비가 같은 repo를 만져도 merge conflict를 줄이기 위해 기본 소유 경로를 나눈다.

## 기본 분담

| 영역 | 기본 Owner | Owned paths | 주의 |
| --- | --- | --- | --- |
| RHI backend/core | Laptop | `Engine/Private/RHI/**`, `Engine/Public/RHI/**` | DX11/DX12 concrete type을 Client/Public 또는 Shared로 올리지 않는다. |
| Common scene renderer | Laptop | `Engine/Private/Renderer/RHISceneRenderer.cpp`, `Engine/Public/Renderer/RHISceneRenderer.h`, `Engine/Public/Renderer/RenderWorldSnapshot.h` | LoL/Elden 모두 같은 renderer에 snapshot을 공급한다. |
| LoL runtime bridge | Desktop | `Client/Private/Scene/Scene_InGameRender.cpp`, `Client/Private/Scene/Scene_InGameLifecycle.cpp`, `Client/Public/Scene/Scene_InGame.h` | `Scene_InGame.h`는 lock 파일처럼 취급한다. |
| Resource/model bridge | Laptop-first, Desktop handoff | `Engine/Private/Resource/Model.cpp`, `Engine/Public/Resource/Model.h`, `Engine/Private/Resource/Mesh.cpp`, `Engine/Public/Resource/Mesh.h` | public header와 SDK sync가 같이 움직인다. |
| Gameplay/GameSim | Laptop or explicit packet | `Shared/GameSim/**`, `Server/**` | 렌더/RHI 작업과 같은 packet에서 섞지 않는다. |
| Bot AI / Champion tactics | Explicit packet | `Shared/GameSim/Systems/ChampionAI/**`, `Shared/GameSim/Components/ChampionAIComponent.h`, `Shared/GameSim/Systems/CommandExecutor/**`, `Data/LoL/ServerPrivate/Gameplay/**`, `Tools/SimLab/**` | AI는 command-only 원칙을 지키고, gameplay definition data와 scenario harness를 같은 packet에서 검증한다. |
| Runtime resource packaging | Desktop | `Client/Bin/Resource/**`, `.md/build/*RESOURCE*` | 대용량 리소스는 git 대상인지 먼저 확인한다. |
| Docs/reports | Current packet owner | `.md/build/YYYY-MM-DD_*.md`, `.md/collab/work-packets/*.md` | 날짜별 새 파일을 선호한다. |

## Always-Lock Files

아래 파일은 충돌 위험이 높으므로 한 packet에서만 수정한다.

- `Client/Public/Scene/Scene_InGame.h`
- `Engine/Public/Renderer/ModelRenderer.h`
- `Engine/Public/Resource/Model.h`
- `Engine/Public/Resource/Mesh.h`
- `Client/Include/Client.vcxproj`
- `Client/Include/Client.vcxproj.filters`
- `Engine/Include/Engine.vcxproj`
- `Engine/Include/Engine.vcxproj.filters`
- `EngineSDK/inc/**`

## EngineSDK 규칙

- `EngineSDK/inc/**`는 직접 hand-edit하지 않는다.
- Engine public header 변경 후 빌드/`UpdateLib.bat`가 생성한 결과만 반영한다.
- SDK generated diff는 원본 Engine public header diff와 같은 commit에 포함한다.

## 충돌 대응

- conflict가 나면 `git status`로 파일 목록을 먼저 확인한다.
- owner가 아닌 장비에서 생긴 conflict는 해당 packet owner가 정리한다.
- conflict 해결 중 unrelated dirty 파일을 reset하지 않는다.
- public header conflict는 implementation conflict보다 먼저 해결하고, 이후 전체 빌드를 다시 돌린다.
