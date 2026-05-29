# WintersEngine API (Scaffolding 참고)

## 레이어
```
Platform → Core → RHI → Renderer → ECS → Gameplay → Game
```

---

## 공개 API (Engine/Include/)

| 클래스 | 파일 | 핵심 메서드 |
|--------|------|------------|
| WintersMath | WintersMath.h | Vec2/Vec3/Vec4/Mat4, DirectXMath 래퍼 |
| CTransform | CTransform.h | Set/GetPosition/Rotation/Scale, GetWorldMatrix() (Dirty Flag) |
| CCamera | CCamera.h | SetPerspective, Update(dt, input), GetViewProjection() |
| CInput | CInput.h | Get(), IsKeyDown(vKey), GetMouseDeltaX/Y(), IsRButtonDown() |
| CubeRenderer | CubeRenderer.h | Init, UpdateTransform, UpdateCamera, Render, Shutdown (pImpl) |
| TriangleRenderer | TriangleRenderer.h | Init, Render, Shutdown (pImpl, SV_VertexID 기반) |
| IWintersApp | IWintersApp.h | OnInit, OnUpdate, OnRender, OnShutdown |

---

## 내부 API (Engine/Header/)

| 클래스 | 파일 | 역할 |
|--------|------|------|
| CDX11Device | RHI/CDX11Device.h | Device + SwapChain(DISCARD) + BeginFrame/EndFrame |
| DX11Shader | RHI/DX11/DX11Shader.h | Load, Bind/Unbind, GetVSBlob, GetVS/GetPS |
| DX11Buffer | RHI/DX11/DX11Buffer.h | CreateVertex/Index, Bind, DrawIndexed |
| DX11Pipeline | RHI/DX11/DX11Pipeline.h | Create (PosColor), Create3D (PosNormCol) |
| DX11ConstantBuffer\<T\> | RHI/DX11/DX11ConstantBuffer.h | Create, Update, BindVS/PS (16B 정렬) |
| CubeGeometry | RHI/Geometry/CubeGeometry.h | 24v/36i constexpr 큐브 데이터 |

---

## 셰이더

| 파일 | 입력 | cbuffer |
|------|------|---------|
| Triangle.hlsl | SV_VertexID (하드코딩 정점) | 없음 |
| Default3D.hlsl | POSITION + NORMAL + COLOR | b0: ViewProjection, b1: World |

---

## ECS (워크트리 작업 완료, main 미머지)

Entity, ComponentStore\<T\> (Sparse Set), World, ISystem, SystemScheduler, CommandBuffer
Components: Transform, RigidBody, Health, MeshRenderer, AIState, PlayerTag, ChunkData, CollisionResult
Systems: Physics, Render, AI, Collision, Health
