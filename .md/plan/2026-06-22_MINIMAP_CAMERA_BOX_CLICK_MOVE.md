# Session - Minimap Camera Box Click Move

## 1. 반영해야 하는 코드

### 본질

미니맵 기능의 원자는 하나다.

```cpp
world <-> minimap uv
```

이 변환 하나로 두 기능이 나온다.

```cpp
camera world center -> minimap uv -> white camera box
mouse screen pos -> minimap uv -> world target -> existing move command
```

UI는 좌표를 보여주고 클릭을 월드 목표로 바꿀 뿐이다. 실제 이동 진실은 기존 이동 파이프라인이 처리한다.

### Client/Public/UI/MinimapPanel.h

미니맵 상태에 카메라 박스 입력값만 추가한다.

```cpp
Vec3 vCameraWorldCenter{};
f32_t fCameraViewHalfWidth = 18.f;
f32_t fCameraViewHalfDepth = 14.f;
bool_t bShowCameraBounds = false;
```

좌표 변환 API는 세 개만 둔다.

```cpp
const MinimapProjection& GetDefaultMinimapProjection();
Vec3 MinimapUvToWorld(const MinimapProjection& Projection, f32_t fU, f32_t fV, f32_t fY = 0.f);
bool_t ProjectWorldToMinimapUv(const MinimapProjection& Projection, const Vec3& vWorldPos, f32_t& fOutU, f32_t& fOutV);
bool_t TryResolveMinimapClickWorldPos(const MinimapFrameState& State, f32_t fMouseX, f32_t fMouseY, Vec3& vOutWorldPos);
```

### Client/Private/UI/MinimapPanel.cpp

투영은 affine basis다.

```cpp
world = uv00 + (uv10 - uv00) * u + (uv01 - uv00) * v;
uv = inverse(world - uv00);
```

렌더는 기존 미니맵 사각형을 기준으로 한다.

```cpp
screen minimap rect = screen size - padding - minimap size;
camera box = project(camera center +/- world half extent);
click uv = (mouse - minimap rect min) / minimap size;
```

### Client/Private/Scene/Scene_InGame.cpp

입력은 새 이동 루트를 만들지 않는다. 기존 이동 처리를 함수 하나로 접는다.

```cpp
bool_t IssuePlayerMoveTarget(Vec3 rawGround, bool_t networkActive, bool_t spawnIndicator);
```

우클릭과 미니맵 좌클릭은 같은 함수를 호출한다.

```cpp
right click ground target -> IssuePlayerMoveTarget(...)
minimap left click target -> IssuePlayerMoveTarget(...)
```

네트워크 모드면 기존 `SendMove`와 예측 보호를 타고, 로컬 모드면 기존 `NavAgentComponent` 목표 갱신을 탄다.

## 2. 검증

코드 검증:

```powershell
git diff --check
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64 /m
```

수동 검증:

```text
1. 게임 실행
2. 카메라 이동 시 미니맵 흰 박스가 같이 이동하는지 확인
3. 미니맵 내부 좌클릭 시 챔피언이 해당 방향으로 이동하는지 확인
4. 우클릭 이동이 기존처럼 동작하는지 확인
```
