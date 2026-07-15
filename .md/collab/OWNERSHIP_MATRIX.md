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

## Agent 레인 (Claude / Codex)

장비 분담과 독립인 축. 같은 장비 안에서 두 에이전트가 같은 트리를 만질 때의 기본 레인.

| 레인 | 기본 Owner | 산출물/경로 | 주의 |
| --- | --- | --- | --- |
| Implementation | Codex | 코드 적용 전반, `Plan/S{NNN}_*_SESSION_*.md` / `*_RESULT_*.md`, 빌드·하네스 실행 | 세션 시작 전 work packet 등록(Agent 필드 포함) |
| Audit / Design | Claude | 독립 감사(읽기), 설계·게이트·핸드오프 문서, `.md/collab/**`, `.md/plan/**` | 코드 수정은 사용자가 명시 지시 + packet 등록 시에만 |
| 검증 게이트 | 사용자 | 인게임 검증, checkpoint commit 승인 | 실패 채증(로그 라인/스크린샷) 기준으로 후속 레인 배정 |

- 규칙 1: Active packet의 Owned paths는 상대 agent에게 `Handoff` 전까지 read-only다 (장비 규칙과 동일하게 적용).
- 규칙 2: 코드 파일을 두 agent가 같은 시점에 수정하지 않는다. Codex 세션이 Active인 동안 Claude는 해당 경로를 감사(읽기)만 한다.
- 규칙 3: `Plan/S{NNN}` 번호 시퀀스는 두 agent가 공유하되, 상대 agent의 문서에 append하지 않는다. 응답/리뷰는 새 번호 문서로 만든다.
- 규칙 4: Always-Lock 파일 규칙은 agent 레인에도 그대로 적용된다.

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
