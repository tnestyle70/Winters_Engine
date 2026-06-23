# 2026-06-23 클라이언트 화면 경계 카메라 이동 반영 빌드 보고서

## 반영 내용

- `Client/Public/DynamicCamera.h`
  - edge-scroll 처리용 `ApplyEdgeScroll` 선언을 추가했다.
  - 디버그 로그 중복 방지용 `m_bEdgeScrollActive` 상태를 추가했다.
- `Client/Private/DynamicCamera.cpp`
  - 매 프레임 현재 OS 커서 위치를 `GetCursorPos`로 읽고 `ScreenToClient`로 변환해 실제로 현재 클라이언트 내부에 있을 때만 경계 스크롤을 처리한다.
  - `SetCursorPos` 또는 `ClipCursor`를 새로 사용하지 않아 커서를 클라이언트 내부에 고정하지 않는다.
  - 활성 윈도우가 현재 클라이언트일 때만 이동해 여러 클라이언트를 띄운 상태에서 백그라운드 클라이언트가 같이 움직이는 문제를 막는다.
  - 경계 밴드 18px 안에 커서가 들어오면 카메라의 XZ forward/right 기준으로 팬 이동한다.
  - 팔로우 모드 상태에서 edge-scroll이 시작되면 팔로우를 해제해 실제 LoL/RTS식 자유 카메라 이동이 보이도록 했다.
  - Space를 누르는 동안만 기존 follow 카메라 경로로 들어가 캐릭터 중심 고정 시점처럼 동작한다.
  - Space를 떼면 임시 follow를 해제해 자유 카메라와 edge-scroll 이동이 다시 가능하다.
  - Space 임시 follow는 edge-scroll보다 우선한다.
  - FPS 마우스 고정 모드(`m_bFix`)에서는 edge-scroll을 적용하지 않는다.
  - Debug 빌드에서 edge-scroll 시작 순간 `OutputDebugStringA("[CameraEdgeScroll] begin ...")` 로그를 남긴다.

## 검증 파이프라인

1. 정적 diff 체크

```powershell
git diff --check -- Client\Public\DynamicCamera.h Client\Private\DynamicCamera.cpp
```

결과: 통과. LF/CRLF 변환 경고만 출력됨.

2. Client Debug x64 빌드

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 성공.

- 오류: 0개
- 1차 빌드 경고: 86개
- Space 중앙 스냅 반영 후 증분 재빌드 경고: 23개
- Space 임시 follow 고정 반영 후 재빌드 경고: 86개
- 확인된 경고 유형: 기존 DLL 인터페이스 계열 C4251/C4275 중심

3. 작업 트리 영향 확인

```powershell
git status --short -- Client\Public\DynamicCamera.h Client\Private\DynamicCamera.cpp .md\build Shared\Schemas\Generated\cpp
```

결과:

- 수정: `Client/Private/DynamicCamera.cpp`
- 수정: `Client/Public/DynamicCamera.h`
- 신규: `.md/build/2026-06-23_CLIENT_EDGE_SCROLL_CAMERA_REPORT.md`
- 기존 미추적: `.md/build/2026-06-22_RESOURCE_TRANSFER_SLICE_REPORT.md`
- `Shared/Schemas/Generated/cpp`에 이번 빌드로 인한 추가 변경 없음

## 인게임 확인 항목

- 활성 클라이언트 안에서 커서를 좌/우/상/하 경계 18px 근처로 가져가면 카메라가 해당 방향으로 이동해야 한다.
- 커서를 클라이언트 밖으로 빼면 카메라 이동이 멈춰야 한다.
- 커서가 클라이언트 내부에 고정되면 안 된다.
- 여러 클라이언트를 띄웠을 때 현재 활성 클라이언트만 edge-scroll 반응을 해야 한다.
- 팔로우 상태에서 최초 edge-scroll을 하면 자유 카메라 상태로 전환되어 화면 이동이 유지되어야 한다.
- Space를 누르는 동안 카메라가 기존 follow 카메라처럼 캐릭터 중심을 유지해야 한다.
- Space를 누른 상태에서 커서가 화면 경계에 있어도 edge-scroll보다 임시 follow가 우선해야 한다.
- Space를 떼면 follow가 해제되고 화면 경계 이동으로 다시 카메라를 움직일 수 있어야 한다.
- 디버거 Output 창에서 edge-scroll 시작 순간 `[CameraEdgeScroll] begin ...` 로그가 확인되어야 한다.
