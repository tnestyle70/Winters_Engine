# WintersEngine 모듈/클래스 등록 규칙

## 프로젝트 구조
```
Engine/
├── Include/    ← 공개 API (WINTERS_API, Client에 노출)
├── Header/     ← 내부 헤더 (DX11 타입 허용)
├── Code/       ← 구현부
Client/
├── Header/, Code/
Shaders/        ← .hlsl
Shared/         ← Client + Server 공용
```

## 레이어별 클래스 배치

### Engine/Include/ — 공개
| 파일 | 역할 |
|------|------|
| WintersEngine.h | 통합 인클루드 |
| WintersAPI.h | dllexport/import 매크로 |
| WintersTypes.h | int32, float32 등 타입 별칭 |
| WintersMath.h | Vec2/Vec3/Vec4/Mat4 |
| EngineConfig.h | 초기화 설정 |
| IWintersApp.h | 게임 로직 인터페이스 |
| WintersPaths.h | 콘텐츠 경로 해석 |
| CTransform.h | SRT + Dirty Flag |
| CCamera.h | FPS 카메라 |
| CInput.h | 키보드/마우스 입력 |
| TriangleRenderer.h | 삼각형 데모 (pImpl) |
| CubeRenderer.h | 큐브 렌더러 (pImpl) |

### Engine/Header/ — 내부
| 경로 | 역할 |
|------|------|
| Framework/CEngineApp.h | 엔진 메인 루프 |
| Platform/CWin32Window.h | Win32 창 + WndProc |
| Core/CTimer.h | 고해상도 타이머 |
| RHI/CDX11Device.h | DX11 디바이스 + 스왑체인 |
| RHI/DX11/DX11Shader.h | VS/PS 컴파일 |
| RHI/DX11/DX11Buffer.h | VB + IB |
| RHI/DX11/DX11Pipeline.h | InputLayout + RS + DSS |
| RHI/DX11/DX11ConstantBuffer.h | 상수 버퍼 템플릿 |
| RHI/Geometry/CubeGeometry.h | 큐브 정점 데이터 |

---

## vcxproj 등록 규칙

| 파일 종류 | 등록 위치 | 태그 |
|-----------|-----------|------|
| Engine .cpp | Engine.vcxproj | `<ClCompile>` |
| Engine 내부 .h | Engine.vcxproj | `<ClInclude Include="Header\...">` |
| Engine 공개 .h | Engine.vcxproj | `<ClInclude Include="Include\...">` |
| Client .cpp/.h | Client.vcxproj | `<ClCompile>` / `<ClInclude>` |
| .hlsl 셰이더 | Client.vcxproj | `<None Include="..\\Shaders\\...">` |

---

## 타입별 추가 패턴

**새 Renderer 모듈 (pImpl)**: Include/에 공개 헤더 + Code/Renderer/에 구현 + vcxproj 등록
**새 HLSL 셰이더**: Shaders/에 생성 + Client.vcxproj에 None 등록
**새 ECS Component**: ECS/Components/에 순수 데이터 구조체 (main 미머지 상태)
**새 ECS System**: ECS/Systems/에 ISystem 상속 + GetAccess + CommandBuffer
