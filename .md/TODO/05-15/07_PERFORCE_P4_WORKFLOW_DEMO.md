# Perforce P4 Workflow Demo

Session - Perforce/P4 workflow demo로 binary asset lock, changelist, editor checkout, folder-level game asset collaboration을 증명한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/.md/process/P4_WORKFLOW_DEMO.md

새 파일:
- Winters용 Perforce/P4 workflow 절차서를 만든다.

반영:
- depot layout, stream/mainline, workspace mapping, ignore list, typemap, binary lock 규칙을 적는다.
- Git과 P4 역할 분리를 명시한다.
- code는 Git, large binary content는 P4 또는 Git LFS/P4 비교 대상으로 둔다.

### 1-2. C:/Users/user/Desktop/Winters/Tools/SourceControlBridge

새 파일:
- editor에서 source control 상태를 조회하는 bridge interface를 만든다.

반영:
- MVP interface는 `CheckOut`, `RevertUnchanged`, `MarkForAdd`, `GetFileState`만 둔다.
- 구현체는 `P4CommandLineProvider`로 시작한다.
- P4 설치가 없으면 `NullSourceControlProvider`로 graceful fallback한다.

### 1-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Editor.cpp

목표:
- editor save 전에 asset checkout 상태를 확인한다.

반영:
- map/stage/navgrid/fx asset 저장 시 writable 여부와 source control state를 확인한다.
- checkout 실패 시 저장을 중단하고 UI message를 표시한다.
- source control bridge가 disabled일 때는 기존 local save path를 유지한다.

### 1-4. C:/Users/user/Desktop/Winters/Client/Private/UI/SourceControlPanel.cpp

새 파일:
- P4 상태 확인용 ImGui panel을 만든다.

반영:
- workspace root, current changelist, selected asset state, checkout/revert 버튼을 둔다.
- 실제 submit은 MVP에서 하지 않고 command preview만 표시한다.
- artist-friendly 용어로 lock/check-out 상태를 보여준다.

### 1-5. C:/Users/user/Desktop/Winters/Content 또는 Resource 경로

목표:
- binary asset workflow를 보여줄 sample asset set을 만든다.

반영:
- `.wmesh`, `.wanim`, `.wfx`, texture sample을 P4 lock 대상 예시로 둔다.
- One File Per Actor 유사 구조를 Winters world cell/actor asset naming과 연결한다.
- folder move/rename 규칙을 문서화한다.

확인 필요:
- 실제 content root가 `Content/`인지 `Client/Bin/.../Resource` 중심인지 정리한 뒤 demo path를 고정한다.

### 1-6. C:/Users/user/Desktop/Winters/.md/문서/16_Ch16_Collaboration.md

목표:
- 협업 topology 문서와 P4 demo가 같은 규칙을 가리키게 한다.

반영:
- Runtime/Editor/Developer/Tools 의존성 방향과 source control checkout 규칙을 연결한다.
- binary asset lock과 external actor/cell asset 분할 원칙을 짧게 링크한다.

## 2. 검증

검증 명령:
- `git diff --check`
- `p4 info`
- `p4 opened`
- `p4 fstat <sample asset path>`

수동 검증:
- sample `.wfx` 또는 `.wmesh`를 checkout한다.
- Editor panel에서 checked-out 상태를 확인한다.
- checkout하지 않은 locked asset 저장이 차단되는지 확인한다.
- P4 미설치 환경에서 Null provider fallback으로 editor가 죽지 않는지 확인한다.

합격 기준:
- 포트폴리오 영상에서 binary asset lock -> edit -> changelist 상태 확인 흐름을 보여줄 수 있다.
- source control demo가 실제 gameplay/runtime path를 깨지 않는다.
