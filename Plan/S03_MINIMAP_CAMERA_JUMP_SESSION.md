Session - S03 minimap left click moves the camera frame and camera focus to clicked world XZ.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/DynamicCamera.h

기존 코드:

```cpp
void SnapToTarget();
```

아래에 추가:

```cpp
void JumpToWorldXZ(const Vec3& vWorldPos);
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/DynamicCamera.cpp

기존 코드:

```cpp
void CDynamicCamera::SnapToTarget()
{
    if (!m_pTargetTransform) return;
    Vec3 vTargetPos = m_pTargetTransform->GetPosition();
    m_vEye = vTargetPos + m_vFollowOffset;
    m_vAt  = vTargetPos + Vec3(0.f, 1.5f, 0.f);
    m_bFollowInitialized = true;
    RecalcView();
}
```

아래에 추가:

```cpp
void CDynamicCamera::JumpToWorldXZ(const Vec3& vWorldPos)
{
    const Vec3 vEyeOffset{
        m_vEye.x - m_vAt.x,
        m_vEye.y - m_vAt.y,
        m_vEye.z - m_vAt.z
    };

    Exit_FPSMode();
    m_bFollowMode = false;
    m_bFix = false;
    m_bFollowInitialized = false;

    m_vAt.x = vWorldPos.x;
    m_vAt.z = vWorldPos.z;
    m_vEye = {
        m_vAt.x + vEyeOffset.x,
        m_vAt.y + vEyeOffset.y,
        m_vAt.z + vEyeOffset.z
    };

    RecalcView();
}
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
void UpdateTargeting();
```

아래에 추가:

```cpp
bool_t TryResolveMinimapClickTarget(Vec3& vOutWorldPos) const;
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`CScene_InGame::UpdateTargeting` 바로 아래에 추가:

```cpp
bool_t CScene_InGame::TryResolveMinimapClickTarget(Vec3& vOutWorldPos) const
{
    const CInput& input = CInput::Get();
    UI::MinimapFrameState MinimapInputState{};
    const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
    MinimapInputState.Projection = UI::GetDefaultMinimapProjection();
    MinimapInputState.fScreenWidth =
        DisplaySize.x > 0.f ? DisplaySize.x : kFallbackScreenWidth;
    MinimapInputState.fScreenHeight =
        DisplaySize.y > 0.f ? DisplaySize.y : kFallbackScreenHeight;

    return UI::TryResolveMinimapClickWorldPos(
        MinimapInputState,
        static_cast<f32_t>(input.GetMouseX()),
        static_cast<f32_t>(input.GetMouseY()),
        vOutWorldPos);
}
```

`CScene_InGame::UpdateCombatInput`의 좌클릭 공격 판정은 미니맵 좌클릭을 제외한다.

```cpp
Vec3 minimapClickTarget{};
const bool_t bMinimapLeftClick =
    !bImGuiMouse &&
    in.IsLButtonPressed() &&
    TryResolveMinimapClickTarget(minimapClickTarget);
const bool_t bAttackMoveClick =
    !bImGuiMouse && !bMinimapLeftClick && in.IsKeyDown('A') && in.IsLButtonPressed();
```

`CScene_InGame::UpdatePlayerControl`의 미니맵 좌클릭 처리는 이동 명령이 아니라 카메라 점프다.

```cpp
Vec3 minimapCameraTarget{};
const bool_t bMinimapCameraJumpPressed =
    !bImGuiMouse &&
    input.IsLButtonPressed() &&
    TryResolveMinimapClickTarget(minimapCameraTarget);
if (bMinimapCameraJumpPressed && m_pCamera)
    m_pCamera->JumpToWorldXZ(minimapCameraTarget);
```

우클릭 이동은 기존대로 유지한다.

```cpp
const bool_t bMoveIntent = bNetworkActive
    ? input.IsRButtonPressed()
    : input.IsRButtonDown();
```

2. 검증

검증 명령:

```powershell
git diff --check
msbuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
```

PDB 잠금이 IDE 때문에 발생하면 검증용으로 아래 옵션을 추가한다.

```powershell
/p:GenerateDebugInformation=false /p:DebugSymbols=false /p:DebugType=None /p:LinkIncremental=false
```

수동 확인:

```text
1. 게임 실행
2. 미니맵 좌클릭
3. 흰 카메라 프레임이 클릭한 위치로 이동하는지 확인
4. 실제 화면 카메라가 해당 XZ 중심으로 이동하는지 확인
5. 일반 우클릭 이동이 기존처럼 동작하는지 확인
```
