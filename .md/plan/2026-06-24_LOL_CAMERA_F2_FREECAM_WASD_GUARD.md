# LoL Camera F2 Freecam WASD Guard

작성일: 2026-06-24
성격: 버그 원인 분석 + 수정 설계 + 검증 기록

## 증상

인게임에서 F2 자유시점 상태가 아닌데도 `W` 입력으로 카메라가 앞으로 이동한다.

정상 기대:

- F2 자유시점 카메라 모드에서는 WASD가 카메라를 이동한다.
- F2 자유시점이 아니면 `W`는 챔피언 스킬 입력으로만 동작해야 한다.
- LoL식 카메라 이동인 엣지 스크롤, 미니맵 카메라 점프, Space 임시 추적은 유지한다.

## 원인

`CDynamicCamera`의 기존 상태는 `m_bFollowMode` 하나로 너무 많은 의미를 표현한다.

현재 흐름:

```text
CDynamicCamera::Update
  if target exists && m_bFollowMode
    Update_FollowCam
  else
    Update_FreeCam

CDynamicCamera::Update_FreeCam
  Key_Input(WASD)
```

문제는 `m_bFollowMode == false`가 반드시 F2 자유시점을 뜻하지 않는다는 점이다.

`m_bFollowMode`가 false가 되는 정상 경로:

- F2 자유시점 전환
- 엣지 스크롤 시작
- 미니맵 카메라 점프
- Space 임시 follow 해제 후 이전 unlocked camera 상태 복귀

그런데 `Update_FreeCam()`은 이 모든 상태를 동일하게 취급하고 즉시 `Key_Input()`을 호출한다. 그래서 F2가 아닌 unlocked camera 상태에서도 `W/A/S/D`가 카메라 입력으로 소비된다.

## 수정 원리

`m_bFollowMode`와 `WASD free camera input enabled`를 분리한다.

새 의미:

- `m_bFollowMode`: 카메라가 target을 따라갈지 여부
- `m_bFreeCameraMode`: F2 또는 명시적 free camera 설정으로 WASD/마우스 자유시점 입력을 허용할지 여부
- `m_bFix`: 기존 cursor lock / mouse look 활성 상태

수정 규칙:

```text
F2 on:
  m_bFreeCameraMode = true
  m_bFollowMode = false
  m_bFix = true
  Enter_FPSMode

F2 off:
  m_bFreeCameraMode = false
  m_bFollowMode = true
  m_bFix = false
  Exit_FPSMode

Edge scroll / minimap camera jump:
  m_bFollowMode = false
  m_bFreeCameraMode = false
  WASD는 차단

Update_FreeCam:
  m_bFreeCameraMode일 때만 Key_Input(WASD)
  m_bFreeCameraMode일 때만 Tab mouse-look toggle / Mouse_Move
```

## 반영 파일

- `Client/Public/DynamicCamera.h`
- `Client/Private/DynamicCamera.cpp`

## 검증 기준

자동:

- `git diff --check -- Client/Public/DynamicCamera.h Client/Private/DynamicCamera.cpp .md/plan/2026-06-24_LOL_CAMERA_F2_FREECAM_WASD_GUARD.md`
- `MSBuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동:

- F2 off 상태에서 `W`를 눌러도 카메라가 앞으로 이동하지 않는다.
- F2 on 상태에서 WASD로 카메라가 이동한다.
- F2 on 상태에서 마우스 자유시점이 동작한다.
- 미니맵 카메라 점프 후 `W`를 눌러도 카메라가 앞으로 이동하지 않는다.
- 엣지 스크롤은 F2 off 상태에서도 계속 동작한다.

## 검증 결과

자동 검증:

- `git diff --check -- Client/Public/DynamicCamera.h Client/Private/DynamicCamera.cpp .md/plan/2026-06-24_LOL_CAMERA_F2_FREECAM_WASD_GUARD.md`: PASS
- `Client/Include/Client.vcxproj` Debug x64 build: PASS

빌드 참고:

- 기존 EngineSDK DLL interface 계열 `C4251`/`C4275` 경고가 출력되었다.
- 기존 일부 파일의 `C4828` 인코딩 경고가 출력되었다.
- 이번 카메라 변경으로 인한 compile error는 없다.

수동 확인:

- 아직 미실행. F5 인게임에서 F2 off `W`, F2 on WASD, 미니맵 점프 후 `W`, 엣지 스크롤을 확인해야 한다.
